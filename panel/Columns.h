#pragma once
#include <rich_core.h>
#include <algorithm>
#include <vector>
#include <string>

namespace rich::decorator {

namespace {
inline size_t visual_width(std::string_view sv) {
    size_t width = 0;
    size_t i = 0;
    while (i < sv.size()) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (c < 0x80) {
            ++width;
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            ++width;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            ++width;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            width += 2;
            i += 4;
        } else {
            ++i;
        }
    }
    return width;
}
} // namespace

class Columns {
public:
    Columns(std::vector<rich::AnyRenderable> renderables, size_t spacing = 3, size_t max_columns = 0)
        : renderables_(std::move(renderables)), spacing_(spacing), max_columns_(max_columns) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        if (renderables_.empty()) return {};

        // 1. Break each item into lines of segments and determine its width
        struct RenderedItem {
            std::vector<std::vector<rich::Segment>> lines;
            size_t width{0};
        };

        std::vector<RenderedItem> items;
        items.reserve(renderables_.size());

        for (const auto& r : renderables_) {
            auto segs = r.render(con, opt);
            std::vector<std::vector<rich::Segment>> lines;
            lines.emplace_back();

            for (const auto& s : segs) {
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

            if (!lines.empty() && lines.back().empty()) {
                lines.pop_back();
            }

            size_t max_w = 0;
            for (const auto& ln : lines) {
                size_t w = 0;
                for (const auto& seg : ln) w += visual_width(seg.text);
                max_w = std::max(max_w, w);
            }

            items.push_back({std::move(lines), max_w});
        }

        // 2. Chunk items into rows (e.g. 3 columns per row, or auto wrapping)
        size_t cols_per_row = max_columns_;
        if (cols_per_row == 0) {
            // Auto wrap if row exceeds ~85 columns, or default to 3
            cols_per_row = 3;
        }

        std::vector<rich::Segment> out;
        std::string gap(spacing_, ' ');

        for (size_t row_start = 0; row_start < items.size(); row_start += cols_per_row) {
            size_t row_end = std::min(row_start + cols_per_row, items.size());

            // Max line height in this row
            size_t row_max_lines = 0;
            for (size_t i = row_start; i < row_end; ++i) {
                row_max_lines = std::max(row_max_lines, items[i].lines.size());
            }

            // Interleave lines for this row: all left-aligned starting from col 0
            for (size_t line_idx = 0; line_idx < row_max_lines; ++line_idx) {
                for (size_t i = row_start; i < row_end; ++i) {
                    const auto& item = items[i];
                    size_t printed_w = 0;

                    if (line_idx < item.lines.size()) {
                        for (const auto& seg : item.lines[line_idx]) {
                            printed_w += visual_width(seg.text);
                            out.push_back(seg);
                        }
                    }

                    // Pad up to this item's specific width
                    if (printed_w < item.width) {
                        out.push_back({std::string(item.width - printed_w, ' '), {}});
                    }

                    // Separation gap (only between items, not after the last in row)
                    if (i + 1 < row_end) {
                        out.push_back({gap, {}});
                    }
                }
                out.push_back({"\n", {}});
            }
        }

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        size_t total_w = 0;
        size_t max_h = 0;
        for (const auto& r : renderables_) {
            auto [w, h] = r.measure(con, opt);
            total_w += w;
            max_h = std::max(max_h, h);
        }
        return {total_w, max_h};
    }

private:
    std::vector<rich::AnyRenderable> renderables_;
    size_t spacing_;
    size_t max_columns_;
};

inline std::vector<rich::Segment> render_item(const Columns& c, const rich::Console& con, const rich::Options& o) {
    return c.render(con, o);
}
inline std::pair<size_t, size_t> measure_item(const Columns& c, const rich::Console& con, const rich::Options& o) {
    return c.measure(con, o);
}

inline rich::AnyRenderable columns(std::vector<rich::AnyRenderable> renderables, size_t spacing = 3, size_t max_columns = 0) {
    return rich::AnyRenderable(Columns(std::move(renderables), spacing, max_columns));
}

} // namespace rich::decorator
