#pragma once
// Syntax renderable — port of Python Rich's rich.syntax.Syntax (monokai theme).
//
// Pipeline parity (rich/syntax.py):
//   _process_code (trailing \n, dedent, expandtabs)
//   -> regex lexer (python / cpp / json / plain text fallback) emitting
//      pygments token classes
//   -> monokai token styles (PygmentsStyle hierarchy, resolved)
//   -> layout: simple path (render_lines: crop+pad to code width) or complex
//      path (line split, optional line_range slice, indent guides, per-line
//      crop/pad or word wrap, optional line-number gutter).
//
// Layout quirk preserved from Python: with line_numbers, each wrapped line is
// preceded by a col_width+1 gutter, so total line width is
// col_width + 1 + code_width (the console width when code_width=None).
// Continuation lines of a wrapped line carry the blank gutter.
//
// Lexers are regex-based ports that match pygments' token streams for the
// standard code shapes this library targets; unknown lexer names fall back to
// plain text (single Token.Text), exactly like Python's "text" lexer.

#include <rich_core.h>
#include <Text.h>

#include <cctype>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rich::highlight {

struct SyntaxOptions {
    bool dedent = false;
    bool line_numbers = false;
    int start_line = 1;
    int line_range_start = 0; // 0 = no range; else 1-based, end exclusive
    int line_range_end = 0;
    std::set<int> highlight_lines;
    int code_width = 0; // 0 = None (fill the console)
    int tab_size = 4;
    bool word_wrap = false;
    bool indent_guides = false;
    // Python Syntax padding=1 (markdown code blocks): the rendered code is
    // wrapped in Padding(pad, style=monokai-bg, expand=True) — one blank
    // bg-filled line at the top and bottom at the full console width and one
    // bg-filled cell on each side of every code line.
    int padding = 0;
};

struct Syntax {
    std::string code;
    std::string lexer; // "python" | "cpp" | "json" | other -> plain text
    SyntaxOptions opts{};

    std::vector<rich::Segment> render(const rich::Console& c, const rich::Options& opt) const;
    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const;
};

namespace detail {

// ── token classes (pygments hierarchy flattened to what monokai styles) ─────
enum class Tok {
    Text,
    Whitespace,
    Comment,      // Comment / .Single / .Multiline
    Keyword,      // Keyword / .Constant / .Type / .Declaration
    KeywordNamespace, // Keyword.Namespace
    Name,
    NameFunc,     // Name.Function
    NameBuiltin,  // Name.Builtin
    NameClass,    // Name.Class
    Operator,     // Operator
    OperatorWord, // Operator.Word (-> Operator in monokai)
    Punctuation,
    Number,       // Literal.Number.*
    String,       // Literal.String
    StringDoc,    // Literal.String.Doc
};

struct Token {
    Tok kind;
    std::string text;
};

// monokai background (#272822)
inline const Color& mono_bg() {
    static const Color c = Color::rgb(0x27, 0x28, 0x22);
    return c;
}

// monokai style for a token class (pygments style_for_token hierarchy:
// most-specific entry, walking up; the base Token entry #f8f8f2 covers all).
// Python styles here always carry explicit bold/italic/underline=False, so
// the flags are reported as set (explicit_bits) for repr parity.
inline Style token_style(Tok k) {
    Style s;
    s.bg_color = mono_bg();
    s.explicit_bits = 0x01 | 0x04 | 0x08; // bold, italic, underline explicit False
    switch (k) {
    case Tok::Comment:
        s.color = Color::rgb(0x95, 0x90, 0x77);
        break;
    case Tok::Keyword:
        s.color = Color::rgb(0x66, 0xD9, 0xEF);
        break;
    case Tok::KeywordNamespace:
        s.color = Color::rgb(0xFF, 0x46, 0x89);
        break;
    case Tok::NameFunc:
    case Tok::NameBuiltin:
    case Tok::NameClass:
        s.color = Color::rgb(0xA6, 0xE2, 0x2E);
        break;
    case Tok::Operator:
    case Tok::OperatorWord:
        s.color = Color::rgb(0xFF, 0x46, 0x89);
        break;
    case Tok::Number:
        s.color = Color::rgb(0xAE, 0x81, 0xFF);
        break;
    case Tok::String:
    case Tok::StringDoc:
        s.color = Color::rgb(0xE6, 0xDB, 0x74);
        break;
    default: // Text, Whitespace, Name, Punctuation -> #f8f8f2
        s.color = Color::rgb(0xF8, 0xF8, 0xF2);
        break;
    }
    return s;
}

inline bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
inline bool is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}
inline bool is_space_tab(char c) { return c == ' ' || c == '\t'; }

// ── python lexer ─────────────────────────────────────────────────────────────
// Matches pygments PythonLexer on the common corpus: \n -> Whitespace;
// horizontal ws -> Text (expr state rule [^\S\n]+); the docstring rule
// ^(\s*)("""...""") takes the indent as Whitespace; #... -> Comment;
// def/class consume following ws as Whitespace and name the next word
// Name.Function/Name.Class; in/is/not/and/or -> Operator.Word.
inline std::vector<Token> lex_python(std::string_view code) {
    std::vector<Token> out;
    auto push = [&](Tok k, size_t a, size_t b) {
        if (b > a)
            out.push_back({k, std::string(code.substr(a, b - a))});
    };
    static const char* const kw[] = {
        "def",     "class",   "if",     "elif",    "else",   "for",     "while",
        "return",  "import",  "from",   "as",      "with",   "try",     "except",
        "finally", "raise",   "pass",   "break",   "continue", "global", "nonlocal",
        "assert",  "del",     "yield",  "lambda",  "async",  "await",   "None",
        "True",    "False",   "is",     "not",
    };
    auto is_kw = [](std::string_view w) -> int {
        for (size_t i = 0; i < sizeof(kw) / sizeof(kw[0]); ++i)
            if (w == kw[i])
                return static_cast<int>(i);
        return -1;
    };
    static const char* const builtins[] = {
        "print", "range", "len", "str", "int", "float", "list", "dict", "set",
        "tuple", "bool", "bytes", "open", "type", "isinstance", "issubclass",
        "super", "enumerate", "zip", "map", "filter", "sorted", "reversed",
        "repr", "abs", "min", "max", "sum", "any", "all", "round", "format",
        "iter", "next", "property", "staticmethod", "classmethod", "Exception",
        "ValueError", "TypeError", "KeyError", "IndexError", "RuntimeError",
        "ImportError", "NameError", "OSError", "StopIteration",
    };
    auto is_builtin = [](std::string_view w) {
        for (const char* b : builtins)
            if (w == b)
                return true;
        return false;
    };

    const size_t n = code.size();
    size_t i = 0;
    bool after_def = false, after_class = false;
    while (i < n) {
        const char c = code[i];
        if (c == '\n') {
            push(Tok::Whitespace, i, i + 1);
            ++i;
            after_def = after_class = false;
            continue;
        }
        if (is_space_tab(c)) {
            size_t j = i;
            while (j < n && is_space_tab(code[j]))
                ++j;
            push(Tok::Text, i, j);
            i = j;
            continue;
        }
        if (c == '#') {
            size_t j = i;
            while (j < n && code[j] != '\n')
                ++j;
            push(Tok::Comment, i, j);
            i = j;
            continue;
        }
        // docstring (pygments ^(\s*)(\"\"\"...\"\"\")): a triple quote at the
        // first non-whitespace of a line; the whole run is String.Doc (the
        // leading indent keeps its Text/Whitespace color — same in monokai).
        if (c == '"' && i + 2 < n && code[i + 1] == '"' && code[i + 2] == '"') {
            size_t ls = i;
            while (ls > 0 && code[ls - 1] != '\n')
                --ls;
            bool only_ws = true;
            for (size_t p = ls; p < i; ++p)
                if (!is_space_tab(code[p])) {
                    only_ws = false;
                    break;
                }
            if (only_ws) {
                const size_t k = code.find("\"\"\"", i + 3);
                if (k != std::string_view::npos) {
                    push(Tok::StringDoc, i, k + 3);
                    i = k + 3;
                    continue;
                }
            }
        }
        if (c == '"' || c == '\'') {
            const char q = c;
            size_t j = i + 1;
            while (j < n && code[j] != q) {
                if (code[j] == '\\')
                    ++j;
                ++j;
            }
            if (j < n)
                ++j;
            push(Tok::String, i, j);
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = i;
            while (j < n && is_ident_char(code[j]))
                ++j;
            const std::string_view w = code.substr(i, j - i);
            const int kwi = is_kw(w);
            if (kwi == 0) { // def — following ws is Whitespace, name -> Name.Function
                push(Tok::Keyword, i, j);
                size_t k = j;
                while (k < n && is_space_tab(code[k]))
                    ++k;
                push(Tok::Whitespace, j, k);
                i = k;
                after_def = true;
                after_class = false;
            } else if (kwi == 1) { // class
                push(Tok::Keyword, i, j);
                size_t k = j;
                while (k < n && is_space_tab(code[k]))
                    ++k;
                push(Tok::Whitespace, j, k);
                i = k;
                after_class = true;
                after_def = false;
            } else if (after_def) {
                push(Tok::NameFunc, i, j);
                after_def = after_class = false;
                i = j;
            } else if (after_class) {
                push(Tok::NameClass, i, j);
                after_def = after_class = false;
                i = j;
            } else if (kwi >= 2 && kwi <= 26) {
                push(Tok::Keyword, i, j);
                after_def = after_class = false;
                i = j;
            } else if (kwi >= 27) { // is / not
                push(Tok::OperatorWord, i, j);
                after_def = after_class = false;
                i = j;
            } else if (w == "in" || w == "and" || w == "or") {
                push(Tok::OperatorWord, i, j);
                after_def = after_class = false;
                i = j;
            } else if (is_builtin(w)) {
                push(Tok::NameBuiltin, i, j);
                after_def = after_class = false;
                i = j;
            } else {
                push(Tok::Name, i, j);
                after_def = after_class = false;
                i = j;
            }
            continue;
        }
        if (c >= '0' && c <= '9') {
            size_t j = i;
            if (c == '0' && j + 1 < n && (code[j + 1] == 'x' || code[j + 1] == 'X')) {
                j += 2;
                while (j < n && isxdigit(static_cast<unsigned char>(code[j])))
                    ++j;
            } else {
                while (j < n && isdigit(static_cast<unsigned char>(code[j])))
                    ++j;
                if (j < n && code[j] == '.') {
                    ++j;
                    while (j < n && isdigit(static_cast<unsigned char>(code[j])))
                        ++j;
                }
                if (j < n && (code[j] == 'e' || code[j] == 'E')) {
                    size_t k = j + 1;
                    if (k < n && (code[k] == '+' || code[k] == '-'))
                        ++k;
                    if (k < n && isdigit(static_cast<unsigned char>(code[k]))) {
                        k = j + 1;
                        while (k < n && (k == j + 1 ||
                                         isdigit(static_cast<unsigned char>(code[k])) ||
                                         (code[k] == '+' || code[k] == '-')))
                            ++k;
                        j = k;
                    }
                }
            }
            push(Tok::Number, i, j);
            i = j;
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' ||
            c == '>' || c == '=' || c == '!' || c == '&' || c == '|' || c == '^' ||
            c == '~' || c == '@') {
            push(Tok::Operator, i, i + 1);
            ++i;
            continue;
        }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == ',' || c == ':' || c == ';' || c == '.') {
            push(Tok::Punctuation, i, i + 1);
            ++i;
            continue;
        }
        push(Tok::Text, i, i + 1);
        ++i;
    }
    return out;
}

// ── cpp lexer ────────────────────────────────────────────────────────────────
// All whitespace (incl. \n) -> Whitespace; // and /* */ comments (// takes the
// trailing newline); type keywords -> Keyword.Type, control -> Keyword; a name
// followed by optional ws + '(' -> Name.Function; single-char operators.
inline std::vector<Token> lex_cpp(std::string_view code) {
    std::vector<Token> out;
    auto push = [&](Tok k, size_t a, size_t b) {
        if (b > a)
            out.push_back({k, std::string(code.substr(a, b - a))});
    };
    auto kw_type = [](std::string_view w) {
        return w == "int" || w == "float" || w == "double" || w == "char" ||
               w == "bool" || w == "void" || w == "long" || w == "short" ||
               w == "unsigned" || w == "signed" || w == "const" ||
               w == "constexpr" || w == "static" || w == "auto" || w == "struct" ||
               w == "union" || w == "enum" || w == "class";
    };
    auto kw_ctrl = [](std::string_view w) {
        return w == "for" || w == "if" || w == "else" || w == "while" ||
               w == "switch" || w == "case" || w == "default" || w == "break" ||
               w == "continue" || w == "do" || w == "return" || w == "goto" ||
               w == "new" || w == "delete" || w == "sizeof" || w == "namespace" ||
               w == "using" || w == "template" || w == "typename" ||
               w == "public" || w == "private" || w == "protected" ||
               w == "virtual" || w == "final" || w == "override" || w == "try" ||
               w == "catch" || w == "throw" || w == "noexcept" || w == "this" ||
               w == "operator" || w == "inline" || w == "extern" || w == "volatile";
    };

    const size_t n = code.size();
    size_t i = 0;
    while (i < n) {
        const char c = code[i];
        if (is_space_tab(c) || c == '\n') {
            size_t j = i;
            while (j < n && (is_space_tab(code[j]) || code[j] == '\n'))
                ++j;
            push(Tok::Whitespace, i, j);
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && code[i + 1] == '/') {
            size_t j = i + 2;
            while (j < n && code[j] != '\n')
                ++j;
            if (j < n)
                ++j; // include the newline, like pygments
            push(Tok::Comment, i, j);
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && code[i + 1] == '*') {
            size_t j = code.find("*/", i + 2);
            j = (j == std::string_view::npos) ? n : j + 2;
            push(Tok::Comment, i, j);
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = i;
            while (j < n && is_ident_char(code[j]))
                ++j;
            const std::string_view w = code.substr(i, j - i);
            if (kw_type(w))
                push(Tok::Keyword, i, j); // Keyword.Type -> Keyword in monokai
            else if (kw_ctrl(w))
                push(Tok::Keyword, i, j);
            else {
                size_t k = j;
                while (k < n && is_space_tab(code[k]))
                    ++k;
                push(k < n && code[k] == '(' ? Tok::NameFunc : Tok::Name, i, j);
            }
            i = j;
            continue;
        }
        if (c >= '0' && c <= '9') {
            size_t j = i;
            while (j < n && isxdigit(static_cast<unsigned char>(code[j])))
                ++j;
            while (j < n && (code[j] == '.' || code[j] == 'f' || code[j] == 'F' ||
                             code[j] == 'l' || code[j] == 'L' || code[j] == 'u' ||
                             code[j] == 'U')) {
                if (code[j] == '.' && (j + 1 >= n || !isdigit(static_cast<unsigned char>(code[j + 1]))))
                    break;
                ++j;
            }
            push(Tok::Number, i, j);
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            const char q = c;
            size_t j = i + 1;
            while (j < n && code[j] != q) {
                if (code[j] == '\\')
                    ++j;
                ++j;
            }
            if (j < n)
                ++j;
            push(Tok::String, i, j);
            i = j;
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' ||
            c == '>' || c == '=' || c == '!' || c == '&' || c == '|' || c == '^' ||
            c == '~' || c == '?') {
            push(Tok::Operator, i, i + 1);
            ++i;
            continue;
        }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == ';' || c == ',' || c == ':' || c == '.') {
            push(Tok::Punctuation, i, i + 1);
            ++i;
            continue;
        }
        push(Tok::Text, i, i + 1);
        ++i;
    }
    return out;
}

// ── json lexer ───────────────────────────────────────────────────────────────
// pygments JsonLexer shape: strings -> String, numbers -> Number,
// true/false/null -> Keyword, braces/separators -> Punctuation, ws -> Whitespace.
inline std::vector<Token> lex_json(std::string_view code) {
    std::vector<Token> out;
    auto push = [&](Tok k, size_t a, size_t b) {
        if (b > a)
            out.push_back({k, std::string(code.substr(a, b - a))});
    };
    const size_t n = code.size();
    size_t i = 0;
    while (i < n) {
        const char c = code[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            size_t j = i;
            while (j < n && (code[j] == ' ' || code[j] == '\t' || code[j] == '\n' ||
                             code[j] == '\r'))
                ++j;
            push(Tok::Whitespace, i, j);
            i = j;
            continue;
        }
        if (c == '"') {
            size_t j = i + 1;
            while (j < n && code[j] != '"') {
                if (code[j] == '\\')
                    ++j;
                ++j;
            }
            if (j < n)
                ++j;
            push(Tok::String, i, j);
            i = j;
            continue;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t j = i;
            if (code[j] == '-')
                ++j;
            while (j < n && isdigit(static_cast<unsigned char>(code[j])))
                ++j;
            if (j < n && code[j] == '.') {
                ++j;
                while (j < n && isdigit(static_cast<unsigned char>(code[j])))
                    ++j;
            }
            if (j < n && (code[j] == 'e' || code[j] == 'E')) {
                ++j;
                if (j < n && (code[j] == '+' || code[j] == '-'))
                    ++j;
                while (j < n && isdigit(static_cast<unsigned char>(code[j])))
                    ++j;
            }
            push(Tok::Number, i, j);
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = i;
            while (j < n && is_ident_char(code[j]))
                ++j;
            const std::string_view w = code.substr(i, j - i);
            push(w == "true" || w == "false" || w == "null" ? Tok::Keyword : Tok::Text, i, j);
            i = j;
            continue;
        }
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == ':') {
            push(Tok::Punctuation, i, i + 1);
            ++i;
            continue;
        }
        push(Tok::Text, i, i + 1);
        ++i;
    }
    return out;
}

// plain text (pygments "text" lexer): one Token.Text for the whole input
inline std::vector<Token> lex_text(std::string_view code) {
    return {{Tok::Text, std::string(code)}};
}

inline std::vector<Token> lex(std::string_view code, std::string_view name) {
    if (name == "python" || name == "py" || name == "python3")
        return lex_python(code);
    if (name == "cpp" || name == "c++" || name == "c")
        return lex_cpp(code);
    if (name == "json")
        return lex_json(code);
    return lex_text(code);
}

// ── layout helpers ───────────────────────────────────────────────────────────
inline size_t cells_of(const std::vector<Segment>& segs) {
    size_t w = 0;
    for (const auto& s : segs)
        w += unicode::string_cell_len(s.text);
    return w;
}

// Segment.adjust_line_length: keep cells while < width, crop the straddling
// segment to exactly width, pad the remainder with pad_style, stop.
inline std::vector<Segment> crop_pad(const std::vector<Segment>& line, size_t width,
                                     const Style& pad_style) {
    std::vector<Segment> out;
    size_t pos = 0;
    bool truncated = false;
    for (const auto& seg : line) {
        if (truncated || pos >= width)
            break;
        const size_t len = unicode::string_cell_len(seg.text);
        if (pos + len <= width) {
            out.push_back(seg);
            pos += len;
            continue;
        }
        // crop at the (width - pos)th cell — walk codepoints to a boundary
        size_t want = width - pos;
        size_t b = 0, acc = 0;
        while (b < seg.text.size() && acc < want) {
            const unsigned char ch = static_cast<unsigned char>(seg.text[b]);
            size_t clen = 1;
            if (ch >= 0xF0)
                clen = 4;
            else if (ch >= 0xE0)
                clen = 3;
            else if (ch >= 0xC0)
                clen = 2;
            if (b + clen > seg.text.size())
                clen = seg.text.size() - b;
            b += clen;
            ++acc;
        }
        if (b > 0)
            out.push_back({std::string(seg.text.substr(0, b)), seg.style});
        pos = width;
        truncated = true;
    }
    if (pos < width)
        out.push_back({std::string(width - pos, ' '), pad_style});
    return out;
}

// Text.wrap: divide_line offsets on words (\s*\S+\s*), fold for over-long
// words, then split the segment stream at the cell offsets and pad each
// wrapped line to width with the base style (justify="left").
inline std::vector<std::vector<Segment>> wrap_line(const std::vector<Segment>& line,
                                                   size_t width, const Style& base) {
    std::string plain;
    for (const auto& s : line)
        plain += s.text;
    if (width == 0 || unicode::string_cell_len(plain) <= width) {
        auto one = crop_pad(line, width, base);
        return {std::move(one)};
    }
    std::vector<size_t> breaks;
    size_t offset = 0; // cells placed on the current line
    size_t pos = 0;
    auto is_ws = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (pos < plain.size()) {
        const size_t start = pos;
        while (pos < plain.size() && is_ws(plain[pos]))
            ++pos;
        const size_t core_start = pos;
        while (pos < plain.size() && !is_ws(plain[pos]))
            ++pos;
        const size_t core_end = pos;
        while (pos < plain.size() && is_ws(plain[pos]))
            ++pos;
        const std::string_view word(plain.data() + start, pos - start);
        const std::string_view core(plain.data() + core_start, core_end - core_start);
        const size_t core_len = unicode::string_cell_len(core);
        const size_t word_len = unicode::string_cell_len(word);
        if (width - offset >= core_len) {
            offset += word_len;
        } else if (core_len > width) {
            // fold: chop the word into width-cell chunks (hard breaks)
            size_t wstart = start;
            size_t acc = 0, k = core_start;
            while (k < core_end) {
                size_t kb = k;
                while (kb < core_end && acc < width) {
                    const unsigned char ch = static_cast<unsigned char>(plain[kb]);
                    size_t clen = 1;
                    if (ch >= 0xF0)
                        clen = 4;
                    else if (ch >= 0xE0)
                        clen = 3;
                    else if (ch >= 0xC0)
                        clen = 2;
                    kb += (kb + clen <= plain.size()) ? clen : plain.size() - kb;
                    ++acc;
                }
                if (kb > k && kb < core_end) {
                    if (wstart > 0)
                        breaks.push_back(wstart);
                    wstart = kb;
                    acc = 0;
                }
                k = kb;
            }
            offset = width; // chunk fills the line
        } else if (offset > 0 && start > 0) {
            breaks.push_back(start);
            offset = word_len;
        }
    }
    // breaks are char offsets on plain — remap to cell offsets (prefix cells)
    std::vector<size_t> cell_breaks;
    {
        size_t acc = 0, bi = 0;
        for (size_t k = 0; k < plain.size(); ++k) {
            if (bi < breaks.size() && breaks[bi] <= k) {
                cell_breaks.push_back(acc);
                ++bi;
            }
            const unsigned char ch = static_cast<unsigned char>(plain[k]);
            size_t step = 1;
            if (ch >= 0xF0)
                step = 4;
            else if (ch >= 0xE0)
                step = 3;
            else if (ch >= 0xC0)
                step = 2;
            k += (k + step <= plain.size()) ? step - 1 : plain.size() - k - 1;
            ++acc;
        }
        while (bi < breaks.size())
            cell_breaks.push_back(acc);
    }
    // split the segment stream at the cell offsets
    std::vector<std::vector<Segment>> out;
    std::vector<Segment> line_cur;
    size_t consumed = 0, ci = 0;
    for (const auto& seg : line) {
        size_t b = 0;
        while (b < seg.text.size()) {
            const unsigned char ch = static_cast<unsigned char>(seg.text[b]);
            size_t step = 1;
            if (ch >= 0xF0)
                step = 4;
            else if (ch >= 0xE0)
                step = 3;
            else if (ch >= 0xC0)
                step = 2;
            const size_t chars = (b + step <= seg.text.size()) ? step : seg.text.size() - b;
            line_cur.push_back({std::string(seg.text.substr(b, chars)), seg.style});
            ++consumed;
            b += chars;
            if (ci < cell_breaks.size() && consumed == cell_breaks[ci]) {
                out.push_back(std::move(line_cur));
                line_cur.clear();
                ++ci;
            }
        }
    }
    out.push_back(std::move(line_cur));
    for (auto& l : out) {
        // merge adjacent same-style runs — Python Text.divide re-emits
        // consolidated spans, not per-char segments
        for (size_t i2 = 1; i2 < l.size();) {
            if (l[i2].style == l[i2 - 1].style && !l[i2].text.empty()) {
                l[i2 - 1].text += l[i2].text;
                l.erase(l.begin() + static_cast<long>(i2));
            } else
                ++i2;
        }
        const size_t len = cells_of(l);
        if (len < width)
            l.push_back({std::string(width - len, ' '), base});
    }
    return out;
}

} // namespace detail

// ── the renderable ───────────────────────────────────────────────────────────
inline std::vector<rich::Segment> Syntax::render(const rich::Console& c,
                                                 const rich::Options& opt) const {
    using detail::Tok;
    const SyntaxOptions& o = opts;
    const size_t width = (opt.width > 0) ? opt.width : 80;
    const bool legacy = c.legacy_windows();

    // 1. _process_code: trailing newline, dedent, expandtabs
    std::string code = this->code;
    const bool ends_on_nl = !code.empty() && code.back() == '\n';
    if (!ends_on_nl)
        code += '\n';
    if (o.dedent) {
        size_t common = 0;
        const size_t first_line = code.find('\n');
        bool first = true;
        size_t line_start = 0;
        while (line_start < code.size()) {
            size_t line_end = code.find('\n', line_start);
            if (line_end == std::string::npos)
                line_end = code.size();
            std::string_view ln = code.substr(line_start, line_end - line_start);
            if (!ln.empty() && ln.find_first_not_of(" \t") != std::string_view::npos) {
                size_t indent = 0;
                while (indent < ln.size() && (ln[indent] == ' ' || ln[indent] == '\t'))
                    ++indent;
                if (first) {
                    common = indent;
                    first = false;
                } else
                    common = std::min(common, indent);
            }
            line_start = line_end + 1;
        }
        std::string dedented;
        line_start = 0;
        while (line_start < code.size()) {
            size_t line_end = code.find('\n', line_start);
            if (line_end == std::string::npos)
                line_end = code.size();
            std::string_view ln = code.substr(line_start, line_end - line_start);
            if (ln.empty() || ln.find_first_not_of(" \t") == std::string_view::npos)
                dedented += '\n';
            else
                dedented += ln.substr(std::min(common, ln.size()));
            if (line_end < code.size())
                dedented += '\n';
            line_start = line_end + 1;
        }
        code = std::move(dedented);
    }
    if (o.tab_size > 0) {
        std::string expanded;
        expanded.reserve(code.size());
        size_t col = 0;
        for (char ch : code) {
            if (ch == '\n') {
                expanded += ch;
                col = 0;
            } else if (ch == '\t') {
                size_t pad = static_cast<size_t>(o.tab_size) - col % static_cast<size_t>(o.tab_size);
                expanded.append(pad, ' ');
                col += pad;
            } else {
                expanded += ch;
                ++col;
            }
        }
        code = std::move(expanded);
    }

    // 2. lex + 3. build per-line segment streams (split on \n)
    std::vector<detail::Token> toks = detail::lex(code, lexer);
    std::vector<std::vector<Segment>> lines;
    std::vector<Segment> cur;
    for (const auto& t : toks) {
        const Style st = detail::token_style(t.kind);
        size_t a = 0;
        for (size_t b = 0; b <= t.text.size(); ++b) {
            if (b == t.text.size() || t.text[b] == '\n') {
                if (b > a)
                    cur.push_back({t.text.substr(a, b - a), st});
                a = b + 1;
                if (b < t.text.size()) {
                    lines.push_back(std::move(cur));
                    cur.clear();
                }
            }
        }
    }
    lines.push_back(std::move(cur)); // trailing (empty when ends_on_nl)
    if (!ends_on_nl && lines.back().empty())
        lines.pop_back();

    // 4. line_range slice (1-based, end exclusive — Python lines[offset:end])
    if (o.line_range_start > 0) {
        const size_t begin = static_cast<size_t>(o.line_range_start - 1);
        size_t end = (o.line_range_end > 0) ? static_cast<size_t>(o.line_range_end)
                                            : lines.size();
        if (begin >= lines.size())
            return {};
        end = std::min(end, lines.size());
        lines.assign(lines.begin() + begin, lines.begin() + end);
    }

    // 5. indent guides (with_indent_guides: per tab_size block -> │+spaces;
    //    blank lines are deferred and re-emitted with the following line's
    //    indent, or empty at the end)
    if (o.indent_guides && o.tab_size > 0) {
        const Style guide{
            .dim = true,
            .color = Color::rgb(0x95, 0x90, 0x77),
            .bg_color = detail::mono_bg(),
            .explicit_bits = 0x01 | 0x04 | 0x08,
        };
        std::vector<std::vector<Segment>> next;
        size_t blank = 0;
        for (auto& line : lines) {
            std::string plain;
            for (const auto& s : line)
                plain += s.text;
            size_t indent = 0;
            while (indent < plain.size() && plain[indent] == ' ')
                ++indent;
            if (plain.size() == indent) {
                ++blank;
                continue;
            }
            const size_t full = indent / static_cast<size_t>(o.tab_size);
            const size_t rem = indent % static_cast<size_t>(o.tab_size);
            std::string new_indent;
            for (size_t k = 0; k < full; ++k) {
                new_indent += "\xE2\x94\x82"; // │
                new_indent.append(static_cast<size_t>(o.tab_size) - 1, ' ');
            }
            new_indent.append(rem, ' ');
            for (size_t k = 0; k < blank; ++k)
                next.push_back({{new_indent, guide}});
            blank = 0;
            std::vector<Segment> nline;
            nline.push_back({new_indent, guide});
            size_t skip = indent;
            for (auto& seg : line) {
                if (skip >= seg.text.size()) {
                    skip -= seg.text.size();
                    continue;
                }
                if (skip) {
                    seg.text.erase(0, skip);
                    skip = 0;
                    if (seg.text.empty())
                        continue;
                }
                nline.push_back(std::move(seg));
            }
            next.push_back(std::move(nline));
        }
        for (size_t k = 0; k < blank; ++k)
            next.push_back({});
        lines = std::move(next);
    }

    // 6. widths (Python: numbers_column_width = digits(start_line + raw
    //    newline count) + 2; code_width = max_width - col - 1 when numbered)
    size_t raw_nl = 0;
    for (char ch : this->code)
        if (ch == '\n')
            ++raw_nl;
    const size_t col_width =
        o.line_numbers
            ? std::to_string(static_cast<size_t>(o.start_line) + raw_nl).size() + 2
            : 0;
    // Padding (expand=True) shrinks the width seen by the inner render.
    const size_t inner_width =
        o.padding > 0 ? std::max<size_t>(0, width - 2 * static_cast<size_t>(o.padding))
                      : width;
    size_t code_width;
    if (o.code_width > 0)
        code_width = static_cast<size_t>(o.code_width);
    else
        code_width =
            std::max<size_t>(0, inner_width - (o.line_numbers ? col_width + 1 : 0));

    // 7. per-line layout
    const Style bg_style{.bg_color = detail::mono_bg()};
    const Style number_style{.dim = true, .bg_color = detail::mono_bg()};
    const Style hl_number_style{.bg_color = detail::mono_bg(), .explicit_bits = 0x02};
    const Style pointer_style{.color = Color::from_ansi(31)}; // red
    const std::string pointer = legacy ? "> " : "\xE2\x9D\xB1 "; // ❱
    std::vector<Segment> out;
    const int range_offset = (o.line_range_start > 0) ? o.line_range_start - 1 : 0;
    for (size_t idx = 0; idx < lines.size(); ++idx) {
        const int line_no = o.start_line + range_offset + static_cast<int>(idx);
        std::vector<std::vector<Segment>> wrapped;
        if (o.word_wrap) {
            wrapped = detail::wrap_line(lines[idx], code_width, bg_style);
        } else {
            std::vector<Segment> one = detail::crop_pad(lines[idx], code_width, bg_style);
            if (lines[idx].empty() && (o.line_numbers || o.indent_guides))
                // complex path: Text("").render() yields Segment("", None)
                one.insert(one.begin(), {"", rich::detail::kNoneStyle()});
            wrapped.push_back(std::move(one));
        }
        if (o.line_numbers) {
            for (size_t w = 0; w < wrapped.size(); ++w) {
                if (w == 0) {
                    std::string col = std::to_string(line_no);
                    if (col.size() < col_width - 2)
                        col.insert(0, col_width - 2 - col.size(), ' ');
                    col += ' ';
                    if (o.highlight_lines.count(line_no)) {
                        out.push_back({pointer, pointer_style});
                        out.push_back({col, hl_number_style});
                    } else {
                        out.push_back({"  ", hl_number_style});
                        out.push_back({col, number_style});
                    }
                } else {
                    out.push_back({std::string(col_width, ' ') + " ", bg_style});
                }
                for (auto& seg : wrapped[w])
                    out.push_back(std::move(seg));
                out.push_back({"\n", Style{}});
            }
        } else {
            for (auto& wl : wrapped) {
                for (auto& seg : wl)
                    out.push_back(std::move(seg));
                out.push_back({"\n", Style{}});
            }
        }
    }

    // 8. Padding wrapper (Python Padding(pad, style=bg, expand=True)):
    // top and bottom blank lines at the full outer console width, one bg
    // cell on each side of every code line.
    if (o.padding > 0) {
        const size_t pad = static_cast<size_t>(o.padding);
        std::vector<Segment> wrapped;
        wrapped.push_back({std::string(width, ' '), bg_style});
        wrapped.push_back({"\n", Style{}});
        std::vector<Segment> line;
        for (auto& s : out) {
            if (s.text == "\n") {
                for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
                for (auto& seg : line) wrapped.push_back(std::move(seg));
                for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
                wrapped.push_back({"\n", Style{}});
                line.clear();
            } else {
                line.push_back(std::move(s));
            }
        }
        if (!line.empty()) { // trailing line without newline (defensive)
            for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
            for (auto& seg : line) wrapped.push_back(std::move(seg));
            for (size_t k = 0; k < pad; ++k) wrapped.push_back({" ", bg_style});
            wrapped.push_back({"\n", Style{}});
        }
        wrapped.push_back({std::string(width, ' '), bg_style});
        wrapped.push_back({"\n", Style{}});
        out = std::move(wrapped);
    }
    return out;
}

inline std::pair<size_t, size_t> Syntax::measure(const rich::Console&,
                                                 const rich::Options&) const {
    size_t raw_nl = 0;
    for (char ch : code)
        if (ch == '\n')
            ++raw_nl;
    const size_t col_width =
        opts.line_numbers
            ? std::to_string(static_cast<size_t>(opts.start_line) + raw_nl).size() + 2
            : 0;
    if (opts.code_width > 0)
        return {col_width, static_cast<size_t>(opts.code_width) + col_width + 1};
    size_t max_line = 0;
    size_t start = 0;
    for (size_t i = 0; i <= code.size(); ++i) {
        if (i == code.size() || code[i] == '\n') {
            max_line = std::max(max_line,
                                unicode::string_cell_len(code.substr(start, i - start)));
            start = i + 1;
        }
    }
    size_t width = col_width + max_line;
    if (opts.line_numbers)
        ++width;
    return {col_width, width};
}

} // namespace rich::highlight

// ADL render/measure hooks — must be declared before the factory so the
// AnyRenderable Renderable constraint can find them at instantiation.
namespace rich {

inline std::vector<Segment> render_item(const highlight::Syntax& s, const Console& c,
                                        const Options& o) {
    return s.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const highlight::Syntax& s, const Console& c,
                                              const Options& o) {
    return s.measure(c, o);
}

} // namespace rich

namespace rich::highlight {

// console.print(rich::highlight::syntax(code, "python", opts))
inline rich::AnyRenderable syntax(std::string_view code, std::string_view lexer,
                                  SyntaxOptions opts = {}) {
    Syntax s{std::string(code), std::string(lexer), opts};
    return rich::AnyRenderable(std::move(s));
}

} // namespace rich::highlight
