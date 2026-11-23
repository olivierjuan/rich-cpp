#pragma once
#include <rich_core.h>
#include <algorithm>

namespace rich::constrain {

class Constrain {
public:
    explicit Constrain(rich::AnyRenderable inner, size_t width)
        : inner_(std::move(inner)), width_(width) {}

    // Render the child with a clamped width, mirroring Python
    // options.update_width(min(self.width, options.max_width)).
    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        size_t child_w = (opt.width > 0) ? std::min(width_, opt.width) : width_;
        rich::Options child{.width = child_w};
        return inner_.render(con, child);
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        (void)opt;
        rich::Options child{.width = width_};
        auto [w, h] = inner_.measure(con, child);
        // Python: child measurement .with_maximum(width) caps both ends.
        return {std::min(w, width_), std::min(h, width_)};
    }

private:
    rich::AnyRenderable inner_;
    size_t width_;
};

inline std::vector<rich::Segment> render_item(const Constrain& cn, const rich::Console& c, const rich::Options& o) {
    return cn.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Constrain& cn, const rich::Console& c, const rich::Options& o) {
    return cn.measure(c, o);
}

// Factory for operator| composition
inline auto constrain(size_t width) {
    return [width](rich::AnyRenderable inner) {
        return rich::AnyRenderable(Constrain(std::move(inner), width));
    };
}

} // namespace rich::constrain
