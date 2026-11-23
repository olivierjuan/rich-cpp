#pragma once
// Word wrapping + styling engine — port of Python rich Text
// (rich/text.py 15.0.0 + rich/_wrap.py).
//
// Text holds a UTF-8 content string plus an append-ordered list of spans
// (codepoint offsets) that carry either a named style or a direct Style,
// mirroring rich.text.Span. render() re-implements the Python pipeline:
//   1. split into lines at '\n' (allow_blank=True; trailing blank kept,
//      '\r' dropped as in the legacy engine — CRLF),
//   2. expand tabs (tab_size 8, cell-aware),
//   3. divide each line at cell-aware break offsets (_wrap.divide_line),
//   4. rstrip_end each part (trailing whitespace beyond the width),
//   5. justify per original line ("default" = no-op; left/center/right
//      truncate + pad; full distributes extra spaces before the last
//      part of each line),
//   6. final per-part truncate (crop / fold / ellipsis),
//   7. emit style runs; line breaks are emitted as kNoneStyle segments
//      (Python Segment.split_and_crop_lines turns every '\n' into a
//      style=None segment, so '\n' never carries SGR).
//
// Spans store codepoint offsets: Python Span.start/end are indices into
// the text's characters (len(self)), not UTF-8 bytes.
#include <rich_core.h>
#include <unicode/Unicode.h>
#include "Regex.h"

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <initializer_list>
#include <cstddef>
#include <optional>

namespace rich::text {

using unicode::chop_cells;
using unicode::string_cell_len;

using Overflow = ::rich::Overflow;
// Default mirrors Python's DEFAULT_JUSTIFY = "default": no justification.
enum class Justify { Default, Left, Center, Right, Full };

namespace wrap_detail {

// Python rich._wrap.words: regex \s*\S+\s* over the line.
struct Word { size_t start; size_t end; std::string_view text; };

inline std::vector<Word> words(std::string_view text) {
    std::vector<Word> out;
    size_t pos = 0;
    auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; };
    while (pos < text.size()) {
        size_t ws_start = pos;
        while (pos < text.size() && is_ws(text[pos])) ++pos;
        if (pos >= text.size()) break;
        size_t core_start = pos;
        while (pos < text.size() && !is_ws(text[pos])) ++pos;
        size_t core_end = pos;
        while (pos < text.size() && is_ws(text[pos])) ++pos;
        // Python match is \s*\S+\s* — the trailing whitespace belongs to the
        // word (it is the break-keep position of the previous line).
        out.push_back({ws_start, pos, text.substr(ws_start, pos - ws_start)});
        (void)core_start; (void)core_end;
    }
    return out;
}

// Python rich._wrap.divide_line: break offsets (char indices) where lines
// split so every line fits within `width` cells.
inline std::vector<size_t> divide_line(std::string_view text, size_t width, bool fold = true) {
    std::vector<size_t> breaks;
    size_t cell_offset = 0;
    for (const auto& word : words(text)) {
        std::string_view core = word.text;
        while (!core.empty() && (core.back() == ' ' || core.back() == '\t')) core.remove_suffix(1);
        size_t word_length = string_cell_len(core);
        size_t remaining = (width > cell_offset) ? width - cell_offset : 0;
        if (remaining >= word_length) {
            cell_offset += string_cell_len(word.text);
        } else if (word_length > width) {
            if (fold) {
                auto folded = chop_cells(word.text, width);
                size_t start = word.start;
                for (size_t k = 0; k < folded.size(); ++k) {
                    if (start) breaks.push_back(start);
                    if (k + 1 < folded.size())
                        start += folded[k].size();
                    else
                        cell_offset = string_cell_len(folded[k]);
                }
            } else {
                if (word.start) breaks.push_back(word.start);
                cell_offset = string_cell_len(word.text);
            }
        } else if (cell_offset && word.start) {
            breaks.push_back(word.start);
            cell_offset = string_cell_len(word.text);
        }
    }
    return breaks;
}

// Python Text.rstrip_end(size): remove trailing whitespace beyond `size`
// cells (only when the line is longer than `size` cells). Whitespace is the
// ASCII set (\r\n\t \f\v), so counting characters equals counting cells.
inline void rstrip_end(std::string& line, size_t size) {
    size_t len = unicode::string_cell_len(line);
    if (len <= size) return;
    size_t excess = len - size;
    size_t i = line.size();
    size_t ws = 0;
    while (i > 0 && ws < excess) {
        char c = line[i - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            --i;
            ++ws;
        } else {
            break;
        }
    }
    line.erase(i);
}

// Python line.truncate(width, overflow) cut position as a char offset:
// cell-based, a wide character straddling the cut is dropped whole.
inline size_t char_offset_at_cell(std::string_view text, size_t cells) {
    size_t i = 0, used = 0;
    while (i < text.size()) {
        size_t j = i;
        char32_t cp = unicode::decode_utf8(text, j);
        size_t cw = static_cast<size_t>(unicode::get_character_cell_size(cp));
        if (used + cw > cells) break;
        used += cw;
        i = j;
    }
    return i;
}

// Styled-segment wrapper (console.render parity): split segments into lines
// at '\n' (trailing '\r' dropped, CRLF), expand tabs, divide each line at
// cell-aware break offsets, then re-emit per-line pieces with their original
// styles. Line separators are emitted as separate segments with `nl_style`
// (Python records inter-line newlines as style=None → pass the sentinel).
// `overflow`/`no_wrap` mirror Python Text.wrap's final per-line
// `line.truncate(width, overflow)` (and no-wrap skips divide + rstrip).
inline std::vector<Segment> wrap_styled(const std::vector<Segment>& segs,
                                        size_t width, Overflow overflow,
                                        const Style& nl_style, bool no_wrap = false) {
    const bool fold = (overflow == Overflow::Fold);
    // 1. Group segments into lines at '\n'; keep every blank line (Python
    //    split(allow_blank=True), incl. the trailing one after a final '\n').
    using Run = std::pair<std::string, const Style*>; // (text, style)
    std::vector<std::vector<Run>> lines;
    for (const auto& s : segs) {
        size_t start = 0;
        while (start <= s.text.size()) {
            size_t nl = s.text.find('\n', start);
            if (nl == std::string::npos) {
                if (start < s.text.size()) {
                    if (lines.empty()) lines.push_back({});
                    lines.back().emplace_back(s.text.substr(start), &s.style);
                }
                break;
            }
            if (nl > start) {
                if (lines.empty()) lines.push_back({});
                lines.back().emplace_back(s.text.substr(start, nl - start), &s.style);
            }
            lines.push_back({}); // newline boundary (blank lines preserved)
            start = nl + 1;
        }
    }
    if (lines.empty()) lines.push_back({});

    // 2. Per line: expand tabs, divide, split runs at break offsets, rstrip.
    std::vector<Segment> out;
    for (size_t li = 0; li < lines.size(); ++li) {
        std::string text;        // expanded line text
        std::vector<const Style*> char_style; // per expanded char
        size_t column = 0;
        for (const auto& [rtext, style] : lines[li]) {
            std::string_view rview(rtext);
            size_t i = 0;
            while (i < rview.size()) {
                size_t j = i;
                char32_t cp = unicode::decode_utf8(rview, j);
                if (cp == '\t') {
                    size_t spaces = 8 - (column % 8);
                    for (size_t k = 0; k < spaces; ++k) {
                        text.push_back(' ');
                        char_style.push_back(style);
                    }
                    column += spaces;
                } else if (cp == '\r') {
                    // dropped (CRLF handling)
                } else {
                    for (size_t b = i; b < j; ++b) {
                        text.push_back(rview[b]);
                        char_style.push_back(style);
                    }
                    // Tab stops are measured in cells (Python expand_tabs).
                    column += static_cast<size_t>(unicode::get_character_cell_size(cp));
                }
                i = j;
            }
        }
        std::vector<size_t> breaks = no_wrap ? std::vector<size_t>{}
                                             : divide_line(text, width, fold);
        // Final per-line truncate (Python Text.wrap: line.truncate(width,
        // overflow)) — applied after divide; fold always fits, so skip it.
        // no_wrap also skips rstrip_end (Python keeps the line whole).
        const size_t original_size = text.size();
        size_t cut_char = original_size;
        if (overflow != Overflow::Fold && string_cell_len(text) > width) {
            cut_char = char_offset_at_cell(
                text, overflow == Overflow::Ellipsis ? (width > 0 ? width - 1 : 0) : width);
            breaks.erase(std::remove_if(breaks.begin(), breaks.end(),
                                        [cut_char](size_t b) { return b >= cut_char; }),
                         breaks.end());
        }
        const bool ellipsize =
            overflow == Overflow::Ellipsis && cut_char < original_size;
        const Style* ellipsis_style =
            cut_char < original_size
                ? char_style[cut_char < char_style.size() ? cut_char : char_style.size() - 1]
                : nullptr;
        if (cut_char < original_size) {
            text.resize(cut_char);
            char_style.resize(cut_char);
        }
        // Emit this line's parts (split back across the original style runs),
        // separated from the previous line by a style-None newline.
        std::vector<size_t> bounds;
        bounds.reserve(breaks.size() + 1);
        bounds.push_back(0);
        for (size_t b : breaks) bounds.push_back(b);
        bounds.push_back(text.size());
        for (size_t p = 0; p + 1 < bounds.size(); ++p) {
            size_t start = bounds[p];
            size_t end = bounds[p + 1];
            std::string part = text.substr(start, end - start);
            if (!no_wrap) rstrip_end(part, width);
            if (!out.empty()) out.push_back({"\n", nl_style});
            size_t k = start;
            const size_t part_end = start + part.size();
            while (k < part_end) {
                const Style* st = char_style[k];
                size_t k2 = k + 1;
                while (k2 < part_end && char_style[k2] == st) ++k2;
                out.push_back({part.substr(k - start, k2 - k), *st});
                k = k2;
            }
        }
        if (ellipsize)
            out.push_back({"\xe2\x80\xa6", *ellipsis_style});
    }
    return out;
}

} // namespace wrap_detail

// Python rich.text.Span: a codepoint range carrying a named style or a
// direct Style, kept in append order (later spans overlay earlier ones).
struct Span {
    size_t start{0};
    size_t end{0};
    bool is_name{true};
    std::string name;   // when is_name
    rich::Style style;  // when !is_name
};

namespace span_detail {

// Python rich.control.STRIP_CONTROL_CODES = [7, 8, 11, 12, 13] — dropped
// from str input in Text.__init__ and Text.append.
inline std::string strip_control_codes(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in)
        if (c != '\a' && c != '\b' && c != '\v' && c != '\f' && c != '\r')
            out.push_back(c);
    return out;
}

inline bool is_ws_cp(char32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\v' || cp == '\f';
}

inline size_t cp_width(char32_t cp) {
    return unicode::get_character_cell_size(cp);
}

// A rendered line cell: a content codepoint (orig = codepoint index into the
// text, for span lookup) or a synthesized cell (orig < 0: tab-expansion
// space, justification padding, ellipsis). bnd marks the start of a new
// render segment (Python Text.render emits one segment per span event
// interval; full-justify tokens each carry a base-style span, so every
// word and space run is its own segment).
struct Cell {
    char32_t cp;
    long long orig;
    bool syn{false};       // carry explicit style (full-justify spaces)
    bool bnd{false};       // start a new segment here
    rich::Style syn_style;
};

inline size_t cell_len_vec(const std::vector<Cell>& v) {
    size_t n = 0;
    for (const auto& c : v) n += cp_width(c.cp);
    return n;
}

// Codepoint-level port of rich._wrap.divide_line (byte version in
// wrap_detail::divide_line is oracle-verified; same structure here, cp
// indices instead of byte offsets).
inline std::vector<size_t> divide_cps(const std::vector<Cell>& line, size_t width, bool fold) {
    std::vector<size_t> breaks;
    const size_t n = line.size();
    size_t cell_offset = 0;
    size_t pos = 0;
    while (pos < n) {
        size_t ws_start = pos;
        while (pos < n && is_ws_cp(line[pos].cp)) ++pos;
        if (pos >= n) break;
        size_t core_start = pos;
        while (pos < n && !is_ws_cp(line[pos].cp)) ++pos;
        size_t core_end = pos;
        while (pos < n && is_ws_cp(line[pos].cp)) ++pos;
        size_t word_len = 0;
        for (size_t k = core_start; k < core_end; ++k) word_len += cp_width(line[k].cp);
        size_t remaining = (width > cell_offset) ? width - cell_offset : 0;
        if (remaining >= word_len) {
            for (size_t k = ws_start; k < pos; ++k) cell_offset += cp_width(line[k].cp);
        } else if (word_len > width) {
            if (fold) {
                // Fold the word (full \s*\S+\s* range) into <= width-cell
                // chunks; break before every chunk except the first.
                size_t start = ws_start;
                size_t cur = ws_start;
                size_t acc = 0;
                while (cur < pos) {
                    size_t chunk_start = cur;
                    while (cur < pos && acc + cp_width(line[cur].cp) <= width) {
                        acc += cp_width(line[cur].cp);
                        ++cur;
                    }
                    if (chunk_start == pos) break; // no progress (0-width run)
                    if (chunk_start) breaks.push_back(chunk_start);
                    if (cur >= pos) cell_offset = acc;
                    acc = 0;
                }
            } else {
                if (ws_start) breaks.push_back(ws_start);
                size_t cells = 0;
                for (size_t k = ws_start; k < pos; ++k) cells += cp_width(line[k].cp);
                cell_offset = cells;
            }
        } else if (cell_offset && ws_start) {
            breaks.push_back(ws_start);
            size_t cells = 0;
            for (size_t k = ws_start; k < pos; ++k) cells += cp_width(line[k].cp);
            cell_offset = cells;
        }
    }
    return breaks;
}

// Crop a cell vector to at most `take` cells without splitting a codepoint
// (rich.cells.set_cell_size crop direction).
inline std::vector<Cell> crop_cells_vec(const std::vector<Cell>& v, size_t take) {
    std::vector<Cell> out;
    size_t acc = 0;
    for (const auto& c : v) {
        size_t w = cp_width(c.cp);
        if (acc + w > take) break;
        out.push_back(c);
        acc += w;
    }
    return out;
}

} // namespace span_detail

class Text {
public:
    Text(std::string content = "", rich::Style style = {})
        : content_(span_detail::strip_control_codes(content)), style_(style) {}

    Text& set_style(rich::Style s) { style_ = std::move(s); return *this; }
    Text& set_justify(Justify j) { justify_ = j; return *this; }
    Text& set_overflow(Overflow o) { overflow_ = o; return *this; }

    const std::string& content() const { return content_; }
    const std::vector<Span>& spans() const { return spans_; }
    rich::Style base_style() const { return style_; }

    // Python Text.stylize(style, start=0, end=None). Style may be a named
    // style string (resolved via console.get_style at render) or a Style.
    // Negative offsets count from the end (codepoint count); end omitted
    // (std::nullopt) means "to the end of the text" — distinct from an
    // explicit -1, which means the last character. No-op for a falsy style
    // or an empty range. Returns *this for chaining.
    Text& stylize(std::string_view style, long long start = 0,
                  std::optional<long long> end = std::nullopt) {
        if (style.empty()) return *this;
        push_span(std::string(style), nullptr, start, end, false);
        return *this;
    }
    Text& stylize(const rich::Style& style, long long start = 0,
                  std::optional<long long> end = std::nullopt) {
        if (!style.truthy()) return *this;
        push_span(std::string(), &style, start, end, false);
        return *this;
    }

    // Python Text.stylize_before: identical, but the span is inserted at
    // position 0 (rendered underneath later spans).
    Text& stylize_before(std::string_view style, long long start = 0,
                         std::optional<long long> end = std::nullopt) {
        if (style.empty()) return *this;
        push_span(std::string(style), nullptr, start, end, true);
        return *this;
    }
    Text& stylize_before(const rich::Style& style, long long start = 0,
                         std::optional<long long> end = std::nullopt) {
        if (!style.truthy()) return *this;
        push_span(std::string(), &style, start, end, true);
        return *this;
    }

    // Python Text.highlight_regex(re_highlight, style, style_prefix).
    // style may be "" (whole-match span suppressed — used by the built-in
    // highlighters, which style only named groups). Throws
    // std::invalid_argument for unsupported patterns (Python: re.error).
    // Returns the number of matches.
    size_t highlight_regex(const std::string& pattern, std::string_view style = "",
                           std::string_view style_prefix = "") {
        regex::Pattern pat(pattern);
        const auto& names = pat.names();
        size_t count = 0;
        for (const auto& m : pat.find_all(content_)) {
            // Python: whole-match span only when a style is given and the
            // match is non-empty; named-group spans (definition order) are
            // always appended under style_prefix + name.
            if (!style.empty() && m.end > m.start) {
                Span s;
                s.start = m.start;
                s.end = m.end;
                s.is_name = true;
                s.name = std::string(style);
                spans_.push_back(std::move(s));
            }
            ++count;
            for (size_t g = 0; g < names.size() && g < m.groups.size(); ++g) {
                const auto& [gs, ge] = m.groups[g];
                if (gs != SIZE_MAX && ge > gs) {
                    Span s;
                    s.start = gs;
                    s.end = ge;
                    s.is_name = true;
                    s.name = std::string(style_prefix) + names[g];
                    spans_.push_back(std::move(s));
                }
            }
        }
        return count;
    }

    // Python Text.append: str (optional style) or Text (style must be
    // None — C++ has no (Text, style) overload, matching Python's ValueError).
    Text& append(std::string_view text) { return append_raw(text, nullptr); }
    Text& append(const std::string& text) { return append_raw(text, nullptr); }
    Text& append(std::string_view text, const rich::Style& style) {
        return append_raw(text, style.truthy() ? &style : nullptr);
    }
    Text& append(const Text& t) {
        if (t.content_.empty()) return *this;
        const long long offset = cp_count();
        if (t.style_.truthy()) {
            Span s;
            s.start = static_cast<size_t>(offset);
            s.end = static_cast<size_t>(offset + t.cp_count());
            s.is_name = false;
            s.style = t.style_;
            spans_.push_back(std::move(s));
        }
        for (const auto& sp : t.spans_) {
            Span s = sp;
            s.start += static_cast<size_t>(offset);
            s.end += static_cast<size_t>(offset);
            spans_.push_back(std::move(s));
        }
        content_.append(t.content_);
        return *this;
    }

    // One assembled part: a string plus an optional style.
    struct Part {
        std::string text;
        rich::Style style{};
        bool has_style{false};
        Part(std::string t) : text(std::move(t)) {}
        Part(std::string t, rich::Style s)
            : text(std::move(t)), style(std::move(s)), has_style(true) {}
    };

    // Python Text.assemble(*parts, style=""): parts are str or (str, style).
    static Text assemble(std::initializer_list<Part> parts, const rich::Style& style = {}) {
        Text t("", style);
        for (const auto& p : parts)
            if (p.has_style) t.append(p.text, p.style);
            else t.append(p.text);
        return t;
    }

    // Python Text.styled(text, style): base style stays empty; `style` is a
    // full-range span (so justification padding is NOT styled by it).
    static Text styled(const std::string& text, const rich::Style& style = {}) {
        Text t(text);
        if (style.truthy()) {
            Span s;
            s.start = 0;
            s.end = static_cast<size_t>(t.cp_count());
            s.is_name = false;
            s.style = style;
            t.spans_.push_back(std::move(s));
        }
        return t;
    }

    // The view Console.print renders (AnyRenderable::print_render): content
    // and spans only — base style, justify, and overflow are dropped.
    static Text flattened(const Text& t) {
        Text out;
        out.content_ = t.content_;
        out.spans_ = t.spans_;
        return out;
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        const size_t width = (opt.width > 0) ? opt.width : 80;
        // Python wrap(): fold=wrap_overflow == "fold" — only "fold" chunks
        // over-long words at divide time; crop/ellipsis keep the word whole
        // for the later truncate step.
        const bool fold = (overflow_ == Overflow::Fold);
        const bool plain = spans_.empty() && !style_.truthy();

        // 1. Decode; split into lines at '\n' (allow_blank=True keeps the
        //    trailing blank line); '\r' dropped (legacy engine, CRLF).
        std::vector<std::vector<span_detail::Cell>> lines;
        {
            std::vector<char32_t> cps;
            size_t i = 0;
            while (i < content_.size()) cps.push_back(unicode::decode_utf8(content_, i));
            std::vector<span_detail::Cell> cur;
            for (size_t k = 0; k < cps.size(); ++k) {
                char32_t cp = cps[k];
                if (cp == '\n') {
                    lines.push_back(std::move(cur));
                    cur.clear();
                } else if (cp == '\r') {
                    continue;
                } else {
                    cur.push_back({cp, static_cast<long long>(k), false, {}});
                }
            }
            lines.push_back(std::move(cur));
        }

        // 2. Expand tabs per line (tab_size 8, column in cells; the
        //    expansion spaces carry the tab's original index, so they inherit
        //    whatever span covered the tab — Python expand_tabs parity).
        //    Python expand_tabs + Text("").join(parts) makes every tab a
        //    part boundary: a span covering the tab is extended over the
        //    spaces (Span.extend) and the next part's spans start fresh, so
        //    a render-segment boundary sits right after the expansion when
        //    a span touches either side of the tab.
        for (auto& line : lines) {
            bool has_tab = false;
            for (const auto& c : line)
                if (c.cp == '\t') { has_tab = true; break; }
            if (!has_tab) continue;
            std::vector<span_detail::Cell> out;
            out.reserve(line.size() + 8);
            size_t column = 0;
            bool pending_bnd = false;
            for (auto& c : line) {
                if (c.cp == '\t') {
                    size_t spaces = 8 - (column % 8);
                    for (size_t k = 0; k < spaces; ++k)
                        out.push_back({' ', c.orig, false, pending_bnd && k == 0, {}});
                    pending_bnd = false;
                    for (const auto& sp : spans_) {
                        if (sp.start <= c.orig && c.orig < sp.end || sp.start == c.orig + 1) {
                            pending_bnd = true;
                            break;
                        }
                    }
                    column += spaces;
                } else {
                    c.bnd = c.bnd || pending_bnd;
                    pending_bnd = false;
                    out.push_back(c);
                    column += span_detail::cp_width(c.cp);
                }
            }
            line = std::move(out);
        }

        // 3+4. Divide each line into parts, rstrip_end (Python counts
        //      characters beyond the width, not cells).
        std::vector<std::vector<span_detail::Cell>> parts;
        for (const auto& line : lines) {
            auto breaks = span_detail::divide_cps(line, width, fold);
            std::vector<size_t> bounds;
            bounds.reserve(breaks.size() + 2);
            bounds.push_back(0);
            for (size_t b : breaks) bounds.push_back(b);
            bounds.push_back(line.size());
            for (size_t p = 0; p + 1 < bounds.size(); ++p) {
                std::vector<span_detail::Cell> part(line.begin() + bounds[p],
                                                    line.begin() + bounds[p + 1]);
                if (part.size() > width) {
                    size_t excess = part.size() - width;
                    while (excess && !part.empty() && span_detail::is_ws_cp(part.back().cp)) {
                        part.pop_back();
                        --excess;
                    }
                }
                parts.push_back(std::move(part));
            }
        }

        // Resolve span styles once (named spans via the console theme).
        std::vector<rich::Style> resolved(spans_.size());
        for (size_t k = 0; k < spans_.size(); ++k) resolved[k] = resolve_span(spans_[k], con);

        // 5. Justify per original line (Python Lines.justify; "default" is
        //    a no-op). Center/right rstrip the whole part first.
        switch (justify_) {
        case Justify::Left:
            for (auto& part : parts) truncate_part(part, width, true);
            break;
        case Justify::Center:
            for (auto& part : parts) {
                rstrip_all(part);
                truncate_part(part, width, false);
                const size_t l = span_detail::cell_len_vec(part);
                const size_t left = (width > l) ? (width - l) / 2 : 0;
                const size_t right = (width > l) ? (width - l) - left : 0;
                pad_left_n(part, left);
                pad_right_n(part, right);
            }
            break;
        case Justify::Right:
            for (auto& part : parts) {
                rstrip_all(part);
                truncate_part(part, width, false);
                const size_t l = span_detail::cell_len_vec(part);
                pad_left_n(part, (width > l) ? width - l : 0);
            }
            break;
        case Justify::Full:
            for (size_t p = 0; p + 1 < parts.size(); ++p)
                full_justify_part(parts[p], width, resolved);
            break;
        case Justify::Default:
        default:
            break;
        }

        // 6. Python wrap(): final per-part truncate.
        for (auto& part : parts) truncate_part(part, width, false);

        // 7. Emit.
        std::vector<rich::Segment> out;
        if (plain) {
            // Python fast path (no spans, falsy base style): one style-None
            // segment for the whole wrapped text plus the end newline.
            std::string joined;
            for (size_t p = 0; p < parts.size(); ++p) {
                if (p) joined.push_back('\n');
                for (const auto& c : parts[p]) unicode::encode_utf8(c.cp, joined);
            }
            joined.push_back('\n');
            out.push_back({std::move(joined), rich::detail::kNoneStyle()});
            return out;
        }
        // Python Text.render: one segment per span event interval — even
        // when adjacent intervals resolve to the same style (overlapping
        // spans fold, but the console re-emits SGR for every segment).
        // Event boundaries: every span start/end codepoint index, plus the
        // full-justify token starts (marked bnd).
        std::vector<bool> bnd_at; // per content codepoint index
        {
            long long ncp = 0;
            {
                size_t i = 0;
                while (i < content_.size()) {
                    unicode::decode_utf8(content_, i);
                    ++ncp;
                }
            }
            bnd_at.assign(static_cast<size_t>(ncp), false);
            for (const auto& sp : spans_) {
                if (sp.start < bnd_at.size()) bnd_at[sp.start] = true;
                if (sp.end < bnd_at.size()) bnd_at[sp.end] = true;
            }
        }
        bool first = true;
        for (const auto& part : parts) {
            if (!first) out.push_back({"\n", rich::detail::kNoneStyle()});
            first = false;
            if (part.empty()) continue;
            std::string text;
            rich::Style cur{};
            bool has = false;
            long long prev_orig = -1;
            for (const auto& c : part) {
                rich::Style st = style_of(c, resolved);
                // Span boundaries are checked on the first cell of a
                // duplicated orig group (tab-expansion spaces share the
                // tab's index; Python re-indexes spans at expansion).
                const bool at_boundary =
                    c.bnd || (c.orig >= 0 && c.orig != prev_orig &&
                             static_cast<size_t>(c.orig) < bnd_at.size() && bnd_at[c.orig]);
                if (!has || at_boundary || !(st == cur)) {
                    if (has) out.push_back({std::move(text), cur});
                    text.clear();
                    cur = st;
                    has = true;
                }
                unicode::encode_utf8(c.cp, text);
                prev_orig = c.orig;
            }
            if (has) out.push_back({std::move(text), cur});
        }
        out.push_back({"\n", rich::detail::kNoneStyle()});
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options& opt) const {
        // Python __rich_measure__: (longest word cells, longest line cells),
        // lines split at '\n' (splitlines approximated by the engine's line
        // model), words split on whitespace runs.
        (void)opt;
        std::vector<char32_t> cps;
        size_t i = 0;
        while (i < content_.size()) cps.push_back(unicode::decode_utf8(content_, i));
        if (cps.empty()) return {1, 1};
        size_t longest_line = 0, line_cells = 0;
        size_t longest_word = 0, word_cells = 0;
        for (char32_t cp : cps) {
            if (cp == '\n') {
                longest_line = std::max(longest_line, line_cells);
                longest_word = std::max(longest_word, word_cells);
                line_cells = 0;
                word_cells = 0;
                continue;
            }
            if (span_detail::is_ws_cp(cp)) {
                word_cells = 0;
                continue;
            }
            const size_t w = span_detail::cp_width(cp);
            line_cells += w;
            word_cells += w;
        }
        longest_line = std::max(longest_line, line_cells);
        longest_word = std::max(longest_word, word_cells);
        if (longest_word == 0) longest_word = longest_line;
        return {longest_word, longest_line};
    }

private:
    long long cp_count() const {
        long long n = 0;
        size_t i = 0;
        while (i < content_.size()) {
            unicode::decode_utf8(content_, i);
            ++n;
        }
        return n;
    }

    // Python stylize range semantics shared by stylize/stylize_before:
    // negative offsets count from the end, end nullopt = full length,
    // empty ranges are skipped. A start left negative after adjustment is
    // clamped to 0 (Python keeps it negative; no character can index below
    // 0, so coverage is identical).
    void push_span(std::string name, const rich::Style* style, long long start,
                   std::optional<long long> end, bool before) {
        const long long length = cp_count();
        if (start < 0) start = length + start;
        long long e = end.value_or(length);
        if (end.has_value() && e < 0) e = length + e;
        if (start >= length || e <= start) return;
        Span s;
        s.start = static_cast<size_t>(start < 0 ? 0 : start);
        s.end = static_cast<size_t>(e > length ? length : e);
        s.is_name = (style == nullptr);
        if (s.is_name) s.name = std::move(name);
        else s.style = *style;
        if (before) spans_.insert(spans_.begin(), std::move(s));
        else spans_.push_back(std::move(s));
    }

    Text& append_raw(std::string_view text, const rich::Style* style) {
        if (text.empty()) return *this;
        // Python append() sanitizes str input (strip_control_codes).
        const std::string clean = span_detail::strip_control_codes(text);
        if (clean.empty()) return *this;
        const long long offset = cp_count();
        const long long n = cp_count_of(clean);
        content_.append(clean);
        if (style) {
            Span s;
            s.start = static_cast<size_t>(offset);
            s.end = static_cast<size_t>(offset + n);
            s.is_name = false;
            s.style = *style;
            spans_.push_back(std::move(s));
        }
        return *this;
    }

    static long long cp_count_of(std::string_view sv) {
        long long n = 0;
        size_t i = 0;
        while (i < sv.size()) {
            unicode::decode_utf8(sv, i);
            ++n;
        }
        return n;
    }

    static rich::Style resolve_span(const Span& sp, const rich::Console& con) {
        if (!sp.is_name) return sp.style;
        try {
            return con.get_style(sp.name, "");
        } catch (const std::runtime_error&) {
            // Python: get_style(..., default=Style.null()) — an unknown span
            // name degrades to the null style.
            return rich::Style{};
        }
    }

    // Per-cell style: base style + spans (in append order) covering the
    // cell's original codepoint index; synthesized cells with syn carry
    // their explicit style, others fall back to base only (Python
    // get_style_at_offset: no span reaches padding/ellipsis chars).
    rich::Style style_of(const span_detail::Cell& c, const std::vector<rich::Style>& resolved) const {
        if (c.syn) return c.syn_style;
        rich::Style s = style_;
        if (c.orig >= 0) {
            const size_t o = static_cast<size_t>(c.orig);
            for (size_t k = 0; k < spans_.size(); ++k) {
                const auto& sp = spans_[k];
                if (sp.start <= o && o < sp.end) s = s + resolved[k];
            }
        }
        return s;
    }

    static void rstrip_all(std::vector<span_detail::Cell>& part) {
        while (!part.empty() && span_detail::is_ws_cp(part.back().cp)) part.pop_back();
    }

    static void pad_left_n(std::vector<span_detail::Cell>& part, size_t n) {
        for (size_t k = 0; k < n; ++k) part.insert(part.begin(), {' ', -1, false, {}});
    }
    static void pad_right_n(std::vector<span_detail::Cell>& part, size_t n) {
        for (size_t k = 0; k < n; ++k) part.push_back({' ', -1, false, {}});
    }

    // Python Text.truncate(max_width, overflow, pad): crop the part to the
    // width (ellipsis leaves room for '…'), then pad right when pad is set
    // and the part is short (padding gets the base style only).
    void truncate_part(std::vector<span_detail::Cell>& part, size_t width, bool pad) const {
        const size_t l = span_detail::cell_len_vec(part);
        if (l > width) {
            const size_t take =
                (overflow_ == Overflow::Ellipsis) ? (width > 0 ? width - 1 : 0) : width;
            part = span_detail::crop_cells_vec(part, take);
            if (overflow_ == Overflow::Ellipsis)
                part.push_back({U'\u2026', -1, false, {}});
        } else if (pad && l < width) {
            pad_right_n(part, width - l);
        }
    }

    // Python Lines.justify(full): rebuild all but the last part with the
    // extra space distributed from the right over the single-space gaps;
    // the run between word i and i+1 takes the style both words share at
    // the boundary, else the base style.
    void full_justify_part(std::vector<span_detail::Cell>& part, size_t width,
                           const std::vector<rich::Style>& resolved) const {
        // words = plain.split(" ") (segments kept, one trailing empty
        // dropped when the part ends with a space — probed parity).
        std::vector<std::vector<span_detail::Cell>> words;
        {
            std::vector<span_detail::Cell> cur;
            for (const auto& c : part) {
                if (c.cp == ' ') {
                    words.push_back(std::move(cur));
                    cur.clear();
                } else {
                    cur.push_back(c);
                }
            }
            words.push_back(std::move(cur));
            if (!part.empty() && part.back().cp == ' ') words.pop_back();
        }
        if (words.size() < 2) return;
        size_t words_size = 0;
        for (const auto& w : words) words_size += span_detail::cell_len_vec(w);
        int num_spaces = static_cast<int>(words.size()) - 1;
        std::vector<int> spaces(num_spaces, 1);
        size_t index = 0;
        while (words_size + static_cast<size_t>(num_spaces) < width) {
            spaces[spaces.size() - index - 1] += 1;
            num_spaces += 1;
            index = (index + 1) % spaces.size();
        }
        std::vector<span_detail::Cell> out;
        out.reserve(part.size() + spaces.size());
        for (size_t i = 0; i < words.size(); ++i) {
            // Python rebuilds the justified line as Text("").join(tokens);
            // each token's base style becomes a full-range span, so every
            // token start is a render-segment boundary.
            if (!words[i].empty()) words[i].front().bnd = true;
            out.insert(out.end(), words[i].begin(), words[i].end());
            if (i < spaces.size()) {
                const auto& prev = words[i];
                const auto& next = words[i + 1];
                // get_style_at_offset(console, -1/0): base style for an
                // empty word, full per-char style otherwise.
                rich::Style a = prev.empty() ? style_ : style_of(prev.back(), resolved);
                rich::Style b = next.empty() ? style_ : style_of(next.front(), resolved);
                const rich::Style& space_style = (a == b) ? a : style_;
                // The whole run is one token in Python's join; the segment
                // boundary is at the run's first space.
                for (int k = 0; k < spaces[i]; ++k)
                    out.push_back({' ', -1, true, k == 0, space_style});
            }
        }
        part.swap(out);
    }

    std::string content_;
    rich::Style style_;
    std::vector<Span> spans_;
    Justify justify_{Justify::Default};
    Overflow overflow_{Overflow::Fold};
};

inline std::vector<rich::Segment> render_item(const Text& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Text& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::text

// Python Console._collect_renderables: Text objects printed via
// Console.print (or Console.log) are collected through Text(" ").join([t]);
// join() builds its result from the separator's blank_copy(), so the
// original Text's base style, justify, and overflow are lost — only the
// content and spans survive. The dedicated AnyRenderable model supplies
// that flattened view to the print path, while render/measure (used by
// containers like Panel and Group, which call Console.render directly)
// keep the full Text semantics.
//
// (Full specializations of a class member template must be declared in the
// namespace enclosing the class — namespace rich — not rich::text.)
namespace rich {

template<>
struct AnyRenderable::Model<text::Text> final : AnyRenderable::Concept {
    explicit Model(const text::Text& t) : t_(t), flat_(text::Text::flattened(t)) {
#ifdef SPEC_DEBUG
        std::cerr << "SPECIALIZED MODEL USED" << std::endl;
#endif
    }

    std::vector<Segment> render(const Console& c, const Options& o) const override {
        return text::render_item(t_, c, o);
    }
    std::pair<size_t, size_t> measure(const Console& c, const Options& o) const override {
        return text::measure_item(t_, c, o);
    }
    std::vector<Segment> print_render(const Console& c, const Options& o) const override {
        return text::render_item(flat_, c, o);
    }

    text::Text t_;
    text::Text flat_;
};

} // namespace rich
