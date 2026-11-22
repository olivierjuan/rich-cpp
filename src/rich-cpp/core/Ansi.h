#pragma once
// Python rich.ansi.AnsiDecoder (+ Text.from_ansi join semantics).
//
// The decoder's style state carries across lines within one instance
// (Python self.style), matching FileProxy's persistent decoder. C++ Text
// is a single styled run, so each decoded line is merged into one Text
// carrying the last in-effect style (Python keeps one span per run;
// per-run styling is not representable without a core-type change).
#include "rich_core.h"
#include <string>
#include <vector>

namespace rich {

namespace ansi_detail {

// Python rich.ansi.SGR_STYLE_MAP.
inline const struct { int code; const char* def; } kSgrStyleMap[] = {
    {1, "bold"},      {2, "dim"},      {3, "italic"},   {4, "underline"},
    {5, "blink"},     {6, "blink2"},   {7, "reverse"},  {8, "conceal"},
    {9, "strike"},    {21, "underline2"}, {22, "not dim not bold"}, {23, "not italic"},
    {24, "not underline"}, {25, "not blink"}, {26, "not blink2"}, {27, "not reverse"},
    {28, "not conceal"}, {29, "not strike"}, {30, "color(0)"}, {31, "color(1)"},
    {32, "color(2)"}, {33, "color(3)"}, {34, "color(4)"}, {35, "color(5)"},
    {36, "color(6)"}, {37, "color(7)"}, {39, "default"}, {40, "on color(0)"},
    {41, "on color(1)"}, {42, "on color(2)"}, {43, "on color(3)"}, {44, "on color(4)"},
    {45, "on color(5)"}, {46, "on color(6)"}, {47, "on color(7)"}, {49, "on default"},
    {51, "frame"},    {52, "encircle"}, {53, "overline"}, {54, "not frame not encircle"},
    {55, "not overline"}, {90, "color(8)"}, {91, "color(9)"}, {92, "color(10)"},
    {93, "color(11)"}, {94, "color(12)"}, {95, "color(13)"}, {96, "color(14)"},
    {97, "color(15)"}, {100, "on color(8)"}, {101, "on color(9)"}, {102, "on color(10)"},
    {103, "on color(11)"}, {104, "on color(12)"}, {105, "on color(13)"},
    {106, "on color(14)"}, {107, "on color(15)"},
};

// Python rich.color.Color.from_ansi(number): <16 STANDARD (SGR 30-37/90-97),
// >=16 EIGHT_BIT (38;5;N).
inline Color color_from_ansi(int number) {
    if (number < 16)
        return Color::from_ansi(static_cast<uint8_t>(number < 8 ? 30 + number
                                                               : 90 + number - 8));
    return Color::charcoal(static_cast<uint8_t>(number));
}

// Python decode_line SGR handling: "Translate in to semi-colon separated
// codes. Ignore invalid codes, because we want to be lenient."
inline void apply_sgr(std::string_view params, Style& style) {
    std::vector<int> codes;
    size_t start = 0;
    while (true) {
        const auto semi = params.find(';', start);
        std::string_view part =
            params.substr(start, semi == std::string::npos ? std::string::npos
                                                           : semi - start);
        if (part.empty()) {
            codes.push_back(0);  // _code == "" -> 0
        } else {
            bool digits = true;
            for (char c : part)
                if (c < '0' || c > '9') {
                    digits = false;
                    break;
                }
            if (digits) {
                int v = 0;
                for (char c : part) v = v * 10 + (c - '0');
                codes.push_back(std::min(255, v));
            }
        }
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    size_t i = 0;
    while (i < codes.size()) {
        const int code = codes[i++];
        if (code == 0) {
            style = Style();  // Python _Style.null()
        } else {
            const char* def = nullptr;
            for (const auto& kv : kSgrStyleMap)
                if (kv.code == code) {
                    def = kv.def;
                    break;
                }
            if (def != nullptr) {
                // Python: self.style += _Style.parse(SGR_STYLE_MAP[code])
                style = style + parse_style_def(def);
            } else if (code == 38 || code == 48) {
                // with suppress(StopIteration): color_type = next(...)
                if (i < codes.size()) {
                    const int color_type = codes[i++];
                    if (color_type == 5 && i < codes.size()) {
                        const Color c = color_from_ansi(codes[i++]);
                        if (code == 38)
                            style.color = c;
                        else
                            style.bg_color = c;
                    } else if (color_type == 2 && i + 2 < codes.size()) {
                        const Color c = Color::rgb(static_cast<uint8_t>(codes[i]),
                                                   static_cast<uint8_t>(codes[i + 1]),
                                                   static_cast<uint8_t>(codes[i + 2]));
                        i += 3;
                        if (code == 38)
                            style.color = c;
                        else
                            style.bg_color = c;
                    }
                }
            }
        }
    }
}

} // namespace ansi_detail

class AnsiDecoder {
public:
    // Python AnsiDecoder.decode_line: tokenize (re_ansi) and fold the SGR
    // stream into a Text. The decoder's style state (Python self.style)
    // persists across calls; the returned Text carries the last in-effect
    // style at the end of the line.
    Text decode_line(const std::string& line_in) {
        // Python: line = line.rsplit("\r", 1)[-1]
        std::string line = line_in;
        const auto cr = line.rfind('\r');
        if (cr != std::string::npos) line.erase(0, cr + 1);

        std::string content;
        size_t p = 0;
        while (p < line.size()) {
            const size_t esc = line.find('\x1b', p);
            if (esc == std::string::npos) {
                content.append(line, p, std::string::npos);
                break;
            }
            content.append(line, p, esc - p);
            if (esc + 1 >= line.size()) {  // dangling ESC: no match -> plain
                content.push_back('\x1b');
                break;
            }
            const char c2 = line[esc + 1];
            if (c2 >= '0' && c2 <= '?') {  // alt 1: \x1b[0-?] -> ignored
                p = esc + 2;
                continue;
            }
            if (c2 == ']') {  // alt 2: OSC (ESC ] ... ESC \)
                const size_t end = line.find("\x1b\\", esc + 2);
                if (end == std::string::npos) {  // unterminated: no match
                    content.push_back('\x1b');
                    p = esc + 1;
                    continue;
                }
                std::string_view osc(line.data() + esc + 2, end - esc - 2);
                // Python: if osc.startswith("8;"): link = partition(";")
                if (osc.size() > 2 && osc[0] == '8' && osc[1] == ';') {
                    const auto semi = osc.find(';', 2);
                    if (semi != std::string::npos)
                        style_.link = std::string(osc.substr(semi + 1));
                }
                p = end + 2;
                continue;
            }
            if (c2 == '(') {  // charset designation: ESC ( X — skip X
                p = esc + 3;
                continue;
            }
            if (c2 == '[') {  // CSI: ESC [ params(0x30-0x3F)* inter(0x20-0x2F)* final
                size_t q = esc + 2;
                while (q < line.size() && line[q] >= '0' && line[q] <= '?') ++q;
                while (q < line.size() && line[q] >= ' ' && line[q] <= '/') ++q;
                if (q < line.size() && line[q] >= '@' && line[q] <= '~') {
                    if (line[q] == 'm')
                        ansi_detail::apply_sgr(line.substr(esc + 2, q - esc - 2), style_);
                    // non-m CSI final byte -> ignored token
                    p = q + 1;
                    continue;
                }
                content.push_back('\x1b');  // invalid CSI: no match -> plain
                p = esc + 1;
                continue;
            }
            if ((c2 >= 'A' && c2 <= 'Z') || c2 == '\\' || c2 == '_') {
                p = esc + 2;  // single-char sequence; non-SGR -> ignored
                continue;
            }
            content.push_back('\x1b');  // no match at ESC -> plain
            p = esc + 1;
        }
        return Text(std::move(content), style_);
    }

    // Python AnsiDecoder.decode: split on (?<=\n), rstrip each line, one
    // Text per line; the style state carries across lines (self.style).
    std::vector<Text> decode(const std::string& terminal_text) {
        std::vector<Text> out;
        size_t start = 0;
        while (start < terminal_text.size()) {
            const auto nl = terminal_text.find('\n', start);
            if (nl == std::string::npos) {
                out.push_back(decode_line(terminal_text.substr(start)));
                break;
            }
            out.push_back(decode_line(terminal_text.substr(start, nl - start)));
            start = nl + 1;
        }
        return out;
    }

private:
    Style style_;  // Python self.style = Style.null()
};

// Python Text.from_ansi: fresh decoder, join the decoded lines with "\n";
// the base style (joiner style) is applied under every run, so the
// single-run result carries base + the last run's in-effect style.
inline Text text_from_ansi(const std::string& text, const Style& base = Style{}) {
    AnsiDecoder decoder;
    const auto lines = decoder.decode(text);
    std::string content;
    Style final_style = base;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) content.push_back('\n');
        content += lines[i].content;
        final_style = final_style + lines[i].style;
    }
    return Text(std::move(content), final_style);
}

} // namespace rich
