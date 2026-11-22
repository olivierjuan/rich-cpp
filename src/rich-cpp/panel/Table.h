#pragma once
#include <rich_core.h>
#include <box.h>
#include <unicode/Unicode.h>
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
};

struct TableOptions {
    std::string title;
    box::Box box{box::ROUNDED};
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

inline std::string align_text(std::string_view text, size_t width, Justify j) {
    size_t tw = cell_width_table(text);
    if (tw >= width) return std::string(text);
    size_t diff = width - tw;
    if (j == Justify::Right) {
        return std::string(diff, ' ') + std::string(text);
    } else if (j == Justify::Center) {
        size_t left = diff / 2;
        size_t right = diff - left;
        return std::string(left, ' ') + std::string(text) + std::string(right, ' ');
    }
    return std::string(text) + std::string(diff, ' ');
}
} // namespace

class Table {
public:
    Table(TableOptions options = {}) : options_(std::move(options)) {}
    Table(std::string title) : options_{.title = std::move(title)} {}

    Table& add_column(std::string header, Justify justify = Justify::Left, std::string style = "bold") {
        cols_.push_back({std::move(header), justify, std::move(style)});
        return *this;
    }

    Table& add_row(std::vector<std::string> cells) {
        rows_.push_back(std::move(cells));
        return *this;
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        if (cols_.empty()) return out;

        const auto& bx = options_.box;

        std::vector<size_t> col_widths(cols_.size(), 0);
        for (size_t i = 0; i < cols_.size(); ++i) {
            col_widths[i] = std::max(col_widths[i], cell_width_table(cols_[i].header));
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

        rich::Style border_style{.dim = true, .color = rich::Color::from_ansi(90)};

        // Top border
        std::string top = std::string(bx.top_left);
        for (size_t i = 0; i < cols_.size(); ++i) {
            top += repeat_box(bx.top, col_widths[i]);
            top += (i + 1 < cols_.size()) ? bx.top_divider : (std::string(bx.top_right) + "\n");
        }
        out.push_back({std::move(top), border_style});

        // Header row
        for (size_t i = 0; i < cols_.size(); ++i) {
            out.push_back({std::string(bx.mid_left), border_style});
            size_t inner_w = (col_widths[i] >= 2) ? (col_widths[i] - 2) : 0;
            std::string aligned = align_text(cols_[i].header, inner_w, cols_[i].justify);
            out.push_back({" " + aligned + " ", rich::Style{.bold = true, .color = rich::Color::from_ansi(97)}});
        }
        out.push_back({std::string(bx.mid_right) + "\n", border_style});

        // Header separator
        std::string sep = std::string(bx.head_row_left);
        for (size_t i = 0; i < cols_.size(); ++i) {
            sep += repeat_box(bx.head_row_horizontal, col_widths[i]);
            sep += (i + 1 < cols_.size()) ? bx.head_row_cross : (std::string(bx.head_row_right) + "\n");
        }
        out.push_back({std::move(sep), border_style});

        // Rows
        for (const auto& r : rows_) {
            for (size_t i = 0; i < cols_.size(); ++i) {
                out.push_back({std::string(bx.mid_left), border_style});
                std::string cell_raw = (i < r.size()) ? r[i] : "";
                auto segs = rich::parse_markup(cell_raw);
                size_t text_len = 0;
                for (const auto& s : segs) text_len += cell_width_table(s.text);

                size_t inner_w = (col_widths[i] >= 2) ? (col_widths[i] - 2) : 0;
                size_t pad_diff = (inner_w > text_len) ? (inner_w - text_len) : 0;

                if (cols_[i].justify == Justify::Right) {
                    out.push_back({" " + std::string(pad_diff, ' '), {}});
                    for (auto& s : segs) out.push_back(std::move(s));
                    out.push_back({" ", {}});
                } else if (cols_[i].justify == Justify::Center) {
                    size_t left_pad = pad_diff / 2;
                    size_t right_pad = pad_diff - left_pad;
                    out.push_back({" " + std::string(left_pad, ' '), {}});
                    for (auto& s : segs) out.push_back(std::move(s));
                    out.push_back({std::string(right_pad, ' ') + " ", {}});
                } else {
                    out.push_back({" ", {}});
                    for (auto& s : segs) out.push_back(std::move(s));
                    out.push_back({std::string(pad_diff, ' ') + " ", {}});
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

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        size_t total_w = 1;
        for (const auto& c : cols_) total_w += cell_width_table(c.header) + 3;
        return {total_w, rows_.size() + 4};
    }

private:
    TableOptions options_;
    std::vector<ColumnOptions> cols_;
    std::vector<std::vector<std::string>> rows_;
};

inline std::vector<rich::Segment> render_item(const Table& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Table& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::table
