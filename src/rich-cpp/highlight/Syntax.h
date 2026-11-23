#pragma once
// Syntax renderable — port of Python Rich's rich.syntax.Syntax.
//
// Pipeline parity (rich/syntax.py, rich 15.0.0):
//   _process_code (trailing \n if missing, dedent, expandtabs)
//   -> pygments-compatible lexer: 69 generated regex lexers (see
//      SyntaxLexers.h) + native JSON lexer, exact pygments state machine
//      (highlight/Pygments.h); unknown lexer name -> plain text (rich's
//      lexer property returns None on ClassNotFound)
//   -> theme styles (generated Pygments themes, SyntaxThemes.h):
//      per-token style via style_for_token hierarchy walk; background =
//      theme background unless SyntaxOptions.background_color overrides it
//      (post-tokenization, like rich's text.stylize("on <color>"))
//   -> layout: line split, optional line_range slice, indent guides
//      (theme Comment + dim + bg, complex path forces italic=False),
//      per-line crop/pad or word wrap, optional line-number gutter
//      (number styles: truecolor/256 = bg + Token.Text style + blended
//      fg (0.3 / 0.9 cross-fade, bold for highlight); else dim / dim=False).
//
// Layout quirk preserved from Python: with line_numbers, each wrapped line
// is preceded by a col_width+1 gutter, so total line width is
// col_width + 1 + code_width (the console width when code_width=None).
// Continuation lines of a wrapped line carry the blank gutter.

#include <rich_core.h>
#include "Pygments.h"
#include "SyntaxLexers.h"
#include "SyntaxThemes.h"
#include <Text.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace rich::highlight {

struct SyntaxOptions {
    bool dedent = false;
    bool line_numbers = false;
    int start_line = 1;
    int line_range_start = 0; // 0 = no range; else 1-based, end exclusive
    int line_range_end = 0;
    std::set<int> highlight_lines;
    int code_width = 0; // 0 = None (fill the console)
    int tab_size = 4;
    bool word_wrap = false;
    bool indent_guides = false;
    // Python Syntax padding=1 (markdown code blocks): the rendered code is
    // wrapped in Padding(pad, style=theme-bg, expand=True) — one blank
    // bg-filled line at the top and bottom at the full console width and one
    // bg-filled cell on each side of every code line.
    int padding = 0;
    // Python background_color: optional background override, applied to the
    // whole code block (token styles included).
    std::optional<Color> background_color;
};

struct Syntax {
    std::string code;
    std::string lexer; // any pygments name (case-insensitive); "" -> text
    std::string theme = "monokai"; // pygments theme name; unknown -> "default"
    SyntaxOptions opts{};

    std::vector<rich::Segment> render(const rich::Console& c, const rich::Options& opt) const;
    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const;
};

namespace detail {

// ── theme helpers ───────────────────────────────────────────────────────────

inline Color hex_color(const char* hex, const Color& fallback) {
    if (hex && hex[0] && hex[1] && hex[2] && hex[3] && hex[4] && hex[5]) {
        auto hv = [](char c) -> int {
            return (c >= '0' && c <= '9') ? c - '0'
                   : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                   : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                            : 0;
        };
        return Color::rgb(uint8_t(hv(hex[0]) * 16 + hv(hex[1])),
                          uint8_t(hv(hex[2]) * 16 + hv(hex[3])),
                          uint8_t(hv(hex[4]) * 16 + hv(hex[5])));
    }
    return fallback;
}

// rich PygmentsSyntaxTheme.get_style_for_token: pygments
// StyleMeta resolves parent inheritance at class creation, so the generated
// table is the complete per-token lookup — exact match only; a missing
// token is a KeyError in Python, which rich maps to Style.null() (no span).
// color "" -> #000000, bg "" -> theme background; bold/italic/underline
// explicit only when set True (False -> not explicit, same SGR either way).
inline Style theme_token_style(const pg::PgTheme* th, const Color& theme_bg,
                               const char* tok) {
    for (size_t i = 0; i < th->n_entries; ++i) {
        const pg::PgThemeEntry& e = th->entries[i];
        if (strcmp(e.token, tok) == 0) {
            Style s;
            s.color = hex_color(e.color, Color::rgb(0, 0, 0));
            s.bg_color = hex_color(e.bg, theme_bg);
            if (e.bold == 1) {
                s.bold = true;
                s.explicit_bits |= 0x0001;
            }
            if (e.italic == 1) {
                s.italic = true;
                s.explicit_bits |= 0x0004;
            }
            if (e.underline == 1) {
                s.underline = true;
                s.explicit_bits |= 0x0008;
            }
            return s;
        }
    }
    return Style{}; // no entry at all (Style.null())
}

// theme Token.Text foreground for the line-numbers blend (None -> default).
inline Color theme_text_color(const pg::PgTheme* th, const Color& theme_bg) {
    const Style s = theme_token_style(th, theme_bg, "Text");
    return s.color.type == Color::Type::None ? Color::dflt() : s.color;
}

// rich.color.blend_rgb: int(r1 + (r2 - r1) * cross_fade) per channel
// (double arithmetic, truncate).
inline Color blend_number_color(const Color& bg, const Color& fg,
                                double cross_fade) {
    auto chan = [](uint8_t r1, uint8_t r2, double t) -> uint8_t {
        double v = double(r1) + (double(r2) - double(r1)) * t;
        return uint8_t(int(v));
    };
    return Color::rgb(chan(bg.r, fg.r, cross_fade), chan(bg.g, fg.g, cross_fade),
                      chan(bg.b, fg.b, cross_fade));
}

// rich Syntax._get_number_styles.
inline std::tuple<Style, Style, Style> number_styles(
    const Console& c, const pg::PgTheme* th, const Color& theme_bg,
    const Color& base_bg) {
    Style background_style;
    background_style.bg_color = base_bg;
    const auto cs = c.active_color_system();
    if (!cs.has_value() || *cs == ColorSystem::Standard) {
        Style dim;
        dim.dim = true;
        dim.explicit_bits = 0x0002;
        Style dim_off; // Style(dim=False) — SGR-equivalent to Style()
        dim_off.explicit_bits = 0x0002;
        return {background_style, background_style + dim,
                background_style + dim_off};
    }
    // 256 / truecolor
    Style text_style = theme_token_style(th, theme_bg, "Text");
    const Color bg_fg = theme_text_color(th, theme_bg);
    Style number_style = background_style + text_style;
    number_style.color = blend_number_color(base_bg, bg_fg, 0.3);
    Style hl = background_style + text_style;
    hl.color = blend_number_color(base_bg, bg_fg, 0.9);
    hl.bold = true;
    hl.explicit_bits |= 0x0001;
    return {background_style, number_style, hl};
}

// ── line layout ─────────────────────────────────────────────────────────────
// (crop_pad / wrap_line / cells_of: unchanged from the pre-pygments port —
// pure cell arithmetic on segment streams)

inline size_t cells_of(const std::vector<Segment>& segs) {
    size_t n = 0;
    for (const auto& s : segs)
        n += unicode::string_cell_len(s.text);
    return n;
}

inline std::vector<Segment> crop_pad(const std::vector<Segment>& line, size_t width,
                                     const Style& pad_style) {
    std::vector<Segment> out;
    size_t pos = 0;
    bool truncated = false;
    for (const auto& seg : line) {
        if (truncated || pos >= width)
            break;
        const size_t len = unicode::string_cell_len(seg.text);
        if (pos + len <= width) {
            out.push_back(seg);
            pos += len;
            continue;
        }
        // crop at the (width - pos)th cell — walk codepoints to a boundary
        size_t want = width - pos;
        size_t b = 0, acc = 0;
        while (b < seg.text.size() && acc < want) {
            const unsigned char ch = static_cast<unsigned char>(seg.text[b]);
            size_t clen = 1;
            if (ch >= 0xF0)
                clen = 4;
            else if (ch >= 0xE0)
                clen = 3;
            else if (ch >= 0xC0)
                clen = 2;
            if (b + clen > seg.text.size())
                clen = seg.text.size() - b;
            b += clen;
            ++acc;
        }
        if (b > 0)
            out.push_back({std::string(seg.text.substr(0, b)), seg.style});
        pos = width;
        truncated = true;
    }
    if (pos < width)
        out.push_back({std::string(width - pos, ' '), pad_style});
    return out;
}

// Text.wrap: divide_line offsets on words (\s*\S+\s*), fold for over-long
// words, then split the segment stream at the cell offsets and pad each
// wrapped line to width with the base style (justify="left").
inline std::vector<std::vector<Segment>> wrap_line(const std::vector<Segment>& line,
                                                   size_t width, const Style& base) {
    std::string plain;
    for (const auto& s : line)
        plain += s.text;
    if (width == 0 || unicode::string_cell_len(plain) <= width) {
        auto one = crop_pad(line, width, base);
        return {std::move(one)};
    }
    std::vector<size_t> breaks;
    size_t offset = 0; // cells placed on the current line
    size_t pos = 0;
    auto is_ws = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (pos < plain.size()) {
        const size_t start = pos;
        while (pos < plain.size() && is_ws(plain[pos]))
            ++pos;
        const size_t core_start = pos;
        while (pos < plain.size() && !is_ws(plain[pos]))
            ++pos;
        const size_t core_end = pos;
        while (pos < plain.size() && is_ws(plain[pos]))
            ++pos;
        const std::string_view word(plain.data() + start, pos - start);
        const std::string_view core(plain.data() + core_start, core_end - core_start);
        const size_t core_len = unicode::string_cell_len(core);
        const size_t word_len = unicode::string_cell_len(word);
        if (width - offset >= core_len) {
            offset += word_len;
        } else if (core_len > width) {
            // fold: chop the word into width-cell chunks (hard breaks)
            size_t wstart = start;
            size_t acc = 0, k = core_start;
            while (k < core_end) {
                size_t kb = k;
                while (kb < core_end && acc < width) {
                    const unsigned char ch = static_cast<unsigned char>(plain[kb]);
                    size_t clen = 1;
                    if (ch >= 0xF0)
                        clen = 4;
                    else if (ch >= 0xE0)
                        clen = 3;
                    else if (ch >= 0xC0)
                        clen = 2;
                    kb += (kb + clen <= plain.size()) ? clen : plain.size() - kb;
                    ++acc;
                }
                if (kb > k && kb < core_end) {
                    if (wstart > 0)
                        breaks.push_back(wstart);
                    wstart = kb;
                    acc = 0;
                }
                k = kb;
            }
            offset = width; // chunk fills the line
        } else if (offset > 0 && start > 0) {
            breaks.push_back(start);
            offset = word_len;
        }
    }
    // breaks are char offsets on plain — remap to cell offsets (prefix cells)
    std::vector<size_t> cell_breaks;
    {
        size_t acc = 0, bi = 0;
        for (size_t k = 0; k < plain.size(); ++k) {
            if (bi < breaks.size() && breaks[bi] <= k) {
                cell_breaks.push_back(acc);
                ++bi;
            }
            const unsigned char ch = static_cast<unsigned char>(plain[k]);
            size_t step = 1;
            if (ch >= 0xF0)
                step = 4;
            else if (ch >= 0xE0)
                step = 3;
            else if (ch >= 0xC0)
                step = 2;
            k += (k + step <= plain.size()) ? step - 1 : plain.size() - k - 1;
            ++acc;
        }
        while (bi < breaks.size())
            cell_breaks.push_back(acc);
    }
    // split the segment stream at the cell offsets
    std::vector<std::vector<Segment>> out;
    std::vector<Segment> line_cur;
    size_t consumed = 0, ci = 0;
    for (const auto& seg : line) {
        size_t b = 0;
        while (b < seg.text.size()) {
            const unsigned char ch = static_cast<unsigned char>(seg.text[b]);
            size_t step = 1;
            if (ch >= 0xF0)
                step = 4;
            else if (ch >= 0xE0)
                step = 3;
            else if (ch >= 0xC0)
                step = 2;
            const size_t chars = (b + step <= seg.text.size()) ? step : seg.text.size() - b;
            line_cur.push_back({std::string(seg.text.substr(b, chars)), seg.style});
            ++consumed;
            b += chars;
            if (ci < cell_breaks.size() && consumed == cell_breaks[ci]) {
                out.push_back(std::move(line_cur));
                line_cur.clear();
                ++ci;
            }
        }
    }
    out.push_back(std::move(line_cur));
    for (auto& l : out) {
        // merge adjacent same-style runs — Python Text.divide re-emits
        // consolidated spans, not per-char segments
        for (size_t i2 = 1; i2 < l.size();) {
            if (l[i2].style == l[i2 - 1].style && !l[i2].text.empty()) {
                l[i2 - 1].text += l[i2].text;
                l.erase(l.begin() + static_cast<long>(i2));
            } else
                ++i2;
        }
        const size_t len = cells_of(l);
        if (len < width)
            l.push_back({std::string(width - len, ' '), base});
    }
    return out;
}

} // namespace detail

// ── the renderable ───────────────────────────────────────────────────────────
inline std::vector<rich::Segment> Syntax::render(const rich::Console& c,
                                                 const rich::Options& opt) const {
    const SyntaxOptions& o = opts;
    const size_t width = (opt.width > 0) ? opt.width : 80;
    const bool legacy = c.legacy_windows();

    // theme + base styles
    const pg::PgTheme* th = pg::pg_find_theme(theme.c_str());
    if (!th)
        th = pg::pg_find_theme("default");
    const Color theme_bg = detail::hex_color(th->background, Color::rgb(0, 0, 0));
    const Color base_bg = o.background_color ? *o.background_color : theme_bg;
    const Style bg_style{.bg_color = base_bg};

    // 1. _process_code: trailing newline, dedent, expandtabs
    std::string code = this->code;
    const bool ends_on_nl = !code.empty() && code.back() == '\n';
    if (!ends_on_nl)
        code += '\n';
    if (o.dedent) {
        // textwrap.dedent parity: whitespace-only lines normalize to empty
        // (before and after margin stripping); margin = min leading-space/tabs
        // over non-blank lines, stripped character-wise.
        size_t common = 0;
        bool first = true;
        size_t line_start = 0;
        while (line_start < code.size()) {
            size_t line_end = code.find('\n', line_start);
            if (line_end == std::string::npos)
                line_end = code.size();
            const std::string_view ln(code.data() + line_start, line_end - line_start);
            if (!ln.empty() && ln.find_first_not_of(" \t") != std::string_view::npos) {
                size_t indent = 0;
                while (indent < ln.size() && (ln[indent] == ' ' || ln[indent] == '\t'))
                    ++indent;
                if (first) {
                    common = indent;
                    first = false;
                } else
                    common = std::min(common, indent);
            }
            line_start = line_end + 1;
        }
        std::string dedented;
        line_start = 0;
        while (line_start < code.size()) {
            size_t line_end = code.find('\n', line_start);
            if (line_end == std::string::npos)
                line_end = code.size();
            const std::string_view ln(code.data() + line_start, line_end - line_start);
            if (!ln.empty() && ln.find_first_not_of(" \t") != std::string_view::npos)
                dedented += ln.substr(std::min(common, ln.size()));
            // blank/whitespace-only lines contribute nothing (Python
            // normalizes them to empty before stripping)
            if (line_end < code.size())
                dedented += '\n';
            line_start = line_end + 1;
        }
        code = std::move(dedented);
    }
    if (o.tab_size > 0) {
        std::string expanded;
        expanded.reserve(code.size());
        size_t col = 0;
        for (char ch : code) {
            if (ch == '\n') {
                expanded += ch;
                col = 0;
            } else if (ch == '\t') {
                size_t pad = static_cast<size_t>(o.tab_size) - col % static_cast<size_t>(o.tab_size);
                expanded.append(pad, ' ');
                col += pad;
            } else {
                expanded += ch;
                ++col;
            }
        }
        code = std::move(expanded);
    }

    // 2. lex (pygments engine) + 3. build per-line segment streams (split on
    //    \n). Unknown lexer name -> plain text (rich: lexer property None).
    pg::PgText pt;
    pt.decode(code.data(), code.size());
    const pg::PgView view = pg::PgView::of(pt);
    std::vector<pg::detail::PgTok> toks;
    if (lexer.empty())
        toks.push_back({0, pt.size(), "Text"});
    else if (pg::detail::pg_lex_is_json(lexer))
        pg::detail::pg_lex_json(view, toks);
    else {
        const pg::PgLexer* lx = pg::detail::pg_find_lexer_ci(lexer);
        if (lx)
            pg::detail::pg_lex_into(lx, view, nullptr, toks);
        else
            toks.push_back({0, pt.size(), "Text"});
    }
    std::vector<std::vector<Segment>> lines;
    std::vector<Segment> cur;
    for (const auto& t : toks) {
        Style st = detail::theme_token_style(th, theme_bg, t.token.c_str());
        // Python: text.stylize(f"on {background_color}") after tokenization —
        // overrides the background of every token span.
        if (o.background_color)
            st.bg_color = *o.background_color;
        const std::string bytes = pt.bytes(t.start, t.end);
        size_t a = 0;
        for (size_t b = 0; b <= bytes.size(); ++b) {
            if (b == bytes.size() || bytes[b] == '\n') {
                if (b > a)
                    cur.push_back({bytes.substr(a, b - a), st});
                a = b + 1;
                if (b < bytes.size()) {
                    lines.push_back(std::move(cur));
                    cur.clear();
                }
            }
        }
    }
    lines.push_back(std::move(cur)); // trailing (empty when ends_on_nl)
    if (!ends_on_nl && lines.back().empty())
        lines.pop_back();

    // 4. line_range slice (1-based, end exclusive — Python lines[offset:end])
    if (o.line_range_start > 0) {
        const size_t begin = static_cast<size_t>(o.line_range_start - 1);
        size_t end = (o.line_range_end > 0) ? static_cast<size_t>(o.line_range_end)
                                            : lines.size();
        if (begin >= lines.size())
            return {};
        end = std::min(end, lines.size());
        lines.assign(lines.begin() + begin, lines.begin() + end);
    }

    // 5. indent guides (with_indent_guides: per tab_size block -> │+spaces;
    //    blank lines are deferred and re-emitted with the following line's
    //    indent, or empty at the end). Complex-path style: base + theme
    //    Comment + dim + user bg, then italic=False forced.
    if (o.indent_guides && o.tab_size > 0) {
        Style guide = bg_style + detail::theme_token_style(th, theme_bg, "Comment");
        guide.dim = true;
        guide.explicit_bits |= 0x0002; // dim explicit
        // + Style(italic=False): drop any theme italic
        guide.italic = false;
        guide.explicit_bits &= ~0x0004;
        std::vector<std::vector<Segment>> next;
        size_t blank = 0;
        for (auto& line : lines) {
            std::string plain;
            for (const auto& s : line)
                plain += s.text;
            size_t indent = 0;
            while (indent < plain.size() && plain[indent] == ' ')
                ++indent;
            if (plain.size() == indent) {
                ++blank;
                continue;
            }
            const size_t full = indent / static_cast<size_t>(o.tab_size);
            const size_t rem = indent % static_cast<size_t>(o.tab_size);
            std::string new_indent;
            for (size_t k = 0; k < full; ++k) {
                new_indent += "\xE2\x94\x82"; // │
                new_indent.append(static_cast<size_t>(o.tab_size) - 1, ' ');
            }
            new_indent.append(rem, ' ');
            for (size_t k = 0; k < blank; ++k)
                next.push_back({{new_indent, guide}});
            blank = 0;
            std::vector<Segment> nline;
            // zero-length indent: Python stylizes (0, 0) — a no-op, no span
            if (!new_indent.empty())
                nline.push_back({new_indent, guide});
            size_t skip = indent;
            for (auto& seg : line) {
                if (skip >= seg.text.size()) {
                    skip -= seg.text.size();
                    continue;
                }
                if (skip) {
                    seg.text.erase(0, skip);
                    skip = 0;
                    if (seg.text.empty())
                        continue;
                }
                nline.push_back(std::move(seg));
            }
            next.push_back(std::move(nline));
        }
        for (size_t k = 0; k < blank; ++k)
            next.push_back({});
        lines = std::move(next);
    }

    // 6. widths (Python: numbers_column_width = digits(start_line + raw
    //    newline count) + 2; code_width = max_width - col - 1 when numbered)
    size_t raw_nl = 0;
    for (char ch : this->code)
        if (ch == '\n')
            ++raw_nl;
    const size_t col_width =
        o.line_numbers
            ? std::to_string(static_cast<size_t>(o.start_line) + raw_nl).size() + 2
            : 0;
    // Padding (expand=True) shrinks the width seen by the inner render.
    const size_t inner_width =
        o.padding > 0 ? std::max<size_t>(0, width - 2 * static_cast<size_t>(o.padding))
                      : width;
    size_t code_width;
    if (o.code_width > 0)
        code_width = static_cast<size_t>(o.code_width);
    else
        code_width =
            std::max<size_t>(0, inner_width - (o.line_numbers ? col_width + 1 : 0));

    // 7. per-line layout
    const auto [number_bg, number_style, hl_number_style] =
        detail::number_styles(c, th, theme_bg, base_bg);
    const Style pointer_style{.color = Color::from_ansi(31)}; // red
    const std::string pointer = legacy ? "> " : "\xE2\x9D\xB1 "; // ❱
    std::vector<Segment> out;
    const int range_offset = (o.line_range_start > 0) ? o.line_range_start - 1 : 0;
    for (size_t idx = 0; idx < lines.size(); ++idx) {
        const int line_no = o.start_line + range_offset + static_cast<int>(idx);
        std::vector<std::vector<Segment>> wrapped;
        if (o.word_wrap) {
            wrapped = detail::wrap_line(lines[idx], code_width, bg_style);
        } else {
            std::vector<Segment> one = detail::crop_pad(lines[idx], code_width, bg_style);
            if (lines[idx].empty() && (o.line_numbers || o.indent_guides))
                // complex path: Text("").render() yields Segment("", None)
                one.insert(one.begin(), {"", rich::detail::kNoneStyle()});
            wrapped.push_back(std::move(one));
        }
        if (o.line_numbers) {
            for (size_t w = 0; w < wrapped.size(); ++w) {
                if (w == 0) {
                    std::string col = std::to_string(line_no);
                    if (col.size() < col_width - 2)
                        col.insert(0, col_width - 2 - col.size(), ' ');
                    col += ' ';
                    if (o.highlight_lines.count(line_no)) {
                        out.push_back({pointer, pointer_style});
                        out.push_back({col, hl_number_style});
                    } else {
                        out.push_back({"  ", hl_number_style});
                        out.push_back({col, number_style});
                    }
                } else {
                    out.push_back({std::string(col_width, ' ') + " ", number_bg});
                }
                for (auto& seg : wrapped[w])
                    out.push_back(std::move(seg));
                out.push_back({"\n", Style{}});
            }
        } else {
            for (auto& wl : wrapped) {
                for (auto& seg : wl)
                    out.push_back(std::move(seg));
                out.push_back({"\n", Style{}});
            }
        }
    }

    // 8. Padding wrapper (Python Padding(pad, style=bg, expand=True)):
    // top and bottom blank lines at the full outer console width, one bg
    // cell on each side of every code line.
    if (o.padding > 0) {
        const size_t pad = static_cast<size_t>(o.padding);
        std::vector<Segment> wrapped;
        wrapped.push_back({std::string(width, ' '), bg_style});
        wrapped.push_back({"\n", Style{}});
        std::vector<Segment> line;
        for (auto& s : out) {
            if (s.text == "\n") {
                for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
                for (auto& seg : line) wrapped.push_back(std::move(seg));
                for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
                wrapped.push_back({"\n", Style{}});
                line.clear();
            } else {
                line.push_back(std::move(s));
            }
        }
        if (!line.empty()) { // trailing line without newline (defensive)
            for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
            for (auto& seg : line) wrapped.push_back(std::move(seg));
            for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
            wrapped.push_back({"\n", Style{}});
        }
        wrapped.push_back({std::string(width, ' '), bg_style});
        wrapped.push_back({"\n", Style{}});
        out = std::move(wrapped);
    }
    return out;
}

inline std::pair<size_t, size_t> Syntax::measure(const rich::Console&,
                                                 const rich::Options&) const {
    size_t raw_nl = 0;
    for (char ch : code)
        if (ch == '\n')
            ++raw_nl;
    const size_t col_width =
        opts.line_numbers
            ? std::to_string(static_cast<size_t>(opts.start_line) + raw_nl).size() + 2
            : 0;
    if (opts.code_width > 0)
        return {col_width, static_cast<size_t>(opts.code_width) + col_width + 1};
    size_t max_line = 0;
    size_t start = 0;
    for (size_t i = 0; i <= code.size(); ++i) {
        if (i == code.size() || code[i] == '\n') {
            max_line = std::max(max_line,
                                unicode::string_cell_len(code.substr(start, i - start)));
            start = i + 1;
        }
    }
    size_t width = col_width + max_line;
    if (opts.line_numbers)
        ++width;
    return {col_width, width};
}

} // namespace rich::highlight

// ADL render/measure hooks — must be declared before the factory so the
// AnyRenderable Renderable constraint can find them at instantiation.
namespace rich {

inline std::vector<Segment> render_item(const highlight::Syntax& s, const Console& c,
                                        const Options& o) {
    return s.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const highlight::Syntax& s, const Console& c,
                                              const Options& o) {
    return s.measure(c, o);
}

} // namespace rich

namespace rich::highlight {

// console.print(rich::highlight::syntax(code, "python", opts))
inline rich::AnyRenderable syntax(std::string_view code, std::string_view lexer,
                                  SyntaxOptions opts = {}) {
    Syntax s{std::string(code), std::string(lexer), "monokai", opts};
    return rich::AnyRenderable(std::move(s));
}

} // namespace rich::highlight
