#pragma once
// Python-parity repr/format helpers shared by SVG export and pretty-print.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unicode/Unicode.h>

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

// Python str(float) / repr(float) — port of the floatobject.c format rules:
// shortest round-trip digits (p in 1..17 significant digits), then
//   decpt = exp10 + 1  (decimal point position within the digit string)
//   fixed point iff -4 <= decpt <= 16, else scientific with a signed
//   two-digit-at-least exponent ("1e+16", "1.5e-07").
// Non-finite: "nan"/"inf"/"-inf"; -0.0 -> "-0.0".
inline std::string py_str_float(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    if (v == 0.0) return std::signbit(v) ? "-0.0" : "0.0";

    const bool neg = v < 0;
    const double av = neg ? -v : v;

    char buf[48];
    int p = 1;
    for (; p <= 17; ++p) {
        std::snprintf(buf, sizeof(buf), "%.*g", p, av);
        if (std::strtod(buf, nullptr) == av) break;
    }

    // Unambiguous digits + exponent: d[.ddd]e±XX at exactly p significant
    // digits (same rounding as %.*g above).
    std::snprintf(buf, sizeof(buf), "%.*e", p - 1, av);
    std::string sci = buf;
    const size_t epos = sci.find('e');
    const int exp10 = std::atoi(sci.substr(epos + 1).c_str());
    std::string digits;
    for (size_t i = 0; i < epos; ++i)
        if (sci[i] != '.') digits.push_back(sci[i]);

    const int decpt = exp10 + 1;
    std::string out;
    if (decpt <= -4 || decpt > 16) {
        // Scientific: d[.ddd]e±XX, exponent signed, >= 2 digits.
        out = digits.substr(0, 1);
        if (digits.size() > 1) {
            out += '.';
            out += digits.substr(1);
        }
        char ebuf[8];
        std::snprintf(ebuf, sizeof(ebuf), "e%+03d", decpt - 1);
        out += ebuf;
    } else if (decpt <= 0) {
        out = "0.";
        out.append(static_cast<size_t>(-decpt), '0');
        out += digits;
    } else if (decpt < static_cast<int>(digits.size())) {
        out = digits.substr(0, static_cast<size_t>(decpt));
        out += '.';
        out += digits.substr(static_cast<size_t>(decpt));
    } else {
        out = digits;
        out.append(static_cast<size_t>(decpt - static_cast<int>(digits.size())), '0');
        out += ".0";
    }
    if (neg) out.insert(out.begin(), '-');
    return out;
}

// Python cell_len for layout: full East-Asian-width parity via the
// generated unicode width table (control chars 0 cells, combining 0,
// wide/fullwidth 2, default 1).
inline size_t cell_len(std::string_view s) {
    return unicode::string_cell_len(s);
}

} // namespace rich::repr
