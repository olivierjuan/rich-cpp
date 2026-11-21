#pragma once
#include <rich_core.h>
#include <algorithm>
#include <string>
#include <vector>

namespace rich::align {

enum class Justify { Left, Center, Right };

class Align {
public:
    // width == 0: fit to inner's measured max width; otherwise clamp to it.
    Align(rich::AnyRenderable inner, Justify justify, size_t width = 0, bool pad = true)
        : inner_(std::move(inner)), justify_(justify), width_(width), pad_(pad) {}

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        auto [mw, mh] = inner_.measure(con, opt);
        (void)mh;
        size_t target = (width_ == 0) ? mw : std::min(mw, width_);

        // Render inner constrained to target width (mirrors Python Constrain inside Align).
        rich::Options inner_opt{.width = target};
        auto segs = inner_.render(con, inner_opt);

        // Split into lines; drop trailing empty line produced by final '\n'.
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
        if (!lines.empty() && lines.back().empty()) lines.pop_back();
        if (lines.empty()) return {};

        // Normalize every line to the content width (Segment.set_shape parity).
        size_t line_w = 0;
        for (const auto& ln : lines) {
            size_t w = 0;
            for (const auto& seg : ln) w += visual_width(seg.text);
            line_w = std::max(line_w, w);
        }

        // Pad to console width per alignment (excess_space logic parity).
        size_t excess = (opt.width > line_w) ? (opt.width - line_w) : 0;
        size_t left_pad = 0, right_pad = 0;
        if (excess > 0 && pad_) {
            switch (justify_) {
                case Justify::Left:   right_pad = excess; break;
                case Justify::Center: left_pad = excess / 2; right_pad = excess - left_pad; break;
                case Justify::Right:  left_pad = excess; break;
            }
        }

        std::vector<rich::Segment> out;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (left_pad) out.push_back({std::string(left_pad, ' '), {}});
            size_t printed = 0;
            for (const auto& seg : lines[i]) {
                printed += visual_width(seg.text);
                out.push_back(seg);
            }
            if (printed < line_w) out.push_back({std::string(line_w - printed, ' '), {}});
            if (right_pad) out.push_back({std::string(right_pad, ' '), {}});
            if (i + 1 < lines.size()) out.push_back({"\n", {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        auto [w, h] = inner_.measure(con, opt);
        size_t mw = (width_ == 0) ? w : std::min(w, width_);
        return {mw, h};
    }

private:
    static size_t visual_width(std::string_view sv) {
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

    rich::AnyRenderable inner_;
    Justify justify_;
    size_t width_;
    bool pad_;
};

inline std::vector<rich::Segment> render_item(const Align& a, const rich::Console& c, const rich::Options& o) {
    return a.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Align& a, const rich::Console& c, const rich::Options& o) {
    return a.measure(c, o);
}

// Factory for operator| composition
inline auto align(Justify justify, size_t width = 0, bool pad = true) {
    return [justify, width, pad](rich::AnyRenderable inner) {
        return rich::AnyRenderable(Align(std::move(inner), justify, width, pad));
    };
}

} // namespace rich::align
