#pragma once
// rich::progress — port of Python rich.progress + rich.progress_bar.
//
//   rich::progress_bar::ProgressBar  — the bar/pulse widget (━/╸/╺ bar model,
//       pulse animation with cosine-blended truecolor segments, PULSE_SIZE=20,
//       offset = int(-t * 15) % 20).
//   rich::progress::Task             — task state (total/completed, start/stop
//       times, finished_time/speed, rolling speed samples).
//   rich::progress::ProgressColumn   — base + the column widgets:
//       RenderableColumn, SpinnerColumn, TextColumn, TaskProgressColumn,
//       BarColumn, TimeElapsedColumn, TimeRemainingColumn, FileSizeColumn,
//       TotalFileSizeColumn, MofNCompleteColumn, DownloadColumn,
//       TransferSpeedColumn.
//   rich::progress::Progress         — task table (Table.grid padding=(0,1),
//       expand), add_task/start_task/stop_task/update/advance.
//
// Determinism: the clock is injected as `now` (seconds, double) into
// add_task/update/advance/render — the same pattern as ProgressTracker. With
// a frozen clock the output is byte-stable (spinner frame 0, fixed pulse
// offset, fixed speed samples).
//
// The tasks table renders through rich::grid_detail::render_grid_table
// (Table.grid(box=None, padding=(0,1), collapse_padding=True, pad_edge=False));
// TextColumn/str cells get Column(no_wrap=True).

#include <rich_core.h>
#include <unicode/Unicode.h>
#include <panel/GridTable.h>
#include <Status.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

// ── rich.progress_bar.ProgressBar ────────────────────────────────────────────

namespace rich::progress_bar {

class ProgressBar {
public:
    struct Options {
        std::optional<double> total{100.0};
        double completed{0.0};
        std::optional<size_t> width{};
        bool pulse{false};
        std::string style{"bar.back"};
        std::string complete_style{"bar.complete"};
        std::string finished_style{"bar.finished"};
        std::string pulse_style{"bar.pulse"};
        std::optional<double> animation_time{};
    };

    explicit ProgressBar(Options options = {}) : o_(std::move(options)) {}

    // Python __rich_measure__: (width, width) or (4, max_width).
    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options& opt) const {
        const size_t max_w = (opt.width > 0) ? opt.width : 0;
        if (o_.width.has_value()) return {*o_.width, *o_.width};
        return {4, max_w};
    }

    std::vector<rich::Segment> render(const rich::Console& con,
                                      const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        const size_t max_w = (opt.width > 0) ? opt.width : 80;
        const size_t width = (o_.width.has_value())
                                 ? std::min(*o_.width, max_w)
                                 : max_w;
        const bool ascii = con.legacy_windows();
        if (o_.pulse || !o_.total.has_value()) {
            render_pulse(con, width, ascii, out);
            return out;
        }

        const double total = *o_.total;
        const double completed =
            (total > 0) ? std::min(total, std::max(0.0, o_.completed)) : 0.0;
        const char* bar = ascii ? "-" : "\xe2\x94\x81";       // ━
        const char* half_right = ascii ? " " : "\xe2\x95\xb8"; // ╸
        const char* half_left = ascii ? " " : "\xe2\x95\xbA";  // ╺

        const long long complete_halves =
            (total > 0.0)
                ? static_cast<long long>(static_cast<double>(width) * 2.0 * completed / total)
                : static_cast<long long>(width) * 2;
        const size_t bar_count = static_cast<size_t>(complete_halves / 2);
        const size_t half_bar_count = static_cast<size_t>(complete_halves % 2);

        rich::Style back_style;
        try {
            back_style = con.get_style(o_.style, "");
        } catch (...) {
        }
        const bool is_finished =
            !o_.total.has_value() || o_.completed >= *o_.total;
        rich::Style complete_style;
        try {
            complete_style =
                con.get_style(is_finished ? o_.finished_style : o_.complete_style, "");
        } catch (...) {
        }

        if (bar_count) {
            std::string s;
            for (size_t i = 0; i < bar_count; ++i) s += bar;
            out.push_back({s, complete_style});
        }
        if (half_bar_count) {
            std::string s;
            for (size_t i = 0; i < half_bar_count; ++i) s += half_right;
            out.push_back({s, complete_style});
        }

        // Back bars only with an active color system and colors enabled
        // (Python: if not console.no_color / color_system is not None).
        if (!con.no_color() && con.active_color_system().has_value()) {
            size_t remaining_bars = (width >= bar_count + half_bar_count)
                                        ? width - bar_count - half_bar_count
                                        : 0;
            if (remaining_bars && half_bar_count == 0 && bar_count) {
                out.push_back({half_left, back_style});
                remaining_bars -= 1;
            }
            if (remaining_bars) {
                std::string s;
                for (size_t i = 0; i < remaining_bars; ++i) s += bar;
                out.push_back({s, back_style});
            }
        }
        return out;
    }

private:
    Options o_;

    static void color_to_triplet(const rich::Color& c, int& r, int& g, int& b) {
        switch (c.type) {
        case rich::Color::Type::Rgb:
            r = c.r; g = c.g; b = c.b;
            break;
        case rich::Color::Type::Charcoal:
            rich::Color::charcoal_rgb(c.ansi, r, g, b);
            break;
        default:
            r = g = b = 0;
            break;
        }
    }

    // Python _get_pulse_segments: PULSE_SIZE one-char segments.
    std::vector<rich::Segment> pulse_segments(const rich::Console& con, bool ascii) const {
        constexpr int PULSE_SIZE = 20;
        const char* bar = ascii ? "-" : "\xe2\x94\x81";
        rich::Style fore_style, back_style;
        try {
            fore_style = con.get_style(o_.pulse_style, "white");
            back_style = con.get_style(o_.style, "black");
        } catch (...) {
        }

        if (!con.active_color_system().has_value() || con.no_color()) {
            std::vector<rich::Segment> segs;
            for (int i = 0; i < PULSE_SIZE / 2; ++i) segs.push_back({bar, fore_style});
            for (int i = 0; i < PULSE_SIZE - PULSE_SIZE / 2; ++i)
                segs.push_back({con.no_color() ? std::string(1, ' ') : std::string(bar),
                                back_style});
            return segs;
        }

        int fr = 255, fg = 0, fb = 255; // Python default when style.color is None
        if (fore_style.color.type != rich::Color::Type::None)
            color_to_triplet(fore_style.color, fr, fg, fb);
        int br = 0, bg = 0, bb = 0;
        if (back_style.color.type != rich::Color::Type::None)
            color_to_triplet(back_style.color, br, bg, bb);

        std::vector<rich::Segment> segs;
        segs.reserve(PULSE_SIZE);
        for (int index = 0; index < PULSE_SIZE; ++index) {
            const double position = static_cast<double>(index) / PULSE_SIZE;
            const double fade = 0.5 + std::cos(position * std::numbers::pi * 2.0) / 2.0;
            // Python blend_rgb: int(r1 + (r2 - r1) * cross_fade)
            const int r = std::min(255, std::max(0, static_cast<int>(fr + (br - fr) * fade)));
            const int g = std::min(255, std::max(0, static_cast<int>(fg + (bg - fg) * fade)));
            const int b = std::min(255, std::max(0, static_cast<int>(fb + (bb - fb) * fade)));
            rich::Style style;
            style.color = rich::Color::rgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                           static_cast<uint8_t>(b));
            segs.push_back({std::string(bar), style});
        }
        return segs;
    }

    // Python _render_pulse.
    void render_pulse(const rich::Console& con, size_t width, bool ascii,
                      std::vector<rich::Segment>& out) const {
        constexpr int PULSE_SIZE = 20;
        auto pulse = pulse_segments(con, ascii);
        const size_t segment_count = pulse.size();
        const double current_time =
            o_.animation_time.has_value() ? *o_.animation_time : 0.0;
        if (segment_count == 0 || width == 0) return;
        size_t reps = width / segment_count + 2;
        std::vector<rich::Segment> segments;
        segments.reserve(segment_count * reps);
        for (size_t r = 0; r < reps; ++r)
            for (auto& s : pulse) segments.push_back(s);
        const long long raw = static_cast<long long>(-current_time * 15.0);
        const size_t offset = static_cast<size_t>((raw % PULSE_SIZE + PULSE_SIZE) % PULSE_SIZE);
        for (size_t i = 0; i < width && offset + i < segments.size(); ++i)
            out.push_back(segments[offset + i]);
    }
};

inline std::vector<rich::Segment> render_item(const ProgressBar& p, const rich::Console& c,
                                              const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const ProgressBar& p, const rich::Console& c,
                                              const rich::Options& o) {
    return p.measure(c, o);
}

} // namespace rich::progress_bar

// ── rich.filesize helpers (decimal/binary + unit picking) ────────────────────

namespace rich::filesize_detail {

inline std::string comma_int(int64_t v) {
    std::string s = std::to_string(v);
    std::string out;
    int count = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (count && count % 3 == 0) out.push_back(',');
        out.push_back(*it);
        ++count;
    }
    return std::string(out.rbegin(), out.rend());
}

// Python f"{value:,.{precision}f}" — fixed-point with thousands separator.
inline std::string comma_fixed(double value, int precision) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, value);
    std::string s = buf;
    size_t dot = s.find('.');
    std::string int_part = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string frac = (dot == std::string::npos) ? "" : s.substr(dot);
    std::string sign;
    size_t start = 0;
    if (!int_part.empty() && int_part[0] == '-') {
        sign = "-";
        start = 1;
    }
    std::string grouped;
    int count = 0;
    for (size_t i = int_part.size(); i > start; --i) {
        if (count && count % 3 == 0) grouped.push_back(',');
        grouped.push_back(int_part[i - 1]);
        ++count;
    }
    return sign + std::string(grouped.rbegin(), grouped.rend()) + frac;
}

// Python filesize.pick_unit_and_suffix.
inline std::pair<int64_t, std::string>
pick_unit_and_suffix(int64_t size, const std::vector<std::string>& suffixes, int64_t base) {
    int64_t unit = 1;
    for (auto& suffix : suffixes) {
        if (size < unit * base) return {unit, suffix};
        unit *= base;
    }
    return {unit / base, suffixes.back()};
}

// Python filesize.decimal (SI prefixes, 1000 base).
inline std::string decimal(int64_t size, int precision = 1, const char* separator = " ") {
    if (size == 1) return "1 byte";
    if (size < 1000) return comma_int(size) + " bytes";
    static const std::vector<std::string> suffixes = {"kB", "MB", "GB", "TB",
                                                      "PB", "EB", "ZB", "YB"};
    int64_t unit = 1000 * 1000;
    const char* suffix = suffixes.front().c_str();
    for (size_t i = 0; i < suffixes.size(); ++i) {
        if (size < unit) {
            suffix = suffixes[i].c_str();
            break;
        }
        unit *= 1000;
    }
    const double value = static_cast<double>(1000) * size / unit;
    return comma_fixed(value, precision) + separator + suffix;
}

// Python filesize.binary (IEC prefixes, 1024 base).
inline std::string binary(int64_t size, int precision = 1, const char* separator = " ") {
    if (size == 1) return "1 byte";
    if (size < 1024) return comma_int(size) + " bytes";
    static const std::vector<std::string> suffixes = {"KiB", "MiB", "GiB", "TiB",
                                                      "PiB", "EiB", "ZiB", "YiB"};
    int64_t unit = 1024LL * 1024;
    const char* suffix = suffixes.front().c_str();
    for (size_t i = 0; i < suffixes.size(); ++i) {
        if (size < unit) {
            suffix = suffixes[i].c_str();
            break;
        }
        unit *= 1024;
    }
    const double value = static_cast<double>(1024) * size / unit;
    return comma_fixed(value, precision) + separator + suffix;
}

} // namespace rich::filesize_detail

// ── rich.progress.Task ───────────────────────────────────────────────────────

namespace rich::progress {

using TaskId = int;

struct Task {
    TaskId id{0};
    std::string description;
    std::optional<double> total; // nullopt = unknown (pulsing bar)
    double completed{0.0};
    bool visible{true};
    std::optional<double> start_time;
    std::optional<double> stop_time;
    std::optional<double> finished_time;
    std::optional<double> finished_speed;
    std::deque<std::pair<double, double>> samples; // (timestamp, delta)

    bool started() const { return start_time.has_value(); }
    bool finished() const { return finished_time.has_value(); }

    std::optional<double> remaining() const {
        if (!total.has_value()) return std::nullopt;
        return *total - completed;
    }

    std::optional<double> elapsed(double now) const {
        if (!start_time.has_value()) return std::nullopt;
        if (stop_time.has_value()) return *stop_time - *start_time;
        return now - *start_time;
    }

    double percentage() const {
        if (!total.has_value() || *total == 0.0) return 0.0;
        double p = (completed / *total) * 100.0;
        return std::min(100.0, std::max(0.0, p));
    }

    // Python Task.speed: sum of sample deltas after the first, divided by the
    // timestamp span; None when < 2 samples or zero span.
    std::optional<double> speed() const {
        if (!start_time.has_value() || samples.empty()) return std::nullopt;
        const double total_time =
            samples.back().first - samples.front().first;
        if (total_time == 0.0) return std::nullopt;
        double total_completed = 0.0;
        bool first = true;
        for (auto& [ts, delta] : samples) {
            (void)ts;
            if (first) {
                first = false;
                continue;
            }
            total_completed += delta;
        }
        return total_completed / total_time;
    }

    // Python Task.time_remaining: 0 finished; ceil(remaining / speed);
    // None without speed/total.
    std::optional<double> time_remaining() const {
        if (finished()) return 0.0;
        auto sp = speed();
        if (!sp.has_value() || *sp <= 0.0) return std::nullopt;
        auto rem = remaining();
        if (!rem.has_value()) return std::nullopt;
        return std::ceil(*rem / *sp);
    }
};

// ── column formatting helpers ────────────────────────────────────────────────

// str(timedelta(seconds=N)) — "H:MM:SS", "N day(s), H:MM:SS".
inline std::string timedelta_str(double seconds) {
    const long long total = static_cast<long long>(seconds);
    const long long days = total / 86400;
    const long long rem = total % 86400;
    const long long h = rem / 3600;
    const long long m = (rem % 3600) / 60;
    const long long s = rem % 60;
    char buf[32];
    if (days > 0)
        std::snprintf(buf, sizeof(buf), "%lld day%s, %lld:%02lld:%02lld", days,
                      days == 1 ? "" : "s", h, m, s);
    else
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", h, m, s);
    return buf;
}

// Python str.format with a Task: replaces `{task.field[:spec]}` where field
// is one of description/completed/total/percentage/finished_time/elapsed/
// time_remaining. Spec: [align][width][.prec][f|d]. Unknown fields pass
// through verbatim (like Python's format would need them as attributes).
inline std::string format_task_field(const std::string& name, const Task& t,
                                     double now, const std::string& spec) {
    std::string align;
    size_t width = 0;
    int prec = -1;
    char type = 0;
    size_t i = 0;
    if (i < spec.size() &&
        (spec[i] == '<' || spec[i] == '>' || spec[i] == '=' || spec[i] == '^'))
        align = spec.substr(0, 1), ++i;
    while (i < spec.size() && spec[i] >= '0' && spec[i] <= '9') {
        width = width * 10 + (spec[i] - '0');
        ++i;
    }
    if (i < spec.size() && spec[i] == '.') {
        ++i;
        prec = 0;
        while (i < spec.size() && spec[i] >= '0' && spec[i] <= '9') {
            prec = prec * 10 + (spec[i] - '0');
            ++i;
        }
    }
    if (i < spec.size()) type = spec[i];

    auto pad_to = [&](std::string s) {
        if (align.empty() || s.size() >= width) return s;
        const size_t gap = width - s.size();
        if (align == ">") return std::string(gap, ' ') + s;
        if (align == "<") return s + std::string(gap, ' ');
        if (align == "^") {
            const size_t l = gap / 2;
            return std::string(l, ' ') + s + std::string(gap - l, ' ');
        }
        if (align == "=") return s; // no numeric fill for plain fields
        return s;
    };

    if (name == "description") return t.description;
    if (name == "finished_time")
        return t.finished_time.has_value()
                   ? timedelta_str(*t.finished_time)
                   : std::string("None");
    auto numeric = [&](std::optional<double> v) -> std::string {
        if (!v.has_value()) return "None";
        double d = *v;
        std::string s;
        if (type == 'd') {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(d));
            s = buf;
        } else if (type == 'f' || prec >= 0) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.*f", (prec >= 0) ? prec : 6, d);
            s = buf;
        } else {
            // Python str(float) — shortest round-trip; integer values show
            // one decimal ("42.0").
            if (d == std::floor(d) && std::fabs(d) < 1e15) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.1f", d);
                s = buf;
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g", d);
                s = buf;
            }
        }
        return pad_to(s);
    };
    if (name == "completed") return numeric(std::optional<double>(t.completed));
    if (name == "total") return numeric(t.total);
    if (name == "percentage") return numeric(std::optional<double>(t.percentage()));
    if (name == "elapsed") return numeric(t.elapsed(now));
    if (name == "time_remaining") return numeric(t.time_remaining());
    return "{" + name + "}"; // unknown: leave verbatim
}

inline std::string format_task(const std::string& fmt, const Task& t, double now) {
    std::string out;
    size_t i = 0;
    while (i < fmt.size()) {
        if (fmt[i] != '{') {
            out.push_back(fmt[i]);
            ++i;
            continue;
        }
        if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
            out += "{{";
            i += 2;
            continue;
        }
        size_t open = i++;
        size_t close = fmt.find('}', i);
        if (close == std::string::npos) {
            out += fmt.substr(open);
            break;
        }
        if (close > open && fmt[close - 1] == '}') {
            // trailing "}}" escape: literal field + '}'
            out += fmt.substr(open, close - open);
            i = close + 1;
            continue;
        }
        std::string field = fmt.substr(i, close - i);
        std::string spec;
        size_t colon = field.find(':');
        if (colon != std::string::npos) {
            spec = field.substr(colon + 1);
            field = field.substr(0, colon);
        }
        if (field.rfind("task.", 0) == 0)
            out += format_task_field(field.substr(5), t, now, spec);
        else
            out += fmt.substr(open, close - open + 1);
        i = close + 1;
    }
    return out;
}

// ── ProgressColumn base + widgets ────────────────────────────────────────────

class ProgressColumn {
public:
    virtual ~ProgressColumn() = default;
    virtual rich::AnyRenderable render(const Task& task, double now,
                                       const rich::Console& con,
                                       const rich::Options& opt) const = 0;
    // Python table_column: TextColumn uses Column(no_wrap=True).
    virtual bool no_wrap_column() const { return false; }
};
using ProgressColumnPtr = std::shared_ptr<ProgressColumn>;

// RenderableColumn: a fixed renderable in every cell.
class RenderableColumn : public ProgressColumn {
public:
    explicit RenderableColumn(rich::AnyRenderable renderable = rich::AnyRenderable())
        : r_(std::move(renderable)) {}
    rich::AnyRenderable render(const Task&, double, const rich::Console&,
                               const rich::Options&) const override {
        return r_;
    }

private:
    rich::AnyRenderable r_;
};

// SpinnerColumn: spinner frame, or finished_text when the task is done.
class SpinnerColumn : public ProgressColumn {
public:
    SpinnerColumn(std::string spinner_name = "dots",
                  std::string style = "progress.spinner", double speed = 1.0,
                  std::string finished_text = " ")
        : spinner_(std::move(spinner_name), style, speed), style_(std::move(style)),
          finished_(std::move(finished_text)) {}

    void set_spinner(std::string spinner_name,
                     std::string spinner_style = "progress.spinner", double speed = 1.0) {
        spinner_ = rich::status::Spinner(std::move(spinner_name), std::move(spinner_style),
                                         speed);
        style_ = std::move(spinner_style);
    }

    rich::AnyRenderable render(const Task& task, double now, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style(style_, "");
        } catch (...) {
        }
        if (task.finished())
            return rich::AnyRenderable(rich::Text{finished_, style});
        return rich::AnyRenderable(rich::Text{spinner_.frame_at(now), style});
    }

private:
    mutable rich::status::Spinner spinner_;
    std::string style_;
    std::string finished_;
};

// TextColumn: a task-format string, optional markup + base style.
// (Python's `highlighter` kwarg is not wired: every built-in Progress usage
// passes highlighter=None.)
class TextColumn : public ProgressColumn {
public:
    TextColumn(std::string text_format, std::string style = "none",
               std::string justify = "left", bool markup = true)
        : format_(std::move(text_format)), style_(std::move(style)),
          justify_(std::move(justify)), markup_(markup) {}

    rich::AnyRenderable render(const Task& task, double now, const rich::Console& con,
                               const rich::Options&) const override {
        std::string s = format_task(format_, task, now);
        if (markup_) {
            // Text.from_markup(s, style=...) — the base style is "none" for
            // every built-in usage; markup tags carry the column styling.
            return rich::AnyRenderable(rich::StyledText{s});
        }
        rich::Style style;
        if (style_ != "none" && !style_.empty()) {
            try {
                style = con.get_style(style_, "");
            } catch (...) {
            }
        }
        return rich::AnyRenderable(rich::Text{s, style});
    }

    bool no_wrap_column() const override { return true; }

protected:
    std::string format_;
    std::string style_;
    std::string justify_;
    bool markup_;
};

// TaskProgressColumn: percentage, or speed when the total is unknown.
class TaskProgressColumn : public TextColumn {
public:
    TaskProgressColumn(std::string text_format = "[progress.percentage]{task.percentage:>3.0f}%",
                       std::string text_format_no_percentage = "",
                       std::string style = "none", std::string justify = "left",
                       bool markup = true, bool show_speed = false)
        : TextColumn(std::move(text_format), std::move(style), std::move(justify), markup),
          no_pct_(std::move(text_format_no_percentage)), show_speed_(show_speed) {}

    // Python TaskProgressColumn.render_speed (it/s with ×10³ suffixes).
    static rich::AnyRenderable render_speed(std::optional<double> speed,
                                            const rich::Console& con) {
        rich::Style style;
        try {
            style = con.get_style("progress.percentage", "");
        } catch (...) {
        }
        if (!speed.has_value())
            return rich::AnyRenderable(rich::Text{"", style});
        static const std::vector<std::string> suffixes = {
            "",
            "\xc3\x97" "10\xe2\x81\xb3",  // ×10³
            "\xc3\x97" "10\xe2\x81\xb6",  // ×10⁶
            "\xc3\x97" "10\xe2\x81\xb9",  // ×10⁹
            "\xc3\x97" "10\xe2\x81\xba"   // ×10¹²
        };
        auto [unit, suffix] = filesize_detail::pick_unit_and_suffix(
            static_cast<int64_t>(*speed), suffixes, 1000);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.1f%s it/s", *speed / static_cast<double>(unit),
                      suffix.c_str());
        return rich::AnyRenderable(rich::Text{buf, style});
    }

    rich::AnyRenderable render(const Task& task, double now, const rich::Console& con,
                               const rich::Options&) const override {
        if (!task.total.has_value() && show_speed_) {
            // Python: task.finished_speed or task.speed
            auto sp = task.finished_speed.has_value() ? task.finished_speed : task.speed();
            return render_speed(sp, con);
        }
        const std::string& fmt = task.total.has_value() ? format_ : no_pct_;
        std::string s = format_task(fmt, task, now);
        if (markup_) return rich::AnyRenderable(rich::StyledText{s});
        rich::Style style;
        if (style_ != "none" && !style_.empty()) {
            try {
                style = con.get_style(style_, "");
            } catch (...) {
            }
        }
        return rich::AnyRenderable(rich::Text{s, style});
    }

private:
    std::string no_pct_;
    bool show_speed_;
};

// BarColumn: the progress bar widget (pulse until the task starts).
class BarColumn : public ProgressColumn {
public:
    BarColumn(size_t bar_width = 40, std::string style = "bar.back",
              std::string complete_style = "bar.complete",
              std::string finished_style = "bar.finished",
              std::string pulse_style = "bar.pulse")
        : bar_width_(bar_width), style_(std::move(style)),
          complete_style_(std::move(complete_style)),
          finished_style_(std::move(finished_style)),
          pulse_style_(std::move(pulse_style)) {}

    rich::AnyRenderable render(const Task& task, double now, const rich::Console&,
                               const rich::Options&) const override {
        progress_bar::ProgressBar::Options p;
        if (task.total.has_value()) p.total = std::max(0.0, *task.total);
        else p.total = std::nullopt;
        p.completed = std::max(0.0, task.completed);
        if (bar_width_ > 0) p.width = std::max<size_t>(1, bar_width_);
        p.pulse = !task.started();
        p.style = style_;
        p.complete_style = complete_style_;
        p.finished_style = finished_style_;
        p.pulse_style = pulse_style_;
        p.animation_time = now;
        return rich::AnyRenderable(progress_bar::ProgressBar(p));
    }

private:
    size_t bar_width_;
    std::string style_, complete_style_, finished_style_, pulse_style_;
};

// TimeElapsedColumn: H:MM:SS elapsed ("H:MM:SS" finished time when done).
class TimeElapsedColumn : public ProgressColumn {
public:
    rich::AnyRenderable render(const Task& task, double now, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style("progress.elapsed", "");
        } catch (...) {
        }
        auto v = task.finished() ? std::optional<double>(*task.finished_time)
                                 : task.elapsed(now);
        if (!v.has_value())
            return rich::AnyRenderable(rich::Text{"-:--:--", style});
        return rich::AnyRenderable(
            rich::Text{timedelta_str(std::max(0.0, std::floor(*v))) , style});
    }
};

// TimeRemainingColumn: ETA; compact MM:SS; elapsed-when-finished.
class TimeRemainingColumn : public ProgressColumn {
public:
    TimeRemainingColumn(bool compact = false, bool elapsed_when_finished = false)
        : compact_(compact), elapsed_when_finished_(elapsed_when_finished) {}

    rich::AnyRenderable render(const Task& task, double, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style(
                (elapsed_when_finished_ && task.finished()) ? "progress.elapsed"
                                                            : "progress.remaining",
                "");
        } catch (...) {
        }
        std::optional<double> task_time;
        if (elapsed_when_finished_ && task.finished())
            task_time = task.finished_time;
        else
            task_time = task.time_remaining();

        if (!task.total.has_value())
            return rich::AnyRenderable(rich::Text{"", style});
        if (!task_time.has_value())
            return rich::AnyRenderable(
                rich::Text{compact_ ? std::string("--:--") : std::string("-:--:--"), style});

        const long long t = static_cast<long long>(*task_time);
        const long long seconds = t % 60;
        const long long minutes = (t / 60) % 60;
        const long long hours = t / 3600;
        char buf[16];
        if (compact_ && hours == 0)
            std::snprintf(buf, sizeof(buf), "%02lld:%02lld", minutes, seconds);
        else
            std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", hours, minutes, seconds);
        return rich::AnyRenderable(rich::Text{buf, style});
    }

private:
    bool compact_;
    bool elapsed_when_finished_;
};

// FileSizeColumn: completed data (SI decimal).
class FileSizeColumn : public ProgressColumn {
public:
    rich::AnyRenderable render(const Task& task, double, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style("progress.filesize", "");
        } catch (...) {
        }
        return rich::AnyRenderable(
            rich::Text{filesize_detail::decimal(static_cast<int64_t>(task.completed)),
                       style});
    }
};

// TotalFileSizeColumn: total data (SI decimal); empty when unknown.
class TotalFileSizeColumn : public ProgressColumn {
public:
    rich::AnyRenderable render(const Task& task, double, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style("progress.filesize.total", "");
        } catch (...) {
        }
        std::string s = task.total.has_value()
                            ? filesize_detail::decimal(static_cast<int64_t>(*task.total))
                            : "";
        return rich::AnyRenderable(rich::Text{s, style});
    }
};

// MofNCompleteColumn: "  10/1000" (completed right-aligned to total width).
class MofNCompleteColumn : public ProgressColumn {
public:
    explicit MofNCompleteColumn(std::string separator = "/")
        : separator_(std::move(separator)) {}

    rich::AnyRenderable render(const Task& task, double, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style("progress.download", "");
        } catch (...) {
        }
        const long long completed = static_cast<long long>(task.completed);
        std::string total_str = task.total.has_value()
                                    ? std::to_string(static_cast<long long>(*task.total))
                                    : "?";
        // Python f"{completed:{total_width}d}{separator}{total}"
        char cbuf[64];
        std::snprintf(cbuf, sizeof(cbuf), "%*lld", static_cast<int>(total_str.size()),
                      completed);
        const std::string s = cbuf + separator_ + total_str;
        return rich::AnyRenderable(rich::Text{s, style});
    }

private:
    std::string separator_;
};

// DownloadColumn: "0.5/2.3 GB" (SI or binary units).
class DownloadColumn : public ProgressColumn {
public:
    explicit DownloadColumn(bool binary_units = false) : binary_units_(binary_units) {}

    rich::AnyRenderable render(const Task& task, double, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style("progress.download", "");
        } catch (...) {
        }
        const int64_t completed = static_cast<int64_t>(task.completed);
        const int64_t base_value =
            task.total.has_value() ? static_cast<int64_t>(*task.total) : completed;
        std::vector<std::string> suffixes = binary_units_
                                                ? std::vector<std::string>{"bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"}
                                                : std::vector<std::string>{"bytes", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
        const int64_t base = binary_units_ ? 1024 : 1000;
        auto [unit, suffix] = filesize_detail::pick_unit_and_suffix(base_value, suffixes,
                                                                    base);
        const int precision = (unit == 1) ? 0 : 1;
        std::string completed_str =
            filesize_detail::comma_fixed(static_cast<double>(completed) / unit, precision);
        std::string total_str =
            task.total.has_value()
                ? filesize_detail::comma_fixed(
                      static_cast<double>(static_cast<int64_t>(*task.total)) / unit,
                      precision)
                : "?";
        return rich::AnyRenderable(
            rich::Text{completed_str + "/" + total_str + " " + suffix, style});
    }

private:
    bool binary_units_;
};

// TransferSpeedColumn: "1.2 MB/s" (SI decimal) or "?".
class TransferSpeedColumn : public ProgressColumn {
public:
    rich::AnyRenderable render(const Task& task, double, const rich::Console& con,
                               const rich::Options&) const override {
        rich::Style style;
        try {
            style = con.get_style("progress.data.speed", "");
        } catch (...) {
        }
        std::optional<double> speed =
            task.finished_speed.has_value() ? task.finished_speed : task.speed();
        if (!speed.has_value())
            return rich::AnyRenderable(rich::Text{"?", style});
        return rich::AnyRenderable(
            rich::Text{filesize_detail::decimal(static_cast<int64_t>(*speed)) + "/s",
                       style});
    }
};

// ── rich.progress.Progress ───────────────────────────────────────────────────

class Progress {
public:
    // A cell source: a ProgressColumn or a raw task-format string (Python
    // allows `Progress("Elapsed:", TimeElapsedColumn())`; str columns get
    // Column(no_wrap=True) and no markup).
    struct AnyColumn {
        ProgressColumnPtr column;
        std::string text;
        static AnyColumn of(ProgressColumnPtr c) { return {c, {}}; }
        static AnyColumn of_text(std::string s) { return {{}, std::move(s)}; }
    };

    struct Update {
        std::optional<double> total;
        std::optional<double> completed;
        std::optional<double> advance;
        std::optional<std::string> description;
        std::optional<bool> visible;
    };

    explicit Progress(std::vector<AnyColumn> columns = {}, bool expand = false,
                      double speed_estimate_period = 30.0)
        : expand_(expand), speed_estimate_period_(speed_estimate_period) {
        if (columns.empty()) columns_ = get_default_columns();
        else columns_ = std::move(columns);
    }

    // Python Progress.get_default_columns: TextColumn(description), BarColumn,
    // TaskProgressColumn, TimeRemainingColumn.
    static std::vector<AnyColumn> get_default_columns() {
        return {
            AnyColumn::of(std::make_shared<TextColumn>(
                TextColumn("[progress.description]{task.description}"))),
            AnyColumn::of(std::make_shared<BarColumn>(BarColumn())),
            AnyColumn::of(std::make_shared<TaskProgressColumn>(TaskProgressColumn())),
            AnyColumn::of(std::make_shared<TimeRemainingColumn>(TimeRemainingColumn())),
        };
    }

    // Python add_task (default total=100.0; total=None → pulsing bar).
    TaskId add_task(const std::string& description, std::optional<double> total = 100.0,
                    double completed = 0.0, bool visible = true, bool start = true,
                    double now = 0.0) {
        Task t;
        t.id = next_id_++;
        t.description = description;
        t.total = total;
        t.completed = completed;
        t.visible = visible;
        tasks_.push_back(std::move(t));
        if (start) start_task(t.id, now);
        return t.id;
    }

    void start_task(TaskId id, double now = 0.0) {
        auto& t = tasks_[static_cast<size_t>(id)];
        if (!t.start_time.has_value()) t.start_time = now;
    }

    void stop_task(TaskId id, double now = 0.0) {
        auto& t = tasks_[static_cast<size_t>(id)];
        if (!t.start_time.has_value()) t.start_time = now;
        t.stop_time = now;
    }

    // Python Progress.update.
    void update(TaskId id, const Update& u, double now = 0.0) {
        auto& t = tasks_[static_cast<size_t>(id)];
        const double completed_start = t.completed;
        if (u.total.has_value() && (!t.total.has_value() || *u.total != *t.total)) {
            t.total = u.total;
            t.samples.clear();
            t.finished_time = std::nullopt;
            t.finished_speed = std::nullopt;
        }
        if (u.advance.has_value()) t.completed += *u.advance;
        if (u.completed.has_value()) t.completed = *u.completed;
        if (u.description.has_value()) t.description = *u.description;
        if (u.visible.has_value()) t.visible = *u.visible;

        const double update_completed = t.completed - completed_start;
        const double old_sample_time = now - speed_estimate_period_;
        while (!t.samples.empty() && t.samples.front().first < old_sample_time)
            t.samples.pop_front();
        if (update_completed > 0.0) t.samples.push_back({now, update_completed});
        if (t.total.has_value() && t.completed >= *t.total &&
            !t.finished_time.has_value())
            t.finished_time = t.elapsed(now);
    }

    // Python Progress.advance.
    void advance(TaskId id, double delta = 1.0, double now = 0.0) {
        auto& t = tasks_[static_cast<size_t>(id)];
        t.completed += delta;
        const double old_sample_time = now - speed_estimate_period_;
        while (!t.samples.empty() && t.samples.front().first < old_sample_time)
            t.samples.pop_front();
        while (t.samples.size() > 1000) t.samples.pop_front();
        t.samples.push_back({now, delta});
        if (t.total.has_value() && t.completed >= *t.total &&
            !t.finished_time.has_value()) {
            t.finished_time = t.elapsed(now);
            t.finished_speed = t.speed();
        }
    }

    void remove_task(TaskId id) {
        tasks_.erase(tasks_.begin() + static_cast<long>(id));
    }

    // Python get_renderable: Group(make_tasks_table(tasks)) — the table is
    // the only renderable, so the group is transparent.
    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt,
                                      double now = 0.0) const {
        if (tasks_.empty()) return {};
        std::vector<std::vector<std::optional<rich::AnyRenderable>>> rows;
        for (auto& t : tasks_) {
            if (!t.visible) continue;
            std::vector<std::optional<rich::AnyRenderable>> row;
            row.reserve(columns_.size());
            for (auto& col : columns_) {
                if (col.column)
                    row.push_back(col.column->render(t, now, con, opt));
                else
                    row.push_back(rich::AnyRenderable(rich::Text{format_task(col.text, t, now), {}}));
            }
            rows.push_back(std::move(row));
        }
        grid_detail::GridOptions g;
        g.pad_right = 1;
        g.expand = expand_;
        g.no_wrap.resize(columns_.size());
        for (size_t i = 0; i < columns_.size(); ++i)
            g.no_wrap[i] = columns_[i].column ? columns_[i].column->no_wrap_column() : true;
        return grid_detail::render_grid_table(con, opt, rows, g);
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options& opt) const {
        return {0, (opt.width > 0) ? opt.width : 0};
    }

private:
    std::vector<AnyColumn> columns_;
    std::vector<Task> tasks_;
    TaskId next_id_{0};
    bool expand_;
    double speed_estimate_period_;
};

inline std::vector<rich::Segment> render_item(const Progress& p, const rich::Console& c,
                                              const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Progress& p, const rich::Console& c,
                                              const rich::Options& o) {
    return p.measure(c, o);
}

} // namespace rich::progress
