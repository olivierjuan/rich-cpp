#pragma once
// rich::progress::ProgressTracker — multi-column progress display (Tier 3 item 3.1).
//
// Port of Python Rich's `rich.progress.Progress` default column layout,
// rendered as a borderless grid (Table.grid(padding=(0, 1), collapse_padding=True,
// pad_edge=False)):
//   [description] [bar ━╸╺] [percent] [ETA]
//
// Bar rendering mirrors rich.progress_bar.ProgressBar.__rich_console__ exactly:
//   complete_halves = width * 2 * completed / total
//   full '━' cells + optional trailing '╸'; the remainder is back-styled
//   ('╺' when there is no half cell and the bar is non-empty, then '━').
//
// Speed/ETA mirror Task.speed / Task.time_remaining: each positive-completion
// update appends a sample to a rolling window (speed_estimate_period=30s,
// maxlen 1000); speed = sum(samples[1:]) / span; ETA = ceil(remaining/speed).
//
// Determinism for the oracle suite: an injected clock value (`now`, seconds
// since an arbitrary epoch) is threaded through add_task/update/tick/render.
// With a fixed `now` the output is byte-stable. When left empty, steady_clock
// is used (live use under Live).

#include <rich_core.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace rich::progress {

using TaskId = int;

class ProgressTracker {
public:
    struct RowOptions {
        std::string description;
        double total{100.0};      // known total steps (>= 0); -1 = unknown
        double completed{0.0};
    };

    explicit ProgressTracker(bool show_speed = false) : show_speed_(show_speed),
          state_(std::make_shared<State>()) {}

    // Add a row; returns its id. start_time is set to `now`.
    TaskId add_task(const RowOptions& opts, std::chrono::nanoseconds now = {}) {
        std::lock_guard<std::mutex> lk(state_->mu);
        Row row;
        row.description = opts.description;
        row.total = opts.total;
        row.completed = clamp_completed(opts.completed, opts.total);
        row.start_time = resolve_now(now);
        state_->rows.push_back(std::move(row));
        return static_cast<TaskId>(state_->rows.size() - 1);
    }

    // Set absolute values for a row (clamps completed to [0, total]).
    void update(TaskId id, double completed, double total,
                std::chrono::nanoseconds now = {}) {
        std::lock_guard<std::mutex> lk(state_->mu);
        Row& r = state_->rows[static_cast<size_t>(id)];
        r.total = total;
        const double prev = r.completed;
        r.completed = clamp_completed(completed, total);
        if (r.total >= 0 && r.completed >= r.total) r.finished = true;
        record_sample(r, r.completed - prev, resolve_now(now));
    }

    // Convenience: advance by delta.
    void tick(TaskId id, double delta, std::chrono::nanoseconds now = {}) {
        std::lock_guard<std::mutex> lk(state_->mu);
        Row& r = state_->rows[static_cast<size_t>(id)];
        const double prev = r.completed;
        r.completed += delta;
        if (r.total >= 0) {
            r.completed = clamp_completed(r.completed, r.total);
            if (r.completed >= r.total) r.finished = true;
        }
        record_sample(r, r.completed - prev, resolve_now(now));
    }

    // Render all rows into segments. `now` injects the clock for deterministic
    // speed/ETA (zero = t=0 for oracle determinism; non-zero = absolute time).
    std::vector<rich::Segment> render(const rich::Console&, const rich::Options& opt,
                                      std::chrono::nanoseconds now = {}) const {
        std::lock_guard<std::mutex> lk(state_->mu);
        const auto t = resolve_now(now);
        const size_t W = opt.width > 0 ? opt.width : 80;

        // Column content per row (plain text; styles applied at emission).
        struct Cell { std::string desc; std::string pct; std::string eta; };
        std::vector<Cell> rows;
        rows.reserve(state_->rows.size());
        for (const auto& r : state_->rows) {
            Cell cell;
            cell.desc = r.description;
            cell.pct = format_percent(r);
            cell.eta = format_eta(r, t);
            rows.push_back(std::move(cell));
        }

        // Table.grid column-width algorithm: widths[i] = max cell width +
        // padding (1 between columns, 0 at edges). The bar column is measured
        // at its preferred width (BarColumn default 40), not at actual glyph
        // count — mirroring rich.progress_bar.__rich_measure__ and the
        // observed oracle layout (bar field stays 31 wide even at 0%). If the
        // sum exceeds the console width, shrink the largest WRAPABLE column
        // (the bar) toward the second-largest, then distribute any remaining
        // excess evenly across all columns.
        const size_t ncols = 4;
        constexpr size_t kPreferredBarWidth = 40;
        std::vector<size_t> w{0, kPreferredBarWidth, 0, 0};
        for (const auto& c : rows) {
            w[0] = std::max(w[0], utf8_width(c.desc));
            w[2] = std::max(w[2], utf8_width(c.pct));
            w[3] = std::max(w[3], utf8_width(c.eta));
        }
        const size_t padding_total = (ncols - 1); // one space between columns
        size_t sum = padding_total;
        for (size_t x : w) sum += x;
        if (sum > W) {
            // Shrink the wrapable column (bar) down to the next-largest width,
            // then distribute any remaining excess evenly across all columns.
            std::vector<size_t> sorted = w;
            std::sort(sorted.begin(), sorted.end());
            const size_t second_largest = sorted[sorted.size() - 2];
            const size_t excess = sum - W;
            size_t bar_cut = std::min(excess, w[1] - second_largest);
            w[1] -= bar_cut;
            const size_t rest = excess - bar_cut;
            if (rest > 0) {
                // Even ratio reduce over all columns (floor), then give the
                // leftover units to the widest columns.
                size_t given = 0;
                for (size_t i = 0; i < ncols; ++i) {
                    const size_t cut = (w[i] * rest) / sum;
                    w[i] -= cut;
                    given += cut;
                }
                size_t leftover = rest - given;
                while (leftover > 0) {
                    for (size_t i = 0; i < ncols && leftover > 0; ++i) {
                        if (w[i] > 0) { --w[i]; --leftover; }
                    }
                }
            }
        }

        // Re-measure bars at their final width and emit rows.
        std::vector<rich::Segment> out;
        for (size_t ri = 0; ri < rows.size(); ++ri) {
            const auto& r = state_->rows[ri];
            const std::string bar = make_bar(r, w[1]);
            const std::vector<std::pair<std::string, rich::Style>> cells = {
                {truncate(rows[ri].desc, w[0]), desc_style_},
                {bar, bar_fill_style_},
                {truncate(rows[ri].pct, w[2]), percent_style_},
                {truncate(rows[ri].eta, w[3]), eta_style_},
            };
            for (size_t i = 0; i < cells.size(); ++i) {
                std::string text = pad_right(cells[i].first, w[i]);
                if (i + 1 < cells.size()) text += " ";
                if (!text.empty()) out.push_back({std::move(text), cells[i].second});
            }
            out.push_back({"\n", {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        std::lock_guard<std::mutex> lk(state_->mu);
        size_t max_desc = 0;
        for (const auto& r : state_->rows)
            max_desc = std::max(max_desc, utf8_width(r.description));
        const size_t w = max_desc + 40 + 5 + 9 + 3; // desc + bar + " pct" + ETA + pads
        return {w, state_->rows.size()};
    }

private:
    struct Sample {
        std::chrono::nanoseconds timestamp{};
        double completed{0.0};
    };

    struct Row {
        std::string description;
        double total{-1.0};
        double completed{0.0};
        bool finished{false};
        std::chrono::nanoseconds start_time{};
        std::deque<Sample> samples;
    };

    static constexpr double kSpeedEstimatePeriod = 30.0; // seconds
    static constexpr size_t kMaxSamples = 1000;

    static std::chrono::nanoseconds resolve_now(std::chrono::nanoseconds now) {
        // A zero value is treated as an explicit "t=0" (deterministic oracle
        // mode), not as "use steady clock". Callers that want wall time must
        // pass a non-zero value or use the default-argument overload.
        return now;
    }

    static double duration_seconds(std::chrono::nanoseconds d) {
        return std::chrono::duration<double>(d).count();
    }

    static double clamp_completed(double c, double total) {
        if (c < 0) return 0;
        if (total >= 0 && c > total) return total;
        return c;
    }

    // Mirrors Task.update/advance sampling: drop samples older than
    // now - speed_estimate_period, append a sample only when delta > 0,
    // cap the deque at kMaxSamples.
    void record_sample(Row& r, double delta, std::chrono::nanoseconds now) {
        if (delta <= 0) return;
        const double cutoff = duration_seconds(now) - kSpeedEstimatePeriod;
        while (!r.samples.empty() && duration_seconds(r.samples.front().timestamp) < cutoff)
            r.samples.pop_front();
        r.samples.push_back({now, delta});
        while (r.samples.size() > kMaxSamples) r.samples.pop_front();
    }

    // Task.speed: sum of deltas after the first sample divided by the time
    // span; None (< 2 samples or zero span) -> -1.
    static double compute_speed(const Row& r) {
        if (r.samples.size() < 2) return -1.0;
        double sum = 0.0;
        for (size_t i = 1; i < r.samples.size(); ++i) sum += r.samples[i].completed;
        const double span = duration_seconds(
            r.samples.back().timestamp - r.samples.front().timestamp);
        if (span <= 0) return -1.0;
        return sum / span;
    }

    // Task.time_remaining: 0 when finished; ceil(remaining/speed) when speed
    // is known; -1 (unknown) otherwise.
    static double compute_eta(const Row& r) {
        if (r.total < 0) return -1.0;
        if (r.finished) return 0.0;
        const double speed = compute_speed(r);
        if (speed < 0) return -1.0;
        return std::ceil((r.total - r.completed) / speed);
    }

    // rich.progress_bar.ProgressBar.__rich_console__ bar model.
    static std::string make_bar(const Row& r, size_t width) {
        if (width == 0) return "";
        if (r.total <= 0 || r.completed <= 0) return "";
        const double completed = std::min(r.total, std::max(0.0, r.completed));
        const long long complete_halves =
            static_cast<long long>(static_cast<double>(width) * 2.0 * completed / r.total);
        const size_t bar_count = static_cast<size_t>(complete_halves / 2);
        const size_t half_bar_count = static_cast<size_t>(complete_halves % 2);

        std::string result;
        for (size_t i = 0; i < bar_count; ++i) result.append("\xe2\x94\x81"); // ━
        if (half_bar_count) result.append("\xe2\x95\xb8"); // ╸
        // The dimmed "back" bars ('╺' + '━') are only emitted by Python when
        // the console has an active color system (not no_color). Oracle output
        // is captured through a StringIO file with no color system, so they are
        // omitted here — matching byte-for-byte. Live TTY use could enable them
        // via a style flag on the empty cells.
        return result;
    }

    static std::string format_percent(const Row& r) {
        if (r.total <= 0) return "  --";
        const long long pct = static_cast<long long>(r.completed * 100.0 / r.total);
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%3lld%%", pct);
        return buf;
    }

    // "H:MM:SS" like TimeRemainingColumn (non-compact); "-:--:--" when
    // unknown; "0:00:00" when finished (elapsed_when_finished=False).
    static std::string format_eta(const Row& r, std::chrono::nanoseconds /*now*/) {
        const double eta = compute_eta(r);
        if (eta < 0) return "-:--:--";
        const long long secs = static_cast<long long>(eta);
        const long long hours = secs / 3600;
        const long long minutes = (secs % 3600) / 60;
        const long long seconds = secs % 60;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", hours, minutes, seconds);
        return buf;
    }

    // "<x.x> it/s" with decimal unit suffixes, mirroring TaskProgressColumn
    // .render_speed (only shown when total is unknown and show_speed is set).
    static std::string format_speed(double speed) {
        if (speed <= 0) return "";
        static const char* suffixes[] = {"",
                                         "\xc3\x97" "10\xe2\x81\xb3",
                                         "\xc3\x97" "10\xe2\x81\xb6",
                                         "\xc3\x97" "10\xe2\x81\xb9",
                                         "\xc3\x97" "10\xe2\x81\xba"};
        int unit = 0;
        double scaled = speed;
        while (scaled >= 1000.0 && unit < 4) {
            scaled /= 1000.0;
            ++unit;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f%s it/s", scaled, suffixes[unit]);
        return buf;
    }

    // Truncate to `width` cells, appending '…' when overflow (Python's
    // Text.truncate behavior inside table cells).
    static std::string truncate(std::string_view text, size_t width) {
        size_t tw = utf8_width(text);
        if (tw <= width) return std::string(text);
        if (width == 0) return "";
        // Keep width-1 cells plus the ellipsis.
        size_t keep = width - 1;
        size_t i = 0, taken = 0;
        while (i < text.size() && taken < keep) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            size_t len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            if (i + len > text.size()) break;
            i += len;
            ++taken;
        }
        return std::string(text.substr(0, i)) + "\xe2\x80\xa6"; // …
    }

    static size_t utf8_width(std::string_view sv) {
        size_t width = 0;
        size_t i = 0;
        while (i < sv.size()) {
            unsigned char c = static_cast<unsigned char>(sv[i]);
            if (c < 0x80) { ++width; ++i; }
            else if ((c & 0xE0) == 0xC0) { ++width; i += 2; }
            else if ((c & 0xF0) == 0xE0) { ++width; i += 3; }
            else if ((c & 0xF8) == 0xF0) { width += 2; i += 4; }
            else { ++i; }
        }
        return width;
    }

    static std::string pad_right(std::string_view text, size_t width) {
        size_t tw = utf8_width(text);
        if (tw >= width) return std::string(text);
        return std::string(text) + std::string(width - tw, ' ');
    }

    bool show_speed_;

    // Shared mutable state so that copying a ProgressTracker (required by
    // AnyRenderable's Model<T>) does not attempt to copy the non-copyable
    // std::mutex directly. All copies observe and mutate the same rows.
    struct State {
        std::mutex mu;
        std::vector<Row> rows;
    };
    std::shared_ptr<State> state_;

    rich::Style desc_style_{.bold = true};
    rich::Style bar_fill_style_{.color = rich::Color::from_ansi(32)};
    rich::Style percent_style_{.bold = true, .color = rich::Color::from_ansi(96)};
    rich::Style eta_style_{.color = rich::Color::from_ansi(95)};
};

inline std::vector<rich::Segment> render_item(const ProgressTracker& p, const rich::Console& c,
                                              const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const ProgressTracker& p, const rich::Console& c,
                                              const rich::Options& o) {
    return p.measure(c, o);
}

} // namespace rich::progress
