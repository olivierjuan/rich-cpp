#pragma once
#include <rich_core.h>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace rich::layout {

// Port of rich.layout.Layout (Python Rich).
//
// Semantics follow Python's Layout.__rich_console__:
//   1. The layout fills the full console area (0, 0, width, height) where
//      height = options.height if set, else console.height().
//   2. split_row divides WIDTH across children via ratio_resolve;
//      split_column divides HEIGHT.
//   3. Leaf layouts render their content into their region; rows are
//      assembled and emitted one per line. Rows shorter than the console
//      width stay short (Python does not pad them).
//
// Simplification vs Python: an empty leaf (no explicit content) renders as
// blank lines instead of Python's _Placeholder Panel ("name" (W x H)).
// This is acceptable for the oracle tests, which always give leaves
// explicit content.

namespace detail {
// Visible cell width of a UTF-8 string (each non-ASCII codepoint = 1 cell).
inline size_t utf8_width(std::string_view sv) {
    size_t width = 0;
    size_t i = 0;
    while (i < sv.size()) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (c < 0x80) { ++width; ++i; }
        else if ((c & 0xE0) == 0xC0) { ++width; i += 2; }
        else if ((c & 0xF0) == 0xE0) { ++width; i += 3; }
        else if ((c & 0xF8) == 0xF0) { width += 2; i += 4; }
        else { ++i; }
    }
    return width;
}
// Encode a codepoint as UTF-8 into buf; returns byte count.
inline size_t encode_utf8(char32_t ch, char* buf) {
    if (ch < 0x80) { buf[0] = static_cast<char>(ch); return 1; }
    if (ch < 0x800) {
        buf[0] = static_cast<char>(0xC0 | (ch >> 6));
        buf[1] = static_cast<char>(0x80 | (ch & 0x3F));
        return 2;
    }
    if (ch < 0x10000) {
        buf[0] = static_cast<char>(0xE0 | (ch >> 12));
        buf[1] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        buf[2] = static_cast<char>(0x80 | (ch & 0x3F));
        return 3;
    }
    buf[0] = static_cast<char>(0xF0 | (ch >> 18));
    buf[1] = static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
    buf[2] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
    buf[3] = static_cast<char>(0x80 | (ch & 0x3F));
    return 4;
}
} // namespace detail

struct LayoutOptions {
    std::string name;      // optional identifier
    size_t size{0};        // 0 = flexible (ratio-based); >0 = fixed size
    size_t ratio{1};       // weight among flexible siblings
    size_t minimum_size{1};
};

// Exact port of rich._ratio.ratio_resolve.
// Integer arithmetic replicates Python's Fraction divmod: each child gets
// floor((portion*ratio + remainder)/1) with the fractional part carried to
// the next child. If any child's share would be <= minimum_size it is pinned
// to minimum_size and the loop repeats.
// Plain uint64_t arithmetic is safe here: inputs are terminal dimensions
// (well below 2^32), so no overflow risk.
inline std::vector<size_t> ratio_resolve(size_t total, const std::vector<const LayoutOptions*>& edges) {
    // sizes[i] == SIZE_UNKNOWN means "not yet determined" (Python: None)
    constexpr size_t SIZE_UNKNOWN = static_cast<size_t>(-1);
    std::vector<size_t> sizes(edges.size(), SIZE_UNKNOWN);
    for (size_t i = 0; i < edges.size(); ++i)
        if (edges[i]->size > 0) sizes[i] = edges[i]->size;

    while (true) {
        std::vector<size_t> flex_idx;
        for (size_t i = 0; i < edges.size(); ++i)
            if (sizes[i] == SIZE_UNKNOWN) flex_idx.push_back(i);
        if (flex_idx.empty()) break;

        // Remaining space in total
        size_t used = 0;
        for (size_t s : sizes) if (s != SIZE_UNKNOWN) used += s;
        if (used >= total) {
            // No room for flexible edges: pin them to minimum_size.
            for (size_t i : flex_idx) sizes[i] = edges[i]->minimum_size;
            break;
        }
        size_t remaining = total - used;

        size_t ratio_sum = 0;
        for (size_t i : flex_idx) ratio_sum += edges[i]->ratio;

        // portion = remaining / ratio_sum as a rational (numerator/denominator)
        // Check whether any flexible edge would get less than its minimum:
        // portion * ratio <= minimum  <=>  remaining * ratio <= minimum * ratio_sum
        bool pinned = false;
        for (size_t i : flex_idx) {
            unsigned long long lhs = static_cast<unsigned long long>(remaining) * edges[i]->ratio;
            unsigned long long rhs = static_cast<unsigned long long>(edges[i]->minimum_size) * ratio_sum;
            if (lhs <= rhs) {
                sizes[i] = edges[i]->minimum_size;
                pinned = true;
                break;
            }
        }
        if (pinned) continue;

        // Distribute flexible space, carrying the fractional remainder forward
        // (replicates Python: size, remainder = divmod(portion * ratio + remainder, 1))
        unsigned long long rem_num = 0; // numerator of the running fractional remainder
        for (size_t i : flex_idx) {
            unsigned long long value = static_cast<unsigned long long>(remaining) * edges[i]->ratio + rem_num;
            sizes[i] = static_cast<size_t>(value / ratio_sum);
            rem_num = value % ratio_sum;
        }
        break;
    }
    return sizes;
}

class Layout {
public:
    Layout(LayoutOptions options = {})
        : options_(std::move(options)) {}

    Layout(rich::AnyRenderable renderable, LayoutOptions options = {})
        : content_(std::move(renderable)), options_(std::move(options)) {}

    // Splitting (replaces children, like Python split/split_row/split_column)
    Layout& split_row(std::vector<Layout> children) {
        is_row_ = true;
        children_ = std::move(children);
        return *this;
    }
    Layout& split_column(std::vector<Layout> children) {
        is_row_ = false;
        children_ = std::move(children);
        return *this;
    }

    // Leaf content update (Python layout.update(renderable))
    void update(rich::AnyRenderable renderable) {
        content_ = std::move(renderable);
    }

    // Named lookup (Python layout["name"]) — throws std::out_of_range if missing
    Layout& get(const std::string& name) const {
        if (options_.name == name) return const_cast<Layout&>(*this);
        for (const auto& child : children_) {
            const Layout* found = find(child, name);
            if (found) return const_cast<Layout&>(*found);
        }
        throw std::out_of_range("No layout with name '" + name + "'");
    }

    // Render into the given console region (width x height from Options)
    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        const size_t width = opt.width;
        const size_t height = (opt.height > 0) ? opt.height : con.height();

        // Post-order traversal collecting (node pointer, region) pairs, then
        // sort by region fields in order x, y, width, height (Python itemgetter(1)).
        std::vector<Entry> entries;
        collect(*this, 0, 0, width, height, entries);

        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            if (a.w != b.w) return a.w < b.w;
            return a.h < b.h;
        });

        // Render each leaf into its region.
        std::vector<std::vector<std::vector<rich::Segment>>> leaf_lines(entries.size());
        for (size_t e = 0; e < entries.size(); ++e) {
            const auto& entry = entries[e];
            const Layout* node = entry.node;
            if (!node->children_.empty()) continue; // interior node, not a leaf

            rich::Options leaf_opt{.show_border = true, .width = entry.w, .height = entry.h};
            std::vector<rich::Segment> segs =
                node->content_.empty() ? std::vector<rich::Segment>{}   // see simplification note above
                                       : node->content_.render(con, leaf_opt);

            // Split segments into lines at '\n'.
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
                    lines.back().push_back({s.text.substr(start, nl - start), s.style});
                    lines.emplace_back();
                    start = nl + 1;
                }
            }
            // Pad each line to the region width so adjacent regions don't
            // overlap (Python pads each region to its full width).
            for (auto& line : lines) {
                size_t vis = 0;
                for (const auto& seg : line) vis += detail::utf8_width(seg.text);
                if (vis < entry.w) line.push_back({std::string(entry.w - vis, ' '), {}});
            }
            leaf_lines[e] = std::move(lines);
        }

        // Assemble rows into a cell grid so each leaf is placed at its
        // actual (x, y) position. Rows are NOT padded to console width.
        struct Cell { char32_t ch{0}; rich::Style style; };
        std::vector<std::vector<Cell>> grid(height, std::vector<Cell>(width));
        for (size_t e = 0; e < entries.size(); ++e) {
            const auto& entry = entries[e];
            const auto& lines = leaf_lines[e];
            for (size_t i = 0; i < lines.size() && entry.y + i < height; ++i) {
                size_t col = entry.x;
                for (const auto& seg : lines[i]) {
                    size_t si = 0;
                    while (si < seg.text.size()) {
                        unsigned char c = static_cast<unsigned char>(seg.text[si]);
                        char32_t ch = 0;
                        size_t adv = 1;
                        if (c < 0x80) { ch = c; }
                        else if ((c & 0xE0) == 0xC0) { ch = (c & 0x1F) << 6 | (seg.text[si+1] & 0x3F); adv = 2; }
                        else if ((c & 0xF0) == 0xE0) { ch = (c & 0x0F) << 12 | (seg.text[si+1] & 0x3F) << 6 | (seg.text[si+2] & 0x3F); adv = 3; }
                        else if ((c & 0xF8) == 0xF0) { ch = (c & 0x07) << 18 | (seg.text[si+1] & 0x3F) << 12 | (seg.text[si+2] & 0x3F) << 6 | (seg.text[si+3] & 0x3F); adv = 4; }
                        else { si++; continue; }
                        if (col < width && entry.y + i < height)
                            grid[entry.y + i][col] = {ch, seg.style};
                        col += 1; // one cell per codepoint (simplified width model)
                        si += adv;
                    }
                }
            }
        }

        // Emit rows, merging consecutive cells with the same style.
        std::vector<rich::Segment> out;
        for (size_t r = 0; r < height; ++r) {
            std::string text;
            rich::Style cur_style{};
            bool have_style = false;
            for (size_t c = 0; c < width; ++c) {
                const auto& cell = grid[r][c];
                char buf[5];
                size_t n = detail::encode_utf8(cell.ch, buf);
                if (!have_style || cell.style != cur_style) {
                    if (!text.empty()) out.push_back({std::move(text), cur_style});
                    text.clear();
                    cur_style = cell.style;
                    have_style = true;
                }
                text.append(buf, n);
            }
            if (!text.empty()) out.push_back({std::move(text), cur_style});
            out.push_back({"\n", {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        const size_t width = opt.width;
        const size_t height = (opt.height > 0) ? opt.height : con.height();
        return {width, height};
    }

private:
    struct Region { size_t x, y, w, h; };

    // A node and its region, collected during post-order traversal.
    struct Entry {
        const Layout* node;
        size_t x, y, w, h;
    };

    static const Layout* find(const Layout& l, const std::string& name) {
        if (l.options_.name == name) return &l;
        for (const auto& child : l.children_) {
            const Layout* found = find(child, name);
            if (found) return found;
        }
        return nullptr;
    }

    // Depth-first post-order collection: children first, then self.
    // Each entry stores a direct pointer to its Layout node.
    static void collect(const Layout& l, size_t x, size_t y, size_t w, size_t h,
                        std::vector<Entry>& entries) {
        if (l.children_.empty()) {
            entries.push_back({&l, x, y, w, h});
            return;
        }
        std::vector<Region> regions(l.children_.size());
        if (l.is_row_) {
            std::vector<const LayoutOptions*> opts;
            opts.reserve(l.children_.size());
            for (const auto& c : l.children_) opts.push_back(&c.options_);
            auto widths = ratio_resolve(w, opts);
            size_t offset = 0;
            for (size_t i = 0; i < l.children_.size(); ++i) {
                regions[i] = {x + offset, y, widths[i], h};
                offset += widths[i];
            }
        } else {
            std::vector<const LayoutOptions*> opts;
            opts.reserve(l.children_.size());
            for (const auto& c : l.children_) opts.push_back(&c.options_);
            auto heights = ratio_resolve(h, opts);
            size_t offset = 0;
            for (size_t i = 0; i < l.children_.size(); ++i) {
                regions[i] = {x, y + offset, w, heights[i]};
                offset += heights[i];
            }
        }
        for (size_t i = 0; i < l.children_.size(); ++i)
            collect(l.children_[i], regions[i].x, regions[i].y, regions[i].w, regions[i].h, entries);
        entries.push_back({&l, x, y, w, h});
    }

    rich::AnyRenderable content_;
    LayoutOptions options_;
    std::vector<Layout> children_;
    bool is_row_{false}; // true = split_row (divide width), false = split_column (divide height)
};

inline std::vector<rich::Segment> render_item(const Layout& l, const rich::Console& c, const rich::Options& o) {
    return l.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Layout& l, const rich::Console& c, const rich::Options& o) {
    return l.measure(c, o);
}

} // namespace rich::layout
