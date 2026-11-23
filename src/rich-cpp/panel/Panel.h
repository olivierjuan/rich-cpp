#pragma once
#include <rich_core.h>
#include <box.h>
#include <unicode/Unicode.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

namespace rich::panel {

struct PanelOptions {
    std::string title;
    std::string subtitle;    // Python Panel.subtitle (drawn on the bottom border)
    bool show_border{true};
    size_t width{0};  // 0 = auto, >0 = fixed width (clamped to console width)
    size_t height{0}; // 0 = fit content, >0 = fixed height (content padded)
    box::Box box{box::ROUNDED};
    bool expand{true};       // Python default True: stretch to console width
    std::string style;       // Python style: border + contents (default "none")
    std::string border_style; // Python border_style: added onto style for borders
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

inline size_t cell_width(std::string_view sv) {
    return unicode::string_cell_len(sv);
}

// Max line cell-width + line count of a segment list (cell-aware, wide chars).
inline std::pair<size_t, size_t> text_segs_extent(
    const std::vector<rich::Segment>& segs) {
    size_t max_line = 0, current = 0, line_count = 1;
    for (const auto& s : segs) {
        std::string_view sv(s.text);
        size_t i = 0;
        while (i < sv.size()) {
            if (sv[i] == '\n') {
                max_line = std::max(max_line, current);
                current = 0;
                ++line_count;
                ++i;
            } else {
                size_t j = i;
                char32_t cp = unicode::decode_utf8(sv, j);
                current += static_cast<size_t>(unicode::get_character_cell_size(cp));
                i = j;
            }
        }
    }
    max_line = std::max(max_line, current);
    return {max_line, line_count};
}

// Python Text.truncate parity: cut segments at max_cells cells (a wide char
// is dropped whole when it would not fit).
inline std::vector<rich::Segment> truncate_segs(
    const std::vector<rich::Segment>& segs, size_t max_cells) {
    std::vector<rich::Segment> out;
    size_t used = 0;
    for (const auto& s : segs) {
        std::string_view sv(s.text);
        size_t i = 0;
        while (i < sv.size()) {
            size_t j = i;
            char32_t cp = unicode::decode_utf8(sv, j);
            size_t cw = static_cast<size_t>(unicode::get_character_cell_size(cp));
            if (used + cw > max_cells) return out;
            used += cw;
            if (!out.empty() && out.back().style == s.style)
                out.back().text.append(sv.substr(i, j - i));
            else
                out.push_back({std::string(sv.substr(i, j - i)), s.style});
            i = j;
        }
    }
    return out;
}
} // namespace

class Panel {
public:
    Panel(rich::AnyRenderable inner, PanelOptions options = {})
        : inner_(std::move(inner)), options_(std::move(options)) {}

    // Python Panel.fit: alternative constructor with expand=False.
    static Panel fit(rich::AnyRenderable inner, PanelOptions options = {}) {
        options.expand = false;
        return Panel(std::move(inner), std::move(options));
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        const size_t max_w = (opt.width > 0) ? opt.width : 80;

        auto title_segs = rich::parse_markup(options_.title);
        size_t title_vis_w = 0;
        for (const auto& s : title_segs) title_vis_w += cell_width(s.text);
        auto sub_segs = rich::parse_markup(options_.subtitle);

        // Python: width = options.max_width if self.width is None
        // else min(options.max_width, self.width);
        // child_width = width - 2 (expand) or the content measurement (fit).
        size_t total_w;
        if (options_.width > 0) {
            total_w = std::min(options_.width, max_w);
        } else if (options_.expand) {
            total_w = max_w;
        } else {
            auto inner_probe = inner_.render(con, opt);
            total_w = text_segs_extent(inner_probe).first + 4; // borders + padding
        }
        // Python: with a title,
        // child_width = min(max_width - 2, max(child_width, title.cell_len + 2))
        // (title.cell_len includes the pad(1) spaces → +2 here); width = child_width + 2.
        if (!options_.title.empty())
            total_w = std::min(max_w, std::max(total_w, title_vis_w + 4));

        // Python: style = console.get_style(self.style);
        // border_style = style + console.get_style(self.border_style).
        auto resolve_style = [&con](const std::string& name) {
            if (name.empty() || name == "none") return rich::Style{};
            try {
                return con.get_style(name, "");
            } catch (const std::exception&) {
                return rich::Style{};
            }
        };
        rich::Style style = resolve_style(options_.style);
        rich::Style border_style = style + resolve_style(options_.border_style);
        const auto& bx = options_.box;

        if (options_.show_border) {
            // Python _title: markup text, no_wrap, expand_tabs, pad(1);
            // align_text: truncate(avail) then fill the excess with the box
            // top glyph (centered by default). Runs: [top_left+top]
            // [L x top] [" " T " " (pad spaces are part of the text runs)]
            // [R x top] [top + top_right].
            size_t avail = (total_w >= 4) ? (total_w - 4) : 0;
            std::vector<rich::Segment> title_runs = title_segs;
            title_runs.insert(title_runs.begin(), {" ", rich::Style{}});
            title_runs.push_back({" ", rich::Style{}});
            auto title_trunc = truncate_segs(title_runs, avail);
            size_t title_cells = 0;
            for (const auto& s : title_trunc) title_cells += cell_width(s.text);
            if (options_.title.empty() || title_cells == 0 || total_w <= 4) {
                size_t dashes = (total_w >= 2) ? (total_w - 2) : 0;
                out.push_back({std::string(bx.top_left) + repeat_utf8(bx.top, dashes) + std::string(bx.top_right) + "\n", border_style});
            } else {
                size_t excess = (avail > title_cells) ? (avail - title_cells) : 0;
                size_t left_dashes = excess / 2;
                size_t right_dashes = excess - left_dashes;
                out.push_back({std::string(bx.top_left) + std::string(bx.top), border_style});
                out.push_back({repeat_utf8(bx.top, left_dashes), border_style});
                for (auto& ts : title_trunc) out.push_back({std::move(ts.text), border_style + ts.style});
                out.push_back({repeat_utf8(bx.top, right_dashes), border_style});
                out.push_back({std::string(bx.top) + std::string(bx.top_right) + "\n", border_style});
            }
        }

        // Content is rendered at the content-region width (Python
        // child_options: width - 2, minus the (0,1) padding on each side).
        rich::Options inner_opt = opt;
        inner_opt.width = (total_w >= 4) ? (total_w - 4) : 0;
        auto inner_segs = inner_.render(con, inner_opt);

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
                    if (nl - start > 0)
                        lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
        }

        if (!lines.empty() && lines.back().empty()) {
            lines.pop_back();
        }

        // Fixed-height mode: pad with empty content lines (Python Panel height= parity).
        size_t content_rows = (options_.height > 2) ? (options_.height - 2) : 0;
        while (lines.size() < content_rows) lines.emplace_back();

        size_t inner_content_w = (total_w >= 4) ? (total_w - 4) : 0;

        // Python: lines = console.render_lines(renderable, child_options,
        // style=style, pad=True) — per line: mid_left, left pad space,
        // content runs (style + segment.style), right pad space, filler to
        // the content region, mid_right. The panel style applies to all.
        for (const auto& line : lines) {
            size_t line_vis_w = 0;
            for (const auto& seg : line) line_vis_w += cell_width(seg.text);

            if (options_.show_border) {
                out.push_back({std::string(bx.mid_left), border_style});
                out.push_back({" ", style});
            }
            for (const auto& seg : line)
                if (!seg.text.empty()) out.push_back({seg.text, style + seg.style});

            if (options_.show_border) {
                size_t pad = (inner_content_w > line_vis_w) ? (inner_content_w - line_vis_w) : 0;
                out.push_back({std::string(pad, ' '), style});
                out.push_back({" ", style});
                out.push_back({std::string(bx.mid_right) + "\n", border_style});
            } else {
                out.push_back({"\n", {}});
            }
        }

        if (options_.show_border) {
            // Python _subtitle: same pipeline as the title, on the bottom
            // border (box.bottom glyphs, subtitle_align default center).
            size_t avail = (total_w >= 4) ? (total_w - 4) : 0;
            std::vector<rich::Segment> sub_runs = sub_segs;
            sub_runs.insert(sub_runs.begin(), {" ", rich::Style{}});
            sub_runs.push_back({" ", rich::Style{}});
            auto sub_trunc = truncate_segs(sub_runs, avail);
            size_t sub_cells = 0;
            for (const auto& s : sub_trunc) sub_cells += cell_width(s.text);
            if (options_.subtitle.empty() || sub_cells == 0 || total_w <= 4) {
                size_t bottom_dashes = (total_w >= 2) ? (total_w - 2) : 0;
                std::string bot = std::string(bx.bottom_left) + repeat_utf8(bx.bottom, bottom_dashes) + std::string(bx.bottom_right) + "\n";
                out.push_back({std::move(bot), border_style});
            } else {
                size_t excess = (avail > sub_cells) ? (avail - sub_cells) : 0;
                size_t left_dashes = excess / 2;
                size_t right_dashes = excess - left_dashes;
                out.push_back({std::string(bx.bottom_left) + std::string(bx.bottom), border_style});
                out.push_back({repeat_utf8(bx.bottom, left_dashes), border_style});
                for (auto& ss : sub_trunc) out.push_back({std::move(ss.text), border_style + ss.style});
                out.push_back({repeat_utf8(bx.bottom, right_dashes), border_style});
                out.push_back({std::string(bx.bottom) + std::string(bx.bottom_right) + "\n", border_style});
            }
        }

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        const size_t max_w = (opt.width > 0) ? opt.width : 80;
        if (options_.width > 0) {
            return {options_.width, 3};
        }

        auto title_segs = rich::parse_markup(options_.title);
        size_t title_vis_w = 0;
        for (const auto& s : title_segs) title_vis_w += cell_width(s.text);

        auto inner_segs = inner_.render(con, opt);
        auto ext = text_segs_extent(inner_segs);

        size_t w;
        if (options_.expand) {
            w = max_w;
        } else {
            w = ext.first + 4;
        }
        if (!options_.title.empty())
            w = std::min(max_w, std::max(w, title_vis_w + 4));
        return {w, ext.second + 2};
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

inline auto panel(PanelOptions options) {
    return [options = std::move(options)](rich::AnyRenderable inner) mutable {
        return rich::AnyRenderable(Panel(std::move(inner), std::move(options)));
    };
}

} // namespace rich::panel
