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

// Python Node.render: expand any line whose token stream exceeds max_width.
inline std::string render(const Node& root, size_t max_width, size_t indent_size = 4) {
    using detail::Line;
    std::vector<Line> lines(1);
    lines[0].node = &root;

    size_t line_no = 0;
    while (line_no < lines.size()) {
        const Line& line = lines[line_no];
        bool expandable = line.node && !line.node->children.empty();
        if (expandable && !line.node->check_length(detail::line_start_length(line), max_width)) {
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
std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>, std::string>
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

template <typename T>
Node to_node(const T& value);

template <typename T>
Node leaf(const T& value) {
    Node n;
    n.value_repr = scalar_repr(value);
    return n;
}

// std::optional → value or None (Python Optional repr).
template <typename T>
Node to_node(const std::optional<T>& opt) {
    return opt ? to_node(*opt) : leaf(std::nullptr_t{});
}

// Pairs render as 1-tuples ("(1, 'two')").
template <typename A, typename B>
Node to_node(const std::pair<A, B>& p) {
    Node n;
    n.open_brace = "(";
    n.close_brace = ")";
    n.is_tuple = true;
    n.children.push_back(to_node(p.first));
    n.children.push_back(to_node(p.second));
    return n;
}

// std::tuple → "(a, b, ...)".
template <typename... As>
Node to_node(const std::tuple<As...>& t) {
    Node n;
    n.open_brace = "(";
    n.close_brace = ")";
    n.is_tuple = true;
    std::apply([&n](const auto&... elems) {
        (n.children.push_back(to_node(elems)), ...);
    }, t);
    return n;
}

// Map-like containers (std::map, std::unordered_map): keys are repr'd into
// key_repr, values stay fully traversable.
template <typename M>
std::enable_if_t<has_mapped_type<M>::value && is_iterable<M>::value, Node>
container_node(const M& m) {
    Node n;
    n.open_brace = "{";
    n.close_brace = "}";
    n.empty = "{}";
    for (const auto& [key, val] : m) {
        Node child = to_node(val);
        child.key_repr = to_node(key).value_repr;
        n.children.push_back(std::move(child));
    }
    return n;
}

// Sequence containers: vector, list, deque, array, C arrays.
template <typename S>
std::enable_if_t<!has_mapped_type<S>::value && is_iterable<S>::value &&
                 !is_string<S>::value && !is_pair<S>::value && !is_tuple_type<S>::value,
                 Node>
container_node(const S& s) {
    Node n;
    n.open_brace = "[";
    n.close_brace = "]";
    n.empty = "[]";
    for (const auto& elem : s) n.children.push_back(to_node(elem));
    return n;
}

template <typename T>
Node to_node(const T& value) {
    if constexpr (is_iterable<T>::value && !is_string<T>::value &&
                  !is_pair<T>::value && !is_tuple_type<T>::value) {
        return container_node(value);
    } else if constexpr (is_pair<T>::value || is_tuple_type<T>::value) {
        return to_node(value); // pair/tuple overloads above
    } else {
        return leaf(value);
    }
}

} // namespace detail

// Public builder: any supported value → Node tree.
template <typename T>
Node to_node(const T& value) {
    return detail::to_node(value);
}

// Python pretty_repr(value, max_width): single line while it fits, expanded
// with a 4-space indent otherwise.
template <typename T>
std::string pretty_repr(const T& value, size_t max_width = 80) {
    Node root = to_node(value);
    return render(root, max_width);
}

// ── renderable ───────────────────────────────────────────────────────────────
// Usage: console.print(pretty(my_vector)); console.print(pretty(my_map));
// Note: Python's console.print auto-prettifies containers; C++ needs the
// explicit pretty() wrapper. Syntax highlighting (ReprHighlighter) is not
// ported — segments render unstyled.
namespace detail {
template <typename T>
struct PrettyValue {
    T value;
};

template <typename T>
inline std::vector<Segment> render_item(const PrettyValue<T>& p, const Console&,
                                        const Options& opt) {
    size_t width = (opt.width > 0) ? opt.width : 80;
    return {{pretty_repr(p.value, width), {}}};
}

template <typename T>
inline std::pair<size_t, size_t> measure_item(const PrettyValue<T>& p, const Console&,
                                              const Options& opt) {
    size_t width = (opt.width > 0) ? opt.width : 80;
    std::string repr = pretty_repr(p.value, width);
    size_t max_w = 0, lines = 1;
    size_t start = 0;
    for (size_t i = 0; i <= repr.size(); ++i) {
        if (i == repr.size() || repr[i] == '\n') {
            max_w = std::max(max_w, cell_len(std::string_view(repr).substr(start, i - start)));
            ++lines;
            start = i + 1;
        }
    }
    return {max_w, lines - 1};
}
} // namespace detail

template <typename T>
AnyRenderable pretty(const T& value) {
    return AnyRenderable(detail::PrettyValue<std::decay_t<T>>{value});
}

} // namespace rich::pretty
