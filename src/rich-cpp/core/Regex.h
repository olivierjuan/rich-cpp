#pragma once
// Minimal Python-`re`-compatible regex engine — the subset needed by
// rich's highlighter patterns (named groups, negative lookbehind,
// conditionals, anchors, quantifiers) and typical user
// RegexHighlighter rules.
//
// Supported syntax (Python re subset):
//   literals; escapes: \n \t \r \f \v \a \0, \xHH \uHHHH \UHHHHHHHH,
//     octal \NNN, \cX, \\ and any other escaped char (literal);
//     outside classes: \b \B \w \W \d \D \s \S \A \Z;
//   classes: [...] with negation [^...], ranges a-z, shorthands
//     \w \d \s (union semantics, Python-style);
//   groups: (...) (?P<name>...) (?:...) (?=...) (?!...) (?<=...) (?<!...)
//   conditionals: (?(name)yes|no) — Python semantics (group participation);
//   inline flag: (?i) (ASCII case folding, applies from that point on);
//   quantifiers: * + ? {n} {n,} {n,m}, each optionally lazy with a trailing ?;
//   alternation |, anchors ^ $ (no MULTILINE), . (excludes \n).
//
// Matching: backtracking with leftmost (finditer) semantics, same as
// Python re. Unsupported syntax throws std::invalid_argument (Python:
// re.error).
//
// Unicode: patterns and text operate on codepoints. \w approximates
// Python's Unicode word set (ASCII + common letter blocks incl. CJK);
// \s is the ASCII whitespace set.
#include <rich_export.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../unicode/Unicode.h"

namespace rich::regex {

// Python re \w (UNICODE default): [a-zA-Z0-9_] plus Unicode letters and
// decimal digits — approximated by ASCII plus common letter blocks.
inline bool is_word_cp(char32_t cp) {
    if (cp >= '0' && cp <= '9') return true;
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) return true;
    if (cp == '_') return true;
    auto in = [&cp](uint32_t lo, uint32_t hi) { return cp >= lo && cp <= hi; };
    return in(0x00C0, 0x024F) || in(0x0370, 0x03FF) || in(0x0400, 0x04FF) ||
           in(0x0500, 0x052F) || in(0x0531, 0x0556) || in(0x0559, 0x055F) ||
           in(0x0590, 0x05BF) || in(0x05C1, 0x05C4) || in(0x05D0, 0x05EA) ||
           in(0x0600, 0x06FF) || in(0x0700, 0x074F) || in(0x0900, 0x097F) ||
           in(0x0980, 0x09FF) || in(0x0A00, 0x0A7F) || in(0x0A80, 0x0BFF) ||
           in(0x0C00, 0x0C7F) || in(0x0C80, 0x0D7F) || in(0x0D80, 0x0DFF) ||
           in(0x0E01, 0x0E7F) || in(0x0E81, 0x0EFF) || in(0x1000, 0x109F) ||
           in(0x10A0, 0x10FF) || in(0x1100, 0x11FF) || in(0x1200, 0x137F) ||
           in(0x13A0, 0x13FF) || in(0x1400, 0x167F) || in(0x1700, 0x171F) ||
           in(0x1720, 0x173F) || in(0x1740, 0x175F) || in(0x1760, 0x177F) ||
           in(0x1780, 0x17DD) || in(0x17E0, 0x17EF) || in(0x17F0, 0x17FF) ||
           in(0x1800, 0x18AF) || in(0x1900, 0x194F) || in(0x1950, 0x197F) ||
           in(0x1980, 0x19DF) || in(0x1A00, 0x1A1F) || in(0x1A20, 0x1AAF) ||
           in(0x1B00, 0x1B7F) || in(0x1B80, 0x1BBF) || in(0x1C00, 0x1C4F) ||
           in(0x1C50, 0x1C7F) || in(0x1D00, 0x1DBF) || in(0x1DC0, 0x1DFF) ||
           in(0x1E00, 0x1EFF) || in(0x2C00, 0x2C7F) || in(0x2C80, 0x2CFF) ||
           in(0x2D30, 0x2D7F) || in(0x2D80, 0x2DFF) || in(0x2F00, 0x2FDF) ||
           in(0x3041, 0x309F) || in(0x30A1, 0x30FF) || in(0x3105, 0x312F) ||
           in(0x3131, 0x318E) || in(0x31A0, 0x31BF) || in(0x31F0, 0x31FF) ||
           in(0x3400, 0x4DBF) || in(0x4E00, 0x9FFF) || in(0xA000, 0xA4CF) ||
           in(0xA500, 0xA69F) || in(0xA6A0, 0xA6FF) || in(0xA700, 0xA7CF) ||
           in(0xA7F0, 0xA82F) || in(0xA840, 0xA87F) || in(0xA880, 0xA8FF) ||
           in(0xA900, 0xA92F) || in(0xA930, 0xA95F) || in(0xA960, 0xA97F) ||
           in(0xA980, 0xA9DF) || in(0xA9E0, 0xA9FF) || in(0xAA00, 0xAAEF) ||
           in(0xAAF0, 0xAABF) || in(0xAB01, 0xAB6F) || in(0xFB1D, 0xFB4F) ||
           in(0xFE00, 0xFE4F) || in(0xFF41, 0xFF5A) || in(0xFF66, 0xFFBE) ||
           in(0xFFC2, 0xFFC9) || in(0xFFCA, 0xFFCB) || in(0xFFD2, 0xFFD7) ||
           in(0xFFDA, 0xFFDB);
}

inline bool is_space_cp(char32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' ||
           cp == '\v';
}

inline char32_t fold_cp(char32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + ('a' - 'A');
    return cp;
}

namespace detail {

struct Range {
    char32_t lo{0};
    char32_t hi{0};
};

enum class Kind {
    Char,     // ch
    Class,    // ranges / word / space, negated
    Dot,
    Seq,      // items
    Alt,      // items (branches, each a Seq)
    Repeat,   // items[0], min, max (-1 = unlimited), greedy
    Anchor,   // anchor: 0 = ^, 1 = $, 2 = \b, 3 = \B
    LookBehind, // items[0] (Seq), negated
    LookAhead,  // items[0] (Seq), negated
    Cond,     // name, items[0] = body (Seq or Alt)
    Group,    // capture, group_index, name, items[0] (Seq)
};

struct Node {
    Kind kind{Kind::Seq};
    char32_t ch{0};
    bool negated{false};
    bool greedy{true};
    int min{0};
    int max{-1};
    int anchor{-1};
    bool capture{false};
    int group_index{-1};
    std::string name; // named-group name; Cond: condition name
    bool word_class{false};
    bool space_class{false};
    bool word_neg{false};
    bool space_neg{false};
    std::vector<Range> ranges;
    std::vector<std::unique_ptr<Node>> items;
};

inline std::unique_ptr<Node> make_node(Kind k) {
    auto n = std::make_unique<Node>();
    n->kind = k;
    return n;
}

struct Parser {
    std::string_view s;
    size_t i{0};
    int group_count{0};
    bool icase{false};
    std::vector<std::string> names; // named groups in definition order
    std::vector<int> name_group_indexes; // names[k] -> its group_index

    [[noreturn]] void fail() const {
        throw std::invalid_argument(std::string("invalid regex pattern at ") +
                                    std::to_string(i) + ": " +
                                    std::string(s.substr(i, (s.size() - i) < 32 ? s.size() - i : 32)));
    }

    static bool is_digit(char c) { return c >= '0' && c <= '9'; }

    // --- top level ---
    std::vector<std::unique_ptr<Node>> parse_pattern() {
        // Reuse the group-body logic for top-level alternation, then wrap
        // the result in a Seq (root_ is always a plain item list).
        auto body = parse_group_body();
        if (i < s.size()) fail(); // stray ')' or '|'
        std::vector<std::unique_ptr<Node>> items;
        if (body->kind == Kind::Seq && body->items.size() == 1) {
            items.push_back(std::move(body->items[0]));
        } else {
            items.push_back(std::move(body));
        }
        return items;
    }

    // Parse atoms until '|' (branch boundary) or end; '(' groups handle
    // their own closing.
    void parse_atoms(std::vector<std::unique_ptr<Node>>& items) {
        for (;;) {
            if (i >= s.size() || s[i] == '|' || s[i] == ')') return;
            auto atom = parse_atom();
            if (atom) items.push_back(std::move(atom));
        }
    }

    // A branch body: atoms plus any '|'-separated alternatives. Returns a
    // Seq (single branch) or Alt node. Stops before the closing ')'
    // (caller consumes it) or at end of pattern.
    std::unique_ptr<Node> parse_group_body() {
        std::vector<std::unique_ptr<Node>> first;
        parse_atoms(first);
        if (i >= s.size() || s[i] != '|') return finish_seq(std::move(first));
        auto alt = make_node(Kind::Alt);
        alt->items.push_back(finish_seq(std::move(first)));
        while (i < s.size() && s[i] == '|') {
            ++i;
            std::vector<std::unique_ptr<Node>> more;
            parse_atoms(more);
            alt->items.push_back(finish_seq(std::move(more)));
        }
        return alt;
    }

    static std::unique_ptr<Node> finish_seq(std::vector<std::unique_ptr<Node>> items) {
        auto seq = make_node(Kind::Seq);
        seq->items = std::move(items);
        return seq;
    }

    std::unique_ptr<Node> parse_atom() {
        auto n = parse_unquantified();
        if (!n) return nullptr; // inline flag consumed, no atom
        if (i < s.size()) {
            char c = s[i];
            if (c == '*' || c == '+' || c == '?') {
                int mn = 0, mx = -1;
                if (c == '*') { mn = 0; mx = -1; }
                else if (c == '+') { mn = 1; mx = -1; }
                else { mn = 0; mx = 1; }
                ++i;
                bool gr = true;
                if (i < s.size() && s[i] == '?') { gr = false; ++i; }
                auto rep = make_node(Kind::Repeat);
                rep->min = mn;
                rep->max = mx;
                rep->greedy = gr;
                rep->items.push_back(std::move(n));
                return rep;
            }
            if (c == '{') {
                size_t j = i + 1;
                int n1 = 0;
                bool have_num = j < s.size() && is_digit(s[j]);
                while (have_num && j < s.size() && is_digit(s[j])) {
                    n1 = n1 * 10 + (s[j] - '0');
                    ++j;
                }
                int n2 = -1;
                if (have_num && j < s.size() && s[j] == ',') {
                    ++j;
                    if (j < s.size() && is_digit(s[j])) {
                        n2 = 0;
                        while (j < s.size() && is_digit(s[j])) {
                            n2 = n2 * 10 + (s[j] - '0');
                            ++j;
                        }
                    }
                }
                if (have_num && j < s.size() && s[j] == '}') {
                    int mx = (j - (i + 1) == 1 || n2 < 0) ? -1 : n2;
                    if (have_num && j - (i + 1) == 1) mx = n1; // {n}
                    ++j;
                    bool gr = true;
                    if (j < s.size() && s[j] == '?') { gr = false; ++j; }
                    auto rep = make_node(Kind::Repeat);
                    rep->min = n1;
                    rep->max = mx;
                    rep->greedy = gr;
                    rep->items.push_back(std::move(n));
                    i = j;
                    return rep;
                }
                // Not a valid quantifier: '{' is a literal — fall through.
            }
        }
        return n;
    }

    std::unique_ptr<Node> parse_unquantified() {
        if (i >= s.size()) fail();
        char c = s[i];
        if (c == '(') return parse_group();
        if (c == '[') return parse_class();
        if (c == '.') {
            ++i;
            return make_node(Kind::Dot);
        }
        if (c == '^') { ++i; return make_anchor(0); }
        if (c == '$') { ++i; return make_anchor(1); }
        if (c == '\\') return parse_escape(false);
        // Literal (UTF-8 decoded).
        char32_t cp = 0;
        size_t j = i;
        cp = unicode::decode_utf8(s, j);
        i = j;
        auto n = make_node(Kind::Char);
        n->ch = cp;
        return n;
    }

    std::unique_ptr<Node> make_anchor(int a) {
        auto n = make_node(Kind::Anchor);
        n->anchor = a;
        return n;
    }

    // (?i) inline flag consumes without producing an atom.
    bool parse_flag() {
        // At s[i] == '?' and s[i+1] == 'i' and s[i+2] == ')'
        icase = true; // approximation: applies from this point on
        i += 3;
        return true;
    }

    std::unique_ptr<Node> parse_group() {
        // s[i] == '('
        if (i + 1 < s.size() && s[i + 1] == '?') {
            char c = s[i + 2];
            if (c == 'P') {
                // (?P<name>...)
                size_t lt = s.find('<', i + 3);
                size_t gt = lt == std::string_view::npos ? std::string_view::npos
                                                         : s.find('>', lt + 1);
                if (lt == std::string_view::npos || gt == std::string_view::npos)
                    fail();
                std::string name = std::string(s.substr(lt + 1, gt - lt - 1));
                if (name.empty() || !is_name_ok(name.c_str())) fail();
                for (const auto& existing : names)
                    if (existing == name) fail(); // duplicate name
                int index = group_count++;
                names.push_back(name);
                name_group_indexes.push_back(index);
                i = gt + 1;
                auto n = make_node(Kind::Group);
                n->capture = true;
                n->group_index = index;
                n->name = name;
                n->items.push_back(parse_group_body());
                expect_closing();
                return n;
            }
            if (c == ':') {
                i += 3;
                auto n = make_node(Kind::Group);
                n->items.push_back(parse_group_body());
                expect_closing();
                return n;
            }
            if (c == '=' || c == '!') {
                bool negated = (c == '!');
                i += 3;
                auto n = make_node(Kind::LookAhead);
                n->negated = negated;
                n->items.push_back(parse_group_body());
                expect_closing();
                return n;
            }
            if (c == '<') {
                char op = s[i + 3];
                if (op != '=' && op != '!') fail();
                bool negated = (op == '!');
                i += 4;
                auto n = make_node(Kind::LookBehind);
                n->negated = negated;
                n->items.push_back(parse_group_body());
                expect_closing();
                return n;
            }
            if (c == '(') {
                // (?(name)yes|no) conditional
                size_t close = s.find(')', i + 3);
                if (close == std::string_view::npos) fail();
                std::string name = std::string(s.substr(i + 3, close - i - 3));
                if (name.empty()) fail();
                bool known = false;
                for (const auto& existing : names)
                    if (existing == name) known = true;
                if (!known) fail(); // unknown group name
                i = close + 1;
                auto n = make_node(Kind::Cond);
                n->name = name;
                n->items.push_back(parse_group_body());
                expect_closing();
                return n;
            }
            if (c == 'i' && i + 3 < s.size() && s[i + 3] == ')') {
                parse_flag(); // consumes (?i, now at ')'
                ++i;          // consume ')'
                return nullptr;
            }
            fail();
        }
        // Plain capture group.
        ++i;
        auto n = make_node(Kind::Group);
        n->capture = true;
        n->group_index = group_count++;
        n->items.push_back(parse_group_body());
        expect_closing();
        return n;
    }

    static bool is_name_ok(const char* p) {
        if (!isalpha0(p[0])) return false;
        for (const char* q = p + 1; *q; ++q)
            if (!isalnum0(*q) && *q != '_') return false;
        return true;
    }

    static bool isalpha0(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }
    static bool isalnum0(char c) { return isalpha0(c) || is_digit0(c); }
    static bool is_digit0(char c) { return c >= '0' && c <= '9'; }

    void expect_closing() {
        if (i >= s.size() || s[i] != ')') fail();
        ++i;
    }

    // Class shorthand node for \w / \s.
    std::unique_ptr<Node> make_shorthand(bool word, bool space) {
        auto n = make_node(Kind::Class);
        n->word_class = word;
        n->space_class = space;
        return n;
    }

    // Parse an escape OUTSIDE a class. Returns nullptr for escapes that
    // are pure atoms vs. anchors... (all return a node here).
    std::unique_ptr<Node> parse_escape(bool in_class) {
        // s[i] == '\\'
        if (i + 1 >= s.size()) fail();
        char c = s[i + 1];
        if (!in_class) {
            switch (c) {
            case 'b': i += 2; return make_anchor(2);
            case 'B': i += 2; return make_anchor(3);
            case 'w': i += 2; return make_shorthand(true, false);
            case 'W': { auto n = make_shorthand(true, false); n->negated = true; i += 2; return n; }
            case 's': i += 2; return make_shorthand(false, true);
            case 'S': { auto n = make_shorthand(false, true); n->negated = true; i += 2; return n; }
            case 'd': {
                auto n = make_node(Kind::Class);
                n->ranges.push_back({'0', '9'});
                i += 2;
                return n;
            }
            case 'D': {
                auto n = make_node(Kind::Class);
                n->ranges.push_back({0, '0' - 1});
                n->ranges.push_back({'9' + 1, 0x10FFFF});
                i += 2;
                return n;
            }
            case 'A': i += 2; return make_anchor(0);
            case 'Z': i += 2; return make_anchor(1);
            case '<': i += 2; return make_anchor(2);
            case '>': i += 2; return make_anchor(3);
            }
        }
        char32_t cp = 0;
        i += 2;
        switch (c) {
        case 'n': cp = '\n'; break;
        case 't': cp = '\t'; break;
        case 'r': cp = '\r'; break;
        case 'f': cp = '\f'; break;
        case 'v': cp = '\v'; break;
        case 'a': cp = '\a'; break;
        case '0': cp = 0; break;
        case 'w':
            return make_shorthand(true, false); // in_class only
        case 's':
            return make_shorthand(false, true); // in_class only
        default: break;
        }
        if (c == 'd' && in_class) {
            auto n = make_node(Kind::Class);
            n->ranges.push_back({'0', '9'});
            return n;
        }
        if (c == 'x') {
            if (i + 2 > s.size() || !is_hex(s[i]) || !is_hex(s[i + 1])) fail();
            cp = hexval(s[i]) * 16 + hexval(s[i + 1]);
            i += 2;
            auto n = make_node(Kind::Char);
            n->ch = cp;
            return n;
        }
        if (c == 'u') return parse_unicode_escape(4);
        if (c == 'U') return parse_unicode_escape(8);
        if (c == 'c') {
            if (i >= s.size()) fail();
            char letter = s[i];
            i += 1;
            cp = static_cast<char32_t>(static_cast<unsigned char>(letter) ^ 0x20);
            auto n = make_node(Kind::Char);
            n->ch = cp;
            return n;
        }
        if (is_digit0(c)) {
            // Octal \NNN (1-3 digits, value < 256)
            int v = c - '0';
            size_t k = i; // i already points past the digit
            while (k < s.size() && is_digit0(s[k]) && v < 128) {
                v = v * 8 + (s[k] - '0');
                ++k;
            }
            if (v >= 256) fail();
            i = k;
            auto n = make_node(Kind::Char);
            n->ch = static_cast<char32_t>(v);
            return n;
        }
        // Any other escape is the literal character.
        auto n = make_node(Kind::Char);
        n->ch = static_cast<char32_t>(static_cast<unsigned char>(c));
        return n;
    }

    static bool is_hex(char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }
    static int hexval(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return c - 'A' + 10;
    }

    std::unique_ptr<Node> parse_unicode_escape(int digits) {
        if (i + digits > s.size()) fail();
        char32_t v = 0;
        for (int k = 0; k < digits; ++k) {
            if (!is_hex(s[i + k])) fail();
            v = v * 16 + static_cast<char32_t>(hexval(s[i + k]));
        }
        i += digits;
        if (v > 0x10FFFF) fail();
        auto n = make_node(Kind::Char);
        n->ch = v;
        return n;
    }

    std::unique_ptr<Node> parse_class() {
        // s[i] == '['
        ++i;
        auto n = make_node(Kind::Class);
        bool negated = false;
        if (i < s.size() && s[i] == '^') {
            negated = true;
            ++i;
        }
        // First ']' is literal.
        bool first = true;
        char32_t pending_lo = 0;
        bool have_lo = false;
        for (;;) {
            if (i >= s.size()) fail(); // unterminated class
            char c = s[i];
            if (c == ']' && !first) break;
            first = false;
            if (c == '\\') {
                char32_t lo = 0, hi = 0;
                int what = parse_class_escape(n, lo, hi);
                // A pending range start only forms a range with an explicit
                // '-' (handled on the next iteration); otherwise emit it as
                // a single character first.
                if (have_lo) {
                    add_range(n, pending_lo, pending_lo);
                    have_lo = false;
                }
                if (what == 0) {
                    pending_lo = lo;
                    have_lo = true;
                }
                continue;
            }
            char32_t ch = 0;
            size_t j = i;
            ch = unicode::decode_utf8(s, j);
            i = j;
            if (have_lo && c == '-' && i < s.size() && s[i] != ']') {
                // Range lo-...: the high endpoint starts at s[i] (i already
                // points just past '-').
                size_t k = i;
                char32_t hi = 0;
                if (s[i] == '\\') {
                    size_t e = k;
                    hi = simple_escape_value(s, e);
                    k = e;
                } else {
                    size_t kk = k;
                    hi = unicode::decode_utf8(s, kk);
                    k = kk;
                }
                i = k;
                add_range(n, pending_lo, hi);
                have_lo = false;
                continue;
            }
            if (have_lo) {
                add_range(n, pending_lo, pending_lo);
                have_lo = false;
            }
            pending_lo = ch;
            have_lo = true;
        }
        if (have_lo) {
            add_range(n, pending_lo, pending_lo);
            have_lo = false;
        }
        ++i; // consume ']'
        n->negated = negated;
        return n;
    }

    static char32_t simple_escape_value(std::string_view s, size_t& i) {
        char c = s[i + 1];
        i += 2;
        switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case 'f': return '\f';
        case 'v': return '\v';
        case 'a': return '\a';
        case '0': return 0;
        default: return static_cast<char32_t>(static_cast<unsigned char>(c));
        }
    }

        // Parse a class escape. Returns: 0 = value in lo/hi (single
        // char or range endpoint), 1 = shorthand union added to the node.
        int parse_class_escape(std::unique_ptr<Node>& n, char32_t& lo, char32_t& hi) {
            // s[i] == '\\'
            if (i + 1 >= s.size()) fail();
            char c = s[i + 1];
            i += 2;
            switch (c) {
            case 'w':
                n->word_class = true;
                return 1;
            case 'W':
                n->word_neg = true;
                return 1;
            case 'd':
                n->ranges.push_back({'0', '9'});
                return 1;
            case 'D': {
                // Complement of digits: two ranges.
                n->ranges.push_back({0, '0' - 1});
                n->ranges.push_back({'9' + 1, 0x10FFFF});
                return 1;
            }
            case 's':
                n->space_class = true;
                return 1;
            case 'S':
                n->space_neg = true;
                return 1;
            case 'n': lo = hi = '\n'; return 0;
            case 't': lo = hi = '\t'; return 0;
            case 'r': lo = hi = '\r'; return 0;
            case 'f': lo = hi = '\f'; return 0;
            case 'v': lo = hi = '\v'; return 0;
            case 'a': lo = hi = '\a'; return 0;
            case '0': lo = hi = 0; return 0;
            case 'x': {
                if (i + 2 > s.size() || !is_hex(s[i]) || !is_hex(s[i + 1])) fail();
                char32_t v = static_cast<char32_t>(hexval(s[i]) * 16 + hexval(s[i + 1]));
                i += 2;
                lo = hi = v;
                return 0;
            }
            default:
                lo = hi = static_cast<char32_t>(static_cast<unsigned char>(c));
                return 0;
            }
        }

    static void add_range(std::unique_ptr<Node>& n, char32_t lo, char32_t hi) {
        if (lo > hi) std::swap(lo, hi);
        n->ranges.push_back({lo, hi});
    }
};

} // namespace detail

// ---------------------------------------------------------------------------

struct Match {
    size_t start{0}; // codepoint offset
    size_t end{0};   // codepoint offset
    // One entry per named group (definition order); {SIZE_MAX, SIZE_MAX}
    // = not participating.
    std::vector<std::pair<size_t, size_t>> groups;
};

class Pattern {
public:
    explicit Pattern(std::string_view pattern) {
        detail::Parser p;
        p.s = pattern;
        root_ = p.parse_pattern();
        group_count_ = p.group_count;
        names_ = std::move(p.names);
        name_indexes_ = std::move(p.name_group_indexes);
        icase_ = p.icase;
    }

    const std::vector<std::string>& names() const { return names_; }
    size_t group_count() const { return group_count_; }

    // Python re.finditer: leftmost, non-overlapping; a zero-width match
    // advances the search by one codepoint.
    std::vector<Match> find_all(std::string_view text) const {
        std::vector<char32_t> cps;
        cps.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) cps.push_back(unicode::decode_utf8(text, i));
        std::vector<Match> out;
        Matcher m(*this, cps);
        size_t p = 0;
        while (p <= cps.size()) {
            size_t end = 0;
            if (m.match_at(p, end)) {
                Match r;
                r.start = p;
                r.end = end;
                for (size_t g = 0; g < names_.size(); ++g)
                    r.groups.push_back(m.group_span(g));
                out.push_back(std::move(r));
                p = (end == p) ? p + 1 : end;
            } else {
                ++p;
            }
        }
        return out;
    }

private:
    struct GroupVal {
        size_t start{SIZE_MAX};
        size_t end{SIZE_MAX};
    };

    class Matcher {
    public:
        Matcher(const Pattern& pat, const std::vector<char32_t>& cps)
            : pat_(pat), cps_(cps), name_indexes_(pat.name_indexes_),
              groups_(pat.group_count()) {}

        bool match_at(size_t start, size_t& end) {
            for (auto& g : groups_) g = GroupVal{};
            size_t pos = start;
            bool ok = match_seq(pat_.root_, 0, pos,
                                [&end](size_t p2) { end = p2; return true; });
            return ok;
        }

        std::pair<size_t, size_t> group_span(size_t named_index) const {
            const GroupVal& g = groups_[name_indexes_[named_index]];
            return {g.start, g.end};
        }

    private:
        // Continuation: invoked with the position after the current node
        // matched; returning true completes the match, false backtracks.
        using Cont = std::function<bool(size_t)>;

        const Pattern& pat_;
        const std::vector<char32_t>& cps_;
        const std::vector<int>& name_indexes_;
        std::vector<GroupVal> groups_;
        bool fx_{true}; // capture side effects enabled (off inside assertions)

        static bool char_eq(const Pattern& pat, char32_t a, char32_t b) {
            if (a == b) return true;
            return pat.icase_ && fold_cp(a) == fold_cp(b);
        }

        bool in_class(const detail::Node& n, char32_t c) const {
            bool hit = false;
            if (n.word_class && is_word_cp(c)) hit = true;
            if (!hit && n.word_neg && !is_word_cp(c)) hit = true;
            if (!hit && n.space_class && is_space_cp(c)) hit = true;
            if (!hit && n.space_neg && !is_space_cp(c)) hit = true;
            if (!hit) {
                char32_t d = c;
                if (pat_.icase_) d = fold_cp(c);
                for (const auto& r : n.ranges) {
                    if (d >= r.lo && d <= r.hi) { hit = true; break; }
                    if (pat_.icase_) {
                        char32_t fl = fold_cp(r.lo), fh = fold_cp(r.hi);
                        if (fl <= fh && d >= fl && d <= fh) { hit = true; break; }
                    }
                }
            }
            return n.negated ? !hit : hit;
        }

        bool anchor_ok(const detail::Node& n, size_t pos) const {
            auto is_word = [&](size_t q) {
                return q < cps_.size() && is_word_cp(cps_[q]);
            };
            switch (n.anchor) {
            case 0: return pos == 0;
            case 1: return pos == cps_.size();
            case 2: return ((pos > 0 && is_word_cp(cps_[pos - 1])) != is_word(pos));
            case 3: return ((pos > 0 && is_word_cp(cps_[pos - 1])) == is_word(pos));
            default: return false;
            }
        }

        bool cond_participated(const detail::Node& n) const {
            for (size_t k = 0; k < pat_.names_.size(); ++k)
                if (pat_.names_[k] == n.name)
                    return groups_[static_cast<size_t>(k)].start != SIZE_MAX;
            return false;
        }

        const detail::Node& cond_choice(const detail::Node& n, bool participated) const {
            const detail::Node& body = *n.items[0];
            if (body.kind != detail::Kind::Alt) return body;
            if (participated) return *body.items[0];
            return body.items.size() > 1 ? *body.items[1] : *body.items[0];
        }

        // Match the sequence nodes[i..] from pos, backtracking across all
        // choice points; on success invoke cont with the final position.
        bool match_seq(const std::vector<std::unique_ptr<detail::Node>>& nodes,
                       size_t i, size_t pos, const Cont& cont) {
            if (i >= nodes.size()) return cont(pos);
            return match_node(*nodes[i], pos,
                              [this, &nodes, i, &cont](size_t p2) {
                                  return match_seq(nodes, i + 1, p2, cont);
                              });
        }

        // Match a single node from pos; on success invoke cont.
        bool match_node(const detail::Node& n, size_t pos, const Cont& cont) {
            switch (n.kind) {
            case detail::Kind::Char: {
                if (pos >= cps_.size()) return false;
                if (char_eq(pat_, cps_[pos], n.ch)) return cont(pos + 1);
                return false;
            }
            case detail::Kind::Class: {
                if (pos >= cps_.size()) return false;
                if (in_class(n, cps_[pos])) return cont(pos + 1);
                return false;
            }
            case detail::Kind::Dot: {
                if (pos >= cps_.size() || cps_[pos] == '\n') return false;
                return cont(pos + 1);
            }
            case detail::Kind::Anchor:
                return anchor_ok(n, pos) ? cont(pos) : false;
            case detail::Kind::LookBehind: {
                bool saved = fx_;
                fx_ = false;
                bool found = false;
                for (size_t s = pos; s > 0 && !found; --s)
                    found = match_node(*n.items[0], s - 1,
                                       [this, pos](size_t p2) {
                                           return p2 == pos;
                                       });
                fx_ = saved;
                return found == !n.negated ? cont(pos) : false;
            }
            case detail::Kind::LookAhead: {
                bool saved = fx_;
                fx_ = false;
                bool ok = match_node(*n.items[0], pos,
                                     [](size_t) { return true; });
                fx_ = saved;
                return ok == !n.negated ? cont(pos) : false;
            }
            case detail::Kind::Cond: {
                if (cond_participated(n))
                    return match_node(cond_choice(n, true), pos, cont);
                const detail::Node& body = *n.items[0];
                if (body.kind == detail::Kind::Alt)
                    return match_node(cond_choice(n, false), pos, cont);
                return cont(pos);
            }
            case detail::Kind::Group: {
                if (n.capture && fx_) {
                    size_t start = pos;
                    const size_t gi = static_cast<size_t>(n.group_index);
                    GroupVal old = groups_[gi];
                    groups_[gi] = GroupVal{start, SIZE_MAX};
                    bool ok = match_node(*n.items[0], pos,
                                         [this, start, gi, &cont](size_t p2) {
                                             groups_[gi] = GroupVal{start, p2};
                                             return cont(p2);
                                         });
                    if (!ok) groups_[gi] = old;
                    return ok;
                }
                return match_node(*n.items[0], pos, cont);
            }
            case detail::Kind::Seq:
                return match_seq(n.items, 0, pos, cont);
            case detail::Kind::Alt:
                for (const auto& a : n.items)
                    if (match_node(*a, pos, cont)) return true;
                return false;
            case detail::Kind::Repeat: {
                // Consume the inner node repeatedly (with side effects),
                // backtracking over the count: greedy tries the longest run
                // first, lazy the shortest.  Every consumed repetition must
                // make progress (zero-width inners do not extend the run).
                const detail::Node& inner = *n.items[0];
                const int left0 = (n.max < 0) ? INT_MAX : n.max;
                auto go = [&](auto&& self, int left, int need, size_t p)
                              -> bool {
                    if (n.greedy) {
                        if (left > 0) {
                            bool consumed = match_node(inner, p,
                                                       [&](size_t p1) {
                                                           return p1 != p &&
                                                                  self(self,
                                                                       left - 1,
                                                                       need > 0
                                                                           ? need - 1
                                                                           : 0,
                                                                       p1);
                                                       });
                            if (consumed) return true;
                        }
                        return need == 0 ? cont(p) : false;
                    }
                    if (need == 0) {
                        if (cont(p)) return true;
                        if (left == 0) return false;
                    }
                    return match_node(inner, p, [&](size_t p1) {
                        return p1 != p &&
                               self(self, left - 1,
                                    need > 0 ? need - 1 : 0, p1);
                    });
                };
                return go(go, left0, n.min, pos);
            }
            }
            return false;
        }
    };
    std::vector<std::unique_ptr<detail::Node>> root_;
    std::vector<std::string> names_;
    std::vector<int> name_indexes_; // names_[k] -> its group_index
    size_t group_count_{0};
    bool icase_{false};
};

} // namespace rich::regex
