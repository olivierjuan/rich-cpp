#pragma once
#include <rich_core.h>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>

namespace rich::text {

enum class Overflow { Fold, Crop, Ellipsis };
enum class Justify { Left, Center, Right, Full };

class Text {
public:
    Text(std::string content = "", rich::Style style = {})
        : content_(std::move(content)), style_(style) {}

    Text& set_style(rich::Style s) { style_ = s; return *this; }
    Text& set_justify(Justify j) { justify_ = j; return *this; }
    Text& set_overflow(Overflow o) { overflow_ = o; return *this; }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        size_t max_width = (opt.width > 0) ? opt.width : 80;
        std::vector<rich::Segment> out;

        if (content_.empty()) return out;

        // Word wrapping at whitespace
        std::vector<std::string> words;
        size_t start = 0;
        while (start < content_.size()) {
            size_t end = content_.find_first_of(" \t\n", start);
            if (end == std::string::npos) {
                words.push_back(content_.substr(start));
                break;
            } else {
                if (end > start) {
                    words.push_back(content_.substr(start, end - start));
                }
                start = end + 1;
            }
        }

        std::string current_line;
        for (const auto& w : words) {
            if (current_line.empty()) {
                current_line = w;
            } else {
                if (current_line.size() + 1 + w.size() <= max_width) {
                    current_line += " " + w;
                } else {
                    // Python Rich appends a trailing space when wrapping lines
                    out.push_back({current_line + " \n", style_});
                    current_line = w;
                }
            }
        }
        if (!current_line.empty()) {
            out.push_back({current_line + "\n", style_});
        }

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options& opt) const {
        return {std::min(content_.size(), opt.width), 1};
    }

private:
    std::string content_;
    rich::Style style_;
    Justify justify_{Justify::Left};
    Overflow overflow_{Overflow::Fold};
};

inline std::vector<rich::Segment> render_item(const Text& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Text& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::text
