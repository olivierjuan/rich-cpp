#pragma once
#include <rich_core.h>
#include <algorithm>
namespace rich::panel {
#include <string>
#include <sstream>

namespace rich::panel {

struct PanelOptions {
    std::string title;
    bool show_border{true};
};

namespace {
inline std::string repeat_utf8(std::string_view glyph, size_t count) {
    std::string res;
    res.reserve(glyph.size() * count);
    for (size_t i = 0; i < count; ++i) {
        res.append(glyph);
    }
    return res;
}
} // namespace

class Panel {
public:
    Panel(rich::AnyRenderable inner, PanelOptions options)
        : inner_(std::move(inner)), options_(std::move(options)) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        auto [w, h] = measure(con, opt);
        std::vector<rich::Segment> out;

        size_t title_len = options_.title.size();
        size_t border_dashes = (w >= title_len + 4) ? (w - title_len - 4) : 1;

        if (options_.show_border) {
            out.push_back({"╭─ " + options_.title + " " + repeat_utf8("─", border_dashes) + "╮\n", {}});
        }

        auto inner_segs = inner_.render(con, opt);
        size_t inner_text_len = 0;
        for (const auto& s : inner_segs) {
            inner_text_len += s.text.size();
        }
        rich::Style border_style{.dim = true, .color = rich::Color::from_ansi(90)};

        if (options_.show_border) {
            out.push_back({"│ ", {}});
            std::string top = "╭─ ";
            if (!options_.title.empty()) top += options_.title + " ";
            top += repeat_utf8("─", border_dashes) + "╮\n";
            out.push_back({std::move(top), border_style});
        }
        for (auto& s : inner_segs) {
            out.push_back(std::move(s));

        auto inner_segs = inner_.render(con, opt);

        // Group inner segments by line
        std::vector<std::vector<rich::Segment>> lines;
        lines.emplace_back();

        for (const auto& s : inner_segs) {
            size_t start = 0;
            while (start < s.text.size()) {
                size_t nl = s.text.find('\n', start);
                if (nl == std::string::npos) {
                    lines.back().push_back({s.text.substr(start), s.style});
                    break;
                } else {
                    lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
        }

            size_t inner_space = (w >= 2) ? (w - 2) : 0;
            size_t pad = (inner_space >= inner_text_len) ? (inner_space - inner_text_len) : 0;
            out.push_back({std::string(pad, ' ') + " │\n", {}});
            out.push_back({"╰" + repeat_utf8("─", inner_space + 2) + "╯\n", {}});
        size_t inner_box_width = (w >= 2) ? (w - 2) : 0;

        for (const auto& line : lines) {
            size_t line_len = 0;
            for (const auto& seg : line) line_len += seg.text.size();
        auto inner_segs = inner_.render(con, opt);

            if (options_.show_border) out.push_back({"│ ", border_style});
            for (const auto& seg : line) out.push_back(seg);
        // Group inner segments by line
        std::vector<std::vector<rich::Segment>> lines;
        lines.emplace_back();

            if (options_.show_border) {
                size_t pad = (inner_box_width > line_len) ? (inner_box_width - line_len) : 0;
                out.push_back({std::string(pad, ' ') + "│\n", border_style});
            } else {
                out.push_back({"\n", {}});
        for (const auto& s : inner_segs) {
            size_t start = 0;
            while (start < s.text.size()) {
                size_t nl = s.text.find('\n', start);
                if (nl == std::string::npos) {
                    lines.back().push_back({s.text.substr(start), s.style});
                    break;
                } else {
                    lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
        }

            std::string bot = "╰" + repeat_utf8("─", inner_box_width + 1) + "╯\n";
            out.push_back({std::move(bot), border_style});
        size_t inner_box_width = (w >= 2) ? (w - 2) : 0;

        for (const auto& line : lines) {
            size_t line_len = 0;
            for (const auto& seg : line) line_len += seg.text.size();
        auto inner_segs = inner_.render(con, opt);

            if (options_.show_border) out.push_back({"│ ", border_style});
            for (const auto& seg : line) out.push_back(seg);
        // Group inner segments by line
        std::vector<std::vector<rich::Segment>> lines;
        lines.emplace_back();

            if (options_.show_border) {
                size_t pad = (inner_box_width > line_len) ? (inner_box_width - line_len) : 0;
                out.push_back({std::string(pad, ' ') + "│\n", border_style});
            } else {
                out.push_back({"\n", {}});
        for (const auto& s : inner_segs) {
            size_t start = 0;
            while (start < s.text.size()) {
                size_t nl = s.text.find('\n', start);
                if (nl == std::string::npos) {
                    lines.back().push_back({s.text.substr(start), s.style});
                    break;
                } else {
                    lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
        }

            std::string bot = "╰" + repeat_utf8("─", inner_box_width + 1) + "╯\n";
            out.push_back({std::move(bot), border_style});
        size_t inner_box_width = (w >= 2) ? (w - 2) : 0;

        for (const auto& line : lines) {
            size_t line_len = 0;
            for (const auto& seg : line) line_len += seg.text.size();
        auto inner_segs = inner_.render(con, opt);

            if (options_.show_border) out.push_back({"│ ", border_style});
            for (const auto& seg : line) out.push_back(seg);
        // Group inner segments by line
        std::vector<std::vector<rich::Segment>> lines;
        lines.emplace_back();

            if (options_.show_border) {
                size_t pad = (inner_box_width > line_len) ? (inner_box_width - line_len) : 0;
                out.push_back({std::string(pad, ' ') + "│\n", border_style});
            } else {
                out.push_back({"\n", {}});
        for (const auto& s : inner_segs) {
            size_t start = 0;
            while (start < s.text.size()) {
                size_t nl = s.text.find('\n', start);
                if (nl == std::string::npos) {
                    lines.back().push_back({s.text.substr(start), s.style});
                    break;
                } else {
                    lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
        }

            std::string bot = "╰" + repeat_utf8("─", inner_box_width + 1) + "╯\n";
            out.push_back({std::move(bot), border_style});
        size_t inner_box_width = (w >= 2) ? (w - 2) : 0;

        for (const auto& line : lines) {
            size_t line_len = 0;
            for (const auto& seg : line) line_len += seg.text.size();
        auto inner_segs = inner_.render(con, opt);

            if (options_.show_border) out.push_back({"│ ", border_style});
            for (const auto& seg : line) out.push_back(seg);
        // Group inner segments by line
        std::vector<std::vector<rich::Segment>> lines;
        lines.emplace_back();

            if (options_.show_border) {
                size_t pad = (inner_box_width > line_len) ? (inner_box_width - line_len) : 0;
                out.push_back({std::string(pad, ' ') + "│\n", border_style});
            } else {
                out.push_back({"\n", {}});
        for (const auto& s : inner_segs) {
            size_t start = 0;
            while (start < s.text.size()) {
                size_t nl = s.text.find('\n', start);
                if (nl == std::string::npos) {
                    lines.back().push_back({s.text.substr(start), s.style});
                    break;
                } else {
                    lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
        }

            std::string bot = "╰" + repeat_utf8("─", inner_box_width + 1) + "╯\n";
            out.push_back({std::move(bot), border_style});
        size_t inner_box_width = (w >= 2) ? (w - 2) : 0;

        for (const auto& line : lines) {
            size_t line_len = 0;
            for (const auto& seg : line) line_len += seg.text.size();

            if (options_.show_border) out.push_back({"│ ", border_style});
            for (const auto& seg : line) out.push_back(seg);

            if (options_.show_border) {
                size_t pad = (inner_box_width > line_len) ? (inner_box_width - line_len) : 0;
                out.push_back({std::string(pad, ' ') + "│\n", border_style});
            } else {
                out.push_back({"\n", {}});
            }
        }

        if (options_.show_border) {
            std::string bot = "╰" + repeat_utf8("─", inner_box_width + 1) + "╯\n";
            out.push_back({std::move(bot), border_style});
        }

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto inner_segs = inner_.render(con, opt);
        size_t max_line = 0;
        size_t current = 0;
        size_t line_count = 1;
        for (const auto& s : inner_segs) {
            for (char c : s.text) {
                if (c == '\n') {
                    max_line = std::max(max_line, current);
                    current = 0;
                    ++line_count;
                } else {
                    ++current;
                }
            }
        }
        max_line = std::max(max_line, current);
        size_t width = std::max(max_line + 4, options_.title.size() + 6);
        return {width, line_count + 2};
    }

private:
    rich::AnyRenderable inner_;
    PanelOptions options_;
};

inline std::vector<rich::Segment> render_item(const Panel& p, const rich::Console& c, const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Panel& p, const rich::Console& c, const rich::Options& o) {
    return p.measure(c, o);
}

// Factory for operator|
inline auto panel(PanelOptions options) {
    return [options = std::move(options)](rich::AnyRenderable inner) mutable {
        return rich::AnyRenderable(Panel(std::move(inner), std::move(options)));
    };
}

} // namespace rich::panel
