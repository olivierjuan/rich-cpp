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
#include <cstring>
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
    // transient: if true, erase the live region entirely on stop;
    // screen: alternate-screen mode (Python forces transient=True when screen);
    // vertical_overflow: "crop" | "ellipsis" | "visible" (Python default "ellipsis";
    //   forced to "visible" for the final stop frame, matching Live.stop()).
    explicit Live(rich::AnyRenderable renderable,
                  double refresh_per_second = 4.0,
                  bool transient = false,
                  std::ostream& os = std::cout,
                  size_t width = 0,
                  bool screen = false,
                  const char* vertical_overflow = "ellipsis")
        : renderable_(std::move(renderable)),
          refresh_per_second_(refresh_per_second > 0 ? refresh_per_second : 4.0),
          transient_(transient || screen),
          screen_(screen),
          vertical_overflow_(vertical_overflow ? vertical_overflow : "ellipsis"),
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
            // Degrade: Python's process_renderables emits the final frame only
            // when not transient (screen forces transient). LiveRender renders
            // without a trailing newline, so the frame is joined with "\n"
            // and nothing follows the last line.
            if (!transient_) render_plain_locked();
            return;
        }

        if (screen_) {
            // Alternate screen (Python Console.set_alt_screen(True)).
            os_ << "\x1b[?1049h";
            os_.flush();
            alt_screen_ = true;
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

        // Python stop(): force vertical_overflow="visible", render one final
        // frame, then leave the alternate screen (if any).
        if (!alt_screen_) {
            const char* saved = vertical_overflow_;
            vertical_overflow_ = "visible";
            refresh_locked();
            vertical_overflow_ = saved;
        }
        if (alt_screen_) {
            os_ << "\x1b[?1049l";
            os_.flush();
            alt_screen_ = false;
        }

        if (transient_ && prev_height_ > 0 && !screen_) {
            clear_region(prev_height_);
        }
    }

    bool started() const {
        std::lock_guard<std::mutex> lk(mu_);
        return started_;
    }

    // Vertical overflow handling (port of LiveRender.__rich_console__):
    //   "crop"     -> keep the first `height` lines;
    //   "ellipsis" -> keep the first `height - 1` lines and append one centered
    //                 "..." line (Text("...", justify="center") at `width`);
    //   "visible"  -> unchanged.
    // Lines carry no trailing newlines. Exposed so callers (and the oracle)
    // can drive the same logic LiveRender uses.
    static std::vector<std::vector<rich::Segment>> apply_vertical_overflow(
        std::vector<std::vector<rich::Segment>> lines, size_t height,
        const char* method, size_t width = 80) {
        if (method && height > 0 && lines.size() > height) {
            if (std::strcmp(method, "crop") == 0) {
                lines.resize(height);
            } else if (std::strcmp(method, "ellipsis") == 0) {
                lines.resize(height > 0 ? height - 1 : 0);
                size_t text_width = std::min<size_t>(width, 3);
                size_t lead = (width - text_width) / 2;
                size_t trail = width - text_width - lead;
                std::string line(text_width, ' ');
                for (size_t i = 0; i < text_width; ++i) line[i] = '.';
                std::string padded(lead, ' ');
                padded += line;
                padded.append(trail, ' ');
                lines.push_back({{padded, {}}});
            }
            // "visible" (or unknown): unchanged
        }
        return lines;
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

    // Plain text render (no ANSI, no control codes) used for non-TTY
    // degradation. Mirrors Python's final frame: lines joined by "\n" with
    // NO trailing newline (LiveRender renders pad=False, end=""; the hook
    // appends it to a print(Control()) that carries no end of its own).
    void render_plain_locked() {
        auto lines = current_lines_locked();
        for (size_t i = 0; i < lines.size(); ++i) {
            for (const auto& s : lines[i]) os_ << s.text;
            if (i + 1 < lines.size()) os_ << '\n';
        }
        os_.flush();
    }

    // One refresh tick: rewrite the region in place. Caller holds mu_.
    // Vertical overflow is applied at the console height (Python applies it in
    // LiveRender.__rich_console__ against options.size.height).
    void refresh_locked() {
        auto lines = apply_vertical_overflow(current_lines_locked(), console_height(),
                                             vertical_overflow_);
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
                    const std::string sgr = style_sgr(s.style, ColorSystem::Truecolor);
                    if (!sgr.empty()) buf += sgr;
                    buf += s.text;
                    if (!sgr.empty()) buf += "\x1b[0m";
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

    // Console height for overflow decisions (Python: options.size.height;
    // Console.size.height falls back to 25 when unset/non-terminal).
    size_t console_height() const {
        size_t h = console_->height();
        return h > 0 ? h : 25;
    }

    rich::AnyRenderable renderable_;
    double refresh_per_second_;
    bool transient_;
    bool screen_{false};
    const char* vertical_overflow_{"ellipsis"};
    std::ostream& os_;
    bool tty_;

    mutable std::mutex mu_;
    bool started_{false};
    std::jthread worker_;
    size_t prev_height_{0};
    bool alt_screen_{false};

    // Console used only for width/render context (never writes through it).
    Console owned_console_;   // used when an explicit width was given
    Console default_console_; // used otherwise (width 0 = auto-detect)
    Console* console_{nullptr};
};

} // namespace rich::live
