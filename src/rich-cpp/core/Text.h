#pragma once
// Word wrapping engine — port of Python rich Text.wrap / _wrap.divide_line.
// Wraps at cell boundaries (rich.cells.cell_len), folds overlong words with
// chop_cells, expands tabs (tab_size 8) and rstrips trailing whitespace
// beyond the width, byte-for-byte Python Rich parity.
#include <rich_core.h>
#include <unicode/Unicode.h>

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace rich::text {

using unicode::chop_cells;
using unicode::string_cell_len;

enum class Overflow { Fold, Crop, Ellipsis };
enum class Justify { Left, Center, Right, Full };

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

// Styled-segment wrapper (console.render parity): split segments into lines
// at '\n' (trailing '\r' dropped, CRLF), expand tabs, divide each line at
// cell-aware break offsets, then re-emit per-line pieces with their original
// styles. Line separators are emitted as separate segments with `nl_style`
// (Python records inter-line newlines as style=None → pass the sentinel).
inline std::vector<Segment> wrap_styled(const std::vector<Segment>& segs,
                                        size_t width, bool fold,
                                        const Style& nl_style) {
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
        auto breaks = divide_line(text, width, fold);
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
            rstrip_end(part, width);
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
    }
    return out;
}

} // namespace wrap_detail

class Text {
public:
    Text(std::string content = "", rich::Style style = {})
        : content_(std::move(content)), style_(style) {}

    Text& set_style(rich::Style s) { style_ = s; return *this; }
    Text& set_justify(Justify j) { justify_ = j; return *this; }
    Text& set_overflow(Overflow o) { overflow_ = o; return *this; }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        size_t max_width = (opt.width > 0) ? opt.width : 80;
        (void)con;
        // Python Text.__rich_console__: wrap the single styled run, join the
        // wrapped lines with newlines, then append the trailing end ("\n").
        std::vector<rich::Segment> single{{content_, style_}};
        auto out = wrap_detail::wrap_styled(single, max_width,
                                            overflow_ != Overflow::Crop,
                                            rich::detail::kNoneStyle());
        out.push_back({"\n", rich::detail::kNoneStyle()});
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options& opt) const {
        // Python __rich_measure__: longest word's cell length.
        size_t longest = 0;
        for (const auto& w : wrap_detail::words(content_)) {
            std::string_view core = w.text;
            while (!core.empty() && (core.back() == ' ' || core.back() == '\t'))
                core.remove_suffix(1);
            longest = std::max(longest, string_cell_len(core));
        }
        return {1, longest};
    }

private:
    std::string content_;
    rich::Style style_;
    Justify justify_{Justify::Left};
    Overflow overflow_{Overflow::Fold};
};

inline std::vector<rich::Segment> render_item(const Text& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Text& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::text
