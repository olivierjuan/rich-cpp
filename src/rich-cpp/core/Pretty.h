#pragma once
// Pretty-printer for C++ containers (port of Python Rich's rich.pretty for
// the data types C++ can introspect: sequences, maps, pairs/tuples, optionals
// and scalars).
//
// Python parity notes:
//   - Node/_Line/render below port rich.pretty.Node.check_length/expand — a
//     container stays on one line while its token stream fits within the
//     console width, otherwise it expands: open brace on its own line, each
//     child indented +4 spaces with a "," suffix (dropped on the last child),
//     closing brace on its own line carrying the parent's suffix.
//   - Scalars/strings repr like Python: True/False/None, str(float), single-
//     quoted strings. (C++ has no runtime introspection, so class/struct
//     objects and syntax highlighting are not ported.)
//   - bool maps to Python's True/False and nullptr to None since this library
//     targets Rich output parity.
#include <rich_core.h>
#include "Repr.h"
#include "Text.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace rich::pretty {

using repr::cell_len;
using repr::py_repr_string;
using repr::py_str_float;

// Python Pretty options (pretty.Pretty constructor; None → 0 here).
// Defaults match Python: no abbreviation, 4-space indent, no guides.
struct PrettyOptions {
    size_t max_length{0};   // max items per container before "... +N"
    size_t max_string{0};   // max string length before '...' + N truncation
    size_t max_depth{0};    // containers at depth >= max_depth render as [..]
    size_t indent_size{4};
    bool expand_all{false};
    bool indent_guides{false};
    bool no_wrap{false};
    size_t margin{0};       // subtracted from width for the expand decision
    size_t max_width{80};   // for standalone pretty_repr (console print uses
                            // the console width)
};

namespace detail {

// Codepoint count / slice for max_string (Python uses len(str) = characters).
inline size_t cp_count(std::string_view s) {
    size_t n = 0;
    for (unsigned char c : s)
        if (c < 0x80 || c >= 0xC0) ++n;
    return n;
}

inline std::string cp_substr(std::string_view s, size_t n) {
    size_t i = 0, taken = 0;
    while (i < s.size() && taken < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if (c >= 0xF0)
            len = 4;
        else if (c >= 0xE0)
            len = 3;
        else if (c >= 0xC0)
            len = 2;
        i += (i + len <= s.size()) ? len : s.size() - i;
        ++taken;
    }
    return std::string(s.substr(0, i));
}

} // namespace detail

// ── repr tree ────────────────────────────────────────────────────────────────
struct Node {
    std::string key_repr;    // dict key repr ("" for sequence children)
    std::string value_repr;  // atomic leaf repr ("" when children are used)
    std::string open_brace;
    std::string close_brace;
    std::string empty;       // repr of an empty container ("[]", "{}", ...)
    bool is_tuple = false;   // single-element tuples keep a trailing ","
    std::vector<Node> children;

    // Python key_separator ": ", separator ", " (fixed for these types).
    void iter_tokens(std::string& out) const {
        if (!key_repr.empty()) {
            out += key_repr;
            out += ": ";
        }
        if (!value_repr.empty()) {
            out += value_repr;
        } else if (!children.empty()) {
            out += open_brace;
            if (is_tuple && children.size() == 1) {
                children[0].iter_tokens(out);
                out += ",";
            } else {
                for (size_t i = 0; i < children.size(); ++i) {
                    children[i].iter_tokens(out);
                    if (i + 1 < children.size()) out += ", ";
                }
            }
            out += close_brace;
        } else {
            out += empty;
        }
    }

    // Python Node.check_length: cumulative token cells must stay <= max_width.
    bool check_length(size_t start_length, size_t max_width) const {
        std::string acc;
        iter_tokens(acc);
        return start_length + cell_len(acc) <= max_width;
    }
};

namespace detail {

struct Line {
    const Node* node = nullptr;
    std::string whitespace;
    std::string text;   // open/close brace text; empty for child item lines
    std::string suffix; // separator carried by the line
    bool last = false;
};

inline size_t line_start_length(const Line& l) {
    return l.whitespace.size() + cell_len(l.text) + cell_len(l.suffix);
}

inline std::string line_str(const Line& l) {
    std::string out = l.whitespace + l.text;
    if (l.node) l.node->iter_tokens(out);
    if (!l.last && !l.suffix.empty()) {
        // Python: suffix.rstrip() on every non-last line.
        std::string suffix = l.suffix;
        while (!suffix.empty() && suffix.back() == ' ') suffix.pop_back();
        out += suffix;
    }
    return out;
}

} // namespace detail

// Python Node.render: expand any line whose token stream exceeds max_width
// (or everything when expand_all).
inline std::string render(const Node& root, size_t max_width, size_t indent_size = 4,
                          bool expand_all = false) {
    using detail::Line;
    std::vector<Line> lines(1);
    lines[0].node = &root;

    size_t line_no = 0;
    while (line_no < lines.size()) {
        const Line& line = lines[line_no];
        bool expandable = line.node && !line.node->children.empty();
        if (expandable &&
            (expand_all || !line.node->check_length(detail::line_start_length(line), max_width))) {
            std::vector<Line> repl;
            Line open;
            open.whitespace = line.whitespace;
            open.text = line.node->key_repr.empty()
                            ? line.node->open_brace
                            : line.node->key_repr + ": " + line.node->open_brace;
            repl.push_back(std::move(open));

            std::string child_ws = line.whitespace + std::string(indent_size, ' ');
            bool tuple_of_one = line.node->is_tuple && line.node->children.size() == 1;
            for (size_t c = 0; c < line.node->children.size(); ++c) {
                Line child;
                child.node = &line.node->children[c];
                child.whitespace = child_ws;
                child.suffix = tuple_of_one ? "," : ", ";
                child.last = (c + 1 == line.node->children.size()) && !tuple_of_one;
                repl.push_back(std::move(child));
            }

            Line close;
            close.whitespace = line.whitespace;
            close.text = line.node->close_brace;
            close.suffix = line.suffix;
            close.last = line.last;
            repl.push_back(std::move(close));

            lines.erase(lines.begin() + static_cast<long>(line_no));
            lines.insert(lines.begin() + static_cast<long>(line_no),
                         std::make_move_iterator(repl.begin()),
                         std::make_move_iterator(repl.end()));
        }
        ++line_no;
    }

    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) out += '\n';
        out += detail::line_str(lines[i]);
    }
    return out;
}

// ── traversal (Python traverse() for the portable subset) ───────────────────
namespace detail {

template <typename T> struct is_string : std::false_type {};
template <> struct is_string<std::string> : std::true_type {};
template <> struct is_string<std::string_view> : std::true_type {};
template <> struct is_string<const char*> : std::true_type {};
template <> struct is_string<char*> : std::true_type {};

template <typename T> struct is_pair : std::false_type {};
template <typename A, typename B> struct is_pair<std::pair<A, B>> : std::true_type {};

template <typename T> struct is_tuple_type : std::false_type {};
template <typename... As> struct is_tuple_type<std::tuple<As...>> : std::true_type {};

template <typename T, typename = void>
struct has_mapped_type : std::false_type {};
template <typename T>
struct has_mapped_type<T, std::void_t<typename T::mapped_type>> : std::true_type {};

template <typename T, typename = void>
struct is_iterable : std::false_type {};
template <typename T>
struct is_iterable<T, std::void_t<decltype(std::declval<const T&>().begin()),
                                  decltype(std::declval<const T&>().end())>> : std::true_type {};

// Scalar reprs — Python repr parity.
template <typename T>
std::enable_if_t<std::is_integral_v<T> &&
                     !std::is_same_v<T, bool> && !std::is_same_v<T, char>,
                 std::string>
scalar_repr(T v) {
    return std::to_string(v);
}
inline std::string scalar_repr(bool v) { return v ? "True" : "False"; }
inline std::string scalar_repr(char v) { return py_repr_string(std::string(1, v)); }
inline std::string scalar_repr(double v) { return py_str_float(v); }
inline std::string scalar_repr(float v) { return py_str_float(static_cast<double>(v)); }
inline std::string scalar_repr(std::nullptr_t) { return "None"; }
inline std::string scalar_repr(const std::string& v) { return py_repr_string(v); }
inline std::string scalar_repr(std::string_view v) { return py_repr_string(v); }
inline std::string scalar_repr(const char* v) { return v ? py_repr_string(v) : "None"; }

inline std::string_view string_view_of(const std::string& s) { return s; }
inline std::string_view string_view_of(std::string_view s) { return s; }
inline std::string_view string_view_of(const char* s) { return s ? std::string_view(s) : std::string_view(); }

template <typename T>
Node build_node(const T& value, const PrettyOptions& o, size_t depth);

template <typename T>
Node leaf(const T& value, const PrettyOptions& o) {
    Node n;
    n.value_repr = scalar_repr(value);
    // Python traverse.to_repr: len(obj) > max_string → repr(obj[:max_string]) + "+N"
    if constexpr (is_string<std::decay_t<T>>::value) {
        if (o.max_string != 0) {
            const std::string_view sv = string_view_of(value);
            const size_t ncp = cp_count(sv);
            if (ncp > o.max_string)
                n.value_repr = py_repr_string(cp_substr(sv, o.max_string)) +
                               "+" + std::to_string(ncp - o.max_string);
        }
    }
    return n;
}

// std::optional → value or None (Python Optional repr). Not a container, so
// no depth increment.
template <typename T>
Node build_node(const std::optional<T>& opt, const PrettyOptions& o, size_t depth) {
    return opt ? build_node(*opt, o, depth) : leaf(std::nullptr_t{}, o);
}

// Pairs render as 1-tuples ("(1, 'two')").
template <typename A, typename B>
Node build_node(const std::pair<A, B>& p, const PrettyOptions& o, size_t depth) {
    Node n;
    n.open_brace = "(";
    n.close_brace = ")";
    n.is_tuple = true;
    if (o.max_depth != 0 && depth >= o.max_depth) {
        n.value_repr = "(...)";
        return n;
    }
    n.children.push_back(build_node(p.first, o, depth + 1));
    n.children.push_back(build_node(p.second, o, depth + 1));
    return n;
}

// std::tuple → "(a, b, ...)".
template <typename... As>
Node build_node(const std::tuple<As...>& t, const PrettyOptions& o, size_t depth) {
    Node n;
    n.open_brace = "(";
    n.close_brace = ")";
    n.is_tuple = true;
    if (o.max_depth != 0 && depth >= o.max_depth) {
        n.value_repr = "(...)";
        return n;
    }
    std::apply([&n, &o, depth](const auto&... elems) {
        (n.children.push_back(build_node(elems, o, depth + 1)), ...);
    }, t);
    return n;
}

// Python traverse: islice(items, max_length) + trailing "... +N" node.
inline void append_truncation(Node& n, size_t count, size_t max_length) {
    if (count > max_length) {
        Node more;
        more.value_repr = "... +" + std::to_string(count - max_length);
        n.children.push_back(std::move(more));
    }
}

// Map-like containers (std::map, std::unordered_map): keys are repr'd into
// key_repr, values stay fully traversable.
template <typename M>
std::enable_if_t<has_mapped_type<M>::value && is_iterable<M>::value, Node>
container_node(const M& m, const PrettyOptions& o, size_t depth) {
    Node n;
    n.open_brace = "{";
    n.close_brace = "}";
    n.empty = "{}";
    if (o.max_depth != 0 && depth >= o.max_depth) {
        n.value_repr = "{...}";
        return n;
    }
    size_t count = 0;
    for (const auto& [key, val] : m) {
        if (o.max_length == 0 || count < o.max_length) {
            Node child = build_node(val, o, depth + 1);
            child.key_repr = build_node(key, o, depth + 1).value_repr;
            n.children.push_back(std::move(child));
        }
        ++count;
    }
    if (o.max_length != 0) append_truncation(n, count, o.max_length);
    return n;
}

// Sequence containers: vector, list, deque, array, C arrays.
template <typename S>
std::enable_if_t<!has_mapped_type<S>::value && is_iterable<S>::value &&
                 !is_string<S>::value && !is_pair<S>::value && !is_tuple_type<S>::value,
                 Node>
container_node(const S& s, const PrettyOptions& o, size_t depth) {
    Node n;
    n.open_brace = "[";
    n.close_brace = "]";
    n.empty = "[]";
    if (o.max_depth != 0 && depth >= o.max_depth) {
        n.value_repr = "[...]";
        return n;
    }
    size_t count = 0;
    for (const auto& elem : s) {
        if (o.max_length == 0 || count < o.max_length)
            n.children.push_back(build_node(elem, o, depth + 1));
        ++count;
    }
    if (o.max_length != 0) append_truncation(n, count, o.max_length);
    return n;
}

template <typename T>
Node build_node(const T& value, const PrettyOptions& o, size_t depth) {
    if constexpr (is_iterable<T>::value && !is_string<T>::value &&
                  !is_pair<T>::value && !is_tuple_type<T>::value) {
        return container_node(value, o, depth);
    } else if constexpr (is_pair<T>::value || is_tuple_type<T>::value) {
        return build_node(value, o, depth); // pair/tuple overloads above
    } else {
        return leaf(value, o);
    }
}

} // namespace detail

// Public builder: any supported value → Node tree.
template <typename T>
Node to_node(const T& value, const PrettyOptions& o = {}, size_t depth = 0) {
    return detail::build_node(value, o, depth);
}

// Python pretty_repr(value, max_width, ..., max_length, max_string,
// max_depth, expand_all): single line while it fits, expanded with a
// 4-space indent otherwise.
template <typename T>
std::string pretty_repr(const T& value, const PrettyOptions& o = {}) {
    Node root = detail::build_node(value, o, 0);
    return render(root, o.max_width, o.indent_size, o.expand_all);
}

// Backwards-compatible width-only overload.
template <typename T>
std::string pretty_repr(const T& value, size_t max_width) {
    PrettyOptions o;
    o.max_width = max_width;
    return pretty_repr(value, o);
}

// ── ReprHighlighter ─────────────────────────────────────────────────────────
// Port of rich.highlighter.ReprHighlighter for the token classes this
// library's reprs contain. Python runs each `highlights` entry as a separate
// regex pass and merges overlapping spans in Text.render by stack order
// (earlier pass = lower id = lower precedence). Pass order: brace
// `[][{}()]`, then the combined pass
// (ipv4/ipv6/eui/uuid/call/bools/ellipsis/numbers/path/str/url).
// On this output alphabet the combined alternatives reachable outside string
// literals are bool/none, ellipsis, number, str (call matches only the
// identifier before '(', which never precedes a bare paren here; the
// hex/dot-slash/url classes cannot occur outside quoted strings, and quoted
// strings are consumed whole by the str alternative).
// A brace inside a str span therefore keeps the str style (str pass is later
// in the stack and wins per attribute).
namespace detail {

inline bool is_word_ch(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_';
}
inline bool is_digit_ch(char c) { return c >= '0' && c <= '9'; }
inline bool is_hex_ch(char c) {
    return is_digit_ch(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// \bTrue\b / \bFalse\b / \bNone\b — word boundaries both sides.
inline size_t word_end(std::string_view t, size_t pos, std::string_view w) {
    if (pos > 0 && is_word_ch(t[pos - 1])) return std::string_view::npos;
    if (t.compare(pos, w.size(), w) != 0) return std::string_view::npos;
    const size_t e = pos + w.size();
    return (e < t.size() && is_word_ch(t[e])) ? std::string_view::npos : e;
}

// (?<!\w)\-?[0-9]+\.?[0-9]*(e[-+]?\d+?)?\b  |  0x[0-9a-fA-F]*
inline size_t number_end(std::string_view t, size_t pos) {
    // 0x[0-9a-fA-F]* has no lookbehind in the Python regex.
    if (pos + 1 < t.size() && t[pos] == '0' && t[pos + 1] == 'x') {
        size_t k = pos + 2;
        while (k < t.size() && is_hex_ch(t[k])) ++k;
        return k;
    }
    if (pos > 0 && is_word_ch(t[pos - 1])) return std::string_view::npos;
    size_t i = pos;
    if (i < t.size() && t[i] == '-') ++i;
    if (i >= t.size() || !is_digit_ch(t[i])) return std::string_view::npos;
    while (i < t.size() && is_digit_ch(t[i])) ++i;
    const size_t after_int = i;
    if (i < t.size() && t[i] == '.') {
        ++i;
        while (i < t.size() && is_digit_ch(t[i])) ++i;
    }
    const size_t after_frac = i;
    if (i < t.size() && t[i] == 'e') {
        size_t j = i + 1;
        if (j < t.size() && (t[j] == '+' || t[j] == '-')) ++j;
        if (j < t.size() && is_digit_ch(t[j])) {
            while (j < t.size() && is_digit_ch(t[j])) ++j;
            i = j;
        }
    }
    const auto boundary = [&](size_t k) { return k >= t.size() || !is_word_ch(t[k]); };
    // Regex backtracking order: exponent, then fraction, then integer part.
    if (boundary(i)) return i;
    if (i != after_int && boundary(after_frac)) return after_frac;
    if (boundary(after_int)) return after_int;
    return std::string_view::npos;
}

// (?<![\\\w])(b?'''.*?(?<!\\)''' | b?'.*?(?<!\\)' | b?\"\"\".*?(?<!\\)\"\"\" |
//               b?\".*?(?<!\\)\") — no DOTALL (compiled without it).
inline size_t str_end(std::string_view t, size_t pos) {
    if (pos > 0 && (t[pos - 1] == '\\' || is_word_ch(t[pos - 1]))) return std::string_view::npos;
    size_t q = pos;
    if (q < t.size() && t[q] == 'b') {
        if (q + 1 >= t.size() || (t[q + 1] != '\'' && t[q + 1] != '"'))
            return std::string_view::npos;
        ++q;
    }
    auto close_at = [&](char quote, bool triple) -> size_t {
        size_t i;
        if (triple) {
            if (q + 2 >= t.size() || t[q + 1] != quote || t[q + 2] != quote)
                return std::string_view::npos;
            i = q + 3;
            while (i < t.size()) {
                if (t[i] == '\n') return std::string_view::npos;
                if (t[i] == quote && t[i - 1] != '\\' && i + 2 < t.size() &&
                    t[i + 1] == quote && t[i + 2] == quote)
                    return i + 3;
                ++i;
            }
            return std::string_view::npos;
        }
        if (q >= t.size() || t[q] != quote) return std::string_view::npos;
        i = q + 1;
        while (i < t.size()) {
            if (t[i] == '\n') return std::string_view::npos;
            if (t[i] == quote && t[i - 1] != '\\') return i + 1;
            ++i;
        }
        return std::string_view::npos;
    };
    // Python alternative order: b?''', b?', b?""", b?"
    size_t e = close_at('\'', true);
    if (e != std::string_view::npos) return e;
    e = close_at('\'', false);
    if (e != std::string_view::npos) return e;
    e = close_at('"', true);
    if (e != std::string_view::npos) return e;
    e = close_at('"', false);
    return e;
}

enum class ReprKind { Brace, True, False_, None_, Ellipsis, Number, Str };
struct ReprSpan {
    size_t start;
    size_t end;
    ReprKind kind;
};

inline std::vector<ReprSpan> scan_repr(std::string_view t) {
    std::vector<ReprSpan> spans;
    // Pass 1: braces.
    for (size_t i = 0; i < t.size(); ++i) {
        const char c = t[i];
        if (c == '[' || c == ']' || c == '{' || c == '}' || c == '(' || c == ')')
            spans.push_back({i, i + 1, ReprKind::Brace});
    }
    // Pass 2: combined, leftmost-first, Python alternative order.
    size_t pos = 0;
    while (pos < t.size()) {
        size_t e = word_end(t, pos, "True");
        if (e != std::string_view::npos) {
            spans.push_back({pos, e, ReprKind::True});
            pos = e;
            continue;
        }
        e = word_end(t, pos, "False");
        if (e != std::string_view::npos) {
            spans.push_back({pos, e, ReprKind::False_});
            pos = e;
            continue;
        }
        e = word_end(t, pos, "None");
        if (e != std::string_view::npos) {
            spans.push_back({pos, e, ReprKind::None_});
            pos = e;
            continue;
        }
        if (t.compare(pos, 3, "...") == 0) {
            spans.push_back({pos, pos + 3, ReprKind::Ellipsis});
            pos += 3;
            continue;
        }
        e = number_end(t, pos);
        if (e != std::string_view::npos) {
            spans.push_back({pos, e, ReprKind::Number});
            pos = e;
            continue;
        }
        e = str_end(t, pos);
        if (e != std::string_view::npos) {
            spans.push_back({pos, e, ReprKind::Str});
            pos = e;
            continue;
        }
        ++pos;
    }
    return spans;
}

// repr.* default styles (default_styles.py). Explicit bits mark the
// attributes Python's Style(bold=True, ...) constructor records, which
// style_sgr emits even when the underlying bool is false.
inline const Style& repr_style(ReprKind k) {
    switch (k) {
        case ReprKind::Brace: {
            // Style(bold=True)
            static const Style s{.bold = true, .explicit_bits = 0x0001};
            return s;
        }
        case ReprKind::True: {
            // Style(color="bright_green", italic=True)
            static const Style s{.italic = true, .color = Color::from_ansi(92),
                                 .explicit_bits = 0x0004};
            return s;
        }
        case ReprKind::False_: {
            // Style(color="bright_red", italic=True)
            static const Style s{.italic = true, .color = Color::from_ansi(91),
                                 .explicit_bits = 0x0004};
            return s;
        }
        case ReprKind::None_: {
            // Style(color="magenta", italic=True)
            static const Style s{.italic = true, .color = Color::from_ansi(35),
                                 .explicit_bits = 0x0004};
            return s;
        }
        case ReprKind::Ellipsis: {
            // Style(color="yellow")
            static const Style s{.color = Color::from_ansi(33)};
            return s;
        }
        case ReprKind::Number: {
            // Style(color="cyan", bold=True, italic=False)
            static const Style s{.bold = true, .color = Color::from_ansi(36),
                                 .explicit_bits = 0x0001 | 0x0004};
            return s;
        }
        case ReprKind::Str: {
            // Style(color="green", italic=False, bold=False)
            static const Style s{.color = Color::from_ansi(32),
                                 .explicit_bits = 0x0001 | 0x0004};
            return s;
        }
    }
    static const Style empty{};
    return empty;
}

// Text.render span-event walk: enter-before-leave at equal offsets, stable
// order, later pass id wins per attribute. Gaps carry the base style, which
// for Pretty is Style() (default_styles: pretty → Style()).
inline std::vector<Segment> emit_repr_segments(std::string_view text) {
    const auto spans = scan_repr(text);
    struct Ev {
        size_t off;
        bool leaving;
        size_t id;
    };
    std::vector<Ev> evs;
    evs.reserve(spans.size() * 2);
    for (size_t i = 0; i < spans.size(); ++i) {
        evs.push_back({spans[i].start, false, i});
        evs.push_back({spans[i].end, true, i});
    }
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
        if (a.off != b.off) return a.off < b.off;
        return !a.leaving && b.leaving;
    });
    std::vector<Segment> out;
    int brace_depth = 0; // adjacent braces overlap; count active brace spans
    int comb_id = -1;    // combined-pass spans never overlap each other
    size_t prev = 0;
    for (size_t k = 0; k < evs.size(); ++k) {
        const Ev& e = evs[k];
        const ReprSpan& sp = spans[e.id];
        const bool combined = sp.kind != ReprKind::Brace;
        if (e.leaving) {
            if (combined)
                comb_id = -1;
            else
                --brace_depth;
        } else {
            if (combined)
                comb_id = static_cast<int>(e.id);
            else
                ++brace_depth;
        }
        const size_t next_off = (k + 1 < evs.size()) ? evs[k + 1].off : text.size();
        if (next_off > prev) {
            const Style& st =
                comb_id >= 0 ? repr_style(spans[static_cast<size_t>(comb_id)].kind)
                             : brace_depth > 0 ? repr_style(ReprKind::Brace) : Style{};
            out.push_back({std::string(text.substr(prev, next_off - prev)), st});
            prev = next_off;
        }
    }
    if (prev < text.size()) out.push_back({std::string(text.substr(prev)), Style{}});
    return out;
}

// Python Text.with_indent_guides (Pretty passes indent_size, character="│",
// style="repr.indent" = green dim). Each line's leading spaces are replaced
// by guide units of the same length; the leading whitespace of a line always
// sits at the tail (after the last '\n') of a gap segment.
inline void apply_indent_guides(std::vector<Segment>& segs, size_t indent_size) {
    if (indent_size == 0) return;
    std::string unit;
    unit += "\xE2\x94\x82"; // │
    unit.append(indent_size - 1, ' ');
    const Style guide{.dim = true, .color = Color::from_ansi(32),
                      .explicit_bits = 0x0002}; // repr.indent: green, dim=True
    for (size_t i = 0; i < segs.size(); ++i) {
        Segment& s = segs[i];
        const size_t nl = s.text.rfind('\n');
        if (nl == std::string::npos) continue;
        const size_t tail_len = s.text.size() - nl - 1;
        if (tail_len == 0) continue;
        bool all_sp = true;
        for (size_t k = nl + 1; k < s.text.size(); ++k)
            if (s.text[k] != ' ') {
                all_sp = false;
                break;
            }
        if (!all_sp) continue;
        std::string new_tail;
        for (size_t k = 0; k < tail_len / indent_size; ++k) new_tail += unit;
        new_tail.append(tail_len % indent_size, ' ');
        if (nl > 0) {
            Segment head{s.text.substr(0, nl + 1), Style{}};
            s = std::move(head);
            segs.insert(segs.begin() + static_cast<long>(i + 1), Segment{new_tail, guide});
        } else {
            s.text = '\n' + new_tail;
            s.style = guide;
        }
    }
}

} // namespace detail

// ── renderable ───────────────────────────────────────────────────────────────
// Usage: console.print(pretty(my_vector)); console.print(pretty(my_map));
// Python's console.print auto-prettifies containers (Pretty defaults:
// indent_guides=False, margin=0, no_wrap=False); C++ needs the explicit
// pretty() wrapper. The text is styled by the ReprHighlighter port above,
// wrapped at the console width (fold overflow) like plain Text, and carries
// its own trailing newline (end="\n" → style None).
namespace detail {
template <typename T>
struct PrettyValue {
    T value;
    PrettyOptions opts;
};

template <typename T>
inline std::vector<Segment> render_item(const PrettyValue<T>& p, const Console&,
                                        const Options& opt) {
    const PrettyOptions& o = p.opts;
    const size_t width = (opt.width > 0) ? opt.width : 80;
    const size_t max_width = width > o.margin ? width - o.margin : 0;
    Node root = build_node(p.value, o, 0);
    const std::string repr = render(root, max_width, o.indent_size, o.expand_all);
    auto segs = emit_repr_segments(repr);
    if (o.indent_guides) apply_indent_guides(segs, o.indent_size);
    std::vector<Segment> out;
    if (o.no_wrap) {
        out = std::move(segs);
    } else {
        out = text::wrap_detail::wrap_styled(std::move(segs), width, text::Overflow::Fold,
                                             rich::detail::kNoneStyle());
    }
    out.push_back({"\n", rich::detail::kNoneStyle()});
    return out;
}

template <typename T>
inline std::pair<size_t, size_t> measure_item(const PrettyValue<T>& p, const Console&,
                                              const Options& opt) {
    // Python Pretty.__rich_measure__: (widest line cells, same) of the
    // repr at options.max_width (no margin).
    const size_t width = (opt.width > 0) ? opt.width : 80;
    PrettyOptions o = p.opts;
    o.max_width = width;
    Node root = build_node(p.value, o, 0);
    const std::string repr = render(root, width, o.indent_size, o.expand_all);
    size_t max_w = 0;
    size_t start = 0;
    for (size_t i = 0; i <= repr.size(); ++i) {
        if (i == repr.size() || repr[i] == '\n') {
            max_w = std::max(max_w, cell_len(std::string_view(repr).substr(start, i - start)));
            start = i + 1;
        }
    }
    return {max_w, max_w};
}
} // namespace detail

template <typename T>
AnyRenderable pretty(const T& value) {
    return AnyRenderable(detail::PrettyValue<std::decay_t<T>>{value, {}});
}

template <typename T>
AnyRenderable pretty(const T& value, const PrettyOptions& o) {
    return AnyRenderable(detail::PrettyValue<std::decay_t<T>>{value, o});
}

} // namespace rich::pretty
