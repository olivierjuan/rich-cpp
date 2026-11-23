#pragma once
#include <rich_core.h>
#include <SpinnerTable.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdio>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace rich::status {

// Python rich.spinner.Spinner: frames + interval from the 73-entry registry
// (SpinnerTable.h, generated from rich._spinners.SPINNERS), speed factor,
// frame_no_offset continuity, and time-based frame selection.
//   render(time): first call fixes start_time (so the first frame is always
//   frame 0); frame_no = ((t - t0) * speed) / (interval_ms / 1000) + offset;
//   frame = frames[int(frame_no) % len(frames)] (Python int() truncates).
// update(speed) mirrors Python's deferred speed change: applied on the NEXT
// render, preserving frame position via frame_no_offset.
// Unknown spinner names throw std::invalid_argument (Python: KeyError).
class Spinner {
public:
    // Python: Spinner(name, text="", *, style=None, speed=1.0).
    Spinner(std::string name = "dots", std::string text = "",
            std::string style = "", double speed = 1.0)
        : text_(std::move(text)), style_(std::move(style)), speed_(speed) {
        const SpinnerDef* def = find_spinner_def(name);
        if (!def)
            throw std::invalid_argument("no spinner called '" + name + "'");
        name_ = def->name;
        interval_ms_ = def->interval_ms;
        frames_ = def->frames;
    }

    // Custom frames/interval (Python: spinner.frames = [...];
    // spinner.interval = ...).
    void set_frames(std::vector<std::string> frames, double interval_ms) {
        frames_ = std::move(frames);
        interval_ms_ = interval_ms;
    }

    // Deferred speed change (Python Spinner.update(speed=...)); applied on
    // the next render() call, preserving the frame position.
    void set_speed(double speed) {
        if (speed != 0.0) pending_speed_ = speed;
    }

    // Python Spinner.render(time): frame glyph, or "frame text" when a
    // text renderable is attached. Non-const: fixes start_time on first call
    // and applies deferred speed updates (Python render() mutates too).
    std::string render(double time) {
        if (frames_.empty()) return "";
        const double t0 = (start_time_ < 0.0) ? time : start_time_;
        if (start_time_ < 0.0) start_time_ = t0;
        double frame_no = ((time - t0) * speed_) / (interval_ms_ / 1000.0) +
                          frame_no_offset_;
        if (pending_speed_ != 0.0) {
            frame_no_offset_ = frame_no;
            start_time_ = time;
            speed_ = pending_speed_;
            pending_speed_ = 0.0;
        }
        const long long idx = static_cast<long long>(frame_no);
        const long long n = static_cast<long long>(frames_.size());
        std::string frame = frames_[(idx % n + n) % n];
        if (text_.empty()) return frame;
        return frame + " " + text_;
    }

    const std::string& name() const { return name_; }
    const std::string& style() const { return style_; }
    double interval_ms() const { return interval_ms_; }
    const std::vector<std::string>& frames() const { return frames_; }

private:
    std::string name_;
    std::vector<std::string> frames_;
    double interval_ms_{80.0};
    std::string text_;
    std::string style_;
    double speed_{1.0};
    double frame_no_offset_{0.0};
    double pending_speed_{0.0};
    mutable double start_time_{-1.0};
};

// Status: single-line spinner + description widget.
//
// Static rendering (renderable interface): emits exactly one line
//   <frame_glyph> <description with markup applied>\n
// using frame 0 of the selected spinner — deterministic and oracle-comparable.
//
// Live animation (start/stop): a background std::jthread rewrites the same
// terminal line in place every `interval` ms using VT escapes
//   \r\x1b[2K  (carriage return to column 0, then erase entire line)
// followed by the new frame. On stop the line is erased with the same
// sequence. This mirrors what Python Rich achieves via Console.control
// (cursor home + clear), but is simpler and works on any VT-capable stream.
// The loop always runs when start() is called (no TTY gating); callers that
// pipe output should simply not call start().
class Status {
public:
    // description may contain markup tags (e.g. "[bold]Loading[/] data").
    // spinner_name: any of the 73 registered spinners (default "dots").
    // speed: frames per second override; 0 = use the spinner's native interval.
    // Unknown spinner names fall back to "dots" (Status is a convenience
    // widget; rich::status::Spinner throws for unknown names).
    Status(std::string description, const char* spinner_name = "dots", double speed = 0)
        : description_(std::move(description)) {
        const SpinnerDef* def = find_spinner_def(spinner_name);
        if (!def) def = find_spinner_def("dots");
        interval_ms_ = def->interval_ms;
        frames_ = def->frames;
        if (speed > 0.0) {
            interval_ms_ = static_cast<double>(1000) / speed;
            if (interval_ms_ < 1.0) interval_ms_ = 1.0;
        }
    }

    ~Status() { stop(); }

    // Copyable: copies share the same animation state via a control block.
    // The jthread/mutex live in a heap-allocated Control so that copying a
    // Status (required by AnyRenderable's Model<T>) does not attempt to copy
    // the non-copyable std::jthread directly.
    struct Control {
        std::atomic<bool> running{false};
        size_t next_frame{0};
        std::jthread worker;
        std::mutex mutex;
        rich::Console* console{nullptr};
    };
    std::shared_ptr<Control> ctrl_;

    void start(rich::Console& con) {
        if (!ctrl_) ctrl_ = std::make_shared<Control>();
        std::lock_guard lock(ctrl_->mutex);
        if (ctrl_->running.load()) return;
        ctrl_->console = &con;
        ctrl_->running = true;
        ctrl_->worker = std::jthread([this](std::stop_token st) {
            auto c = ctrl_;
            // First frame: plain newline-terminated line (same as renderable output).
            write_line(frame(0));
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<long long>(interval_ms_)));
                if (st.stop_requested()) break;
                size_t idx;
                {
                    std::lock_guard l(c->mutex);
                    idx = c->next_frame;
                    c->next_frame = (c->next_frame + 1) % frames_.size();
                }
                // In-place refresh: carriage return + erase line, then new frame.
                emit_raw("\r\x1b[2K");
                write_line(frame(idx));
            }
        });
    }

    void stop() {
        if (!ctrl_) return;
        {
            std::lock_guard lock(ctrl_->mutex);
            if (!ctrl_->running.load()) return;
            ctrl_->running = false;
            if (ctrl_->worker.joinable()) ctrl_->worker.request_stop();
        }
        // Join outside the lock: the worker may try to acquire the mutex on
        // its final frame advance, so holding it here would deadlock.
        if (ctrl_->worker.joinable()) ctrl_->worker.join();
        // Safe to erase now: worker has exited and no longer touches ctrl_.
        {
            std::lock_guard lock(ctrl_->mutex);
            if (ctrl_->console) {
                emit_raw("\r\x1b[2K");
            }
            ctrl_->console = nullptr;
        }
    }

    bool started() const { return ctrl_ && ctrl_->running.load(); }

    // Deterministic frame text (glyph + space + raw description markup), no newline.
    std::string frame(size_t index) const {
        if (frames_.empty()) return description_;
        return frames_[index % frames_.size()] + " " + description_;
    }

    const std::string& description() const { return description_; }

private:
    // Write one full line (markup-expanded) through the console.
    void write_line(const std::string& line) {
        if (!ctrl_ || !ctrl_->console) return;
        ctrl_->console->print(rich::AnyRenderable(rich::StyledText{line}));
    }

    // Emit raw VT escapes through the owning console's stream (write_raw),
    // so output lands on the same stream as the spinner line itself. Escape
    // sequences are NOT recorded in export_text() — live mode is never used
    // together with record=true in the oracle suite.
    void emit_raw(const std::string& seq) {
        if (ctrl_ && ctrl_->console) ctrl_->console->write_raw(seq);
    }

    std::string description_;
    double interval_ms_{80.0};
    std::vector<std::string> frames_;
};

// Renderable ADL customization points: renders frame 0 as one line.
inline std::vector<rich::Segment> render_item(const Status& s, const rich::Console&, const rich::Options&) {
    // Python Spinner.render returns a Text (end="\n") -> trailing newline.
    auto segments = rich::parse_markup(s.frame(0));
    if (segments.empty()) segments.push_back({s.description(), {}});
    segments.push_back({"\n", rich::detail::kNoneStyle()});
    return segments;
}

inline std::pair<size_t, size_t> measure_item(const Status& s, const rich::Console&, const rich::Options&) {
    // Glyph (1 cell) + space (1 cell) + description width.
    return {s.description().size() + 2, 1};
}

} // namespace rich::status
