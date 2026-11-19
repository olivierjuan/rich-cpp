#pragma once
#include <rich_core.h>
#include <algorithm>

namespace rich::decorator {

class Padding {
public:
    Padding(rich::AnyRenderable inner, size_t padding) : inner_(std::move(inner)), padding_(padding) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        // Top padding
        for (size_t i = 0; i < padding_; ++i) {
            out.push_back({std::string(measure(con, opt).first, ' '), {}});
        }
        for (const auto& seg : inner_.render(con, opt)) {
            out.push_back({std::string(padding_, ' ') + seg.text + std::string(padding_, ' '), seg.style});
        }
        // Bottom padding
        for (size_t i = 0; i < padding_; ++i) {
            out.push_back({std::string(measure(con, opt).first, ' '), {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto [iw, ih] = inner_.measure(con, opt);
        return {iw + 2 * padding_, ih + 2 * padding_};
    }

private:
    rich::AnyRenderable inner_;
    size_t padding_;
};

inline std::vector<rich::Segment> render(const Padding& p, const rich::Console& c, const rich::Options& o) { return p.render(c, o); }
inline std::pair<size_t, size_t> measure(const Padding& p, const rich::Console& c, const rich::Options& o) { return p.measure(c, o); }

// Factory for operator|
inline auto pad(size_t padding) {
    return [padding](rich::AnyRenderable inner) {
        return rich::AnyRenderable(Padding(std::move(inner), padding));
    };
}

} // namespace rich::decorator
