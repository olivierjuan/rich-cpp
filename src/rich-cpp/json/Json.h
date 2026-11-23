#pragma once
// JSON viewer — port of Python Rich's rich.json.JSON renderable.
//
// Pipeline parity (rich/json.py): json.loads(input) ->
// json.dumps(data, indent=2, ensure_ascii=False, allow_nan=True) ->
// JSONHighlighter(text) -> Text(no_wrap=True, overflow=None).
// Note: Console.print routes Text renderables through Text.join, which drops
// no_wrap — so printed JSON word-wraps at the console width with fold
// overflow (DEFAULT_OVERFLOW="fold"), exactly like plain Text.
//
// Parse failure never throws: the raw input is rendered as plain text
// (Python raises JSONDecodeError; this library degrades gracefully).

#include <rich_core.h>
#include <Text.h>
#include <Repr.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rich::json_view {

// ── Value tree (insertion-ordered; duplicate keys keep first position with
//    the last value, matching Python dict/json semantics) ────────────────────
struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool boolean{false};
    double number{0.0};
    bool is_int{false};
    long long int_value{0};
    std::string string;
    std::vector<Value> items; // Array
    std::vector<std::pair<std::string, Value>> object; // Object
};

namespace detail {

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
inline bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_';
}
inline bool is_hex(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline void encode_utf8(char32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// ── strict JSON parser (grammar: -?(0|[1-9]\d*)(\.\d+)?([eE][-+]?\d+)? plus
//    NaN / Infinity / -Infinity) ─────────────────────────────────────────────
class Parser {
public:
    explicit Parser(std::string_view text) : t_(text) {}

    bool parse(Value& out) {
        skip_ws();
        if (!parse_value(out)) return false;
        skip_ws();
        return pos_ == t_.size();
    }

private:
    std::string_view t_;
    size_t pos_{0};

    void skip_ws() {
        while (pos_ < t_.size()) {
            char c = t_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    bool literal(std::string_view lit) {
        if (t_.compare(pos_, lit.size(), lit) != 0) return false;
        pos_ += lit.size();
        return true;
    }

    bool parse_value(Value& out) {
        if (pos_ >= t_.size()) return false;
        const char c = t_[pos_];
        switch (c) {
            case '{': return parse_object(out);
            case '[': return parse_array(out);
            case '"': {
                out.type = Value::Type::String;
                return parse_string(out.string);
            }
            case 't':
                if (!literal("true")) return false;
                out.type = Value::Type::Bool;
                out.boolean = true;
                return true;
            case 'f':
                if (!literal("false")) return false;
                out.type = Value::Type::Bool;
                out.boolean = false;
                return true;
            case 'n':
                return literal("null") && (out.type = Value::Type::Null, true);
            case 'N':
                if (!literal("NaN")) return false;
                out.type = Value::Type::Number;
                out.is_int = false;
                out.number = std::nan("");
                return true;
            case 'I':
                if (!literal("Infinity")) return false;
                out.type = Value::Type::Number;
                out.is_int = false;
                out.number = std::numeric_limits<double>::infinity();
                return true;
            default:
                break;
        }
        if (c == '-' || is_digit(c)) return parse_number(out);
        return false;
    }

    bool parse_object(Value& out) {
        out.type = Value::Type::Object;
        ++pos_; // '{'
        skip_ws();
        if (pos_ < t_.size() && t_[pos_] == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            skip_ws();
            if (pos_ >= t_.size() || t_[pos_] != '"') return false;
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (pos_ >= t_.size() || t_[pos_] != ':') return false;
            ++pos_;
            skip_ws();
            Value v;
            if (!parse_value(v)) return false;
            // Duplicate keys: last value wins, first position kept.
            bool replaced = false;
            for (auto& [k, val] : out.object) {
                if (k == key) {
                    val = std::move(v);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) out.object.emplace_back(std::move(key), std::move(v));
            skip_ws();
            if (pos_ >= t_.size()) return false;
            if (t_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (t_[pos_] == '}') {
                ++pos_;
                return true;
            }
            return false;
        }
    }

    bool parse_array(Value& out) {
        out.type = Value::Type::Array;
        ++pos_; // '['
        skip_ws();
        if (pos_ < t_.size() && t_[pos_] == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            skip_ws();
            Value v;
            if (!parse_value(v)) return false;
            out.items.push_back(std::move(v));
            skip_ws();
            if (pos_ >= t_.size()) return false;
            if (t_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (t_[pos_] == ']') {
                ++pos_;
                return true;
            }
            return false;
        }
    }

    uint32_t parse_hex4() {
        if (pos_ + 4 > t_.size()) return 0xFFFFFFFFu;
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = t_[pos_ + i];
            v <<= 4;
            if (is_digit(c)) v += static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v += static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v += static_cast<uint32_t>(c - 'A' + 10);
            else return 0xFFFFFFFFu;
        }
        pos_ += 4;
        return v;
    }

    bool parse_string(std::string& out) {
        ++pos_; // '"'
        out.clear();
        while (pos_ < t_.size()) {
            char c = t_[pos_];
            if (c == '"') {
                ++pos_;
                return true;
            }
            if (c == '\\') {
                ++pos_;
                if (pos_ >= t_.size()) return false;
                char e = t_[pos_];
                switch (e) {
                    case '"': out += '"'; ++pos_; break;
                    case '\\': out += '\\'; ++pos_; break;
                    case '/': out += '/'; ++pos_; break;
                    case 'b': out += '\b'; ++pos_; break;
                    case 'f': out += '\f'; ++pos_; break;
                    case 'n': out += '\n'; ++pos_; break;
                    case 'r': out += '\r'; ++pos_; break;
                    case 't': out += '\t'; ++pos_; break;
                    case 'u': {
                        ++pos_; // past 'u'
                        uint32_t cp = parse_hex4();
                        if (cp == 0xFFFFFFFFu) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= t_.size() &&
                            t_[pos_] == '\\' && t_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint32_t lo = parse_hex4();
                            if (lo == 0xFFFFFFFFu || lo < 0xDC00 || lo > 0xDFFF) {
                                encode_utf8(cp, out); // unpaired high surrogate
                            } else {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                encode_utf8(cp, out);
                            }
                        } else {
                            encode_utf8(cp, out);
                        }
                        break;
                    }
                    default:
                        return false;
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return false; // strict: raw control characters are invalid
            } else {
                out += c;
                ++pos_;
            }
        }
        return false; // unterminated
    }

    bool parse_number(Value& out) {
        const size_t start = pos_;
        bool neg = false;
        if (pos_ < t_.size() && t_[pos_] == '-') {
            neg = true;
            ++pos_;
        }
        // Python json NUMBER_RE also matches -Infinity / -NaN.
        if (pos_ < t_.size() && (t_[pos_] == 'I' || t_[pos_] == 'N')) {
            const bool inf = t_[pos_] == 'I';
            const std::string_view lit = inf ? "Infinity" : "NaN";
            if (!literal(lit)) return false;
            out.type = Value::Type::Number;
            out.is_int = false;
            if (inf) out.number = neg ? -std::numeric_limits<double>::infinity()
                                      : std::numeric_limits<double>::infinity();
            else out.number = std::nan("");
            return true;
        }
        if (pos_ >= t_.size()) return false;
        if (t_[pos_] == '0') {
            ++pos_;
        } else if (t_[pos_] >= '1' && t_[pos_] <= '9') {
            while (pos_ < t_.size() && is_digit(t_[pos_])) ++pos_;
        } else {
            return false;
        }
        bool is_float = false;
        if (pos_ < t_.size() && t_[pos_] == '.') {
            is_float = true;
            ++pos_;
            if (pos_ >= t_.size() || !is_digit(t_[pos_])) return false;
            while (pos_ < t_.size() && is_digit(t_[pos_])) ++pos_;
        }
        if (pos_ < t_.size() && (t_[pos_] == 'e' || t_[pos_] == 'E')) {
            is_float = true;
            ++pos_;
            if (pos_ < t_.size() && (t_[pos_] == '+' || t_[pos_] == '-')) ++pos_;
            if (pos_ >= t_.size() || !is_digit(t_[pos_])) return false;
            while (pos_ < t_.size() && is_digit(t_[pos_])) ++pos_;
        }
        const std::string lit(t_.substr(start, pos_ - start));
        out.type = Value::Type::Number;
        if (!is_float) {
            char* endp = nullptr;
            const long long v = std::strtoll(lit.c_str(), &endp, 10);
            if (endp == lit.c_str() + lit.size()) {
                out.is_int = true;
                out.int_value = v;
            } else {
                // Out of int64 range: approximate as double (Python keeps
                // arbitrary-precision ints; dumps will differ for >19 digits).
                out.is_int = false;
                out.number = std::stod(lit);
            }
        } else {
            out.is_int = false;
            out.number = std::stod(lit);
        }
        return true;
    }
};

// ── json.dumps(data, indent=2, ensure_ascii=False, allow_nan=True) ──────────
inline void escape_string(std::string_view s, std::string& out) {
    out += '"';
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    out += '"';
}

inline void dumps(const Value& v, int depth, std::string& out) {
    using T = Value::Type;
    switch (v.type) {
        case T::Null:
            out += "null";
            break;
        case T::Bool:
            out += v.boolean ? "true" : "false";
            break;
        case T::Number:
            if (v.is_int) {
                out += std::to_string(v.int_value);
            } else {
                const double d = v.number;
                if (std::isnan(d)) out += "NaN";
                else if (std::isinf(d)) out += d < 0 ? "-Infinity" : "Infinity";
                else out += repr::py_str_float(d);
            }
            break;
        case T::String:
            escape_string(v.string, out);
            break;
        case T::Array:
            if (v.items.empty()) {
                out += "[]";
                break;
            }
            out += "[\n";
            for (size_t i = 0; i < v.items.size(); ++i) {
                out.append(static_cast<size_t>(2 * (depth + 1)), ' ');
                dumps(v.items[i], depth + 1, out);
                out += (i + 1 < v.items.size()) ? ",\n" : "\n";
            }
            out.append(static_cast<size_t>(2 * depth), ' ');
            out += "]";
            break;
        case T::Object:
            if (v.object.empty()) {
                out += "{}";
                break;
            }
            out += "{\n";
            for (size_t i = 0; i < v.object.size(); ++i) {
                out.append(static_cast<size_t>(2 * (depth + 1)), ' ');
                escape_string(v.object[i].first, out);
                out += ": ";
                dumps(v.object[i].second, depth + 1, out);
                out += (i + 1 < v.object.size()) ? ",\n" : "\n";
            }
            out.append(static_cast<size_t>(2 * depth), ' ');
            out += "}";
            break;
    }
}

// ── JSONHighlighter (rich/highlighter.py) token styles ──────────────────────
// json.brace  Style(bold=True)
// json.bool_true  Style(color="bright_green", italic=True)
// json.bool_false Style(color="bright_red", italic=True)
// json.null   Style(color="magenta", italic=True)
// json.number Style(color="cyan", bold=True, italic=False)
// json.str    Style(color="green", italic=False, bold=False)
// json.key    Style(color="blue", bold=True) — combined over json.str, so
//              italic=False (explicit) is inherited.
inline const Style& style_brace() {
    static const Style s{.bold = true};
    return s;
}
inline const Style& style_bool_true() {
    static const Style s{.italic = true, .color = Color::from_ansi(92)};
    return s;
}
inline const Style& style_bool_false() {
    static const Style s{.italic = true, .color = Color::from_ansi(91)};
    return s;
}
inline const Style& style_null() {
    static const Style s{.italic = true, .color = Color::from_ansi(35)};
    return s;
}
inline const Style& style_number() {
    static const Style s{.bold = true, .color = Color::from_ansi(36)};
    static bool init = false;
    if (!init) {
        const_cast<Style&>(s).explicit_bits |= 0x04; // italic=False explicit
        init = true;
    }
    return s;
}
inline const Style& style_str() {
    static const Style s{.color = Color::from_ansi(32)};
    static bool init = false;
    if (!init) {
        const_cast<Style&>(s).explicit_bits |= 0x01 | 0x04; // bold=False, italic=False
        init = true;
    }
    return s;
}
inline const Style& style_key() {
    static const Style s{.bold = true, .color = Color::from_ansi(34)};
    static bool init = false;
    if (!init) {
        const_cast<Style&>(s).explicit_bits |= 0x04; // italic=False via combine
        init = true;
    }
    return s;
}

// Combined regex alternative (leftmost-first, in order):
//   brace [\{\[\(\)\]\}]
//   true/false/null with \b
//   number (?<!\w)\-?[0-9]+\.?[0-9]*(e[+-]?\d+?)?\b | 0x[0-9a-fA-F]*
//   str (?<![\\\w])(b?\".*?(?<!\\)\")
enum class Tok { Gap, Brace, True, False, Null, Number, Str, Key };

inline size_t try_number_alt1(std::string_view t, size_t pos) {
    if (pos > 0 && is_word(t[pos - 1])) return std::string_view::npos; // (?<!\w)
    size_t i = pos;
    if (i < t.size() && t[i] == '-') ++i;
    if (i >= t.size() || !is_digit(t[i])) return std::string_view::npos;
    while (i < t.size() && is_digit(t[i])) ++i;
    if (i < t.size() && t[i] == '.') {
        ++i;
        while (i < t.size() && is_digit(t[i])) ++i;
    }
    if (i < t.size() && t[i] == 'e') { // lowercase e only (regex literal)
        size_t j = i + 1;
        if (j < t.size() && (t[j] == '+' || t[j] == '-')) ++j;
        if (j < t.size() && is_digit(t[j])) {
            while (j < t.size() && is_digit(t[j])) ++j;
            if (j >= t.size() || !is_word(t[j])) i = j; // \b holds
        }
    }
    if (i >= t.size() || !is_word(t[i])) return i;
    return std::string_view::npos;
}

inline size_t match_number(std::string_view t, size_t pos) {
    const size_t a1 = try_number_alt1(t, pos);
    if (a1 != std::string_view::npos) return a1;
    // 0x[0-9a-fA-F]* — no lookbehind in the Python regex.
    if (pos + 1 < t.size() && t[pos] == '0' && t[pos + 1] == 'x') {
        size_t k = pos + 2;
        while (k < t.size() && is_hex(t[k])) ++k;
        return k;
    }
    return std::string_view::npos;
}

inline size_t match_str(std::string_view t, size_t pos) {
    if (pos > 0 && (t[pos - 1] == '\\' || is_word(t[pos - 1]))) return std::string_view::npos;
    size_t q = pos;
    if (q < t.size() && t[q] == 'b') {
        if (q + 1 >= t.size() || t[q + 1] != '"') return std::string_view::npos;
        ++q;
    }
    if (q >= t.size() || t[q] != '"') return std::string_view::npos;
    size_t i = q + 1;
    while (i < t.size()) {
        const char c = t[i];
        if (c == '\n') return std::string_view::npos; // no DOTALL
        if (c == '"' && t[i - 1] != '\\') return i + 1;
        ++i;
    }
    return std::string_view::npos;
}

inline bool match_word(std::string_view t, size_t pos, std::string_view w) {
    if (pos > 0 && is_word(t[pos - 1])) return false;
    if (t.compare(pos, w.size(), w) != 0) return false;
    return pos + w.size() >= t.size() || !is_word(t[pos + w.size()]);
}

struct Token {
    size_t start;
    size_t end;
    Tok kind;
};

// Port of JSONHighlighter.highlight: one combined regex pass, then the key
// pass (each str followed by optional whitespace then ':' becomes json.key,
// overwriting json.str on the same range).
inline std::vector<Token> scan(std::string_view text) {
    std::vector<Token> toks;
    size_t pos = 0;
    while (pos < text.size()) {
        Tok kind = Tok::Gap;
        size_t end = pos + 1;
        const char c = text[pos];
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == '(' || c == ')') {
            kind = Tok::Brace;
        } else if (match_word(text, pos, "true")) {
            kind = Tok::True;
            end = pos + 4;
        } else if (match_word(text, pos, "false")) {
            kind = Tok::False;
            end = pos + 5;
        } else if (match_word(text, pos, "null")) {
            kind = Tok::Null;
            end = pos + 4;
        } else if ((end = match_number(text, pos)) != std::string_view::npos) {
            kind = Tok::Number;
        } else if ((end = match_str(text, pos)) != std::string_view::npos) {
            kind = Tok::Str;
        }
        if (kind == Tok::Gap) {
            ++pos;
            continue;
        }
        toks.push_back({pos, end, kind});
        pos = end;
    }
    // Key pass (Python: re.finditer(JSON_STR) + lookahead for ':').
    for (auto& tok : toks) {
        if (tok.kind != Tok::Str) continue;
        size_t cursor = tok.end;
        while (cursor < text.size()) {
            const char c = text[cursor];
            ++cursor;
            if (c == ':') {
                tok.kind = Tok::Key;
                break;
            }
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
            break;
        }
    }
    return toks;
}

inline const Style& style_for(Tok kind) {
    switch (kind) {
        case Tok::Brace: return style_brace();
        case Tok::True: return style_bool_true();
        case Tok::False: return style_bool_false();
        case Tok::Null: return style_null();
        case Tok::Number: return style_number();
        case Tok::Str: return style_str();
        case Tok::Key: return style_key();
        default: {
            static const Style empty{};
            return empty;
        }
    }
}

// JSON Text render: each token/gap piece is one segment (Python Text.render
// emits per span boundary — adjacent same-style tokens stay separate, e.g.
// "[]" yields two bold brace segments), gaps carry an explicit empty Style().
inline std::vector<Segment> highlight(std::string_view text) {
    auto toks = scan(text);
    std::vector<Segment> out;
    out.reserve(toks.size() + 1);
    size_t prev = 0;
    for (const auto& tok : toks) {
        if (tok.start > prev)
            out.push_back({std::string(text.substr(prev, tok.start - prev)), Style{}});
        out.push_back({std::string(text.substr(tok.start, tok.end - tok.start)), style_for(tok.kind)});
        prev = tok.end;
    }
    if (prev < text.size()) out.push_back({std::string(text.substr(prev)), Style{}});
    return out;
}

} // namespace detail

// ── renderable ──────────────────────────────────────────────────────────────
struct Json {
    bool valid{false};
    std::string text; // dumps output (valid) or raw input (fallback)

    std::vector<rich::Segment> render(const rich::Console&, const rich::Options& opt) const {
        const size_t width = (opt.width > 0) ? opt.width : 80;
        const std::string_view content(text);
        const auto segs = valid ? detail::highlight(content)
                                : std::vector<rich::Segment>{{std::string(content), rich::detail::kNoneStyle()}};
        // Text semantics: word wrap (fold) at console width, trailing
        // end="\n" as a style-None segment.
        auto out = text::wrap_detail::wrap_styled(segs, width, text::Overflow::Fold,
                                                  rich::detail::kNoneStyle());
        out.push_back({"\n", rich::detail::kNoneStyle()});
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        // Python Text.__rich_measure__: min = longest word, max = widest line.
        std::string_view sv(text);
        size_t longest_word = 0;
        size_t start = 0;
        for (size_t i = 0; i <= sv.size(); ++i) {
            if (i == sv.size() || sv[i] == '\n') {
                auto line = sv.substr(start, i - start);
                for (const auto& w : text::wrap_detail::words(line)) {
                    std::string_view core = w.text;
                    while (!core.empty() && (core.back() == ' ' || core.back() == '\t'))
                        core.remove_suffix(1);
                    longest_word = std::max(longest_word, unicode::string_cell_len(core));
                }
                start = i + 1;
            }
        }
        return {1, longest_word};
    }
};

} // namespace rich::json_view

// ADL render/measure hooks — must be declared before the json() factory so
// the AnyRenderable Renderable constraint can find them at instantiation.
namespace rich {

inline std::vector<Segment> render_item(const json_view::Json& j, const Console& c, const Options& o) {
    return j.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const json_view::Json& j, const Console& c, const Options& o) {
    return j.measure(c, o);
}

} // namespace rich

namespace rich::json_view {

// console.print(rich::json_view::json("{\"a\": 1}"))
inline rich::AnyRenderable json(std::string_view json_str) {
    Json j;
    detail::Parser p(json_str);
    Value v;
    if (p.parse(v)) {
        detail::dumps(v, 0, j.text);
        j.valid = true;
    } else {
        j.text = std::string(json_str);
        j.valid = false;
    }
    return rich::AnyRenderable(std::move(j));
}

} // namespace rich::json_view
