#pragma once
// Header-only port of rich/highlighter.py (Python rich 15.0.0 reference).
//
// Python `Highlighter.__call__`: str -> fresh Text(text); Text -> text.copy();
// then highlight(text) in place, returning the (new) Text.
// `RegexHighlighter.highlight` runs each rule through
// text.highlight_regex(rule, style_prefix=base_style) — style=None, so only
// NAMED groups receive spans (prefixed with base_style).
// `JSONHighlighter.highlight` adds a post-pass that marks JSON keys: for
// each JSON_STR match it scans forward past whitespace and, if the next
// character is ':', appends Span(start, end, "json.key") AFTER the regex
// spans (later spans win at render).
//
// Note: `NumberHighlighter` and `Text.concatenate` do not exist in
// rich 15.0.0 (matrix row Notes).

#include <rich_core.h>
#include <Text.h>
#include <string>
#include <string_view>
#include <vector>

namespace rich {

class Highlighter {
public:
    virtual ~Highlighter() = default;

    text::Text operator()(const std::string& text) {
        text::Text t(text);
        highlight(t);
        return t;
    }
    text::Text operator()(const text::Text& text) {
        text::Text t(text);
        highlight(t);
        return t;
    }

    virtual void highlight(text::Text& text) const = 0;
};

// Python NullHighlighter — no-op, used to disable highlighting.
class NullHighlighter : public Highlighter {
public:
    void highlight(text::Text&) const override {}
};

// Python RegexHighlighter: `highlights: list[str]`, `base_style: str`.
class RegexHighlighter : public Highlighter {
public:
    RegexHighlighter(std::vector<std::string> rules, std::string base_style = "")
        : rules_(std::move(rules)), base_style_(std::move(base_style)) {}

    void highlight(text::Text& text) const override {
        for (const auto& rule : rules_)
            text.highlight_regex(rule, std::string_view(),
                                 std::string_view(base_style_));
    }

protected:
    std::vector<std::string> rules_;
    std::string base_style_;
};

namespace hl_patterns {
static constexpr const char* REPR_P0 = R"_q((?P<tag_start><)(?P<tag_name>[-\w.:|]*)(?P<tag_contents>[\w\W]*)(?P<tag_end>>))_q";
static constexpr const char* REPR_P1 = R"_q((?P<attrib_name>[\w_]{1,50})=(?P<attrib_value>"?[\w_]+"?)?)_q";
static constexpr const char* REPR_P2 = R"_q((?P<brace>[][{}()]))_q";
static constexpr const char* REPR_P3 = R"_q((?P<ipv4>[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})|(?P<ipv6>([A-Fa-f0-9]{1,4}::?){1,7}[A-Fa-f0-9]{1,4})|(?P<eui64>(?:[0-9A-Fa-f]{1,2}-){7}[0-9A-Fa-f]{1,2}|(?:[0-9A-Fa-f]{1,2}:){7}[0-9A-Fa-f]{1,2}|(?:[0-9A-Fa-f]{4}\.){3}[0-9A-Fa-f]{4})|(?P<eui48>(?:[0-9A-Fa-f]{1,2}-){5}[0-9A-Fa-f]{1,2}|(?:[0-9A-Fa-f]{1,2}:){5}[0-9A-Fa-f]{1,2}|(?:[0-9A-Fa-f]{4}\.){2}[0-9A-Fa-f]{4})|(?P<uuid>[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12})|(?P<call>[\w.]*?)\(|\b(?P<bool_true>True)\b|\b(?P<bool_false>False)\b|\b(?P<none>None)\b|(?P<ellipsis>\.\.\.)|(?P<number_complex>(?<!\w)(?:\-?[0-9]+\.?[0-9]*(?:e[-+]?\d+?)?)(?:[-+](?:[0-9]+\.?[0-9]*(?:e[-+]?\d+)?))?j)|(?P<number>(?<!\w)\-?[0-9]+\.?[0-9]*(e[-+]?\d+?)?\b|0x[0-9a-fA-F]*)|(?P<path>\B(/[-\w._+]+)*\/)(?P<filename>[-\w._+]*)?|(?<![\\\w])(?P<str>b?'''.*?(?<!\\)'''|b?'.*?(?<!\\)'|b?\"\"\".*?(?<!\\)\"\"\"|b?\".*?(?<!\\)\")|(?P<url>(file|https|http|ws|wss)://[-0-9a-zA-Z$_+!`(),.?/;:&=%#~@]*))_q";
static constexpr const char* JSON_COMBINED = R"_q((?P<brace>[\{\[\(\)\]\}])|\b(?P<bool_true>true)\b|\b(?P<bool_false>false)\b|\b(?P<null>null)\b|(?P<number>(?<!\w)\-?[0-9]+\.?[0-9]*(e[\-\+]?\d+?)?\b|0x[0-9a-fA-F]*)|(?<![\\\w])(?P<str>b?\".*?(?<!\\)\"))_q";
static constexpr const char* JSON_STR = R"_q((?<![\\\w])(?P<str>b?\".*?(?<!\\)\"))_q";
static constexpr const char* ISO_P0 = R"_q(^(?P<year>[0-9]{4})-(?P<month>1[0-2]|0[1-9])$)_q";
static constexpr const char* ISO_P1 = R"_q(^(?P<date>(?P<year>[0-9]{4})(?P<month>1[0-2]|0[1-9])(?P<day>3[01]|0[1-9]|[12][0-9]))$)_q";
static constexpr const char* ISO_P2 = R"_q(^(?P<date>(?P<year>[0-9]{4})-?(?P<day>36[0-6]|3[0-5][0-9]|[12][0-9]{2}|0[1-9][0-9]|00[1-9]))$)_q";
static constexpr const char* ISO_P3 = R"_q(^(?P<date>(?P<year>[0-9]{4})-?W(?P<week>5[0-3]|[1-4][0-9]|0[1-9]))$)_q";
static constexpr const char* ISO_P4 = R"_q(^(?P<date>(?P<year>[0-9]{4})-?W(?P<week>5[0-3]|[1-4][0-9]|0[1-9])-?(?P<day>[1-7]))$)_q";
static constexpr const char* ISO_P5 = R"_q(^(?P<time>(?P<hour>2[0-3]|[01][0-9]):?(?P<minute>[0-5][0-9]))$)_q";
static constexpr const char* ISO_P6 = R"_q(^(?P<time>(?P<hour>2[0-3]|[01][0-9])(?P<minute>[0-5][0-9])(?P<second>[0-5][0-9]))$)_q";
static constexpr const char* ISO_P7 = R"_q(^(?P<timezone>(Z|[+-](?:2[0-3]|[01][0-9])(?::?(?:[0-5][0-9]))?))$)_q";
static constexpr const char* ISO_P8 = R"_q(^(?P<time>(?P<hour>2[0-3]|[01][0-9])(?P<minute>[0-5][0-9])(?P<second>[0-5][0-9]))(?P<timezone>Z|[+-](?:2[0-3]|[01][0-9])(?::?(?:[0-5][0-9]))?)$)_q";
static constexpr const char* ISO_P9 = R"_q(^(?P<date>(?P<year>[0-9]{4})(?P<hyphen>-)?(?P<month>1[0-2]|0[1-9])(?(hyphen)-)(?P<day>3[01]|0[1-9]|[12][0-9])) (?P<time>(?P<hour>2[0-3]|[01][0-9])(?(hyphen):)(?P<minute>[0-5][0-9])(?(hyphen):)(?P<second>[0-5][0-9]))$)_q";
static constexpr const char* ISO_P10 = R"_q(^(?P<date>(?P<year>-?(?:[1-9][0-9]*)?[0-9]{4})-(?P<month>1[0-2]|0[1-9])-(?P<day>3[01]|0[1-9]|[12][0-9]))(?P<timezone>Z|[+-](?:2[0-3]|[01][0-9]):[0-5][0-9])?$)_q";
static constexpr const char* ISO_P11 = R"_q(^(?P<time>(?P<hour>2[0-3]|[01][0-9]):(?P<minute>[0-5][0-9]):(?P<second>[0-5][0-9])(?P<frac>\.[0-9]+)?)(?P<timezone>Z|[+-](?:2[0-3]|[01][0-9]):[0-5][0-9])?$)_q";
static constexpr const char* ISO_P12 = R"_q(^(?P<date>(?P<year>-?(?:[1-9][0-9]*)?[0-9]{4})-(?P<month>1[0-2]|0[1-9])-(?P<day>3[01]|0[1-9]|[12][0-9]))T(?P<time>(?P<hour>2[0-3]|[01][0-9]):(?P<minute>[0-5][0-9]):(?P<second>[0-5][0-9])(?P<ms>\.[0-9]+)?)(?P<timezone>Z|[+-](?:2[0-3]|[01][0-9]):[0-5][0-9])?$)_q";
} // namespace hl_patterns

// Python ReprHighlighter (base_style "repr.", 4 rules).
class ReprHighlighter : public RegexHighlighter {
public:
    ReprHighlighter()
        : RegexHighlighter(
              {hl_patterns::REPR_P0, hl_patterns::REPR_P1, hl_patterns::REPR_P2,
               hl_patterns::REPR_P3},
              "repr.") {}
};

// Python JSONHighlighter (base_style "json.", combined rule + key post-pass).
class JSONHighlighter : public RegexHighlighter {
public:
    static constexpr const char* JSON_STR = hl_patterns::JSON_STR;

    JSONHighlighter() : RegexHighlighter({hl_patterns::JSON_COMBINED}, "json.") {}

    void highlight(text::Text& text) const override {
        RegexHighlighter::highlight(text);

        // JSON keys: scan forward from each string match (codepoint-wise,
        // Python indexes `plain` by codepoint); a ':' after optional
        // whitespace marks it a key. The span is appended after the regex
        // spans (later spans overlay earlier ones at render).
        const std::string& plain = text.content();
        std::vector<size_t> byte_of; // codepoint index -> byte offset
        {
            size_t i = 0;
            while (i < plain.size()) {
                byte_of.push_back(i);
                unicode::decode_utf8(plain, i);
            }
        }
        regex::Pattern pat(JSON_STR);
        for (const auto& m : pat.find_all(plain)) {
            long long cursor = static_cast<long long>(m.end);
            while (cursor < static_cast<long long>(byte_of.size())) {
                char32_t ch = unicode::decode_utf8(plain, byte_of[cursor]);
                ++cursor;
                if (ch == ':') {
                    text.stylize(std::string_view("json.key"),
                                 static_cast<long long>(m.start),
                                 static_cast<long long>(m.end));
                    break;
                }
                if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t')
                    continue;
                break;
            }
        }
    }
};

// Python ISO8601Highlighter (base_style "iso8601.", 13 rules).
class ISO8601Highlighter : public RegexHighlighter {
public:
    ISO8601Highlighter()
        : RegexHighlighter(
              {hl_patterns::ISO_P0, hl_patterns::ISO_P1, hl_patterns::ISO_P2,
               hl_patterns::ISO_P3, hl_patterns::ISO_P4, hl_patterns::ISO_P5,
               hl_patterns::ISO_P6, hl_patterns::ISO_P7, hl_patterns::ISO_P8,
               hl_patterns::ISO_P9, hl_patterns::ISO_P10, hl_patterns::ISO_P11,
               hl_patterns::ISO_P12},
              "iso8601.") {}
};

} // namespace rich
