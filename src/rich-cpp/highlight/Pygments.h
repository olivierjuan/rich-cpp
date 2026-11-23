#pragma once
// rich-cpp — pygments-compatible regex lexer engine (FEATURE_MATRIX row 64).
//
// Data tables live in SyntaxLexers.h (69 regex lexers, generated from
// pygments' processed state machines) and SyntaxThemes.h (32 themes,
// generated from pygments styles). This file implements:
//   * a backtracking regex matcher with Python `re` semantics (leftmost-
//     first alternation, greedy/lazy quantifiers, capturing groups,
//     lookarounds, fixed-length lookbehind, backreferences, inline
//     (?i)(?s)(?x) flags, ^ $ \A \Z \b \B, character classes with
//     ranges, verbose-mode parsing)
//   * the pygments RegexLexer.get_tokens_unprocessed state machine
//     (rules tried in order, ANCHORED at pos, first match wins; bygroups
//     per-group emission; using() sub-lexing with state stacks; state
//     transitions "#push" / "#pop" / int → del statestack[N:] / names;
//     unmatched '\n' → reset to root + Token.Text.Whitespace; any other
//     unmatched char → Token.Error single char)
//   * the plain-Lexer JsonLexer port (pygments/lexers/data.py:436)
//   * the markdown / restructuredtext code-block callbacks and scheme's
//     decimal callback (match-only ports of the pygments closures)
//
// Unicode \w / \d / \s are documented approximations (ASCII exact; for
// codepoints >= 0x80, \w excludes the symbol/punctuation blocks
// 0x2000-0x2BFF, 0x3000-0x303F, 0xFE00-0xFE52, 0xFF01-0xFFFF; \d is
// ASCII digits; \s is the standard Unicode whitespace set).

#include "SyntaxLexers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rich::pg {

// ── UTF-8 text ──────────────────────────────────────────────────────────────
struct PgText {
    const char* src = nullptr;
    size_t len = 0;
    std::vector<uint32_t> cps;      // decoded codepoints
    std::vector<size_t> ofs;        // ofs[i] = byte offset of cps[i]; ofs[n]=len

    PgText() = default;
    PgText(const char* s, size_t l) { decode(s, l); }
    void decode(const char* s, size_t l) {
        src = s;
        len = l;
        cps.clear();
        ofs.clear();
        ofs.reserve(l / 3 + 8);
        ofs.push_back(0);
        size_t i = 0;
        while (i < l) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            uint32_t cp = 0;
            size_t k = 0;
            if (c < 0x80) {
                cp = c;
                k = 1;
            } else if ((c >> 5) == 6) {
                cp = c & 0x1F;
                k = 2;
            } else if ((c >> 4) == 14) {
                cp = c & 0x0F;
                k = 3;
            } else if ((c >> 3) == 30) {
                cp = c & 0x07;
                k = 4;
            } else {
                cp = 0xFFFD; // invalid byte → replacement
                k = 1;
            }
            for (size_t j = 1; j < k; ++j) {
                if (i + j >= l) {
                    cp = 0xFFFD;
                    k = 1;
                    break;
                }
                const unsigned char cc = static_cast<unsigned char>(s[i + j]);
                if ((cc >> 6) != 2) {
                    cp = 0xFFFD;
                    k = 1;
                    break;
                }
                cp = (cp << 6) | (cc & 0x3F);
            }
            cps.push_back(cp);
            i += k;
            ofs.push_back(i);
        }
    }
    size_t size() const { return cps.size(); }
    // UTF-8 bytes of codepoints [a, b)
    std::string bytes(size_t a, size_t b) const {
        return std::string(src + ofs[a], ofs[b] - ofs[a]);
    }
};

// A window into a PgText (codepoints [off, off+len))
struct PgView {
    const uint32_t* cp = nullptr;
    size_t n = 0;
    const PgText* base = nullptr;
    size_t off = 0;
    static PgView of(const PgText& t, size_t off, size_t len) {
        return PgView{t.cps.data() + off, len, &t, off};
    }
    static PgView of(const PgText& t) {
        return PgView{t.cps.data(), t.size(), &t, 0};
    }
    uint32_t at(size_t i) const { return cp[i]; }
    std::string bytes(size_t a, size_t b) const {
        return base->bytes(off + a, off + b);
    }
};

// ── Unicode predicates (approximations, see header) ─────────────────────────
inline bool pg_is_digit(uint32_t c) { return c >= '0' && c <= '9'; }

inline bool pg_is_space(uint32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x2028 || c == 0x2029 || (c >= 0x202F && c <= 0x205E) || c == 0x3000;
}

inline bool pg_is_word(uint32_t c) {
    if (c == '_')
        return true;
    if (c < 0x80)
        return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || pg_is_digit(c);
    if (c >= 0x2000 && c <= 0x2BFF)
        return false; // general punctuation + CJK punctuation + symbols
    if (c >= 0x3000 && c <= 0x303F)
        return false;
    if (c >= 0xFE00 && c <= 0xFE0F)
        return false;
    if (c >= 0xFE30 && c <= 0xFE4F)
        return false;
    if (c == 0xFEFF)
        return false;
    if (c >= 0xFF00 && c <= 0xFFFF)
        return false; // fullwidth forms + special (letters are rare here;
                      // documented approximation)
    return true;
}

// Case folding for (?i): ASCII + Latin-1 + Latin Ext A/B + Greek + Cyrillic
// simple folds. Multi-char folds (ß→ss) are not supported (rare in code).
inline uint32_t pg_fold(uint32_t c) {
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (c >= 0x00C0 && c <= 0x00D6)
        return c + 32;
    if (c >= 0x00D8 && c <= 0x00DE)
        return c + 32;
    if (c == 0x0178)
        return 0x00DF;
    if ((c >= 0x0100 && c <= 0x0177) && (c & 1))
        return c - 1; // Latin Ext A/B pairs
    if ((c >= 0x0179 && c <= 0x024F) && (c & 1))
        return c - 1;
    if (c >= 0x0391 && c <= 0x03A9)
        return c + 32;
    if (c == 0x03A2)
        return 0x03B2;
    if (c >= 0x0410 && c <= 0x042F)
        return c + 32;
    if (c >= 0x0400 && c <= 0x040F && (c & 1) == 0 && c != 0x0401)
        return c + 32;
    return c;
}

// ── regex AST ───────────────────────────────────────────────────────────────
struct PgNode {
    enum class T : unsigned char {
        Concat,  // kids
        Alt,     // kids = options
        Quant,   // child, min, max (-1 = ∞), lazy
        Group,   // child, grp
        Look,    // child; neg = negate; lb_len >= 0 → lookbehind of that length
        Class,   // items (negated)
        Char,    // c
        Anchor,  // anchor: 0 ^, 1 $, 2 \A, 3 \Z, 4 \b, 5 \B
        Flags,   // fset / funset
        Backref, // bref
        Marker,  // never parsed; tail sentinel recording a group's end
    };
    T t;
    std::vector<std::unique_ptr<PgNode>> kids;
    std::unique_ptr<PgNode> child;
    int min = 0;
    int max = -1;
    bool lazy = false;
    int grp = 0;
    bool neg = false;
    int lb_len = -1;
    struct CI {
        uint32_t lo, hi;
        uint8_t kind; // 0 range, 1 \d, 2 \D, 3 \w, 4 \W, 5 \s, 6 \S, 7 dot
    };
    std::vector<CI> items;
    bool negated = false;
    uint32_t c = 0;
    int anchor = 0;
    uint16_t fset = 0, funset = 0;
    int bref = 0;

    static std::unique_ptr<PgNode> mk(T t) {
        auto p = std::make_unique<PgNode>();
        p->t = t;
        return p;
    }
};

struct PgPattern {
    std::unique_ptr<PgNode> root;
    int n_groups = 0;
    std::vector<std::pair<std::string, int>> named;
    int named_idx(const char* name) const {
        for (const auto& [n, i] : named)
            if (n == name)
                return i;
        return 0;
    }
};

namespace detail {

// re module flag values
constexpr uint16_t RE_I = 0x01;
constexpr uint16_t RE_L = 0x02;
constexpr uint16_t RE_S = 0x04;
constexpr uint16_t RE_M = 0x08;
constexpr uint16_t RE_U = 0x10;
constexpr uint16_t RE_X = 0x20; // verbose

struct PgParser {
    const uint32_t* p;
    size_t n;
    size_t i = 0;
    uint16_t flags;
    int next_grp = 1;
    std::vector<std::pair<std::string, int>> named;
    bool ok = true;

    PgParser(const uint32_t* s, size_t len, uint16_t fl)
        : p(s), n(len), flags(fl) {}

    bool done() const { return i >= n; }
    uint32_t cur() const { return i < n ? p[i] : 0; }

    void skip_x() {
        // verbose mode: skip ASCII whitespace and # comments
        while (i < n) {
            const uint32_t c = p[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
                c == '\f')
                ++i;
            else if (c == '#') {
                while (i < n && p[i] != '\n')
                    ++i;
            } else
                break;
        }
    }

    // read one character (with escapes)
    bool read_char(uint32_t& out) {
        if (done())
            return ok = false;
        uint32_t c = cur();
        if (c == '\\') {
            ++i;
            if (done())
                return ok = false;
            uint32_t e = cur();
            ++i;
            switch (e) {
            case 'n':
                out = '\n';
                return true;
            case 'r':
                out = '\r';
                return true;
            case 't':
                out = '\t';
                return true;
            case 'f':
                out = '\f';
                return true;
            case 'v':
                out = '\v';
                return true;
            case 'a':
                out = '\a';
                return true;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                out = e - '0';
                for (int k = 0; k < 2; ++k) {
                    if (!done() && cur() >= '0' && cur() <= '7')
                        out = out * 8 + uint32_t(cur() - '0'), ++i;
                    else
                        break;
                }
                return true;
            case 'x': {
                uint32_t v = 0;
                int d = 0;
                if (!done() && cur() == '{') {
                    ++i;
                    while (!done() && cur() != '}') {
                        uint32_t h = cur();
                        int dv = (h >= '0' && h <= '9') ? h - '0'
                                  : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                                  : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                                           : -1;
                        if (dv < 0)
                            return ok = false;
                        v = v * 16 + uint32_t(dv);
                        ++i;
                    }
                    if (done())
                        return ok = false;
                    ++i; // }
                    out = v;
                    return true;
                }
                for (int k = 0; k < 2; ++k) {
                    if (done())
                        break;
                    uint32_t h = cur();
                    int dv = (h >= '0' && h <= '9') ? h - '0'
                              : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                              : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                                       : -1;
                    if (dv < 0)
                        break;
                    v = v * 16 + uint32_t(dv);
                    ++i;
                    ++d;
                }
                if (d == 0)
                    return ok = false;
                out = v;
                return true;
            }
            case 'u': {
                uint32_t v = 0;
                int d = 0;
                for (int k = 0; k < 4; ++k) {
                    if (done())
                        return ok = false;
                    uint32_t h = cur();
                    int dv = (h >= '0' && h <= '9') ? h - '0'
                              : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                              : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                                       : -1;
                    if (dv < 0)
                        return ok = false;
                    v = v * 16 + uint32_t(dv);
                    ++i;
                    ++d;
                }
                out = v;
                return true;
            }
            case 'U': {
                uint32_t v = 0;
                for (int k = 0; k < 8; ++k) {
                    if (done())
                        return ok = false;
                    uint32_t h = cur();
                    int dv = (h >= '0' && h <= '9') ? h - '0'
                              : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                              : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                                       : -1;
                    if (dv < 0)
                        return ok = false;
                    v = v * 16 + uint32_t(dv);
                    ++i;
                }
                out = v;
                return true;
            }
            default:
                // unknown escape → literal char
                out = e;
                return true;
            }
        }
        out = c;
        ++i;
        return true;
    }

    // read a shorthand class escape (\d \D \w \W \s \S) → ci.kind
    bool read_shorthand(PgNode::CI& ci) {
        if (done())
            return false;
        uint8_t kind = shorthand_kind(cur());
        if (kind == 0)
            return false;
        ++i;
        ci = PgNode::CI{0, 0, kind};
        return true;
    }

    static uint8_t shorthand_kind(uint32_t e) {
        switch (e) {
        case 'd':
            return 1;
        case 'D':
            return 2;
        case 'w':
            return 3;
        case 'W':
            return 4;
        case 's':
            return 5;
        case 'S':
            return 6;
        }
        return 0;
    }

    std::unique_ptr<PgNode> parse_class() {
        auto node = PgNode::mk(PgNode::T::Class);
        ++i; // [
        node->negated = (cur() == '^');
        if (node->negated)
            ++i;
        bool first = true;
        while (!done()) {
            if (cur() == ']' && !first)
                break;
            PgNode::CI ci{0, 0, 0};
            if (cur() == '\\' && i + 1 < n) {
                uint32_t e = p[i + 1];
                uint8_t k = shorthand_kind(e);
                if (k != 0) {
                    i += 2;
                    node->items.push_back(PgNode::CI{0, 0, k});
                    first = false;
                    continue;
                }
            }
            uint32_t c1 = 0;
            if (!read_char(c1))
                return nullptr;
            // range?
            if (!done() && cur() == '-' && i + 1 < n && p[i + 1] != ']') {
                ++i; // -
                uint32_t c2 = 0;
                if (!read_char(c2))
                    return nullptr;
                ci = PgNode::CI{c1, c2, 0};
            } else {
                ci = PgNode::CI{c1, c1, 0};
            }
            node->items.push_back(ci);
            first = false;
        }
        if (done())
            return nullptr; // no closing ]
        ++i; // ]
        return node;
    }

    std::unique_ptr<PgNode> parse_atom() {
        if (done())
            return nullptr;
        if (flags & RE_X)
            skip_x();
        if (done())
            return nullptr;
        uint32_t c = cur();
        if (c == '^') {
            ++i;
            auto node = PgNode::mk(PgNode::T::Anchor);
            node->anchor = 0;
            return node;
        }
        if (c == '$') {
            ++i;
            auto node = PgNode::mk(PgNode::T::Anchor);
            node->anchor = 1;
            return node;
        }
        if (c == '(') {
            ++i; // (
            if (done())
                return nullptr;
            uint32_t c2 = cur(); // '?' expected
            if (c2 != '?') {
                // capturing group — body starts at c2 (already positioned)
                auto node = PgNode::mk(PgNode::T::Group);
                node->grp = next_grp++;
                auto body = parse_alt(')');
                if (!body)
                    return nullptr;
                if (done() || cur() != ')')
                    return nullptr;
                ++i; // )
                node->child = std::move(body);
                return node;
            }
            // c2 == '?'
            if (i + 1 >= n)
                return nullptr;
            uint32_t c3 = p[i + 1]; // char after '?'
            if (c3 == '#') {
                // comment — skip to ')'
                i += 2;
                while (!done() && cur() != ')')
                    ++i;
                if (done())
                    return nullptr;
                ++i; // )
                return PgNode::mk(PgNode::T::Concat);
            }
            if (c3 == '=' || c3 == '!') {
                // lookahead ((?= / (?!); lookbehind uses c3 == '<')
                bool neg = (c3 == '!');
                i += 2; // past '?' c3
                auto node = PgNode::mk(PgNode::T::Look);
                node->neg = neg;
                auto body = parse_alt(')');
                if (!body)
                    return nullptr;
                if (done() || cur() != ')')
                    return nullptr;
                ++i; // )
                node->child = std::move(body);
                return node;
            }
            if (c3 == '<' && i + 2 < n && (p[i + 2] == '=' || p[i + 2] == '!')) {
                // lookbehind
                bool neg = (p[i + 2] == '!');
                i += 3; // past '?', '<', c
                auto node = PgNode::mk(PgNode::T::Look);
                node->neg = neg;
                auto body = parse_alt(')');
                if (!body)
                    return nullptr;
                if (done() || cur() != ')')
                    return nullptr;
                ++i; // )
                node->lb_len = fixed_len(body.get());
                if (node->lb_len < 0)
                    return nullptr; // variable-length lookbehind unsupported
                node->child = std::move(body);
                return node;
            }
            if (c3 == 'P') {
                if (i + 2 < n && p[i + 2] == '=') {
                    // (?P=name) backref
                    i += 3;
                    std::string nm;
                    while (!done() && cur() != ')')
                        nm += static_cast<char>(cur()), ++i;
                    if (done())
                        return nullptr;
                    ++i; // )
                    auto node = PgNode::mk(PgNode::T::Backref);
                    for (auto& [nm2, gi] : named)
                        if (nm2 == nm)
                            node->bref = gi;
                    if (node->bref == 0)
                        return nullptr;
                    return node;
                }
                if (i + 2 < n && p[i + 2] == '<') {
                    // (?P<name>...) named group
                    i += 3;
                    std::string nm;
                    while (!done() && cur() != '>')
                        nm += static_cast<char>(cur()), ++i;
                    if (done() || cur() != '>')
                        return nullptr;
                    ++i; // >
                    auto node = PgNode::mk(PgNode::T::Group);
                    node->grp = next_grp++;
                    named.emplace_back(nm, node->grp);
                    auto body = parse_alt(')');
                    if (!body)
                        return nullptr;
                    if (done() || cur() != ')')
                        return nullptr;
                    ++i; // )
                    node->child = std::move(body);
                    return node;
                }
                return nullptr;
            }
            // inline flags (?i) (?im-s) (?x:...)
            uint16_t set = 0, unset = 0;
            // i points at '?'; c3 (the char after '?') is at i+1
            ++i; // to c3
            while (!done()) {
                uint32_t fc = cur();
                if (fc == ':')
                    break;
                if (fc == ')')
                    break;
                if (fc == '-') {
                    ++i;
                    while (!done()) {
                        uint32_t fc2 = cur();
                        if (fc2 == ':' || fc2 == ')')
                            break;
                        if (fc2 == 'i')
                            unset |= RE_I;
                        else if (fc2 == 'm')
                            unset |= RE_M;
                        else if (fc2 == 's')
                            unset |= RE_S;
                        else if (fc2 == 'x')
                            unset |= RE_X;
                        else
                            return nullptr;
                        ++i;
                    }
                    break;
                }
                if (fc == 'i')
                    set |= RE_I;
                else if (fc == 'm')
                    set |= RE_M;
                else if (fc == 's')
                    set |= RE_S;
                else if (fc == 'x')
                    set |= RE_X;
                else if (fc == 'L')
                    set |= RE_L;
                else
                    return nullptr;
                ++i;
            }
            if (!done() && cur() == ':') {
                // scoped flags
                ++i; // :
                uint16_t save = flags;
                flags = (flags & ~unset) | set;
                auto body = parse_alt(')');
                flags = save;
                if (!body)
                    return nullptr;
                if (done() || cur() != ')')
                    return nullptr;
                ++i; // )
                auto conc = PgNode::mk(PgNode::T::Concat);
                auto flnode = PgNode::mk(PgNode::T::Flags);
                flnode->fset = set;
                flnode->funset = unset;
                conc->kids.push_back(std::move(flnode));
                conc->kids.push_back(std::move(body));
                return conc;
            }
            // global inline flags
            flags = (flags & ~unset) | set;
            if (!done() && cur() == ')')
                ++i; // )
            auto node = PgNode::mk(PgNode::T::Flags);
            node->fset = set;
            node->funset = unset;
            return node;
        }
        if (c == '[')
            return parse_class();
        if (c == '\\') {
            if (i + 1 < n) {
                uint32_t e = p[i + 1];
                uint8_t k = shorthand_kind(e);
                if (k != 0) {
                    i += 2;
                    auto node = PgNode::mk(PgNode::T::Class);
                    node->items.push_back(PgNode::CI{0, 0, k});
                    return node;
                }
                if (e >= '1' && e <= '9') {
                    // numeric backreference: read all consecutive decimal
                    // digits; group must already be defined (py3.13:
                    // otherwise re.error; \0xx is octal, handled below)
                    size_t j = i + 1;
                    int num = 0;
                    while (j < n && p[j] >= '0' && p[j] <= '9')
                        num = num * 10 + int(p[j] - '0'), ++j;
                    if (num > next_grp - 1)
                        return nullptr; // invalid group reference
                    i = j;
                    auto node = PgNode::mk(PgNode::T::Backref);
                    node->bref = num;
                    return node;
                }
                if (e == 'b' || e == 'B') {
                    i += 2;
                    auto node = PgNode::mk(PgNode::T::Anchor);
                    node->anchor = (e == 'b') ? 4 : 5;
                    return node;
                }
                if (e == 'A' || e == 'Z' || e == 'z') {
                    i += 2;
                    auto node = PgNode::mk(PgNode::T::Anchor);
                    node->anchor = (e == 'A') ? 2 : 3;
                    return node;
                }
            }
            uint32_t c1 = 0;
            if (!read_char(c1))
                return nullptr;
            auto node = PgNode::mk(PgNode::T::Char);
            node->c = c1;
            return node;
        }
        if (c == '.') {
            ++i;
            auto node = PgNode::mk(PgNode::T::Class);
            node->items.push_back(PgNode::CI{0, 0, 7});
            return node;
        }
        uint32_t c1 = c;
        ++i;
        auto node = PgNode::mk(PgNode::T::Char);
        node->c = c1;
        return node;
    }

    // parse items until stop char; '|' splits alternatives
    std::unique_ptr<PgNode> parse_alt(uint32_t stop) {
        std::vector<std::unique_ptr<PgNode>> opts;
        auto cur_opt = parse_items(stop);
        if (!cur_opt)
            return nullptr;
        opts.push_back(std::move(cur_opt));
        while (!done() && cur() == '|') {
            ++i;
            auto o = parse_items(stop);
            if (!o)
                return nullptr;
            opts.push_back(std::move(o));
        }
        if (opts.size() == 1)
            return std::move(opts[0]);
        auto node = PgNode::mk(PgNode::T::Alt);
        for (auto& o : opts)
            node->kids.push_back(std::move(o));
        return node;
    }

    std::unique_ptr<PgNode> parse_items(uint32_t stop) {
        auto node = PgNode::mk(PgNode::T::Concat);
        for (;;) {
            if (flags & RE_X)
                skip_x();
            if (done() || cur() == stop || cur() == '|')
                break;
            auto atom = parse_atom();
            if (!atom)
                return nullptr;
            // quantifier?
            if (!done()) {
                uint32_t q = cur();
                int min = 0, max = -1;
                bool is_q = false;
                if (q == '*') {
                    is_q = true;
                    min = 0;
                    max = -1;
                } else if (q == '+') {
                    is_q = true;
                    min = 1;
                    max = -1;
                } else if (q == '?') {
                    is_q = true;
                    min = 0;
                    max = 1;
                } else if (q == '{') {
                    size_t j = i + 1;
                    int m = 0;
                    while (j < n && p[j] >= '0' && p[j] <= '9')
                        m = m * 10 + int(p[j] - '0'), ++j;
                    if (j < n && p[j] == '}') {
                        is_q = true;
                        min = max = m;
                    } else if (j < n && p[j] == ',') {
                        ++j;
                        if (j < n && p[j] == '}') {
                            is_q = true;
                            min = m;
                            max = -1;
                        } else {
                            int mx = 0;
                            while (j < n && p[j] >= '0' && p[j] <= '9')
                                mx = mx * 10 + int(p[j] - '0'), ++j;
                            if (j < n && p[j] == '}') {
                                is_q = true;
                                min = m;
                                max = mx;
                            }
                        }
                    }
                }
                if (is_q) {
                    if (q == '{') {
                        // advance i to the '}' (already validated)
                        while (!done() && cur() != '}')
                            ++i;
                        ++i; // }
                    } else
                        ++i;
                    bool lazy = (!done() && cur() == '?');
                    if (lazy)
                        ++i;
                    auto qn = PgNode::mk(PgNode::T::Quant);
                    qn->min = min;
                    qn->max = max;
                    qn->lazy = lazy;
                    qn->child = std::move(atom);
                    atom = std::move(qn);
                }
            }
            node->kids.push_back(std::move(atom));
        }
        return node;
    }

    int fixed_len(const PgNode* n) {
        // -1 = variable (unsupported for lookbehind), else fixed length
        switch (n->t) {
        case PgNode::T::Concat: {
            int t = 0;
            for (auto& k : n->kids) {
                int k1 = fixed_len(k.get());
                if (k1 < 0)
                    return -1;
                t += k1;
            }
            return t;
        }
        case PgNode::T::Group:
            return fixed_len(n->child.get());
        case PgNode::T::Flags:
            return 0;
        case PgNode::T::Char:
            return 1;
        case PgNode::T::Class:
            return 1;
        case PgNode::T::Anchor:
            return 0;
        case PgNode::T::Backref:
            return -1;
        case PgNode::T::Quant: {
            if (n->max == -1)
                return -1;
            int inner = fixed_len(n->child.get());
            if (inner < 0)
                return -1;
            if (n->min != n->max)
                return -1;
            return inner * n->min;
        }
        case PgNode::T::Alt: {
            int t = -1;
            for (auto& k : n->kids) {
                int k1 = fixed_len(k.get());
                if (k1 < 0)
                    return -1;
                if (t >= 0 && t != k1)
                    return -1;
                t = k1;
            }
            return t;
        }
        case PgNode::T::Look:
            return -1;
        }
        return -1;
    }
};

inline PgPattern* parse_cached(std::string_view pattern, uint16_t flags) {
    // key includes flags: the same pattern text under different lexer flags
    // parses differently (x-mode etc.)
    static std::unordered_map<std::string, std::unique_ptr<PgPattern>> cache;
    std::string key(pattern.data(), pattern.size());
    key += '\x00';
    key += static_cast<char>(flags);
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second.get();
    auto pat = std::make_unique<PgPattern>();
    if (pattern.empty()) {
        pat->root = PgNode::mk(PgNode::T::Concat);
    } else {
        PgText t(pattern.data(), pattern.size());
        PgParser pr(t.cps.data(), t.size(), flags);
        auto root = pr.parse_alt(0);
        if (!pr.ok || !root)
            return nullptr;
        pat->root = std::move(root);
        pat->n_groups = pr.next_grp - 1;
        for (auto& [nm, gi] : pr.named)
            pat->named.emplace_back(nm, gi);
    }
    auto* raw = pat.get();
    cache.emplace(std::move(key), std::move(pat));
    return raw;
}

// ── matcher ─────────────────────────────────────────────────────────────────
struct PgGrp {
    size_t start, end;
    bool valid;
};

struct PgMatch {
    size_t end = 0;
    std::vector<PgGrp> g; // 1-based; g[0] unused
};

// tail = stack of (concat, next-idx) continuations, innermost first
struct Tail {
    std::vector<std::pair<const PgNode*, int>> cont;
};

// shared sentinel: tail entry {pg_marker_node, grp} records g[grp].end = pos
// when the group's body has finished (body end), enabling backtracking from
// outer context into quantifiers nested inside the group.
inline const PgNode pg_marker_node = [] {
    PgNode m;
    m.t = PgNode::T::Marker;
    return m;
}();

// shared sentinel: tail entry {pg_quant_mk_node, 0} records q_end = pos when
// a quantifier's child has finished (used to detect empty-match cycles).
inline const PgNode pg_quant_mk_node = [] {
    PgNode m;
    m.t = PgNode::T::Marker;
    return m;
}();

struct PgMatcher {
    const uint32_t* cp;
    size_t n;
    std::vector<PgGrp> g;
    std::vector<bool> marked; // group body finished (marker passed)
    size_t q_end = 0; // last quantifier-child end (marker passed)
    long long steps = 0;
    bool failed = false;
    static constexpr long long kMaxSteps = 8000000;

    PgMatcher(const uint32_t* c, size_t len, int ngroups)
        : cp(c), n(len), g(size_t(ngroups) + 1, PgGrp{0, 0, false}),
          marked(size_t(ngroups) + 1, false) {}

    bool class_hit(const PgNode::CI& ci, uint32_t c, uint16_t fl) const {
        switch (ci.kind) {
        case 0: {
            if (fl & RE_I) {
                uint32_t f = pg_fold(c), lo = pg_fold(ci.lo), hi = pg_fold(ci.hi);
                return f >= lo && f <= hi;
            }
            return c >= ci.lo && c <= ci.hi;
        }
        case 1:
            return pg_is_digit(c);
        case 2:
            return !pg_is_digit(c);
        case 3:
            return pg_is_word(c);
        case 4:
            return !pg_is_word(c);
        case 5:
            return pg_is_space(c);
        case 6:
            return !pg_is_space(c);
        case 7: // dot
            return (fl & RE_S) ? true : c != '\n';
        }
        return false;
    }

    // atom test at pos; returns consume (0 or 1) or -1 on failure
    int atom(const PgNode* node, size_t pos, uint16_t fl, uint16_t& fl2) const {
        fl2 = fl;
        switch (node->t) {
        case PgNode::T::Char: {
            if (pos >= n)
                return -1;
            uint32_t c = cp[pos];
            if (fl & RE_I)
                return pg_fold(c) == pg_fold(node->c) ? 1 : -1;
            return c == node->c ? 1 : -1;
        }
        case PgNode::T::Class: {
            if (pos >= n)
                return -1;
            uint32_t c = cp[pos];
            bool hit = false;
            for (const auto& ci : node->items)
                if (class_hit(ci, c, fl)) {
                    hit = true;
                    break;
                }
            return (node->negated ? !hit : hit) ? 1 : -1;
        }
        case PgNode::T::Anchor:
            switch (node->anchor) {
            case 0: // ^
                return (fl & RE_M) ? (pos == 0 || (pos > 0 && cp[pos - 1] == '\n'))
                                           ? 0
                                           : -1
                                      : (pos == 0 ? 0 : -1);
            case 1: // $
                if (fl & RE_M)
                    return (pos == n || cp[pos] == '\n') ? 0 : -1;
                return (pos == n || (n > 0 && pos == n - 1 && cp[pos] == '\n')) ? 0
                                                                                 : -1;
            case 2:
                return pos == 0 ? 0 : -1;  // \A
            case 3:
                return pos == n ? 0 : -1;  // \Z / \z
            case 4: {                      // \b
                bool l = (pos > 0) ? pg_is_word(cp[pos - 1]) : false;
                bool r = (pos < n) ? pg_is_word(cp[pos]) : false;
                return (l != r) ? 0 : -1;
            }
            case 5: { // \B
                bool l = (pos > 0) ? pg_is_word(cp[pos - 1]) : false;
                bool r = (pos < n) ? pg_is_word(cp[pos]) : false;
                return (l == r) ? 0 : -1;
            }
            }
            return -1;
        case PgNode::T::Flags:
            fl2 = (fl & ~node->funset) | node->fset;
            return 0;
        }
        return -1;
    }

    bool step(const PgNode* node, size_t pos, const Tail& tail, uint16_t fl,
              size_t& end);
    bool step_tail(const Tail& tail, size_t pos, uint16_t fl, size_t& end);
    bool step_quant_cont(int cnt, const Tail& tail, size_t pos, uint16_t fl,
                         size_t& end);
    bool step_kids(const PgNode* conc, int idx, size_t pos, const Tail& tail,
                   uint16_t fl, size_t& end);
};

inline bool PgMatcher::step_kids(const PgNode* conc, int idx, size_t pos,
                                 const Tail& tail, uint16_t fl, size_t& end) {
    if (idx < int(conc->kids.size())) {
        Tail t2 = tail;
        t2.cont.insert(t2.cont.begin(), {conc, idx + 1});
        return step(conc->kids[idx].get(), pos, t2, fl, end);
    }
    return step_tail(tail, pos, fl, end);
}

inline bool PgMatcher::step_tail(const Tail& tail, size_t pos, uint16_t fl,
                                 size_t& end) {
    if (tail.cont.empty()) {
        end = pos;
        return true;
    }
    const auto& front = tail.cont.front();
    if (front.first == &pg_marker_node) {
        g[front.second].end = pos;
        marked[front.second] = true;
    } else if (front.first == &pg_quant_mk_node) {
        q_end = pos;
    } else if (front.first->t == PgNode::T::Quant) {
        return step_quant_cont(front.second, tail, pos, fl, end);
    }
    std::vector<std::pair<const PgNode*, int>> rest(tail.cont.begin() + 1,
                                                    tail.cont.end());
    if (front.second < int(front.first->kids.size())) {
        Tail t2;
        t2.cont = std::move(rest);
        t2.cont.insert(t2.cont.begin(), {front.first, front.second + 1});
        return step(front.first->kids[front.second].get(), pos, t2, fl, end);
    }
    Tail t2;
    t2.cont = std::move(rest);
    return step_tail(t2, pos, fl, end);
}

    // Quantifier continuation at iteration count `cnt` (entry = the Quant
    // node itself). Greedy: one more iteration before the tail; lazy: the
    // tail first, then one more iteration. The child is stepped WITH the
    // outer tail so its nested quantifiers can backtrack for us.
    inline bool PgMatcher::step_quant_cont(int cnt, const Tail& tail,
                                           size_t pos, uint16_t fl,
                                           size_t& end) {
        const PgNode* q = tail.cont.front().first;
        std::vector<std::pair<const PgNode*, int>> rest(tail.cont.begin() + 1,
                                                        tail.cont.end());
        const bool can_more = q->max < 0 || cnt < q->max;
        auto try_iter = [this, &q, &rest, pos, fl, &end, cnt]() {
            if (failed)
                return false;
            const size_t saved_qe = q_end;
            q_end = n + 1; // sentinel
            Tail t2;
            t2.cont = rest;
            t2.cont.insert(t2.cont.begin(), {q, 0}); // re-enter, count patched
            t2.cont.insert(t2.cont.begin(), {&pg_quant_mk_node, 0});
            // the re-entered entry must carry cnt+1; patch after insert
            t2.cont[1].second = cnt + 1;
            bool ok = step(q->child.get(), pos, t2, fl, end);
            const bool consumed = q_end > pos;
            q_end = saved_qe;
            return ok && consumed;
        };
        if (q->lazy) {
            if (cnt >= q->min && step_tail(Tail{rest}, pos, fl, end))
                return true;
            if (can_more && try_iter())
                return true;
            return false;
        }
        if (can_more && try_iter())
            return true;
        if (cnt >= q->min)
            return step_tail(Tail{rest}, pos, fl, end);
        return false;
    }

inline bool PgMatcher::step(const PgNode* node, size_t pos, const Tail& tail,
                            uint16_t fl, size_t& end) {
    if (failed)
        return false;
    if (++steps > kMaxSteps) {
        failed = true;
        return false;
    }
    switch (node->t) {
    case PgNode::T::Concat:
        return step_kids(node, 0, pos, tail, fl, end);
    case PgNode::T::Char:
    case PgNode::T::Class:
    case PgNode::T::Flags: {
        uint16_t fl2;
        int consume = atom(node, pos, fl, fl2);
        if (consume < 0)
            return false;
        return step_tail(tail, pos + size_t(consume), fl2, end);
    }
    case PgNode::T::Anchor: {
        uint16_t fl2;
        if (atom(node, pos, fl, fl2) < 0)
            return false;
        return step_tail(tail, pos, fl2, end);
    }
    case PgNode::T::Alt: {
        auto saved = g;
        auto saved_mk = marked;
        for (auto& opt : node->kids) {
            size_t e2;
            if (step(opt.get(), pos, tail, fl, e2)) {
                end = e2;
                return true;
            }
            g = saved;
            marked = saved_mk;
        }
        return false;
    }
    case PgNode::T::Quant: {
        // Continuation-based: push a quantifier-continuation entry {node, 0};
        // step_tail drives iterations, passing the outer tail INTO the child
        // so nested quantifiers can give characters back (e.g.
        // (?:(\s+)(.*))?y on 'x y').
        Tail t2;
        t2.cont = tail.cont;
        t2.cont.insert(t2.cont.begin(), {node, 0});
        return step_tail(t2, pos, fl, end);
    }
    case PgNode::T::Group: {
        auto saved = g[node->grp];
        bool saved_mk = marked[node->grp];
        g[node->grp] = PgGrp{pos, pos, false};
        marked[node->grp] = false;
        Tail t2;
        t2.cont = tail.cont;
        t2.cont.insert(t2.cont.begin(), {&pg_marker_node, node->grp});
        if (!step(node->child.get(), pos, t2, fl, end)) {
            g[node->grp] = saved;
            marked[node->grp] = saved_mk;
            return false;
        }
        g[node->grp].valid = true; // .end set by marker pass-through
        marked[node->grp] = true;
        return true;
    }
    case PgNode::T::Look: {
        if (node->lb_len >= 0) {
            // lookbehind: body must match [pos - lb_len, pos) exactly
            if (pos < size_t(node->lb_len)) {
                if (node->neg)
                    return step_tail(tail, pos, fl, end);
                return false;
            }
            size_t e2;
            bool ok = step(node->child.get(), pos - size_t(node->lb_len),
                           Tail{}, fl, e2);
            if (node->neg)
                ok = !ok;
            else if (ok)
                ok = (e2 == pos);
            if (!ok)
                return false;
            return step_tail(tail, pos, fl, end);
        }
        // lookahead
        size_t e2;
        bool ok = step(node->child.get(), pos, Tail{}, fl, e2);
        if (node->neg)
            ok = !ok;
        if (!ok)
            return false;
        return step_tail(tail, pos, fl, end);
    }
    case PgNode::T::Backref: {
        if (node->bref == 0 || size_t(node->bref) >= g.size())
            return false;
        const PgGrp& gr = g[node->bref];
        if (!gr.valid && !marked[node->bref])
            return false;
        size_t len = gr.end - gr.start;
        if (pos + len > n)
            return false;
        for (size_t k = 0; k < len; ++k)
            if (cp[gr.start + k] != cp[pos + k])
                return false;
        return step_tail(tail, pos + len, fl, end);
    }
    }
    return false;
}

inline std::optional<PgMatch> pg_match(const PgPattern* pat, const uint32_t* cp,
                                       size_t n, size_t pos, uint16_t fl) {
    if (!pat || !pat->root)
        return std::nullopt;
    PgMatcher m(cp, n, pat->n_groups);
    size_t end;
    if (m.step(pat->root.get(), pos, Tail{}, fl, end)) {
        PgMatch r;
        r.end = end;
        r.g = m.g;
        return r;
    }
    return std::nullopt;
}

// ── state machine ───────────────────────────────────────────────────────────
struct PgTok {
    size_t start, end; // codepoint indices (relative to the view)
    std::string token; // dotted token class
};

inline const PgState* pg_find_state(const PgLexer* lx, const char* name,
                                    size_t len) {
    for (size_t i = 0; i < lx->n_states; ++i) {
        const char* s = lx->states[i].name;
        size_t sl = 0;
        while (s[sl])
            ++sl;
        if (sl == len && (len == 0 || memcmp(s, name, len) == 0))
            return &lx->states[i];
    }
    return nullptr;
}

inline bool pg_is_json_name(const char* name) {
    return strcmp(name, "json") == 0 || strcmp(name, "json-object") == 0;
}

// set-membership token remap (pygments get_tokens_unprocessed overrides):
// first entry in priority order with matching src token and the matched
// text in its name set wins; case-sensitive, byte-wise.
inline const char* pg_remap_token(const PgLexer* lx, const PgView& text,
                                  size_t s, size_t e, const char* tok) {
    if (!lx || !lx->remaps || e <= s)
        return tok;
    bool have = false;
    std::string t;
    for (size_t k = 0; k < lx->n_remaps; ++k) {
        const PgRemap& rm = lx->remaps[k];
        if (strcmp(rm.src, tok) != 0)
            continue;
        if (!have) {
            t = text.bytes(s, e);
            have = true;
        }
        size_t lo = 0, hi = rm.n_names;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            int c = strcmp(rm.names[mid], t.c_str());
            if (c == 0)
                return rm.dst;
            if (c < 0)
                lo = mid + 1;
            else
                hi = mid;
        }
    }
    return tok;
}

// case-insensitive lexer lookup (pygments get_lexer_by_name lowercases)
inline const PgLexer* pg_find_lexer_ci(std::string_view name) {
    auto lower = [](std::string_view s) {
        std::string r(s);
        for (auto& c : r)
            c = static_cast<char>(std::tolower(uint8_t(c)));
        return r;
    };
    static std::unordered_map<std::string, const PgLexer*> cache;
    const std::string key = lower(name);
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    const PgLexer* r = pg_find_lexer(key.c_str());
    cache.emplace(key, r);
    return r;
}
inline void emit_action(const PgAct* a, const PgLexer* lx, const PgView& text,
                        const PgPattern* pat, size_t mstart, const PgMatch& m,
                        std::vector<PgTok>& out, int depth);
inline void pg_lex_states_into(const PgLexer* lx, const char* stack_spec,
                               const PgView& text, std::vector<PgTok>& out,
                               int depth);
inline void pg_lex_sub(const PgLexer* sub, bool is_json,
                       const char* stack_spec, const PgView& text, size_t s,
                       size_t e, std::vector<PgTok>& out, int depth);
inline void pg_lex_json_view(const PgView& text, std::vector<PgTok>& out);
inline void pg_emit_md_code(const PgPattern* pat, const PgMatch& m,
                            const PgView& text, std::vector<PgTok>& out,
                            int depth);
inline void pg_emit_rst_code(const PgMatch& m, const PgView& text,
                             std::vector<PgTok>& out, int depth);

// debug hook: (start, end, state_name, rule_idx) per consumed step
inline std::function<void(size_t, size_t, const char*, int)> pg_trace_hook;

// markdown code-block callback (port of MarkdownLexer._handle_codeblock)
inline void pg_emit_md_code(const PgPattern* pat, const PgMatch& m,
                            const PgView& text, std::vector<PgTok>& out,
                            int depth) {
    auto P = [&](const char* name, const char* tok) {
        int i = pat ? pat->named_idx(name) : 0;
        if (i == 0 || size_t(i) >= m.g.size())
            return;
        const PgGrp& gr = m.g[i];
        if (gr.valid && gr.end > gr.start)
            out.push_back({gr.start, gr.end, tok});
    };
    P("initial", "Literal.String.Backtick");
    P("lang", "Literal.String.Backtick");
    int al = pat ? pat->named_idx("afterlang") : 0;
    bool has_after = al != 0 && size_t(al) < m.g.size() && m.g[al].valid;
    if (has_after) {
        P("whitespace", "Text.Whitespace");
        P("extra", "Text");
    }
    P("newline", "Text.Whitespace");
    int li = pat ? pat->named_idx("lang") : 0;
    const PgLexer* sub = nullptr;
    if (li != 0 && size_t(li) < m.g.size() && m.g[li].valid &&
        m.g[li].end > m.g[li].start) {
        std::string raw = text.bytes(m.g[li].start, m.g[li].end);
        size_t a = 0, b = raw.size();
        while (a < b && std::isspace(uint8_t(raw[a])))
            ++a;
        while (b > a && std::isspace(uint8_t(raw[b - 1])))
            --b;
        std::string lang(raw, a, b - a);
        if (!lang.empty())
            sub = pg_find_lexer_ci(lang);
    }
    int ci = pat ? pat->named_idx("code") : 0;
    if (ci != 0 && size_t(ci) < m.g.size() && m.g[ci].valid &&
        m.g[ci].end > m.g[ci].start) {
        if (depth < 16 && sub)
            pg_lex_sub(sub, false, nullptr, text, m.g[ci].start, m.g[ci].end,
                       out, depth + 1);
        else
            out.push_back({m.g[ci].start, m.g[ci].end, "Literal.String"});
    }
    P("terminator", "Literal.String.Backtick");
}

// restructuredtext code-block callback (port of RstLexer._handle_sourcecode)
inline void pg_emit_rst_code(const PgMatch& m, const PgView& text,
                             std::vector<PgTok>& out, int depth) {
    auto G = [&](int i) -> PgGrp {
        return (size_t(i) < m.g.size()) ? m.g[i] : PgGrp{0, 0, false};
    };
    static const char* fixed_toks[] = {
        "Punctuation", "Text", "Operator.Word",
        "Punctuation", "Text", "Keyword", "Text"};
    for (int i = 1; i <= 7; ++i) {
        const PgGrp& gr = G(i);
        if (gr.valid && gr.end > gr.start)
            out.push_back({gr.start, gr.end, fixed_toks[i - 1]});
    }
    const PgGrp& g6 = G(6);
    const PgGrp& g8 = G(8);
    const PgGrp& g11 = G(11);
    // code = g8 + g9 + g10 + g11 (contiguous by construction)
    size_t code_start = g8.valid ? g8.start : (g11.valid ? g11.end : 0);
    size_t code_end = g11.valid ? g11.end : code_start;
    const PgLexer* sub = nullptr;
    if (g6.valid && g6.end > g6.start) {
        std::string raw = text.bytes(g6.start, g6.end);
        size_t a = 0, b = raw.size();
        while (a < b && std::isspace(uint8_t(raw[a])))
            ++a;
        while (b > a && std::isspace(uint8_t(raw[b - 1])))
            --b;
        std::string lang(raw, a, b - a);
        if (!lang.empty())
            sub = pg_find_lexer_ci(lang);
    }
    if (code_end <= code_start)
        return;
    if (depth >= 16 || !sub) {
        out.push_back({code_start, code_end, "Literal.String"});
        return;
    }
    std::string code = text.bytes(code_start, code_end);
    const size_t ind_size = g8.valid ? (g8.end - g8.start) : 0;
    // splitlines(True) on codepoint text, then strip `ind_size` chars from
    // lines longer than that, recording the indent as insertions.
    std::string code2;
    std::vector<std::pair<size_t, size_t>> ins; // (pos, indent_len)
    {
        size_t ls = 0;
        while (ls < code.size()) {
            size_t le = code.find('\n', ls);
            bool last = (le == std::string::npos);
            if (last)
                le = code.size();
            std::string_view line(code.data() + ls, le - ls);
            if (line.size() > ind_size) {
                ins.push_back({code2.size(), std::min(ind_size, line.size())});
                code2 += std::string(line.substr(ind_size));
            } else {
                code2 += std::string(line);
            }
            if (last)
                break;
            code2 += '\n';
            ls = le + 1;
        }
    }
    // lex code2 with sub
    PgText t2(code2.data(), code2.size());
    std::vector<PgTok> toks2;
    pg_lex_states_into(sub, nullptr, PgView::of(t2), toks2, depth + 1);
    // do_insertions: split token spans at insertion positions
    size_t realpos = 0;
    bool have_real = false;
    size_t ii = 0;
    for (const PgTok& tok : toks2) {
        if (!have_real) {
            realpos = tok.start;
            have_real = true;
        }
        size_t oldi = 0;
        size_t tl = tok.end - tok.start;
        while (ii < ins.size() && tok.start + tl >= ins[ii].first) {
            size_t cut = ins[ii].first - tok.start;
            if (cut > oldi) {
                out.push_back({code_start + realpos,
                               code_start + realpos + (cut - oldi), tok.token});
                realpos += cut - oldi;
            }
            out.push_back({code_start + realpos,
                           code_start + realpos + ins[ii].second, "Text"});
            realpos += ins[ii].second;
            oldi = cut;
            ++ii;
        }
        if (oldi < tl) {
            out.push_back({code_start + realpos,
                           code_start + realpos + (tl - oldi), tok.token});
            realpos += tl - oldi;
        }
    }
    while (ii < ins.size()) {
        out.push_back({code_start + (have_real ? realpos : 0),
                       code_start + (have_real ? realpos : 0) + ins[ii].second,
                       "Text"});
        realpos += ins[ii].second;
        ++ii;
    }
}

// action emission
inline void emit_action(const PgAct* a, const PgLexer* lx, const PgView& text,
                        const PgPattern* pat, size_t mstart, const PgMatch& m,
                        std::vector<PgTok>& out, int depth) {
    if (!a)
        return;
    switch (a->k) {
    case PgAct::K::Null:
        break;
    case PgAct::K::Tok:
        out.push_back({mstart, m.end,
                       pg_remap_token(lx, text, mstart, m.end, a->tok)});
        break;
    case PgAct::K::Bg:
        for (size_t i = 0; i < a->bg_n; ++i) {
            const PgAct* sub = &a->bg[i];
            if (!sub || sub->k == PgAct::K::Null)
                continue;
            if (i + 1 >= m.g.size())
                continue;
            const PgGrp& gr = m.g[i + 1];
            if (!gr.valid)
                continue;
            if (sub->k == PgAct::K::Tok) {
                if (gr.end > gr.start)
                    out.push_back({gr.start, gr.end,
                                   pg_remap_token(lx, text, gr.start, gr.end,
                                                  sub->tok)});
            } else if (sub->k == PgAct::K::Using) {
                if (depth < 16 && gr.end > gr.start) {
                    const PgLexer* tgt = nullptr;
                    bool json = false;
                    if (sub->using_cls) {
                        if (strcmp(sub->using_cls, "self") == 0)
                            tgt = lx;
                        else if (strcmp(sub->using_cls, "JsonLexer") == 0)
                            json = true;
                        else
                            tgt = pg_find_lexer_by_class(sub->using_cls);
                    }
                    if (tgt || json)
                        pg_lex_sub(tgt, json, sub->using_stack, text, gr.start,
                                   gr.end, out, depth + 1);
                }
            }
        }
        break;
    case PgAct::K::Using: {
        const PgLexer* tgt = nullptr;
        bool json = false;
        if (a->using_cls) {
            if (strcmp(a->using_cls, "self") == 0)
                tgt = lx;
            else if (strcmp(a->using_cls, "JsonLexer") == 0)
                json = true;
            else
                tgt = pg_find_lexer_by_class(a->using_cls);
        }
        if (depth < 16 && (tgt || json) && m.end > mstart)
            pg_lex_sub(tgt, json, a->using_stack, text, mstart, m.end, out,
                       depth + 1);
        break;
    }
    case PgAct::K::SchemeDec: {
        std::string s = text.bytes(mstart, m.end);
        out.push_back({mstart, m.end,
                       s.find('.') != std::string::npos
                           ? "Literal.Number.Float"
                           : "Literal.Number.Integer"});
        break;
    }
    case PgAct::K::MdCode:
        pg_emit_md_code(pat, m, text, out, depth);
        break;
    case PgAct::K::RstCode:
        pg_emit_rst_code(m, text, out, depth);
        break;
    }
}

// core state machine
inline void pg_lex_states_into(const PgLexer* lx, const char* stack_spec,
                               const PgView& text, std::vector<PgTok>& out,
                               int depth) {
    (void)depth;
    const PgState* root = pg_find_state(lx, "root", 4);
    if (!root)
        return;
    std::vector<const PgState*> stack{root};
    if (stack_spec) {
        std::string_view ss(stack_spec);
        size_t start = 0;
        while (start < ss.size()) {
            size_t p = ss.find(';', start);
            if (p == std::string_view::npos)
                p = ss.size();
            if (p > start) {
                const PgState* st = pg_find_state(
                    lx, ss.data() + start, p - start);
                if (st)
                    stack.push_back(st);
            }
            if (p == ss.size())
                break;
            start = p + 1;
        }
    }
    const size_t n = text.n;
    size_t pos = 0;
    long long zero_matches = 0;
    while (true) {
        if (zero_matches > 100000) { // safety valve (pygments would hang)
            if (pos < n)
                out.push_back({pos, pos + 1, "Error"});
            break;
        }
        const PgState* st = stack.back();
        bool matched = false;
        for (size_t ri = 0; ri < st->n_rules; ++ri) {
            const PgRule& r = st->rules[ri];
            PgMatch m;
            bool hit = false;
            const PgPattern* pat = nullptr;
            if (r.pattern.empty()) {
                m.end = pos;
                m.g.assign(1, PgGrp{0, 0, false});
                hit = true;
            } else {
                pat = parse_cached(r.pattern, uint16_t(lx->flags));
                auto res = pg_match(pat, text.cp, n, pos, uint16_t(lx->flags));
                if (res) {
                    m = *res;
                    hit = true;
                }
            }
            if (hit) {
                if (r.action)
                    emit_action(r.action, lx, text, pat, pos, m, out, depth);
                if (pg_trace_hook)
                    pg_trace_hook(pos, m.end, st->name, (int)ri);
                if (m.end == pos)
                    ++zero_matches;
                else
                    zero_matches = 0;
                pos = m.end;
                if (!r.new_state.empty()) {
                    // split on ';' (tuple form); process items in order
                    size_t s2 = 0;
                    while (s2 < r.new_state.size()) {
                        size_t p2 = r.new_state.find(';', s2);
                        if (p2 == std::string_view::npos)
                            p2 = r.new_state.size();
                        std::string_view item =
                            r.new_state.substr(s2, p2 - s2);
                        if (item == "#push") {
                            stack.push_back(stack.back());
                        } else if (item == "#pop") {
                            if (stack.size() > 1)
                                stack.pop_back();
                        } else if (item.size() > 5 &&
                                   item.substr(0, 5) == "#del:") {
                            long long v = 0;
                            bool neg = false;
                            size_t k = 5;
                            if (k < item.size() && item[k] == '-') {
                                neg = true;
                                ++k;
                            }
                            for (; k < item.size(); ++k)
                                v = v * 10 + (item[k] - '0');
                            if (neg)
                                v = -v;
                            // del statestack[v:] (guarded, like pygments)
                            if (std::llabs(v) >= (long long)stack.size())
                                stack = {stack[0]};
                            else if (v > 0)
                                stack.resize(size_t(v));
                            else if (v < 0)
                                stack.resize(stack.size() + size_t(v));
                        } else if (!item.empty()) {
                            const PgState* st2 =
                                pg_find_state(lx, item.data(), item.size());
                            if (st2)
                                stack.push_back(st2);
                        }
                        if (p2 == r.new_state.size())
                            break;
                        s2 = p2 + 1;
                    }
                }
                matched = true;
                break;
            }
        }
        if (!matched) {
            if (pos >= n)
                break;
            if (text.at(pos) == '\n') {
                stack = {root};
                out.push_back({pos, pos + 1, "Text.Whitespace"});
                if (pg_trace_hook)
                    pg_trace_hook(pos, pos + 1, "RESETNL", -1);
                ++pos;
            } else {
                out.push_back({pos, pos + 1, "Error"});
                if (pg_trace_hook)
                    pg_trace_hook(pos, pos + 1, "ERROR", -1);
                ++pos;
            }
        }
    }
}

inline void pg_lex_sub(const PgLexer* sub, bool is_json,
                       const char* stack_spec, const PgView& text, size_t s,
                       size_t e, std::vector<PgTok>& out, int depth) {
    if (s >= e)
        return;
    if (depth >= 16 || (!sub && !is_json)) {
        // unknown sub-lexer → plain text (documented fallback)
        out.push_back({s, e, "Text"});
        return;
    }
    std::vector<PgTok> toks;
    PgView subview{text.cp + s, e - s, text.base, text.off + s};
    if (is_json)
        pg_lex_json_view(subview, toks);
    else
        pg_lex_states_into(sub, stack_spec, subview, toks, depth + 1);
    for (auto& t : toks) {
        t.start += s;
        t.end += s;
        out.push_back(std::move(t));
    }
}

// ── native JsonLexer (pygments/lexers/data.py:436 port) ─────────────────────
inline void pg_lex_json_cps(const uint32_t* cp, size_t n, size_t off,
                            std::vector<PgTok>& out) {
    auto in_set = [](uint32_t c, const char* s) {
        for (const char* p = s; *p; ++p)
            if (uint32_t(*p) == c)
                return true;
        return false;
    };
    bool in_string = false, in_escape = false;
    int in_unicode_escape = 0;
    bool in_whitespace = false, in_constant = false, in_number = false;
    bool in_float = false, in_punctuation = false, in_comment_single = false;
    bool in_comment_multiline = false, expecting_second_comment_opener = false;
    bool expecting_second_comment_closer = false;
    size_t start = 0;
    struct Q {
        size_t start, end;
        std::string token;
    };
    std::vector<Q> queue;
    auto flush_queue = [&](bool as_tag) {
        for (auto& q : queue) {
            std::string tok = q.token;
            if (as_tag && tok == "Literal.String.Double")
                tok = "Name.Tag";
            out.push_back({off + q.start, off + q.end, tok});
        }
        queue.clear();
    };

    for (size_t stop = 0; stop < n; ++stop) {
        const uint32_t character = cp[stop];
        if (in_string) {
            if (in_unicode_escape) {
                if (in_set(character, "0123456789abcdefABCDEF")) {
                    --in_unicode_escape;
                    if (in_unicode_escape == 0)
                        in_escape = false;
                } else {
                    in_unicode_escape = 0;
                    in_escape = false;
                }
            } else if (in_escape) {
                if (character == 'u')
                    in_unicode_escape = 4;
                else
                    in_escape = false;
            } else if (character == '\\') {
                in_escape = true;
            } else if (character == '"') {
                queue.push_back({start, stop + 1, "Literal.String.Double"});
                in_string = false;
                in_escape = false;
                in_unicode_escape = 0;
            }
            continue;
        } else if (in_whitespace) {
            if (in_set(character, " \n\r\t"))
                continue;
            queue.push_back({start, stop, "Text.Whitespace"});
            in_whitespace = false;
            // fall through
        } else if (in_constant) {
            if (in_set(character, "truefalsenull"))
                continue;
            out.push_back({off + start, off + stop, "Keyword.Constant"});
            in_constant = false;
            // fall through
        } else if (in_number) {
            if (in_set(character, "-0123456789"))
                continue;
            if (in_set(character, ".eE+")) {
                in_float = true;
                continue;
            }
            out.push_back({off + start, off + stop,
                           in_float ? "Literal.Number.Float"
                                    : "Literal.Number.Integer"});
            in_number = false;
            in_float = false;
            // fall through
        } else if (in_punctuation) {
            if (in_set(character, "{}[],"))
                continue;
            out.push_back({off + start, off + stop, "Punctuation"});
            in_punctuation = false;
            // fall through
        } else if (in_comment_single) {
            if (character != '\n')
                continue;
            queue.push_back({start, stop, "Comment.Single"});
            in_comment_single = false;
            // fall through
        } else if (in_comment_multiline) {
            if (character == '*')
                expecting_second_comment_closer = true;
            else if (expecting_second_comment_closer) {
                expecting_second_comment_closer = false;
                if (character == '/') {
                    queue.push_back({start, stop + 1, "Comment.Multiline"});
                    in_comment_multiline = false;
                }
            }
            continue;
        } else if (expecting_second_comment_opener) {
            expecting_second_comment_opener = false;
            if (character == '/') {
                in_comment_single = true;
                continue;
            } else if (character == '*') {
                in_comment_multiline = true;
                continue;
            }
            flush_queue(false);
            out.push_back({off + start, off + stop, "Error"});
            // fall through
        }

        start = stop;

        if (character == '"') {
            in_string = true;
        } else if (in_set(character, " \n\r\t")) {
            in_whitespace = true;
        } else if (character == 'f' || character == 'n' || character == 't') {
            flush_queue(false);
            in_constant = true;
        } else if (in_set(character, "-0123456789")) {
            flush_queue(false);
            in_number = true;
        } else if (character == ':') {
            flush_queue(true);
            in_punctuation = true;
        } else if (in_set(character, "{}[],")) {
            flush_queue(false);
            in_punctuation = true;
        } else if (character == '/') {
            expecting_second_comment_opener = true;
        } else {
            flush_queue(false);
            out.push_back({off + start, off + start + 1, "Error"});
        }
    }

    flush_queue(false);
    if (in_string)
        out.push_back({off + start, off + n, "Error"});
    else if (in_float)
        out.push_back({off + start, off + n, "Literal.Number.Float"});
    else if (in_number)
        out.push_back({off + start, off + n, "Literal.Number.Integer"});
    else if (in_constant)
        out.push_back({off + start, off + n, "Keyword.Constant"});
    else if (in_whitespace)
        out.push_back({off + start, off + n, "Text.Whitespace"});
    else if (in_punctuation)
        out.push_back({off + start, off + n, "Punctuation"});
    else if (in_comment_single)
        out.push_back({off + start, off + n, "Comment.Single"});
    else if (in_comment_multiline)
        out.push_back({off + start, off + n, "Error"});
    else if (expecting_second_comment_opener)
        out.push_back({off + start, off + n, "Error"});
}

inline void pg_lex_json_view(const PgView& text, std::vector<PgTok>& out) {
    pg_lex_json_cps(text.cp, text.n, 0, out);
}

// public entry points
inline void pg_lex_into(const PgLexer* lx, const PgView& text,
                        const char* stack_spec, std::vector<PgTok>& out) {
    pg_lex_states_into(lx, stack_spec, text, out, 0);
}

inline bool pg_lex_is_json(std::string_view name) {
    auto lower = [](std::string_view s) {
        std::string r(s);
        for (auto& c : r)
            c = static_cast<char>(std::tolower(uint8_t(c)));
        return r;
    };
    std::string k = lower(name);
    return pg_is_json_name(k.c_str());
}

inline void pg_lex_json(const PgView& text, std::vector<PgTok>& out) {
    pg_lex_json_view(text, out);
}

} // namespace detail
} // namespace rich::pg
