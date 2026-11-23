#pragma once
#include <rich_core.h>
#include <box.h>
#include <unicode/Unicode.h>
#include <Text.h>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

namespace rich::table {

enum class Justify { Left, Center, Right };

struct ColumnOptions {
    std::string header;
    Justify justify{Justify::Left};
    std::string style{"bold"};
    std::string footer;
};

struct TableOptions {
    std::string title;
    box::Box box{box::ROUNDED};
    std::string caption;      // Python Table.caption (rendered below, centered)
    bool expand{false};       // Python Table.expand: grow to console width
    bool show_footer{false};  // Python show_footer (footer text otherwise ignored)
};

namespace {
inline std::string repeat_box(std::string_view glyph, size_t count) {
    std::string res;
    res.reserve(glyph.size() * count);
    for (size_t i = 0; i < count; ++i) res.append(glyph);
    return res;
}

inline size_t cell_width_table(std::string_view sv) {
    return unicode::string_cell_len(sv);
}

// Python rich._ratio.ratio_distribute (no minimums): sequential greedy —
// each part takes ceil(ratio * remaining / remaining_ratio).
inline std::vector<size_t> ratio_distribute(size_t total,
                                            const std::vector<size_t>& ratios) {
    std::vector<size_t> out;
    out.reserve(ratios.size());
    long long total_ratio = 0;
    for (auto r : ratios) total_ratio += r;
    long long remaining = total;
    for (auto r : ratios) {
        long long d = (total_ratio > 0)
                          ? (static_cast<long long>(r) * remaining + total_ratio - 1) /
                                total_ratio
                          : remaining;
        out.push_back(static_cast<size_t>(d));
        total_ratio -= r;
        remaining -= d;
    }
    return out;
}
} // namespace

class Table {
public:
    Table(TableOptions options = {}) : options_(std::move(options)) {}
    Table(std::string title) : options_{.title = std::move(title)} {}

    Table& add_column(std::string header, Justify justify = Justify::Left, std::string style = "bold") {
        cols_.push_back({std::move(header), justify, std::move(style), {}});
        return *this;
    }

    // Python add_column(header, footer, *, style, justify, ...).
    Table& add_column(std::string header, std::string footer,
                      Justify justify = Justify::Left, std::string style = "bold") {
        cols_.push_back({std::move(header), justify, std::move(style), std::move(footer)});
        return *this;
    }

    Table& add_row(std::vector<std::string> cells) {
        rows_.push_back(std::move(cells));
        sections_.push_back(false);
        return *this;
    }

    // Python add_section(): end a section and draw a line after the last row.
    Table& add_section() {
        if (!rows_.empty()) sections_.back() = true;
        return *this;
    }

    // Python row_styles: styles applied cyclically to each data row.
    Table& row_styles(std::vector<rich::Style> styles) {
        row_styles_ = std::move(styles);
        return *this;
    }

    Table& caption(std::string c) {
        options_.caption = std::move(c);
        return *this;
    }

    Table& expand(bool e) {
        options_.expand = e;
        return *this;
    }

    Table& show_footer(bool s) {
        options_.show_footer = s;
        return *this;
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        if (cols_.empty()) return out;

        const auto& bx = options_.box;
        const bool show_footer = options_.show_footer;

        // Column widths: max(header, footer, cells) + 2 (pad_edge=True,
        // padding=(0,1) — the Python default), footer only measured when
        // shown (Python _get_cells skips hidden footers).
        std::vector<size_t> col_widths(cols_.size(), 0);
        for (size_t i = 0; i < cols_.size(); ++i) {
            col_widths[i] = std::max(col_widths[i], cell_width_table(cols_[i].header));
            if (show_footer)
                col_widths[i] = std::max(col_widths[i], cell_width_table(cols_[i].footer));
        }
        for (const auto& r : rows_) {
            for (size_t i = 0; i < std::min(r.size(), cols_.size()); ++i) {
                auto segs = rich::parse_markup(r[i]);
                size_t text_len = 0;
                for (const auto& s : segs) text_len += cell_width_table(s.text);
                col_widths[i] = std::max(col_widths[i], text_len);
            }
        }
        for (auto& w : col_widths) w += 2;

        // Python _extra_width: 2 (edge) + (n-1) (dividers).
        const size_t extra_width = 2 + (cols_.size() - 1);
        // Python _calculate_column_widths expand branch (no ratios): the
        // free space up to the console width is distributed proportionally
        // to the current widths (ratio_distribute).
        if (options_.expand) {
            const size_t max_width = (opt.width > 0) ? opt.width : 80;
            size_t table_width = 0;
            for (auto w : col_widths) table_width += w;
            if (table_width < max_width - extra_width) {
                const auto pad =
                    ratio_distribute(max_width - extra_width - table_width, col_widths);
                for (size_t i = 0; i < cols_.size(); ++i) col_widths[i] += pad[i];
            }
        }

        // Python table.py: table_style = console.get_style(style or ""),
        // border_style = table_style + console.get_style(border_style or
        // ""), header_style = console.get_style("table.header" or "").
        // The C++ Table API has no style/border_style parameters (both
        // default None in Python), so the border is console.get_style("") —
        // a theme lookup for "" that parses to the null style when absent
        // (plain border). Rich 15.0.0 default_styles: "table.header" and
        // "table.footer" are Style(bold=True), no color. (Previously
        // hardcoded to bright-black border / bright-white header.)
        auto border_style = con.get_style("", "");
        auto header_style = con.get_style("table.header", "");
        auto footer_style = con.get_style("table.footer", "");

        // Python get_row widths span the whole column incl. padding; the
        // head/row/foot glyph sets are identical in every standard box, so
        // head_row_* serves all three.
        auto row_line = [&](bool with_newline) {
            std::string sep = std::string(bx.head_row_left);
            for (size_t i = 0; i < cols_.size(); ++i) {
                sep += repeat_box(bx.head_row_horizontal, col_widths[i]);
                sep += (i + 1 < cols_.size()) ? bx.head_row_cross : (std::string(bx.head_row_right) + (with_newline ? "\n" : ""));
            }
            out.push_back({std::move(sep), border_style});
        };

        // Top border
        std::string top = std::string(bx.top_left);
        for (size_t i = 0; i < cols_.size(); ++i) {
            top += repeat_box(bx.top, col_widths[i]);
            top += (i + 1 < cols_.size()) ? bx.top_divider : (std::string(bx.top_right) + "\n");
        }
        out.push_back({std::move(top), border_style});

        // Header row (table.header style, no row style)
        {
            for (size_t i = 0; i < cols_.size(); ++i) {
                out.push_back({std::string(i == 0 ? bx.mid_left : bx.mid_vertical), border_style});
                auto segs = rich::parse_markup(cols_[i].header);
                size_t text_len = 0;
                for (const auto& s : segs) text_len += cell_width_table(s.text);
                size_t inner_w = (col_widths[i] >= 2) ? (col_widths[i] - 2) : 0;
                size_t pad_diff = (inner_w > text_len) ? (inner_w - text_len) : 0;
                if (cols_[i].justify == Justify::Right) {
                    out.push_back({" " + std::string(pad_diff, ' '), header_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), header_style});
                    out.push_back({" ", header_style});
                } else if (cols_[i].justify == Justify::Center) {
                    size_t left_pad = pad_diff / 2;
                    size_t right_pad = pad_diff - left_pad;
                    out.push_back({" " + std::string(left_pad, ' '), header_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), header_style});
                    out.push_back({std::string(right_pad, ' ') + " ", header_style});
                } else {
                    out.push_back({" ", header_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), header_style});
                    out.push_back({std::string(pad_diff, ' ') + " ", header_style});
                }
            }
            out.push_back({std::string(bx.mid_right) + "\n", border_style});
        }

        // Header separator
        row_line(true);

        // Rows (row_styles applied cyclically: Python index % len)
        for (size_t ri = 0; ri < rows_.size(); ++ri) {
            const auto& r = rows_[ri];
            rich::Style row_style{};
            if (!row_styles_.empty())
                row_style = row_styles_[ri % row_styles_.size()];
            // Python: the whole row (cell + padding) carries the row style
            // via Segment.apply_style; borders do not.
            for (size_t i = 0; i < cols_.size(); ++i) {
                out.push_back({std::string(i == 0 ? bx.mid_left : bx.mid_vertical), border_style});
                std::string cell_raw = (i < r.size()) ? r[i] : "";
                auto segs = rich::parse_markup(cell_raw);
                size_t text_len = 0;
                for (const auto& s : segs) text_len += cell_width_table(s.text);
                size_t inner_w = (col_widths[i] >= 2) ? (col_widths[i] - 2) : 0;
                size_t pad_diff = (inner_w > text_len) ? (inner_w - text_len) : 0;
                if (cols_[i].justify == Justify::Right) {
                    out.push_back({" " + std::string(pad_diff, ' '), row_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), row_style + s.style});
                    out.push_back({" ", row_style});
                } else if (cols_[i].justify == Justify::Center) {
                    size_t left_pad = pad_diff / 2;
                    size_t right_pad = pad_diff - left_pad;
                    out.push_back({" " + std::string(left_pad, ' '), row_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), row_style + s.style});
                    out.push_back({std::string(right_pad, ' ') + " ", row_style});
                } else {
                    out.push_back({" ", row_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), row_style + s.style});
                    out.push_back({std::string(pad_diff, ' ') + " ", row_style});
                }
            }
            out.push_back({std::string(bx.mid_right) + "\n", border_style});
            // Section line after this row (Python end_section; never after
            // the last data row — a foot line or the bottom border follows).
            if (ri + 1 < rows_.size() && (ri < sections_.size()) && sections_[ri])
                row_line(true);
        }

        // Footer (Python: foot line + footer row, table.footer style —
        // bold by default, separately overridable in a theme)
        if (show_footer) {
            row_line(true);
            for (size_t i = 0; i < cols_.size(); ++i) {
                out.push_back({std::string(i == 0 ? bx.mid_left : bx.mid_vertical), border_style});
                auto segs = rich::parse_markup(cols_[i].footer);
                size_t text_len = 0;
                for (const auto& s : segs) text_len += cell_width_table(s.text);
                size_t inner_w = (col_widths[i] >= 2) ? (col_widths[i] - 2) : 0;
                size_t pad_diff = (inner_w > text_len) ? (inner_w - text_len) : 0;
                if (cols_[i].justify == Justify::Right) {
                    out.push_back({" " + std::string(pad_diff, ' '), footer_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), footer_style});
                    out.push_back({" ", footer_style});
                } else if (cols_[i].justify == Justify::Center) {
                    size_t left_pad = pad_diff / 2;
                    size_t right_pad = pad_diff - left_pad;
                    out.push_back({" " + std::string(left_pad, ' '), footer_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), footer_style});
                    out.push_back({std::string(right_pad, ' ') + " ", footer_style});
                } else {
                    out.push_back({" ", footer_style});
                    for (auto& s : segs) out.push_back({std::move(s.text), footer_style});
                    out.push_back({std::string(pad_diff, ' ') + " ", footer_style});
                }
            }
            out.push_back({std::string(bx.mid_right) + "\n", border_style});
        }

        // Bottom border
        std::string bot = std::string(bx.bottom_left);
        for (size_t i = 0; i < cols_.size(); ++i) {
            bot += repeat_box(bx.bottom, col_widths[i]);
            bot += (i + 1 < cols_.size()) ? bx.bottom_divider : (std::string(bx.bottom_right) + "\n");
        }
        out.push_back({std::move(bot), border_style});

        // Caption (Python: rendered below the table at the table width,
        // centered, table.caption style).
        if (!options_.caption.empty()) {
            size_t table_width = extra_width;
            for (auto w : col_widths) table_width += w;
            rich::Style cap_style;
            try {
                cap_style = con.get_style("table.caption", "");
            } catch (const std::exception&) {
                cap_style = rich::Style{.italic = true, .dim = true};
            }
            rich::text::Text cap(options_.caption, cap_style);
            cap.set_justify(rich::text::Justify::Center);
            auto segs = cap.render(con, rich::Options{.width = table_width});
            // Python yields the caption lines without a trailing newline
            // beyond the normal line ends — render() already ends each line
            // with "\n" segments; the last one is the table's final newline.
            out.insert(out.end(), segs.begin(), segs.end());
        }

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        size_t total_w = 1;
        for (size_t i = 0; i < cols_.size(); ++i) {
            size_t w = std::max(cell_width_table(cols_[i].header),
                                (options_.show_footer)
                                    ? cell_width_table(cols_[i].footer)
                                    : size_t(0));
            for (const auto& r : rows_)
                if (i < r.size()) w = std::max(w, cell_width_table(r[i]));
            total_w += w + 3;
        }
        size_t lines = rows_.size() + 4;
        if (options_.show_footer) lines += 2;
        for (auto s : sections_) if (s) ++lines;
        if (!options_.caption.empty()) lines += 2;
        return {total_w, lines};
    }

private:
    TableOptions options_;
    std::vector<ColumnOptions> cols_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<bool> sections_;
    std::vector<rich::Style> row_styles_;
};

inline std::vector<rich::Segment> render_item(const Table& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Table& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::table
