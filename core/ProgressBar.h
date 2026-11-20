#pragma once
#include <rich_core.h>
#include <string>
#include <vector>
#include <chrono>

namespace rich::progress {

class ProgressBar {
public:
    ProgressBar(size_t total = 100, size_t width = 30)
        : total_(total), width_(width), current_(0) {}

    void update(size_t completed) {
        current_ = (completed > total_) ? total_ : completed;
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;

        size_t bar_width = (width_ > 0) ? width_ : 30;
        size_t filled_cells = (total_ > 0) ? (current_ * bar_width / total_) : 0;
        size_t empty_cells = (bar_width >= filled_cells) ? (bar_width - filled_cells) : 0;

        // Smooth blocks: filled is '█', empty is '░'
        std::string filled_bar;
        filled_bar.reserve(filled_cells * 3);
        for (size_t i = 0; i < filled_cells; ++i) filled_bar += "█";

        std::string empty_bar;
        empty_bar.reserve(empty_cells * 3);
        for (size_t i = 0; i < empty_cells; ++i) empty_bar += "░";

        size_t percent = (total_ > 0) ? (current_ * 100 / total_) : 0;
        std::string pct_str = std::to_string(percent) + "%";
        if (pct_str.size() < 4) pct_str = std::string(4 - pct_str.size(), ' ') + pct_str;

        out.push_back({filled_bar, rich::Style{.bold = true, .color = rich::Color::from_ansi(32)}}); // Green
        out.push_back({empty_bar, rich::Style{.dim = true, .color = rich::Color::from_ansi(90)}});   // Dim gray
        out.push_back({" " + pct_str, rich::Style{.bold = true, .color = rich::Color::from_ansi(96)}}); // Cyan
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        return {width_ + 6, 1};
    }

private:
    size_t total_;
    size_t width_;
    size_t current_;
};

inline std::vector<rich::Segment> render_item(const ProgressBar& p, const rich::Console& c, const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const ProgressBar& p, const rich::Console& c, const rich::Options& o) {
    return p.measure(c, o);
}

} // namespace rich::progress
