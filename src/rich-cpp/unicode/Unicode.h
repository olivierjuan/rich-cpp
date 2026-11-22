#pragma once
// Terminal cell-width measurement, Python rich.cells parity.
// Width data is generated from the installed Python rich unicode tables
// (CellTable.h, regen via src/rich-cpp/scripts/cell_table_gen.py).
#include <rich_export.h>
#include "CellTable.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <string_view>

namespace rich::unicode {

// UTF-8 decode one codepoint starting at i; advances i past it.
// Returns U+FFFD on invalid sequences (Python str input never has these).
inline char32_t decode_utf8(std::string_view sv, size_t& i) {
    if (i >= sv.size()) return 0xFFFD;
    unsigned char c0 = static_cast<unsigned char>(sv[i]);
    if (c0 < 0x80) { ++i; return c0; }
    size_t len = ((c0 & 0xE0) == 0xC0) ? 2
               : ((c0 & 0xF0) == 0xE0) ? 3
               : ((c0 & 0xF8) == 0xF0) ? 4 : 0;
    if (len == 0 || i + len > sv.size()) { ++i; return 0xFFFD; }
    char32_t cp = 0;
    switch (len) {
        case 2: cp = c0 & 0x1F; break;
        case 3: cp = c0 & 0x0F; break;
        case 4: cp = c0 & 0x07; break;
    }
    for (size_t k = 1; k < len; ++k) {
        unsigned char ck = static_cast<unsigned char>(sv[i + k]);
        if ((ck & 0xC0) != 0x80) { ++i; return 0xFFFD; }
        cp = (cp << 6) | (ck & 0x3F);
    }
    i += len;
    return cp;
}

// Python get_character_cell_size: 0 for control chars, generated table
// lookup (0/1/2), default 1 past the last range.
inline int get_character_cell_size(char32_t cp) {
    if ((cp != 0 && cp < 32) || (cp >= 0x7F && cp < 0xA0)) return 0;
    const auto& table = kWidthRanges;
    if (static_cast<uint32_t>(cp) > table.back().end) return 1;
    size_t lo = 0, hi = table.size() - 1;
    while (lo <= hi) {
        size_t mid = (lo + hi) >> 1;
        if (cp < table[mid].start) {
            if (mid == 0) break;
            hi = mid - 1;
        } else if (cp > table[mid].end) {
            lo = mid + 1;
        } else {
            return table[mid].width;
        }
    }
    return 1;
}

// Number of terminal cells a single character occupies (0, 1 or 2).
inline int character_cell_size(std::string_view utf8_char) {
    size_t i = 0;
    return get_character_cell_size(decode_utf8(utf8_char, i));
}

// Binary search in the sorted narrow-to-wide codepoint table.
inline bool in_narrow_to_wide(char32_t cp) {
    return std::binary_search(kNarrowToWide.begin(), kNarrowToWide.end(),
                              static_cast<uint32_t>(cp));
}

// Python rich.cells.cell_len:
//   - control chars (<0x20, 0x7F-0x9F) contribute 0 cells,
//   - wide East-Asian codepoints contribute 2 (generated table),
//   - U+200D (ZWJ) contributes 0 and leaves last-measured state untouched,
//   - U+FE0F (VS16) contributes 0 but adds 1 cell when the last measured
//     character is in narrow_to_wide, then clears that state.
inline size_t string_cell_len(std::string_view sv) {
    static constexpr char32_t kZWJ = 0x200D;
    static constexpr char32_t kVS16 = 0xFE0F;

    size_t total = 0;
    bool has_last_measured = false;
    char32_t last_measured = 0;

    size_t i = 0;
    while (i < sv.size()) {
        char32_t ch = decode_utf8(sv, i);
        if (ch == kZWJ) continue;
        if (ch == kVS16) {
            if (has_last_measured) {
                if (in_narrow_to_wide(last_measured)) ++total;
                has_last_measured = false;
            }
            continue;
        }
        int w = get_character_cell_size(ch);
        if (w != 0) {
            last_measured = ch;
            has_last_measured = true;
            total += static_cast<size_t>(w);
        }
    }
    return total;
}

// Legacy name kept for compatibility; identical to string_cell_len.
inline size_t string_width(std::string_view sv) { return string_cell_len(sv); }

// Python rich.cells.chop_cells: split text so each chunk's cell width is
// <= width (grapheme spans via the width table; zero-cell chars ride along).
inline std::vector<std::string> chop_cells(std::string_view text, size_t width) {
    std::vector<std::string> lines;
    if (width == 0) {
        if (!text.empty()) lines.push_back(std::string(text));
        return lines;
    }
    size_t line_size = 0;   // cells used on the current line
    size_t line_start = 0;  // char offset of the current line
    size_t i = 0;
    while (i < text.size()) {
        size_t j = i;
        char32_t cp = decode_utf8(text, j);
        int w = get_character_cell_size(cp);
        // ZWJ/VS16 are zero-cell for chop purposes (Python split_graphemes
        // treats them as zero-width spans; the width table says 0 anyway).
        if (line_size + static_cast<size_t>(w) > width) {
            lines.push_back(std::string(text.substr(line_start, i - line_start)));
            line_start = i;
            line_size = 0;
        }
        line_size += static_cast<size_t>(w);
        i = j;
    }
    if (line_size)
        lines.push_back(std::string(text.substr(line_start)));
    return lines;
}

} // namespace rich::unicode
