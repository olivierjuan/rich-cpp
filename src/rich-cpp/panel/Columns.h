#pragma once
#include <rich_core.h>
#include <unicode/Unicode.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <vector>
#include <string>

namespace rich::decorator {

namespace {

// Python _ratio.ratio_distribute(total, ratios): sequential greedy ceil.
inline std::vector<size_t> cols_ratio_distribute(size_t total,
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
inline std::vector<size_t> cols_ratio_reduce(size_t total, const std::vector<size_t>& ratios,
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
            size_t distributed = std::min(maximums[i], py_round(r[i] * static_cast<double>(remaining) / tr));
            out.push_back(values[i] >= distributed ? values[i] - distributed : 0);
            remaining -= distributed;
            tr -= r[i];
        } else {
            out.push_back(values[i]);
        }
    }
    return out;
}

// Python Table._collapse_widths: shrink the largest wrapable column(s)
// towards the second-largest until the total fits.
inline size_t cols_collapse_widths(std::vector<size_t>& widths, size_t max_width) {
    size_t total_width = 0;
    for (auto w : widths) total_width += w;
    size_t excess_width = total_width > max_width ? total_width - max_width : 0;
    while (total_width > 0 && excess_width > 0) {
        size_t max_column = 0;
        for (auto w : widths) max_column = std::max(max_column, w);
        size_t second_max_column = 0;
        for (auto w : widths)
            if (w != max_column) second_max_column = std::max(second_max_column, w);
        size_t column_difference = max_column - second_max_column;
        std::vector<size_t> ratios;
        bool any_ratio = false;
        for (auto w : widths) {
            if (w == max_column) {
                ratios.push_back(1);
                any_ratio = true;
            } else ratios.push_back(0);
        }
        if (!any_ratio || column_difference == 0) break;
        std::vector<size_t> max_reduce(widths.size(), std::min(excess_width, column_difference));
        widths = cols_ratio_reduce(excess_width, ratios, max_reduce, widths);
        total_width = 0;
        for (auto w : widths) total_width += w;
        excess_width = total_width > max_width ? total_width - max_width : 0;
    }
    return total_width;
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

} // namespace

// Python rich.columns.Columns port. The layout is a Table.grid
// (box=None, collapse_padding=True, pad_edge=False): no borders; each cell
// is a Padding around the (optionally Constrain/Align-wrapped) renderable;
// column content width = measured max of the column's items (or the fixed
// `width`); the table-column defaults are justify=None (left),
// no_wrap=False, overflow="ellipsis"; `expand` distributes leftover console
// width proportionally to the current column widths (ratio_distribute with
// ratios = widths).
class Columns {
public:
    enum class Align { Left, Center, Right };

    struct Options {
        size_t pad_top{0}, pad_right{1}, pad_bottom{0}, pad_left{1};
        std::optional<size_t> width;  // fixed column width (Python `width`)
        bool expand{false};
        bool equal{false};
        bool column_first{false};
        bool right_to_left{false};
        std::optional<Align> align;
        std::string title;            // table title (centered, table.title theme)
        size_t max_columns{0};        // C++-only cap on column count (0 = auto)
    };

    Columns(std::vector<rich::AnyRenderable> renderables, Options options = {})
        : renderables_(std::move(renderables)), options_(std::move(options)) {}

    // Legacy constructor: spacing maps to symmetric horizontal padding.
    Columns(std::vector<rich::AnyRenderable> renderables, size_t spacing, size_t max_columns)
        : Columns(std::move(renderables),
                  Options{.pad_top = 0, .pad_right = spacing, .pad_bottom = 0,
                          .pad_left = spacing, .max_columns = max_columns}) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        if (renderables_.empty()) return {};
        const size_t max_width = (opt.width > 0) ? opt.width : 80;
        const size_t n = renderables_.size();
        const size_t width_padding = std::max(options_.pad_left, options_.pad_right);

        // Measurement maxima (Python Measurement.get(...).maximum).
        std::vector<size_t> rw(n, 0);
        for (size_t i = 0; i < n; ++i) rw[i] = renderables_[i].measure(con, opt).second;
        if (options_.equal) {
            size_t m = *std::max_element(rw.begin(), rw.end());
            std::fill(rw.begin(), rw.end(), m);
        }

        // Python iter_renderables(column_count): (item-index | null) slots in
        // yield order. column_first fills column-major, yields row-major and
        // stops at the first null; other order yields in item order. The
        // trailing (0, None) padding is shared by both branches.
        auto iter_items = [this, n](size_t cc) {
            struct Slot {
                size_t idx;
                bool valid;
            };
            std::vector<Slot> slots;
            if (options_.column_first) {
                std::vector<size_t> column_lengths(cc, n / cc);
                for (size_t k = 0; k < n % cc; ++k) column_lengths[k] += 1;
                size_t row_count = (n + cc - 1) / cc;
                std::vector<std::vector<Slot>> cells(row_count, std::vector<Slot>(cc, {0, false}));
                size_t row = 0, col = 0;
                for (size_t idx = 0; idx < n; ++idx) {
                    cells[row][col] = {idx, true};
                    column_lengths[col] -= 1;
                    if (column_lengths[col]) {
                        ++row;
                    } else {
                        ++col;
                        row = 0;
                    }
                }
                for (size_t r = 0; r < row_count; ++r) {
                    bool stop = false;
                    for (size_t c = 0; c < cc; ++c) {
                        if (!cells[r][c].valid) {
                            stop = true;
                            break;
                        }
                        slots.push_back(cells[r][c]);
                    }
                    if (stop) break;
                }
            } else {
                for (size_t i = 0; i < n; ++i) slots.push_back({i, true});
            }
            if (n % cc)
                for (size_t k = 0; k < cc - n % cc; ++k) slots.push_back({0, false});
            return slots;
        };

        size_t column_count = n;
        if (options_.max_columns) column_count = std::min(column_count, options_.max_columns);
        if (options_.width) {
            column_count = std::max<size_t>(
                1, max_width / (options_.width.value() + width_padding));
        } else {
            // Python width-fit loop: shrink column_count while the widest
            // prefix of columns exceeds max_width.
            while (column_count > 1) {
                std::vector<size_t> widths(column_count, 0);
                size_t touched = 0;
                size_t column_no = 0;
                bool shrunk = false;
                for (auto& s : iter_items(column_count)) {
                    widths[column_no] = std::max(widths[column_no], s.valid ? rw[s.idx] : size_t{0});
                    if (column_no + 1 > touched) touched = column_no + 1;
                    size_t total = 0;
                    for (size_t ci = 0; ci < touched; ++ci) total += widths[ci];
                    total += width_padding * (touched > 0 ? touched - 1 : 0);
                    if (total > max_width) {
                        column_count = touched - 1;
                        shrunk = true;
                        break;
                    }
                    column_no = (column_no + 1) % column_count;
                }
                if (!shrunk) break;
            }
        }

        // Rows (each row reversed for right_to_left).
        auto slots = iter_items(column_count);
        std::vector<std::vector<std::optional<size_t>>> rows;
        for (size_t start = 0; start < slots.size(); start += column_count) {
            std::vector<std::optional<size_t>> row;
            for (size_t k = start; k < start + column_count && k < slots.size(); ++k)
                row.push_back(slots[k].valid ? std::optional<size_t>{slots[k].idx}
                                             : std::optional<size_t>{});
            if (options_.right_to_left) std::reverse(row.begin(), row.end());
            rows.push_back(std::move(row));
        }

        // Column content widths from the actual (Constrain-capped) items.
        const size_t constrain_w = options_.equal ? rw[0] : 0;
        std::vector<size_t> content_w(column_count, 0);
        for (auto& row : rows)
            for (size_t ci = 0; ci < row.size(); ++ci)
                if (row[ci]) {
                    size_t w = renderables_[*row[ci]].measure(con, opt).second;
                    if (options_.equal && w > constrain_w) w = constrain_w;
                    content_w[ci] = std::max(content_w[ci], w);
                }
        if (options_.width)
            std::fill(content_w.begin(), content_w.end(), options_.width.value());

        // _get_padding_width with collapse_padding=True, pad_edge=False:
        // left collapses to 0, right stays; the last column drops right.
        std::vector<size_t> pad_w(column_count);
        for (size_t ci = 0; ci < column_count; ++ci)
            pad_w[ci] = (ci + 1 == column_count) ? 0 : options_.pad_right;

        std::vector<size_t> widths(column_count);
        for (size_t ci = 0; ci < column_count; ++ci)
            widths[ci] = content_w[ci] + pad_w[ci];
        size_t total = 0;
        for (auto w : widths) total += w;
        if (total > max_width) total = cols_collapse_widths(widths, max_width);
        if (total < max_width && options_.expand) {
            auto pads = cols_ratio_distribute(max_width - total, widths);
            for (size_t ci = 0; ci < column_count; ++ci) widths[ci] += pads[ci];
            total = max_width;
        }

        std::vector<rich::Segment> out;
        if (!options_.title.empty()) {
            // Table title: render_str (plain, no markup) centered at the
            // table width, table.title theme style.
            rich::Style ts;
            try {
                ts = con.get_style("table.title", "");
            } catch (...) {
            }
            size_t tvis = unicode::string_cell_len(options_.title);
            size_t pad = (total > tvis) ? total - tvis : 0;
            size_t left = pad / 2;
            size_t right = pad - left;
            out.push_back({std::string(left, ' ') + options_.title + std::string(right, ' '), ts});
            out.push_back({"\n", {}});
        }

        const size_t row_count = rows.size();
        for (size_t ri = 0; ri < row_count; ++ri) {
            const bool first_row = (ri == 0);
            const bool last_row = (ri + 1 == row_count);
            // get_padding(first_row, last_row) per column.
            auto cell_pad = [&](size_t ci) {
                struct P {
                    size_t top, right, bottom, left;
                };
                P p{options_.pad_top, options_.pad_right, options_.pad_bottom,
                    options_.pad_left};
                if (ci != 0) p.left = (p.left > p.right) ? p.left - p.right : 0;
                if (!last_row) p.bottom = (p.top > p.bottom) ? p.top - p.bottom : 0;
                if (ci == 0) p.left = 0;
                if (ci + 1 == column_count) p.right = 0;
                if (first_row) p.top = 0;
                if (last_row) p.bottom = 0;
                return p;
            };

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
                auto p = cell_pad(ci);
                cell.left = p.left;
                cell.right = p.right;
                cell.top = p.top;
                cell.bottom = p.bottom;
                const auto& item = rows[ri][ci];
                if (!item) continue;  // null cell: shape-fill only
                // rich 15 Padding(expand=True): the cell renders at the full
                // column width, so inner = col_width - left - right and the
                // blank lines are col_width wide.
                cell.pad_width = widths[ci];
                cell.inner = widths[ci] - p.left - p.right;
                rich::Options copt{.show_border = false,
                                   .width = cell.inner,
                                   .height = 0,
                                   .overflow = rich::Overflow::Ellipsis,
                                   .no_wrap = false};
                cell.lines = split_lines(renderables_[*item].render(con, copt));
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
                        // Python Align wrap: center/right pad each line
                        // within the inner (padding) width.
                        size_t w = line_cells(ln);
                        size_t lead = 0, trail = 0;
                        if (w < cell.inner) {
                            size_t extra = cell.inner - w;
                            if (options_.align == Align::Right) {
                                lead = extra;
                            } else if (options_.align == Align::Center) {
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

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        // Python Columns has no __rich_measure__ → Measurement(0, max_width).
        (void)con;
        return {0, (opt.width > 0) ? opt.width : 0};
    }

private:
    std::vector<rich::AnyRenderable> renderables_;
    Options options_;
};

inline std::vector<rich::Segment> render_item(const Columns& c, const rich::Console& con,
                                              const rich::Options& o) {
    return c.render(con, o);
}
inline std::pair<size_t, size_t> measure_item(const Columns& c, const rich::Console& con,
                                              const rich::Options& o) {
    return c.measure(con, o);
}

inline rich::AnyRenderable columns(std::vector<rich::AnyRenderable> renderables, size_t spacing = 3,
                                   size_t max_columns = 0) {
    return rich::AnyRenderable(Columns(std::move(renderables), spacing, max_columns));
}

} // namespace rich::decorator
