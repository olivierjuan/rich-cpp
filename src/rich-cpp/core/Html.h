#pragma once
// HTML export (port of Python Rich's Console.export_html).
//
// Mirrors rich/_export_format.py CONSOLE_HTML_FORMAT and the rendering in
// console.py::export_html:
//   - DEFAULT_TERMINAL_THEME palette (bg #ffffff, fg #000000, 16 ansi colors).
//   - Segment.simplify (merge contiguous equal-style segments) then
//     Segment.filter_control (no control segments are produced here).
//   - html.escape(text, quote=True).
//   - A Style is only wrapped in a span when it is non-empty (Python
//     Style.__bool__: no attributes/colors/link). The style=None sentinel and
//     an explicit empty Style() both render as bare text.
//   - Non-inline: per-rule CSS classes r1..rN (numbered by first appearance);
//     empty rules are assigned a number but omitted from the stylesheet.
#include <rich_core.h>
#include "Svg.h"

#include <string>
#include <utility>
#include <vector>

namespace rich::html {

// ── DEFAULT_TERMINAL_THEME (rich/terminal_theme.py) ─────────────────────────
using svg::Triplet;
using svg::hex;

// ansi_colors = normal[8] + bright[8]; index 0..15.
inline const std::vector<Triplet>& html_theme_ansi() {
    static const std::vector<Triplet> palette = {
        {0, 0, 0},       {128, 0, 0},    {0, 128, 0},    {128, 128, 0},
        {0, 0, 128},     {128, 0, 128},  {0, 128, 128},  {192, 192, 192},
        {128, 128, 128}, {255, 0, 0},    {0, 255, 0},    {255, 255, 0},
        {0, 0, 255},     {255, 0, 255},  {0, 255, 255},  {255, 255, 255},
    };
    return palette;
}
inline const Triplet& theme_background() {
    static const Triplet bg{255, 255, 255};
    return bg;
}
inline const Triplet& theme_foreground() {
    static const Triplet fg{0, 0, 0};
    return fg;
}

// SGR code -> theme palette index, field-aware (foreground vs background).
inline int sgr_index(uint8_t ansi, bool foreground) {
    int idx;
    if (foreground) {
        if (ansi >= 90 && ansi <= 97) idx = ansi - 90;
        else idx = ansi - 30; // 30..37
    } else {
        if (ansi >= 100 && ansi <= 107) idx = ansi - 100;
        else idx = ansi - 40; // 40..47
    }
    if (idx < 0 || idx > 15) idx = 7;
    return idx;
}

inline Triplet truecolor(const Color& c, bool foreground) {
    if (c.type == Color::Type::Rgb) return {c.r, c.g, c.b};
    if (c.type == Color::Type::Standard) return html_theme_ansi()[sgr_index(c.ansi, foreground)];
    return foreground ? theme_foreground() : theme_background();
}

// Python blend_rgb(c1, c2, 0.5): int(r1 + (r2-r1)*0.5) per channel.
inline Triplet blend(const Triplet& a, const Triplet& b) {
    auto mix = [](uint8_t r1, uint8_t r2) -> uint8_t {
        return static_cast<uint8_t>(static_cast<int>(r1) +
                                    static_cast<int>((r2 - r1) * 0.5));
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

// Python Style.__bool__: false iff no attributes, colors, or links. Our Style
// has no link; the style=None sentinel is also empty.
inline bool is_plain_style(const Style& st) {
    return st.color.type == Color::Type::None &&
           st.bg_color.type == Color::Type::None &&
           !st.bold && !st.dim && !st.italic && !st.underline && !st.reverse;
}

// Python Style.get_html_style(DEFAULT_TERMINAL_THEME).
inline std::string get_html_style(const Style& st) {
    std::vector<std::string> css;
    Color color = st.color;
    Color bgcolor = st.bg_color;
    if (st.reverse) std::swap(color, bgcolor);

    bool has_color = (color.type != Color::Type::None);
    Triplet fg_triplet;
    if (st.dim) {
        Triplet base = has_color ? truecolor(color, true) : theme_foreground();
        fg_triplet = blend(base, theme_background());
        has_color = true;
    }
    if (has_color) {
        if (!st.dim) fg_triplet = truecolor(color, true);
        std::string h = hex(fg_triplet);
        css.push_back("color: " + h);
        css.push_back("text-decoration-color: " + h);
    }
    if (bgcolor.type != Color::Type::None) {
        css.push_back("background-color: " + hex(truecolor(bgcolor, false)));
    }
    if (st.bold) css.push_back("font-weight: bold");
    if (st.italic) css.push_back("font-style: italic");
    if (st.underline) css.push_back("text-decoration: underline");
    // strike / overline are not representable in our Style.

    std::string out;
    for (size_t i = 0; i < css.size(); ++i) {
        if (i) out += "; ";
        out += css[i];
    }
    return out;
}

// Python html.escape(text, quote=True): & first, then < > " '.
inline std::string escape_html(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s) {
        switch (ch) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#x27;"; break;
            default:   out += ch;
        }
    }
    return out;
}

// Python Segment.simplify: merge contiguous same-style segments.
inline std::vector<Segment> simplify_segments(const std::vector<Segment>& segs) {
    std::vector<Segment> out;
    out.reserve(segs.size());
    for (const auto& s : segs) {
        if (!out.empty() && out.back().style == s.style)
            out.back().text += s.text;
        else
            out.push_back(s);
    }
    return out;
}

// CONSOLE_HTML_FORMAT (verbatim, {…} format fields substituted).
inline std::string render_html(const std::vector<Segment>& record_segs, bool inline_styles) {
    auto segs = simplify_segments(record_segs);
    std::vector<std::string> fragments;
    fragments.reserve(segs.size());
    std::string stylesheet;

    if (inline_styles) {
        for (const auto& s : segs) {
            std::string text = escape_html(s.text);
            if (!is_plain_style(s.style)) {
                std::string rule = get_html_style(s.style);
                if (!rule.empty())
                    text = "<span style=\"" + rule + "\">" + text + "</span>";
            }
            fragments.push_back(std::move(text));
        }
    } else {
        // Ordered unique rules -> class number (first appearance wins).
        std::vector<std::pair<std::string, int>> styles;
        for (const auto& s : segs) {
            std::string text = escape_html(s.text);
            if (!is_plain_style(s.style)) {
                std::string rule = get_html_style(s.style);
                int num = 0;
                for (const auto& [r, n] : styles)
                    if (r == rule) { num = n; break; }
                if (num == 0) { num = static_cast<int>(styles.size()) + 1; styles.emplace_back(rule, num); }
                text = "<span class=\"r" + std::to_string(num) + "\">" + text + "</span>";
            }
            fragments.push_back(std::move(text));
        }
        for (const auto& [rule, num] : styles) {
            if (!rule.empty()) {
                if (!stylesheet.empty()) stylesheet += "\n";
                stylesheet += ".r" + std::to_string(num) + " {" + rule + "}";
            }
        }
    }

    std::string code;
    for (const auto& f : fragments) code += f;

    return "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\">\n<style>\n"
           + stylesheet
           + "\nbody {\n    color: " + hex(theme_foreground()) + ";\n"
           + "    background-color: " + hex(theme_background()) + ";\n}\n"
           + "</style>\n</head>\n<body>\n"
           + "    <pre style=\"font-family:Menlo,'DejaVu Sans Mono',consolas,'Courier New',monospace\">"
             "<code style=\"font-family:inherit\">"
           + code
           + "</code></pre>\n</body>\n</html>\n";
}

} // namespace rich::html
