#pragma once
// SVG terminal snapshot (port of Python Rich's Console.export_svg).
//
// Mirrors rich/_export_format.py CONSOLE_SVG_FORMAT and the rendering in
// console.py::export_svg:
//   - SVG_EXPORT_THEME palette (bg #292929, fg #c5c8c6, 16 ansi colors).
//   - char_width = 20 * 0.61 = 12.2, line_height = 20 * 1.22 = 24.4.
//   - Per-style CSS classes r1..rN with `fill:` (+ bold/italic/underline).
//   - Background rects for bgcolor/reverse runs; text skipped when all-spaces.
//   - Per-line clipPaths (note: Python emits them for lines [0, y) — the last
//     line index is excluded; replicated verbatim).
//   - unique_id defaults to "terminal-" + adler32 of the joined repr of the
//     recorded segments (Python-parity hashing, deterministic per content).
#include <rich_core.h>
#include "Repr.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace rich::svg {

// ── SVG_EXPORT_THEME (rich/terminal_theme.py) ────────────────────────────────
struct Triplet { uint8_t r, g, b; };
inline std::string hex(const Triplet& t) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", t.r, t.g, t.b);
    return buf;
}

inline const std::vector<Triplet>& svg_theme_ansi() {
    static const std::vector<Triplet> palette = {
        {75, 78, 85},    {204, 85, 90},   {152, 168, 75},  {208, 179, 68},
        {96, 138, 177},  {152, 114, 159}, {104, 160, 179}, {197, 200, 198},
        {154, 155, 153}, {255, 38, 39},   {0, 130, 61},    {208, 132, 66},
        {25, 132, 233},  {255, 44, 122},  {57, 130, 128},  {205, 205, 202},
    };
    return palette;
}
inline const Triplet& theme_background() {
    static const Triplet bg{41, 41, 41};
    return bg;
}
inline const Triplet& theme_foreground() {
    static const Triplet fg{197, 200, 198};
    return fg;
}

// SGR ansi code (30-37 fg / 40-47 bg / 90-97 bright fg) -> palette index.
inline int sgr_to_palette(uint8_t ansi) {
    if (ansi >= 90 && ansi <= 97) return 8 + (ansi - 90);
    if (ansi >= 30 && ansi <= 37) return ansi - 30;
    if (ansi >= 40 && ansi <= 47) return ansi - 40;
    return 7; // white fallback
}

inline Triplet truecolor(const Color& c, bool foreground) {
    if (c.type == Color::Type::Rgb) return {c.r, c.g, c.b};
    if (c.type == Color::Type::Standard) return svg_theme_ansi()[sgr_to_palette(c.ansi)];
    return foreground ? theme_foreground() : theme_background();
}

inline Triplet blend(const Triplet& a, const Triplet& b, double cross_fade) {
    return {
        static_cast<uint8_t>(a.r + (b.r - a.r) * cross_fade),
        static_cast<uint8_t>(a.g + (b.g - a.g) * cross_fade),
        static_cast<uint8_t>(a.b + (b.b - a.b) * cross_fade),
    };
}

// ── helpers ──────────────────────────────────────────────────────────────────
inline uint32_t adler32(const std::string& data) {
    uint32_t a = 1, b = 0;
    for (unsigned char ch : data) {
        a = (a + ch) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// Python repr() of strings: see Repr.h (rich::repr::py_repr_string).
using repr::py_repr_string;

// Style repr — Python parity: Style(color=..., bgcolor=..., bold=True, ...)
// only lists non-default fields; Color repr lists name/type/number/triplet
// with default-valued fields skipped.
inline std::string py_repr_color(const Color& c) {
    if (c.type == Color::Type::Standard) {
        static const char* names[16] = {
            "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
            "bright_black", "bright_red", "bright_green", "bright_yellow",
            "bright_blue", "bright_magenta", "bright_cyan", "bright_white",
        };
        int idx = sgr_to_palette(c.ansi);
        std::string out = "Color('" + std::string(names[idx]) + "', ColorType.STANDARD, number=";
        out += std::to_string(idx);
        out += ")";
        return out;
    }
    if (c.type == Color::Type::Rgb) {
        std::string out = "Color('#";
        char buf[7];
        std::snprintf(buf, sizeof(buf), "%02x%02x%02x", c.r, c.g, c.b);
        out += buf;
        out += "', ColorType.TRUECOLOR, triplet=ColorTriplet(red=";
        out += std::to_string(c.r);
        out += ", green=";
        out += std::to_string(c.g);
        out += ", blue=";
        out += std::to_string(c.b);
        out += "))";
        return out;
    }
    return "None";
}

inline std::string py_repr_style(const Style& st) {
    // Python parity: Style() with no fields set is repr'd as "Style()"
    // (an explicitly empty arg list) — recorded plain segments use it.
    static const Style kDefault{};
    if (st == kDefault) return "Style()";
    std::vector<std::string> fields;
    if (st.color.type != Color::Type::None)
        fields.push_back("color=" + py_repr_color(st.color));
    if (st.bg_color.type != Color::Type::None)
        fields.push_back("bgcolor=" + py_repr_color(st.bg_color));
    // Python repr emits a flag when it is set OR explicitly set-to-false in
    // the constructor (tracked via Style.explicit_bits).
    auto push_flag = [&fields](const char* name, bool value, bool explicit_set) {
        if (!value && !explicit_set) return;
        fields.push_back(std::string(name) + (value ? "=True" : "=False"));
    };
    push_flag("bold", st.bold, (st.explicit_bits & 0x01) != 0);
    push_flag("dim", st.dim, (st.explicit_bits & 0x02) != 0);
    push_flag("italic", st.italic, (st.explicit_bits & 0x04) != 0);
    push_flag("underline", st.underline, (st.explicit_bits & 0x08) != 0);
    push_flag("reverse", st.reverse, (st.explicit_bits & 0x10) != 0);
    std::string out = "Style(";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i) out += ", ";
        out += fields[i];
    }
    out += ")";
    return out;
}

// True for the Python style=None sentinel (plain strings without markup tags,
// the appended newline). Distinguishes it from an explicit empty Style, which
// markup gap pieces carry and whose repr appears in the SVG unique_id hash.
inline bool is_none_style(const Style& st) {
    return st.color.type == Color::Type::None && st.color.ansi == 200 &&
           !st.bold && !st.dim && !st.italic && !st.underline && !st.reverse &&
           st.bg_color.type == Color::Type::None;
}

inline std::string py_repr_segment(const Segment& s) {
    // Python parity: Segment.__rich_repr__ yields text, then style only when
    // style is not None. Plain (unstyled) text records style=None → omitted.
    bool none_style = is_none_style(s.style);
    std::string out = "Segment(";
    out += py_repr_string(s.text);
    if (!none_style) {
        out += ", ";
        out += py_repr_style(s.style);
    }
    out += ")";
    return out;
}

// Python html.escape(text, quote=True) then space -> &#160;
inline std::string escape_svg_text(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s) {
        switch (ch) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#x27;"; break;
            case ' ':  out += "&#160;"; break;
            default:   out += ch;
        }
    }
    return out;
}

// Python format(value, "g") — 6 significant digits, shortest form.
inline std::string fmt_g(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

using repr::py_str_float;
using repr::cell_len;

// Codepoint count (Python len()) — used for textLength.
inline size_t py_len(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = (c < 0x80) ? 1
                   : ((c & 0xE0) == 0xC0) ? 2
                   : ((c & 0xF0) == 0xE0) ? 3
                   : ((c & 0xF8) == 0xF0) ? 4 : 1;
        n += 1;
        i += len;
    }
    return n;
}

inline std::vector<std::vector<Segment>> split_lines(const std::vector<Segment>& segs, size_t width) {
    std::vector<std::vector<Segment>> lines;
    std::vector<Segment> line;
    size_t line_cells = 0;

    auto flush = [&]() {
        if (line_cells < width)
            line.push_back({std::string(width - line_cells, ' '), {}});
        line.push_back({"\n", {}});
        lines.push_back(std::move(line));
        line.clear();
        line_cells = 0;
    };

    for (const auto& s : segs) {
        if (s.text.find('\n') != std::string::npos) {
            size_t start = 0;
            while (start < s.text.size()) {
                size_t nl = s.text.find('\n', start);
                if (nl > start) {
                    std::string part = s.text.substr(start, nl - start);
                    size_t part_cells = cell_len(part);
                    if (line_cells + part_cells > width) {
                        size_t take = width - line_cells;
                        line.push_back({part.substr(0, take), s.style});
                        line_cells = width;
                    } else {
                        line.push_back({part, s.style});
                        line_cells += part_cells;
                    }
                }
                if (nl != std::string::npos) {
                    flush();
                    start = nl + 1;
                } else {
                    start = s.text.size();
                }
            }
        } else if (!s.text.empty()) {
            size_t part_cells = cell_len(s.text);
            if (line_cells + part_cells > width) {
                size_t take = width - line_cells;
                line.push_back({s.text.substr(0, take), s.style});
                line_cells = width;
            } else {
                line.push_back(s);
                line_cells += part_cells;
            }
        }
    }
    if (!line.empty()) {
        if (line_cells < width)
            line.push_back({std::string(width - line_cells, ' '), {}});
        lines.push_back(std::move(line));
    }
    return lines;
}

} // namespace rich::svg
