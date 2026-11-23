#pragma once
// rich::grid_detail — the borderless grid engine behind rich::decorator::Columns
// and rich::progress::Progress.
//
// Full port of rich.table.Table with box=None (Table.grid):
//   _calculate_column_widths: per-column measurement maxima ("or 1"),
//   _collapse_widths over the WRAPABLE columns only (width is None and
//   not no_wrap), last-resort even ratio_reduce, re-measure at the new
//   widths, and the expand fallback ratio_distribute(max - total, widths).
//   _render: each cell is a Padding(expand=True) around the renderable —
//   it renders at the full column width (inner = col_width - left - right),
//   options justify=None, no_wrap=<column>, overflow="ellipsis"; lines are
//   top-aligned and shape-filled to the column width.
//
// rich 15 Padding default expand=True means the blank (top/bottom) lines are
// column-width wide and an Align-wrapped cell pads across the full inner
// width, which is why short items sit at left/right/center in wide columns.

#include <rich_core.h>
#include <unicode/Unicode.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace rich::grid_detail {

// Python _ratio.ratio_distribute(total, ratios): sequential greedy ceil.
inline std::vector<size_t> ratio_distribute(size_t total,
                                            const std::vector<size_t>& ratios) {
    size_t total_ratio = 0;
    for (auto r : ratios) total_ratio += r;
    std::vector<size_t> out;
    out.reserve(ratios.size());
    size_t remaining = total;
    for (auto ratio : ratios) {
        size_t distributed =
            (total_ratio > 0) ? (ratio * remaining + total_ratio - 1) / total_ratio
                              : remaining;
        out.push_back(distributed);
        total_ratio -= ratio;
        remaining -= distributed;
    }
    return out;
}

// Python round() = half-to-even (banker's).
inline size_t py_round(double x) {
    double f = std::floor(x);
    double d = x - f;
    if (d > 0.5) return static_cast<size_t>(f + 1);
    if (d < 0.5) return static_cast<size_t>(f);
    return (static_cast<size_t>(f) % 2 == 0) ? static_cast<size_t>(f)
                                             : static_cast<size_t>(f + 1);
}

// Python _ratio.ratio_reduce(total, ratios, maximums, values): shrink `values`
// by `total`, proportionally to `ratios` (capped by `maximums`).
inline std::vector<size_t> ratio_reduce(size_t total, const std::vector<size_t>& ratios,
                                        const std::vector<size_t>& maximums,
                                        const std::vector<size_t>& values) {
    std::vector<size_t> r;
    r.reserve(ratios.size());
    size_t tr = 0;
    for (size_t i = 0; i < ratios.size(); ++i)
        if (maximums[i]) {
            r.push_back(ratios[i]);
            tr += ratios[i];
        } else r.push_back(0);
    if (tr == 0) return values;
    size_t remaining = total;
    std::vector<size_t> out;
    out.reserve(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        if (r[i] && tr > 0) {
            size_t distributed = std::min(
                maximums[i], py_round(r[i] * static_cast<double>(remaining) / tr));
            out.push_back(values[i] >= distributed ? values[i] - distributed : 0);
            remaining -= distributed;
            tr -= r[i];
        } else {
            out.push_back(values[i]);
        }
    }
    return out;
}

// Python Table._collapse_widths(widths, wrapable, max_width): shrink the
// largest WRAPABLE column(s) towards the second-largest (wrapable) until the
// total fits.
inline void collapse_widths(std::vector<size_t>& widths,
                            const std::vector<bool>& wrapable, size_t max_width) {
    auto total_of = [](const std::vector<size_t>& w) {
        size_t t = 0;
        for (auto x : w) t += x;
        return t;
    };
    size_t total_width = total_of(widths);
    size_t excess_width = total_width > max_width ? total_width - max_width : 0;
    bool any_wrapable = false;
    for (auto w : wrapable) any_wrapable = any_wrapable || w;
    while (total_width > 0 && excess_width > 0 && any_wrapable) {
        size_t max_column = 0;
        for (size_t i = 0; i < widths.size(); ++i)
            if (wrapable[i]) max_column = std::max(max_column, widths[i]);
        size_t second_max_column = 0;
        for (size_t i = 0; i < widths.size(); ++i)
            if (wrapable[i] && widths[i] != max_column)
                second_max_column = std::max(second_max_column, widths[i]);
        size_t column_difference = max_column - second_max_column;
        std::vector<size_t> ratios(widths.size(), 0);
        bool any_ratio = false;
        for (size_t i = 0; i < widths.size(); ++i)
            if (wrapable[i] && widths[i] == max_column) {
                ratios[i] = 1;
                any_ratio = true;
            }
        if (!any_ratio || column_difference == 0) break;
        std::vector<size_t> max_reduce(widths.size(),
                                       std::min(excess_width, column_difference));
        widths = ratio_reduce(excess_width, ratios, max_reduce, widths);
        total_width = total_of(widths);
        excess_width = total_width > max_width ? total_width - max_width : 0;
    }
}

inline std::vector<std::vector<rich::Segment>> split_lines(std::vector<rich::Segment> segs) {
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
            if (nl > start) lines.back().push_back({s.text.substr(start, nl - start), s.style});
            lines.emplace_back();
            start = nl + 1;
        }
    }
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

inline size_t line_cells(const std::vector<rich::Segment>& line) {
    size_t w = 0;
    for (auto& s : line) w += unicode::string_cell_len(s.text);
    return w;
}

// Grid options mirroring Table.grid(...): padding, collapse_padding,
// pad_edge, expand. `no_wrap` is per table column (size 0 = all false).
// `align` applies to every column (Python Columns wraps each cell in
// _Align(renderable, align); Progress uses none → justify=None).
struct GridOptions {
    size_t pad_top{0}, pad_right{1}, pad_bottom{0}, pad_left{1};
    bool collapse_padding{true};
    bool pad_edge{false};
    bool expand{false};
    std::vector<bool> no_wrap;      // per table column (size 0 = all false)
    std::vector<size_t> fixed_width; // per table column content width (0 = flexible)
    std::optional<int> align;  // -1 left, 0 center, 1 right (0 = none/left)
};

// rows: one vector per row; a nullopt cell is an empty slot (shape-fill
// only). Column count = rows[0].size() (all rows must agree).
inline std::vector<rich::Segment>
render_grid_table(const rich::Console& con, const rich::Options& opt,
                  const std::vector<std::vector<std::optional<rich::AnyRenderable>>>& rows,
                  const GridOptions& g, size_t* table_width_out = nullptr) {
    if (rows.empty()) return {};
    const size_t max_width = (opt.width > 0) ? opt.width : 80;
    const size_t column_count = rows[0].size();
    if (column_count == 0) return {};

    // _measure_column: fixed-width columns measure (width + padding) capped
    // at max_width; flexible columns measure their cells (Padding(expand=True)
    // = child + left + right, capped at max_width). A flexible column with no
    // cells measures 0 -> 1.
    std::vector<size_t> max_child(column_count, 0);
    for (auto& row : rows)
        for (size_t ci = 0; ci < row.size() && ci < column_count; ++ci)
            if (row[ci])
                max_child[ci] =
                    std::max(max_child[ci], row[ci]->measure(con, opt).second);

    // _get_padding_width with collapse_padding / pad_edge semantics:
    // the left of every non-first column collapses to max(0, left - right);
    // pad_edge drops the left of the first column and the right of the last.
    std::vector<size_t> pad_w(column_count);
    for (size_t ci = 0; ci < column_count; ++ci)
        pad_w[ci] = (ci + 1 == column_count && !g.pad_edge) ? 0 : g.pad_right;

    std::vector<size_t> widths(column_count, 0);
    for (size_t ci = 0; ci < column_count; ++ci) {
        const bool fixed = ci < g.fixed_width.size() && g.fixed_width[ci] > 0;
        widths[ci] = fixed
                         ? std::min(g.fixed_width[ci] + pad_w[ci], max_width)
                         : (max_child[ci] > 0)
                               ? std::min(max_child[ci] + pad_w[ci], max_width)
                               : 1;
    }

    size_t total = 0;
    for (auto w : widths) total += w;
    if (total > max_width) {
        // [(column.width is None and not column.no_wrap) for column in columns]
        std::vector<bool> wrapable(column_count, true);
        for (size_t ci = 0; ci < column_count; ++ci) {
            if (ci < g.fixed_width.size() && g.fixed_width[ci] > 0) wrapable[ci] = false;
            if (ci < g.no_wrap.size() && g.no_wrap[ci]) wrapable[ci] = false;
        }
        collapse_widths(widths, wrapable, max_width);
        total = 0;
        for (auto w : widths) total += w;
        // last resort, reduce columns evenly
        if (total > max_width) {
            std::vector<size_t> ones(column_count, 1);
            widths = ratio_reduce(total - max_width, ones, widths, widths);
            total = 0;
            for (auto w : widths) total += w;
        }
    }

    // Re-measure at the new widths: cell measurement capped at the column
    // width (Measurement.get .with_maximum(max_width) — the cap equals the
    // width, so a column that grew (expand) re-measures to its natural
    // content + padding). Fixed columns re-measure to width + padding.
    for (size_t ci = 0; ci < column_count; ++ci) {
        const bool fixed = ci < g.fixed_width.size() && g.fixed_width[ci] > 0;
        if (fixed) {
            widths[ci] = std::min(widths[ci], g.fixed_width[ci] + pad_w[ci]);
            continue;
        }
        size_t m = 0;
        bool any = false;
        for (auto& row : rows)
            if (ci < row.size() && row[ci]) {
                any = true;
                m = std::max(m, row[ci]->measure(con, opt).second);
            }
        widths[ci] = any ? std::min(widths[ci], m + pad_w[ci]) : 0;
    }
    total = 0;
    for (auto w : widths) total += w;

    // Expand fallback: ratio_distribute(max_width - table_width, widths).
    if (total < max_width && g.expand) {
        auto pads = ratio_distribute(max_width - total, widths);
        for (size_t ci = 0; ci < column_count; ++ci) widths[ci] += pads[ci];
        total = max_width;
    }
    if (table_width_out) *table_width_out = total;

    // _get_padding(first_row, last_row) per column (Python Table._render).
    const size_t row_count = rows.size();
    auto cell_pad = [&](size_t ci, bool first_row, bool last_row) {
        struct P {
            size_t top, right, bottom, left;
        };
        P p{g.pad_top, g.pad_right, g.pad_bottom, g.pad_left};
        if (ci != 0) p.left = (p.left > p.right) ? p.left - p.right : 0;
        if (!last_row) p.bottom = (p.top > p.bottom) ? p.top - p.bottom : 0;
        if (!g.pad_edge) p.left = 0;
        if (ci + 1 == column_count && !g.pad_edge) p.right = 0;
        if (first_row) p.top = 0;
        if (last_row) p.bottom = 0;
        return p;
    };

    std::vector<rich::Segment> out;
    for (size_t ri = 0; ri < row_count; ++ri) {
        const bool first_row = (ri == 0);
        const bool last_row = (ri + 1 == row_count);

        struct Cell {
            std::vector<std::vector<rich::Segment>> lines;
            size_t pad_width{0};
            size_t inner{0};
            size_t left{0}, right{0}, top{0}, bottom{0};
        };
        std::vector<Cell> cells(column_count);
        size_t row_h = 1;
        for (size_t ci = 0; ci < column_count; ++ci) {
            Cell& cell = cells[ci];
            auto p = cell_pad(ci, first_row, last_row);
            cell.left = p.left;
            cell.right = p.right;
            cell.top = p.top;
            cell.bottom = p.bottom;
            if (ci >= rows[ri].size() || !rows[ri][ci]) continue;  // null cell
            // rich 15 Padding(expand=True): the cell renders at the full
            // column width, so inner = col_width - left - right and the
            // blank lines are col_width wide.
            cell.pad_width = widths[ci];
            cell.inner = widths[ci] - p.left - p.right;
            rich::Options copt{.show_border = false,
                               .width = cell.inner,
                               .height = 0,
                               .overflow = rich::Overflow::Ellipsis,
                               .no_wrap = (ci < g.no_wrap.size()) ? g.no_wrap[ci]
                                                                  : false};
            cell.lines = split_lines(rows[ri][ci]->render(con, copt));
            row_h = std::max(row_h, cell.lines.size());
        }

        for (size_t li = 0; li < row_h; ++li) {
            for (size_t ci = 0; ci < column_count; ++ci) {
                const Cell& cell = cells[ci];
                if (li < cell.top) {
                    out.push_back({std::string(cell.pad_width, ' '), {}});
                } else if (li >= cell.top + cell.lines.size()) {
                    // bottom blanks (padding width), then set_shape
                    // top-align fill (column width)
                    size_t w = (li < cell.top + cell.lines.size() + cell.bottom)
                                   ? cell.pad_width
                                   : widths[ci];
                    out.push_back({std::string(w, ' '), {}});
                } else {
                    const auto& ln = cell.lines[li - cell.top];
                    // Python _Align wrap (Columns): center/right pad each line
                    // within the inner (padding) width. Progress cells have
                    // justify=None → no padding.
                    size_t w = line_cells(ln);
                    size_t lead = 0, trail = 0;
                    if (w < cell.inner && g.align.has_value()) {
                        size_t extra = cell.inner - w;
                        if (*g.align == 1) {
                            lead = extra;
                        } else if (*g.align == 0) {
                            lead = extra / 2;
                            trail = extra - lead;
                        }
                    }
                    size_t line_extra = lead + trail;
                    if (cell.left)
                        out.push_back({std::string(cell.left, ' '), {}});
                    if (lead) out.push_back({std::string(lead, ' '), {}});
                    for (auto& s : ln) out.push_back(s);
                    if (trail) out.push_back({std::string(trail, ' '), {}});
                    // set_shape: pad the whole padded line to col width
                    size_t tail = cell.right +
                                  ((cell.left + w + line_extra + cell.right < widths[ci])
                                       ? widths[ci] - cell.left - w - line_extra - cell.right
                                       : 0);
                    if (tail) out.push_back({std::string(tail, ' '), {}});
                }
            }
            out.push_back({"\n", {}});
        }
    }
    return out;
}

} // namespace rich::grid_detail
