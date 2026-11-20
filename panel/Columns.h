#pragma once
#include <rich_core.h>
#include <algorithm>
#include <vector>
#include <string>

namespace rich::decorator {

class Columns {
public:
    Columns(std::vector<rich::AnyRenderable> renderables, size_t spacing = 2)
        : renderables_(std::move(renderables)), spacing_(spacing) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        if (renderables_.empty()) return {};

        // Each renderable produces lines of segments
        std::vector<std::vector<std::vector<rich::Segment>>> rendered_items;
        std::vector<size_t> item_widths;
        size_t max_lines = 0;

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

            // Remove trailing empty line if it was produced by a trailing \n
            if (!lines.empty() && lines.back().empty()) {
                lines.pop_back();
            }

            size_t max_w = 0;
            for (const auto& ln : lines) {
                size_t w = 0;
                for (const auto& seg : ln) w += seg.text.size();
                max_w = std::max(max_w, w);
            }

            max_lines = std::max(max_lines, lines.size());
            item_widths.push_back(max_w);
            rendered_items.push_back(std::move(lines));
        }

        std::vector<rich::Segment> out;
        std::string gap(spacing_, ' ');

        for (size_t line_idx = 0; line_idx < max_lines; ++line_idx) {
            for (size_t item_idx = 0; item_idx < rendered_items.size(); ++item_idx) {
                const auto& item_lines = rendered_items[item_idx];
                size_t printed_w = 0;

                if (line_idx < item_lines.size()) {
                    for (const auto& seg : item_lines[line_idx]) {
                        printed_w += seg.text.size();
                        out.push_back(seg);
                    }
                }

                // Pad out to item_widths[item_idx]
                if (printed_w < item_widths[item_idx]) {
                    out.push_back({std::string(item_widths[item_idx] - printed_w, ' '), {}});
                }

                // Spacing between columns (not after last column)
                if (item_idx + 1 < rendered_items.size()) {
                    out.push_back({gap, {}});
                }
            }
            out.push_back({"\n", {}});
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
        if (!renderables_.empty()) {
            total_w += spacing_ * (renderables_.size() - 1);
        }
        return {total_w, max_h};
    }

private:
    std::vector<rich::AnyRenderable> renderables_;
    size_t spacing_;
};

inline std::vector<rich::Segment> render_item(const Columns& c, const rich::Console& con, const rich::Options& o) {
    return c.render(con, o);
}
inline std::pair<size_t, size_t> measure_item(const Columns& c, const rich::Console& con, const rich::Options& o) {
    return c.measure(con, o);
}

inline rich::AnyRenderable columns(std::vector<rich::AnyRenderable> renderables, size_t spacing = 2) {
    return rich::AnyRenderable(Columns(std::move(renderables), spacing));
}

} // namespace rich::decorator
