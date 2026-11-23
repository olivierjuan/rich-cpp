#pragma once
#include <rich_core.h>

namespace rich::decorator {

// Python rich.padding.Padding (expand=True, style="none"):
//   - top/bottom padding = blank lines of the full (expanded) width,
//   - left/right padding = padded spaces prepended/appended to EVERY line,
//   - the inner renders at width - left - right, each line padded to that
//     width (console.render_lines, pad=True).
class Padding {
public:
    Padding(rich::AnyRenderable inner, size_t padding)
        : inner_(std::move(inner)), padding_(padding) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        const size_t width = (opt.width > 0) ? opt.width : 80;
        const size_t inner_w = (width >= 2 * padding_) ? (width - 2 * padding_) : 0;
        rich::Style pad_style{};  // Python default style="none" -> empty Style
        const Options inner_opt{.width = inner_w};
        std::vector<rich::AnyRenderable> rables{inner_};
        auto lines = con.render_lines(rables, inner_opt, nullptr, true);
        std::vector<rich::Segment> out;
        for (size_t i = 0; i < padding_; ++i)
            out.push_back({std::string(width, ' ') + "\n", pad_style});
        for (auto& line : lines) {
            if (padding_ > 0) out.push_back({std::string(padding_, ' '), pad_style});
            for (auto& s : line) out.push_back(std::move(s));
            if (padding_ > 0) out.push_back({std::string(padding_, ' '), pad_style});
            out.push_back({"\n", rich::detail::kNoneStyle()});
        }
        for (size_t i = 0; i < padding_; ++i)
            out.push_back({std::string(width, ' ') + "\n", pad_style});
        return out;
    }

    // Python __rich_measure__: (min,max) + left+right, capped at max_width;
    // height unchanged.
    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        // Pair is {min, max} widths; inner max is carried in the second slot.
        auto [mn, mx] = inner_.measure(con, opt);
        mn += 2 * padding_;
        mx += 2 * padding_;
        if (opt.width > 0) {
            mn = std::min(mn, opt.width);  // Python: .with_maximum(max_width)
            mx = std::min(mx, opt.width);
        }
        return {mn, mx};
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
