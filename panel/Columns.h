#pragma once
#include <rich_core.h>
#include <algorithm>
#include <vector>

namespace rich::decorator {

class Columns {
public:
    Columns(std::vector<rich::AnyRenderable> renderables) : renderables_(std::move(renderables)) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        // Simple layout: arrange renderables side-by-side
        std::vector<rich::Segment> out;
        for (const auto& r : renderables_) {
            for (const auto& seg : r.render(con, opt)) {
                out.push_back(seg);
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
};

inline std::vector<rich::Segment> render(const Columns& c, const rich::Console& con, const rich::Options& o) { return c.render(con, o); }
inline std::pair<size_t, size_t> measure(const Columns& c, const rich::Console& con, const rich::Options& o) { return c.measure(con, o); }

inline rich::AnyRenderable columns(std::vector<rich::AnyRenderable> renderables) {
    return rich::AnyRenderable(Columns(std::move(renderables)));
}

} // namespace rich::decorator
