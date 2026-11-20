#pragma once
#include <rich_core.h>
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

namespace {
inline std::string repeat_box(std::string_view glyph, size_t count) {
    std::string res;
    res.reserve(glyph.size() * count);
    for (size_t i = 0; i < count; ++i) res.append(glyph);
    return res;
}

inline std::string align_text(std::string_view text, size_t width, Justify j) {
    if (text.size() >= width) return std::string(text);
    size_t diff = width - text.size();
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
    Table(std::string title = "") : title_(std::move(title)) {}

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

        // Compute column widths
        std::vector<size_t> col_widths(cols_.size(), 0);
        for (size_t i = 0; i < cols_.size(); ++i) {
            col_widths[i] = std::max(col_widths[i], cols_[i].header.size());
        }
        for (const auto& r : rows_) {
            for (size_t i = 0; i < std::min(r.size(), cols_.size()); ++i) {
                // Strip markup approximation for width calculation
                std::string plain;
                bool in_tag = false;
                for (char c : r[i]) {
                    if (c == '[') in_tag = true;
                    else if (c == ']') in_tag = false;
                    else if (!in_tag) plain.push_back(c);
                }
                col_widths[i] = std::max(col_widths[i], plain.size());
            }
        }
        // Add 2 padding chars to each column
        for (auto& w : col_widths) w += 2;

        rich::Style border_style{.dim = true, .color = rich::Color::from_ansi(90)};

        // Top border: ╭─────┬─────╮
        std::string top = "╭";
        for (size_t i = 0; i < cols_.size(); ++i) {
            top += repeat_box("─", col_widths[i]);
            top += (i + 1 < cols_.size()) ? "┬" : "╮\n";
        }
        out.push_back({std::move(top), border_style});

        // Header row
        for (size_t i = 0; i < cols_.size(); ++i) {
            out.push_back({"│", border_style});
            std::string padded = align_text(" " + cols_[i].header + " ", col_widths[i], cols_[i].justify);
            out.push_back({std::move(padded), rich::Style{.bold = true, .color = rich::Color::from_ansi(97)}});
        }
        out.push_back({"│\n", border_style});

        // Header separator: ├─────┼─────┤
        std::string sep = "├";
        for (size_t i = 0; i < cols_.size(); ++i) {
            sep += repeat_box("─", col_widths[i]);
            sep += (i + 1 < cols_.size()) ? "┼" : "┤\n";
        }
        out.push_back({std::move(sep), border_style});

        // Rows
        for (const auto& r : rows_) {
            for (size_t i = 0; i < cols_.size(); ++i) {
                out.push_back({"│", border_style});
                std::string cell_raw = (i < r.size()) ? r[i] : "";
                // Parse markup in cell
                auto segs = rich::parse_markup(cell_raw);
                size_t text_len = 0;
                for (const auto& s : segs) text_len += s.text.size();

                out.push_back({" ", {}});
                for (auto& s : segs) out.push_back(std::move(s));

                size_t pad_need = (col_widths[i] > text_len + 1) ? (col_widths[i] - text_len - 1) : 0;
                out.push_back({std::string(pad_need, ' '), {}});
            }
            out.push_back({"│\n", border_style});
        }

        // Bottom border: ╰─────┴─────╯
        std::string bot = "╰";
        for (size_t i = 0; i < cols_.size(); ++i) {
            bot += repeat_box("─", col_widths[i]);
            bot += (i + 1 < cols_.size()) ? "┴" : "╯\n";
        }
        out.push_back({std::move(bot), border_style});

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        size_t total_w = 1;
        for (const auto& c : cols_) total_w += c.header.size() + 3;
        return {total_w, rows_.size() + 4};
    }

private:
    std::string title_;
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
