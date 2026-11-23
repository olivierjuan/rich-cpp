#pragma once
// Python rich.filesize: decimal(size, precision, separator) and
// pick_unit_and_suffix(size, suffixes, base), ported from _to_str semantics
// (size == 1 → "1 byte"; size < base → "{size:,} bytes"; else the first
// suffix whose unit base**i exceeds size, value base*size/unit formatted
// "{:,.precisionf}{separator}{suffix}"). binary() is the same _to_str with
// the IEC (1024) suffix list.
//
// Unit arithmetic: with int64 sizes and the 8-entry suffix lists the break
// unit reaches 10^21 (decimal) / 2^60 (binary), which overflows int64. The
// comparisons therefore test `size < unit` (or `size < unit * base`) by
// integer compare when the threshold fits int64 and as "always true"
// otherwise (no int64 size reaches 10^21 / 2^70). Units are powers of 10 or
// 2, hence exactly representable in double up to 10^22 / 2^53 — exact for
// every threshold that fits int64.
// pick_unit_and_suffix returns the unit as double; Python's int unit is only
// ever used as a divisor here, and the returned powers (≤ 10^21 / 2^60 for
// int64 sizes) are exactly representable.
#include "rich_core.h"
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace rich::filesize_detail {

inline std::string comma_int(int64_t v) {
    std::string s = std::to_string(v);
    std::string out;
    int count = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (count && count % 3 == 0) out.push_back(',');
        out.push_back(*it);
        ++count;
    }
    return std::string(out.rbegin(), out.rend());
}

// Python f"{value:,.{precision}f}" — fixed-point with thousands separator.
inline std::string comma_fixed(double value, int precision) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, value);
    std::string s = buf;
    size_t dot = s.find('.');
    std::string int_part = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string frac = (dot == std::string::npos) ? "" : s.substr(dot);
    std::string sign;
    size_t start = 0;
    if (!int_part.empty() && int_part[0] == '-') {
        sign = "-";
        start = 1;
    }
    std::string grouped;
    int count = 0;
    for (size_t i = int_part.size(); i > start; --i) {
        if (count && count % 3 == 0) grouped.push_back(',');
        grouped.push_back(int_part[i - 1]);
        ++count;
    }
    return sign + std::string(grouped.rbegin(), grouped.rend()) + frac;
}

// Exact `size < threshold` for a power-of-base threshold that may exceed
// int64: integer compare when it fits, otherwise every int64 size is smaller.
inline bool size_lt_unit(int64_t size, double threshold) {
    if (threshold <= static_cast<double>(std::numeric_limits<int64_t>::max()))
        return size < static_cast<int64_t>(threshold);
    return true;
}

// Python filesize.pick_unit_and_suffix.
inline std::pair<double, std::string>
pick_unit_and_suffix(int64_t size, const std::vector<std::string>& suffixes,
                     int64_t base) {
    double unit = 1;
    for (const auto& suffix : suffixes) {
        if (size_lt_unit(size, unit * static_cast<double>(base)))
            return {unit, suffix};
        unit *= static_cast<double>(base);
    }
    return {unit / static_cast<double>(base), suffixes.back()};
}

// Python filesize._to_str with the SI suffix list (decimal).
inline std::string decimal(int64_t size, int precision = 1,
                           const char* separator = " ") {
    if (size == 1) return "1 byte";
    if (size < 1000) return comma_int(size) + " bytes";
    static const std::vector<std::string> suffixes = {"kB", "MB", "GB", "TB",
                                                      "PB", "EB", "ZB", "YB"};
    double unit = 1000.0 * 1000;
    const std::string* suffix = &suffixes.back();
    for (size_t i = 0; i < suffixes.size(); ++i) {
        if (size_lt_unit(size, unit)) {
            suffix = &suffixes[i];
            break;
        }
        if (i + 1 < suffixes.size()) unit *= 1000;
    }
    const double value =
        static_cast<double>(1000) * static_cast<double>(size) / unit;
    return comma_fixed(value, precision) + separator + *suffix;
}

// Python filesize._to_str with the IEC suffix list (binary, 1024 base).
inline std::string binary(int64_t size, int precision = 1,
                          const char* separator = " ") {
    if (size == 1) return "1 byte";
    if (size < 1024) return comma_int(size) + " bytes";
    static const std::vector<std::string> suffixes = {"KiB", "MiB", "GiB", "TiB",
                                                      "PiB", "EiB", "ZiB", "YiB"};
    double unit = 1024.0 * 1024;
    const std::string* suffix = &suffixes.back();
    for (size_t i = 0; i < suffixes.size(); ++i) {
        if (size_lt_unit(size, unit)) {
            suffix = &suffixes[i];
            break;
        }
        if (i + 1 < suffixes.size()) unit *= 1024;
    }
    const double value =
        static_cast<double>(1024) * static_cast<double>(size) / unit;
    return comma_fixed(value, precision) + separator + *suffix;
}

} // namespace rich::filesize_detail

namespace rich::filesize {

// rich.filesize.decimal — SI prefixes (1000 base).
inline std::string decimal(int64_t size, int precision = 1,
                           const std::string& separator = " ") {
    return filesize_detail::decimal(size, precision, separator.c_str());
}

// IEC prefixes (1024 base) — same _to_str semantics, not part of rich 15.0.0
// (rich.filesize only ships decimal); exposed for C++ parity with progress
// binary-unit columns.
inline std::string binary(int64_t size, int precision = 1,
                          const std::string& separator = " ") {
    return filesize_detail::binary(size, precision, separator.c_str());
}

// rich.filesize.pick_unit_and_suffix — unit as double (see header note).
inline std::pair<double, std::string>
pick_unit_and_suffix(int64_t size, const std::vector<std::string>& suffixes,
                     int64_t base) {
    return filesize_detail::pick_unit_and_suffix(size, suffixes, base);
}

} // namespace rich::filesize
