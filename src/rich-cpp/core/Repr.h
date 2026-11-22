#pragma once
// Python-parity repr/format helpers shared by SVG export and pretty-print.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace rich::repr {

// Python repr() of a str: single quotes, backslash/quote/newline escapes.
inline std::string py_repr_string(std::string_view s) {
    std::string out = "'";
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += ch;
        }
    }
    out += "'";
    return out;
}

// Python str(float) / repr(float): shortest round-trip repr; integral values
// keep a trailing ".0" (str(975.0) == "975.0").
inline std::string py_str_float(double v) {
    char buf[40];
    for (int p = 1; p <= 17; ++p) {
        std::snprintf(buf, sizeof(buf), "%.*g", p, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    std::string s = buf;
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find('n') == std::string::npos && s.find('i') == std::string::npos)
        s += ".0";
    return s;
}

// Python cell_len for layout: codepoint count, control chars are 0 cells.
// (Full wcwidth/East-Asian-width parity is a separate pending item.)
inline size_t cell_len(std::string_view s) {
    size_t cells = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = (c < 0x80) ? 1
                   : ((c & 0xE0) == 0xC0) ? 2
                   : ((c & 0xF0) == 0xE0) ? 3
                   : ((c & 0xF8) == 0xF0) ? 4 : 1;
        if (c < 0x20 || (c >= 0x7F && c < 0xA0)) { i += len; continue; } // 0 cells
        cells += 1;
        i += len;
    }
    return cells;
}

} // namespace rich::repr
