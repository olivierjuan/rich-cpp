#pragma once
#include <rich_core.h>
#include <algorithm>
namespace rich::panel {

namespace rich::panel {

class Panel {
public:
    Panel(rich::AnyRenderable inner, std::string title) : inner_(std::move(inner)), title_(std::move(title)) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        auto [w, h] = measure(con, opt);
        std::vector<rich::Segment> out;
        out.push_back({"╭─ " + title_ + " " + std::string(w - title_.size() - 5, '─') + "╮", {}});
        for (const auto& seg : inner_.render(con, opt)) {
            out.push_back({"│ " + seg.text + std::string(w - seg.text.size() - 3, ' ') + "│", seg.style});
        }
        out.push_back({"╰" + std::string(w - 2, '─') + "╯", {}});
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto [iw, ih] = inner_.measure(con, opt);
        return {std::max(iw + 4, title_.size() + 6), ih + 2};
    }

private:
    rich::AnyRenderable inner_;
    std::string title_;
};

inline std::vector<rich::Segment> render(const Panel& p, const rich::Console& c, const rich::Options& o) { return p.render(c, o); }
inline std::pair<size_t, size_t> measure(const Panel& p, const rich::Console& c, const rich::Options& o) { return p.measure(c, o); }

// Factory for operator|
inline auto panel(std::string title) {
    return [title](rich::AnyRenderable inner) {
        return rich::AnyRenderable(Panel(std::move(inner), std::move(title)));
    };
}

} // namespace rich::panel
