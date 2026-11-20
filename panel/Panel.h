#pragma once
#include <rich_core.h>
#include <algorithm>
namespace rich::panel {
#include <string>

namespace rich::panel {

struct PanelOptions {
    std::string title;
    bool show_border{true};
};

namespace {
inline std::string repeat_utf8(std::string_view glyph, size_t count) {
    std::string res;
    res.reserve(glyph.size() * count);
    for (size_t i = 0; i < count; ++i) {
        res.append(glyph);
    }
    return res;
}
} // namespace

class Panel {
public:
    Panel(rich::AnyRenderable inner, PanelOptions options)
        : inner_(std::move(inner)), options_(std::move(options)) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        auto [w, h] = measure(con, opt);
        std::vector<rich::Segment> out;

        size_t title_len = options_.title.size();
        size_t border_dashes = (w >= title_len + 4) ? (w - title_len - 4) : 1;

        if (options_.show_border) {
            out.push_back({"╭─ " + options_.title + " " + repeat_utf8("─", border_dashes) + "╮\n", {}});
        }

        auto inner_segs = inner_.render(con, opt);
        size_t inner_text_len = 0;
        for (const auto& s : inner_segs) {
            inner_text_len += s.text.size();
        }

        if (options_.show_border) {
            out.push_back({"│ ", {}});
        }
        for (auto& s : inner_segs) {
            out.push_back(std::move(s));
        }
        if (options_.show_border) {
            size_t inner_space = (w >= 2) ? (w - 2) : 0;
            size_t pad = (inner_space >= inner_text_len) ? (inner_space - inner_text_len) : 0;
            out.push_back({std::string(pad, ' ') + " │\n", {}});
            out.push_back({"╰" + repeat_utf8("─", inner_space + 2) + "╯\n", {}});
        }

        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto [iw, ih] = inner_.measure(con, opt);
        size_t inner_w = std::max(iw, options_.title.size() + 2);
        return {inner_w + 4, ih + 2};
    }

private:
    rich::AnyRenderable inner_;
    PanelOptions options_;
};

inline std::vector<rich::Segment> render_item(const Panel& p, const rich::Console& c, const rich::Options& o) {
    return p.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Panel& p, const rich::Console& c, const rich::Options& o) {
    return p.measure(c, o);
}

// Factory for operator|
inline auto panel(PanelOptions options) {
    return [options = std::move(options)](rich::AnyRenderable inner) mutable {
        return rich::AnyRenderable(Panel(std::move(inner), std::move(options)));
    };
}

} // namespace rich::panel
