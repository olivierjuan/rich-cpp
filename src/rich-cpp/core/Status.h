#pragma once
#include <rich_core.h>
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

// Spinner frame tables (subset of Python Rich's SPINNERS).
// "dots": braille dots, 80ms interval (matches rich._spinners.SPINNERS["dots"]).
// "line": classic line spinner, 130ms interval.
inline const std::vector<std::pair<std::string, std::pair<int, std::string>>>& spinner_table() {
    static const std::vector<std::pair<std::string, std::pair<int, std::string>>> table = {
        {"dots", {80, "\xe2\xa0\x8b" "\xe2\xa0\x99" "\xe2\xa0\xb9" "\xe2\xa0\xb8" "\xe2\xa0\xbc" "\xe2\xa1\xb4" "\xe2\xa1\xa6" "\xe2\xa1\xa7" "\xe2\xa0\x87" "\xe2\xa1\x8f"}},
        {"line", {130, "-\\|/"}},
    };
    return table;
}

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
    // spinner_name: "dots" (default) or "line".
    // speed: frames per second override; 0 = use the spinner's native interval.
    Status(std::string description, const char* spinner_name = "dots", double speed = 0)
        : description_(std::move(description)) {
        for (const auto& [name, spec] : spinner_table()) {
            if (name == spinner_name) {
                interval_ms_ = spec.first;
                frames_ = spec.second;
                break;
            }
        }
        if (speed > 0.0) {
            interval_ms_ = static_cast<int>(1000.0 / speed);
            if (interval_ms_ < 1) interval_ms_ = 1;
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
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
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
    // The dots spinner frames are multi-byte UTF-8 codepoints; we store them as
    // a flat byte string and advance by decoding each codepoint's length.
    std::string frame(size_t index) const {
        // Walk the frames_ byte string, counting codepoints to find frame `index`.
        size_t cp = 0;
        size_t pos = 0;
        while (pos < frames_.size() && cp < index) {
            unsigned char c = static_cast<unsigned char>(frames_[pos]);
            if (c < 0x80) pos += 1;
            else if ((c & 0xE0) == 0xC0) pos += 2;
            else if ((c & 0xF0) == 0xE0) pos += 3;
            else if ((c & 0xF8) == 0xF0) pos += 4;
            else pos += 1;
            ++cp;
        }
        // Advance past the target codepoint to get its byte range.
        size_t end = pos;
        if (pos < frames_.size()) {
            unsigned char c = static_cast<unsigned char>(frames_[pos]);
            if (c < 0x80) end = pos + 1;
            else if ((c & 0xE0) == 0xC0) end = pos + 2;
            else if ((c & 0xF0) == 0xE0) end = pos + 3;
            else if ((c & 0xF8) == 0xF0) end = pos + 4;
            else end = pos + 1;
        }
        std::string result;
        result.append(frames_, pos, end - pos);
        result.push_back(' ');
        result.append(description_);
        return result;
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
    int interval_ms_{80};
    std::string frames_{"\xe2\xa0\x8b" "\xe2\xa0\x99" "\xe2\xa0\xb9" "\xe2\xa0\xb8" "\xe2\xa0\xbc" "\xe2\xa1\xb4" "\xe2\xa1\xa6" "\xe2\xa1\xa7" "\xe2\xa0\x87" "\xe2\xa1\x8f"};
};

// Renderable ADL customization points: renders frame 0 as one line.
inline std::vector<rich::Segment> render_item(const Status& s, const rich::Console&, const rich::Options&) {
    auto segments = rich::parse_markup(s.frame(0));
    if (segments.empty()) segments.push_back({s.description(), {}});
    return segments;
}

inline std::pair<size_t, size_t> measure_item(const Status& s, const rich::Console&, const rich::Options&) {
    // Glyph (1 cell) + space (1 cell) + description width.
    return {s.description().size() + 2, 1};
}

} // namespace rich::status
