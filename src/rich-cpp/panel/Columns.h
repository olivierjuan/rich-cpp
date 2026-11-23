#pragma once
#include <rich_core.h>
#include <unicode/Unicode.h>
#include <panel/GridTable.h>
#include <panel/Constrain.h>
#include <algorithm>
#include <optional>
#include <vector>
#include <string>

namespace rich::decorator {

// Python rich.columns.Columns port. The layout is a Table.grid
// (box=None, collapse_padding=True, pad_edge=False) rendered by
// rich::grid_detail::render_grid_table: no borders; each cell is a
// Padding(expand=True) around the (optionally Constrain/Align-wrapped)
// renderable; `expand` distributes leftover console width proportionally
// to the current column widths (ratio_distribute with ratios = widths).
// Items are chunked into rows: column_first fills column-major, equal wraps
// each item in Constrain(max width), right_to_left reverses each row.
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

        // Build the grid cells. `equal` wraps every item in Constrain(max)
        // (Python wraps the yielded renderables the same way).
        const size_t constrain_w = options_.equal ? rw[0] : 0;
        auto cell_item = [&](size_t idx) -> rich::AnyRenderable {
            if (options_.equal)
                return rich::AnyRenderable(
                    rich::constrain::Constrain(renderables_[idx], constrain_w));
            return renderables_[idx];
        };
        std::vector<std::vector<std::optional<rich::AnyRenderable>>> grid(rows.size());
        for (size_t ri = 0; ri < rows.size(); ++ri) {
            grid[ri].resize(column_count);
            for (size_t ci = 0; ci < column_count; ++ci)
                if (rows[ri][ci]) grid[ri][ci] = cell_item(*rows[ri][ci]);
        }

        grid_detail::GridOptions g;
        g.pad_top = options_.pad_top;
        g.pad_right = options_.pad_right;
        g.pad_bottom = options_.pad_bottom;
        g.pad_left = options_.pad_left;
        g.expand = options_.expand;
        if (options_.width)
            g.fixed_width.assign(column_count, options_.width.value());
        if (options_.align.has_value()) {
            switch (*options_.align) {
            case Align::Left: g.align = -1; break;
            case Align::Center: g.align = 0; break;
            case Align::Right: g.align = 1; break;
            }
        }

        size_t table_width = 0;
        auto body = grid_detail::render_grid_table(con, opt, grid, g, &table_width);

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
            size_t pad = (table_width > tvis) ? table_width - tvis : 0;
            size_t left = pad / 2;
            size_t right = pad - left;
            out.push_back({std::string(left, ' ') + options_.title + std::string(right, ' '), ts});
            out.push_back({"\n", {}});
        }
        out.insert(out.end(), body.begin(), body.end());
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
