#pragma once
// rich::live — in-place terminal refresh engine (port of Python Rich's `rich.live.Live`).
//
// Semantics mirrored from live.py / live_render.py:
//   - A background std::jthread redraws the current renderable at a fixed rate
//     (default 4 fps, matching Python Live's default refresh_per_second).
//   - Each tick repositions the cursor to the top of the previously drawn region
//     and rewrites every line; if the new frame is shorter than the old one, the
//     leftover trailing lines are erased. This is a deliberate simplification of
//     Python's per-line "changed only" diffing (LiveRender.position_cursor +
//     ERASE_IN_LINE per line): we always rewrite all lines, which is correct and
//     cheap for typical widget sizes.
//
// VT escape strategy (in-place refresh):
//   For a previous frame of height H (> 0) and a new frame of N lines:
//     \x1b[<H-1>F          move cursor up H-1 lines (to first line of region)
//     for each of the max(N, H) lines:
//       \r\x1b[2K          carriage return + erase whole line
//       <line content>     (ANSI styles around styled runs), then \n
//   After the last line the cursor sits on the line below the region, ready for
//   subsequent output. On stop() with transient=true the region is cleared by
//   moving up N-1 lines and erasing each of the N lines (\r\x1b[2K per line).
//
// Non-TTY degradation: when the stream is not an interactive terminal
// (_isatty on Windows / isatty(1) on POSIX), start() degrades to a single final
// plain render of the current renderable (no control codes, no thread); stop()
// is a no-op. This mirrors Python's behavior of printing the final result for
// files/dumb terminals.
//
// Thread safety: update_renderable() swaps the stored renderable under a mutex;
// the refresh loop renders under the same lock and writes the whole frame
// atomically (single flush), so user threads can update from anywhere.

#include <rich_core.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <ostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <io.h>
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#endif

namespace rich::live {

namespace detail {

inline bool stream_is_tty(std::ostream& os) {
    int fd = -1;
#ifdef _WIN32
    if (&os == &std::cout) fd = _fileno(stdout);
    else if (&os == &std::cerr) fd = _fileno(stderr);
#else
    if (&os == &std::cout || &os == &std::clog) fd = 1;
    else if (&os == &std::cerr) fd = 2;
#endif
    if (fd < 0) return false;
#ifdef _WIN32
    return _isatty(fd) != 0;
#else
    return ::isatty(fd) != 0;
#endif
}

// Split rendered segments into visual lines (each without its trailing '\n').
inline std::vector<std::vector<rich::Segment>> segments_to_lines(std::vector<rich::Segment> segs) {
    std::vector<std::vector<rich::Segment>> lines;
    lines.emplace_back();
    for (auto& s : segs) {
        size_t start = 0;
        while (start < s.text.size()) {
            size_t nl = s.text.find('\n', start);
            if (nl == std::string::npos) {
                lines.back().push_back({s.text.substr(start), s.style});
                break;
            }
            lines.back().push_back({s.text.substr(start, nl - start), s.style});
            lines.emplace_back();
            start = nl + 1;
        }
    }
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

} // namespace detail

class Live {
public:
    // renderable: what to show; refresh_per_second: redraw rate (Python default 4.0);
    // transient: if true, erase the live region entirely on stop.
    explicit Live(rich::AnyRenderable renderable,
                  double refresh_per_second = 4.0,
                  bool transient = false,
                  std::ostream& os = std::cout,
                  size_t width = 0)
        : renderable_(std::move(renderable)),
          refresh_per_second_(refresh_per_second > 0 ? refresh_per_second : 4.0),
          transient_(transient),
          os_(os),
          console_(width > 0 ? &owned_console_ : &default_console_),
          owned_console_(os, ConsoleOptions{.width = width}),
          default_console_(os),
          tty_(detail::stream_is_tty(os)) {}

    ~Live() { stop(); }

    Live(const Live&) = delete;
    Live& operator=(const Live&) = delete;

    // Thread-safe swap of the displayed renderable.
    void update_renderable(rich::AnyRenderable r) {
        std::lock_guard<std::mutex> lk(mu_);
        renderable_ = std::move(r);
    }

    // Begin the background refresh loop (or, on non-TTY streams, emit one final
    // plain render immediately). Safe to call multiple times.
    void start() {
        std::lock_guard<std::mutex> lk(mu_);
        if (started_) return;
        started_ = true;

        if (!tty_) {
            // Degrade: single final plain render, no control codes, no thread.
            render_plain_locked();
            return;
        }

        worker_ = std::jthread([this](std::stop_token st) {
            auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(1.0 / refresh_per_second_));
            if (period.count() <= 0) period = std::chrono::milliseconds(250);
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(period);
                if (st.stop_requested()) break;
                refresh_locked();
            }
        });
    }

    // Stop the loop. If transient, clear the screen region; otherwise leave the
    // last frame in place (matching Python's non-transient stop()).
    void stop() {
        std::lock_guard<std::mutex> lk(mu_);
        if (!started_) return;
        started_ = false;

        if (!tty_) return;

        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }

        if (transient_ && prev_height_ > 0) {
            clear_region(prev_height_);
        }
    }

    bool started() const {
        std::lock_guard<std::mutex> lk(mu_);
        return started_;
    }

private:
    // Render the current renderable to lines at the console width. Caller holds mu_.
    std::vector<std::vector<rich::Segment>> current_lines_locked() const {
        Options opt{.width = tty_width()};
        auto segs = renderable_.render(*console_, opt);
        return detail::segments_to_lines(std::move(segs));
    }

    size_t tty_width() const {
        // Console::width() returns the configured width (0 = auto-detect); fall
        // back to terminal detection for interactive use.
        size_t w = console_->width();
        if (w == 0) {
#ifdef _WIN32
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
                int cw = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                if (cw > 0) return static_cast<size_t>(cw);
            }
#else
            struct winsize ws;
            if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return static_cast<size_t>(ws.ws_col);
#endif
        }
        return w > 0 ? w : 80;
    }

    // Plain text render (no ANSI, no control codes) used for non-TTY degradation.
    void render_plain_locked() {
        for (const auto& line : current_lines_locked()) {
            for (const auto& s : line) os_ << s.text;
            os_ << '\n';
        }
        os_.flush();
    }

    // One refresh tick: rewrite the region in place. Caller holds mu_.
    void refresh_locked() {
        auto lines = current_lines_locked();
        if (lines.empty()) lines.push_back({});

        std::string buf;
        if (prev_height_ > 0) {
            buf += "\x1b[" + std::to_string(prev_height_ - 1) + "F";
        }
        size_t total = std::max(lines.size(), prev_height_);
        for (size_t i = 0; i < total; ++i) {
            buf += "\r\x1b[2K";
            if (i < lines.size()) {
                for (const auto& s : lines[i]) {
                    if (style_active(s.style)) buf += style_to_ansi(s.style);
                    buf += s.text;
                    if (style_active(s.style)) buf += "\x1b[0m";
                }
            }
            buf += "\n";
        }
        os_ << buf;
        os_.flush();
        prev_height_ = lines.size();
    }

    // Erase a region of `height` lines whose bottom edge is at the current cursor
    // position (cursor is on the line just below the region after a refresh).
    void clear_region(size_t height) {
        std::string buf;
        buf += "\x1b[" + std::to_string(height - 1) + "F";
        for (size_t i = 0; i < height; ++i) buf += "\r\x1b[2K\n";
        os_ << buf;
        os_.flush();
        prev_height_ = 0;
    }

    static bool style_active(const rich::Style& s) {
        return s.bold || s.dim || s.italic || s.underline || s.reverse ||
               s.color.type != rich::Color::Type::None ||
               s.bg_color.type != rich::Color::Type::None;
    }

    static std::string style_to_ansi(const rich::Style& s) {
        std::string out;
        if (s.bold) out += "\x1b[1m";
        if (s.dim) out += "\x1b[2m";
        if (s.italic) out += "\x1b[3m";
        if (s.underline) out += "\x1b[4m";
        if (s.reverse) out += "\x1b[7m";
        if (s.color.type == rich::Color::Type::Standard) {
            out += "\x1b[" + std::to_string(s.color.ansi) + "m";
        } else if (s.color.type == rich::Color::Type::Rgb) {
            out += "\x1b[38;2;" + std::to_string(s.color.r) + ";" +
                   std::to_string(s.color.g) + ";" + std::to_string(s.color.b) + "m";
        }
        if (s.bg_color.type == rich::Color::Type::Standard) {
            out += "\x1b[" + std::to_string(s.bg_color.ansi + 10) + "m";
        } else if (s.bg_color.type == rich::Color::Type::Rgb) {
            out += "\x1b[48;2;" + std::to_string(s.bg_color.r) + ";" +
                   std::to_string(s.bg_color.g) + ";" + std::to_string(s.bg_color.b) + "m";
        }
        return out;
    }

    rich::AnyRenderable renderable_;
    double refresh_per_second_;
    bool transient_;
    std::ostream& os_;
    bool tty_;

    mutable std::mutex mu_;
    bool started_{false};
    std::jthread worker_;
    size_t prev_height_{0};

    // Console used only for width/render context (never writes through it).
    Console owned_console_;   // used when an explicit width was given
    Console default_console_; // used otherwise (width 0 = auto-detect)
    Console* console_{nullptr};
};

} // namespace rich::live
