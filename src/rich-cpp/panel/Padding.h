#pragma once
#include <rich_core.h>

namespace rich::decorator {

class Padding {
public:
    Padding(rich::AnyRenderable inner, size_t padding)
        : inner_(std::move(inner)), padding_(padding) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        if (padding_ > 0) {
            out.push_back({std::string(padding_, ' '), {}});
        }
        for (auto& seg : inner_.render(con, opt)) {
            out.push_back(std::move(seg));
        }
        if (padding_ > 0) {
            out.push_back({std::string(padding_, ' '), {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto [iw, ih] = inner_.measure(con, opt);
        return {iw + 2 * padding_, ih};
    }

private:
    rich::AnyRenderable inner_;
    size_t padding_;
};

inline std::vector<rich::Segment> render_item(const Padding& p, const rich::Console& c, const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Padding& p, const rich::Console& c, const rich::Options& o) {
    return p.measure(c, o);
}

// Factory for operator|
inline auto pad(size_t padding) {
    return [padding](rich::AnyRenderable inner) {
        return rich::AnyRenderable(Padding(std::move(inner), padding));
    };
}

} // namespace rich::decorator
