#pragma once
#include <rich_core.h>
#include <algorithm>
namespace rich::panel {

namespace rich::panel {

struct PanelOptions {
    std::string title;
    bool show_border{true};
};

class Panel {
public:
    Panel(rich::AnyRenderable inner, PanelOptions options) : inner_(std::move(inner)), options_(std::move(options)) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        auto [w, h] = measure(con, opt);
        std::vector<rich::Segment> out;
        
        if (options_.show_border) {
            out.push_back({"╭─ " + options_.title + " " + std::string(w - options_.title.size() - 5, '─') + "╮", {}});
        }
        for (const auto& seg : inner_.render(con, opt)) {
            std::string prefix = options_.show_border ? "│ " : "";
            std::string suffix = options_.show_border ? "│" : "";
            out.push_back({prefix + seg.text + std::string(w - seg.text.size() - (options_.show_border ? 3 : 0), ' ') + suffix, seg.style});
        }
        if (options_.show_border) {
            out.push_back({"╰" + std::string(w - 2, '─') + "╯", {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto [iw, ih] = inner_.measure(con, opt);
        if (options_.show_border) {
            return {std::max(iw + 4, options_.title.size() + 6), ih + 2};
        }
        return {iw, ih};
    }

private:
    rich::AnyRenderable inner_;
    PanelOptions options_;
};

inline std::vector<rich::Segment> render(const Panel& p, const rich::Console& c, const rich::Options& o) { return p.render(c, o); }
inline std::pair<size_t, size_t> measure(const Panel& p, const rich::Console& c, const rich::Options& o) { return p.measure(c, o); }

// Factory for operator|
inline auto panel(PanelOptions options) {
    return [options](rich::AnyRenderable inner) {
        return rich::AnyRenderable(Panel(std::move(inner), std::move(options)));
    };
}

} // namespace rich::panel
