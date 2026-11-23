#pragma once
// rich::markdown — Markdown renderable.
//
// Port of Python rich.markdown (corpus-first: headings, paragraphs with
// inline styles (strong/em/code/strike/escapes), links, images, bullet and
// ordered lists (nested), block quotes (nested), fenced and indented code
// blocks, horizontal rules, pipe tables, hard/soft line breaks, <kbd>).
// Zero-dependency: own block+inline parser mirroring the markdown-it token
// stream for the supported constructs, plus md::Text (the multi-span
// rich.text.Text subset that rich.markdown relies on, incl. wrap/justify
// semantics verified against the Python oracle).
#include <rich_core.h>
#include <Text.h>
#include <highlight/Syntax.h>
#include <unicode/Unicode.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rich::markdown {

// ────────────────────────────────────────────────────────────────────────────
// md::Text — multi-span text.
//
// Semantics ported from rich.text.Text (verified by probe):
//  - append(str, style): span recorded only when the style is truthy.
//  - append_text(o): base span over the range when o's base is truthy, then
//    o's spans copied at the offset.
//  - render(width): split at '\n', expand tabs (tab_size 8), word-wrap with
//    divide_line (overflow "crop" → no fold), rstrip_end, then justify
//    (left pads right, center/right rstrip + pad, full redistributes spaces
//    except the last line, "default" is a no-op). Padding is plain text —
//    with spans it falls in a gap (rendered with the base style, repr
//    "Style()"); without spans the whole text is one style-None segment.
//    Adjacent same-style span ranges are NOT merged: each append is its own
//    span, so segment boundaries follow the append calls.
namespace md {

struct Span {
    size_t start;
    size_t end;
    Style style;
};

class Text {
public:
    enum class J { Default, Left, Center, Right, Full };

    Text() = default;
    explicit Text(std::string p, J j = J::Default) : plain_(std::move(p)), justify_(j) {}

    bool empty() const { return plain_.empty(); }
    std::string_view plain() const { return plain_; }
    const std::string& plain_ref() const { return plain_; }
    void set_justify(J j) { justify_ = j; }

    void append(std::string_view t, const Style* s) {
        if (t.empty()) return; // Python: if len(text)
        const size_t off = plain_.size();
        plain_.append(t);
        if (s && s->truthy()) spans_.push_back({off, plain_.size(), *s});
    }
    void append_text(const Text& o) {
        if (o.plain_.empty()) return;
        const size_t off = plain_.size();
        if (o.base_.truthy()) spans_.push_back({off, off + o.plain_.size(), o.base_});
        for (const auto& sp : o.spans_)
            spans_.push_back({sp.start + off, sp.end + off, sp.style});
        plain_ += o.plain_;
    }
    void stylize(Style s) {
        if (!plain_.empty()) spans_.push_back({0, plain_.size(), s});
    }

    // Wrap + justify + emit segments. `end_sv` = trailing segment text
    // (Python Text.end, default "\n"; empty for joined pieces).
    std::vector<Segment> render(size_t width, std::string_view end_sv = "\n") const;

private:
    std::string plain_;
    std::vector<Span> spans_;
    Style base_{}; // Python self.style ("" → explicit empty)
    J justify_{J::Default};
};

namespace detail {

struct LineChars {
    std::string text;                    // expanded line (tabs → spaces)
    std::vector<const Style*> cstyle;    // per byte (nullptr = no style)
};

// Split a (text, per-byte-style) line at divide_line offsets, rstrip_end
// each part, then apply the justify. Returns the rendered parts.
inline std::vector<LineChars> wrap_justify(const LineChars& in, size_t width,
                                           Text::J justify, const Style& base,
                                           bool last_part, bool has_spans) {
    using text::wrap_detail::divide_line;
    using text::wrap_detail::rstrip_end;
    using unicode::string_cell_len;

    auto crop_cells = [](std::string& s, size_t take, std::vector<const Style*>& cs) {
        size_t cells = 0, i = 0;
        while (i < s.size() && cells < take) {
            size_t j = i;
            char32_t cp = unicode::decode_utf8(s, j);
            size_t w = static_cast<size_t>(unicode::get_character_cell_size(cp));
            if (cells + w > take) break;
            cells += w;
            i = j;
        }
        s.erase(i);
        cs.resize(i);
    };

    auto strip_trailing_ws = [](std::string& s, std::vector<const Style*>& cs) {
        while (!s.empty() &&
               (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ||
                s.back() == '\n' || s.back() == '\f' || s.back() == '\v')) {
            s.pop_back();
            cs.pop_back();
        }
    };

    // Break offsets (fold=True — Python DEFAULT_OVERFLOW="fold": overflow
    // words are folded across lines).
    auto breaks = divide_line(in.text, width, true);
    std::vector<size_t> bounds;
    bounds.reserve(breaks.size() + 2);
    bounds.push_back(0);
    for (size_t b : breaks) bounds.push_back(b);
    bounds.push_back(in.text.size());

    const Style* pad_style = has_spans ? &base : nullptr;

    std::vector<LineChars> parts;
    for (size_t p = 0; p + 1 < bounds.size(); ++p) {
        const size_t s = bounds[p], e = bounds[p + 1];
        LineChars part;
        part.text = in.text.substr(s, e - s);
        part.cstyle.assign(in.cstyle.begin() + static_cast<long>(s),
                           in.cstyle.begin() + static_cast<long>(e));
        rstrip_end(part.text, width);
        if (part.text.size() < part.cstyle.size())
            part.cstyle.resize(part.text.size());

        const size_t len = string_cell_len(part.text);
        switch (justify) {
        case Text::J::Left:
            if (len > width)
                crop_cells(part.text, width, part.cstyle);
            else if (len < width)
                for (size_t k = 0; k < width - len; ++k) {
                    part.text.push_back(' ');
                    part.cstyle.push_back(pad_style);
                }
            break;
        case Text::J::Center: {
            strip_trailing_ws(part.text, part.cstyle);
            size_t l = string_cell_len(part.text);
            if (l > width)
                crop_cells(part.text, width, part.cstyle);
            l = string_cell_len(part.text);
            const size_t left = (width - l) / 2;
            const size_t right = width - l - left;
            part.text.insert(part.text.begin(), static_cast<size_t>(left), ' ');
            part.cstyle.insert(part.cstyle.begin(), left, pad_style);
            for (size_t k = 0; k < right; ++k) {
                part.text.push_back(' ');
                part.cstyle.push_back(pad_style);
            }
            break;
        }
        case Text::J::Right: {
            strip_trailing_ws(part.text, part.cstyle);
            size_t l = string_cell_len(part.text);
            if (l > width)
                crop_cells(part.text, width, part.cstyle);
            l = string_cell_len(part.text);
            part.text.insert(part.text.begin(), width - l, ' ');
            part.cstyle.insert(part.cstyle.begin(), width - l, pad_style);
            break;
        }
        case Text::J::Full: {
            if (!last_part && !part.text.empty()) {
                // words = text.split(" ") (single-space split, empties kept)
                std::vector<std::string> words;
                size_t i = 0;
                for (;;) {
                    const size_t sp = part.text.find(' ', i);
                    if (sp == std::string::npos) {
                        words.push_back(part.text.substr(i));
                        break;
                    }
                    words.push_back(part.text.substr(i, sp - i));
                    i = sp + 1;
                }
                size_t words_size = 0;
                for (const auto& w : words) words_size += string_cell_len(w);
                const size_t num = words.size() > 1 ? words.size() - 1 : 0;
                if (num > 0 && words_size + num < width) {
                    std::vector<size_t> spaces(num, 1);
                    size_t total = num, index = 0;
                    while (words_size + total < width) {
                        spaces[spaces.size() - index - 1] += 1;
                        total += 1;
                        index = (index + 1) % spaces.size();
                    }
                    // Word byte ranges (split dropped the single spaces).
                    std::vector<std::pair<size_t, size_t>> ranges;
                    size_t pos = 0;
                    for (size_t wi = 0; wi < words.size(); ++wi) {
                        ranges.push_back({pos, pos + words[wi].size()});
                        pos += words[wi].size() + 1; // + the dropped space
                    }
                    auto style_at = [&](size_t wi, bool first) -> const Style* {
                        auto& [a, b] = ranges[wi];
                        const size_t at = first ? a : (b > a ? b - 1 : a);
                        return (at < part.cstyle.size()) ? part.cstyle[at] : pad_style;
                    };
                    std::string nt;
                    std::vector<const Style*> nc;
                    for (size_t wi = 0; wi < words.size(); ++wi) {
                        auto& [a, b] = ranges[wi];
                        nt.append(part.text, a, b - a);
                        for (size_t k = a; k < b; ++k) nc.push_back(part.cstyle[k]);
                        if (wi < spaces.size()) {
                            const Style* ss = pad_style;
                            const Style* a1 = style_at(wi, false);
                            const Style* a2 = style_at(wi + 1, true);
                            if (a1 && a2 && *a1 == *a2)
                                ss = a1;
                            else if (a1)
                                ss = a1;
                            else if (a2)
                                ss = a2;
                            for (size_t k = 0; k < spaces[wi]; ++k) {
                                nt.push_back(' ');
                                nc.push_back(ss);
                            }
                        }
                    }
                    part.text = std::move(nt);
                    part.cstyle = std::move(nc);
                }
            }
            break;
        }
        case Text::J::Default:
            break; // no-op (Python Lines.justify: no matching branch)
        }
        parts.push_back(std::move(part));
    }
    if (parts.empty()) parts.push_back({});
    return parts;
}

} // namespace detail

std::vector<Segment> Text::render(size_t width, std::string_view end_sv) const {
    using S = Segment;
    const bool has_spans = !spans_.empty();

    // 1. LineChars per source line (split at '\n', blank lines preserved).
    // NOTE: `runs` must outlive `lines` — cstyle entries point into it.
    struct Run {
        std::string text;
        Style style;
        const Style* id; // identity ptr: gap runs share &base_ so trailing
                         // padding merges into them (Python pads w/ base).
    };
    std::vector<Run> runs;
    std::vector<detail::LineChars> lines;
    if (!has_spans) {
        size_t start = 0;
        for (;;) {
            const size_t nl = plain_.find('\n', start);
            detail::LineChars ln;
            if (nl == std::string::npos) {
                ln.text = plain_.substr(start);
                ln.cstyle.assign(ln.text.size(), nullptr);
                lines.push_back(std::move(ln));
                break;
            }
            ln.text = plain_.substr(start, nl - start);
            ln.cstyle.assign(ln.text.size(), nullptr);
            lines.push_back(std::move(ln));
            start = nl + 1;
        }
    } else {
        // Span sweep (Python Text.render): events sorted (offset, leaving);
        // base (id 0) pushed first; active styles combined left→right.
        struct Ev { size_t off; int leaving; int idx; };
        std::vector<Ev> evs;
        evs.reserve(spans_.size() * 2 + 2);
        evs.push_back({0, 0, 0});
        for (size_t i = 0; i < spans_.size(); ++i) {
            evs.push_back({spans_[i].start, 0, static_cast<int>(i + 1)});
            evs.push_back({spans_[i].end, 1, static_cast<int>(i + 1)});
        }
        evs.push_back({plain_.size(), 1, 0});
        std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
            if (a.off != b.off) return a.off < b.off;
            return a.leaving < b.leaving;
        });
        std::vector<int> stack;
        auto current = [&]() -> Style {
            Style cur = base_;
            for (int id : stack)
                if (id != 0) cur = cur + spans_[id - 1].style;
            return cur;
        };
        // Runs across the whole text (gaps → base style). Reserve so
        // &runs[k].style stays stable (no reallocation invalidates ids).
        runs.reserve(evs.size());
        for (size_t k = 0; k + 1 < evs.size(); ++k) {
            const auto& e = evs[k];
            if (e.leaving) {
                auto it = std::find(stack.begin(), stack.end(), e.idx);
                if (it != stack.end()) stack.erase(it);
            } else {
                stack.push_back(e.idx);
            }
            const size_t next = evs[k + 1].off;
            if (next > e.off) {
                const bool is_gap = (stack.size() == 1 && stack[0] == 0);
                runs.push_back(Run{plain_.substr(e.off, next - e.off), current(),
                                   nullptr});
                runs.back().id = is_gap ? &base_ : &runs.back().style;
            }
        }
        // Split runs at '\n'.
        for (const auto& r : runs) {
            size_t i = 0;
            for (;;) {
                const size_t nl = r.text.find('\n', i);
                const size_t e = (nl == std::string_view::npos) ? r.text.size() : nl;
                if (lines.empty()) lines.push_back({});
                auto& ln = lines.back();
                for (size_t b = i; b < e; ++b) {
                    ln.text.push_back(r.text[b]);
                    ln.cstyle.push_back(r.id);
                }
                if (nl == std::string_view::npos) break;
                lines.push_back({});
                i = nl + 1;
            }
        }
        if (lines.empty()) lines.push_back({});
    }

    // 2. Per line: expand tabs, wrap, justify.
    std::vector<std::vector<detail::LineChars>> parts_all;
    for (auto& ln : lines) {
        detail::LineChars exp;
        size_t col = 0;
        for (size_t i = 0; i < ln.text.size();) {
            size_t j = i;
            const char32_t cp = unicode::decode_utf8(ln.text, j);
            if (cp == '\t') {
                const size_t sp = 8 - (col % 8);
                for (size_t k = 0; k < sp; ++k) {
                    exp.text.push_back(' ');
                    exp.cstyle.push_back(ln.cstyle[i]);
                }
                col += sp;
            } else if (cp == '\r') {
                // dropped (CRLF)
            } else {
                for (size_t b = i; b < j; ++b) {
                    exp.text.push_back(ln.text[b]);
                    exp.cstyle.push_back(ln.cstyle[b]);
                }
                col += static_cast<size_t>(unicode::get_character_cell_size(cp));
            }
            i = j;
        }
        parts_all.push_back(detail::wrap_justify(exp, width, justify_, base_,
                                                 /*last_part*/ false, has_spans));
    }

    // 3. Emit (Python Text.__rich_console__ + Text.render): join all wrapped
    // parts with '\n' (a null-style char), then a raw span sweep — the '\n'
    // chars stay embedded in run text; no separate newline segments and no
    // padding here (padding is already inside each part). `end_sv` mirrors
    // Python Text.render's `end` (elements use "").
    std::string joined;
    std::vector<const Style*> jstyle;
    bool first = true;
    for (const auto& parts : parts_all) {
        for (const auto& p : parts) {
            if (!first) {
                joined.push_back('\n');
                jstyle.push_back(nullptr);
            }
            first = false;
            for (size_t b = 0; b < p.text.size(); ++b) {
                joined.push_back(p.text[b]);
                jstyle.push_back(p.cstyle[b]);
            }
        }
    }
    std::vector<Segment> out;
    if (!has_spans) {
        // Python: no spans → single style-None segment of the joined text.
        out.push_back({std::move(joined), rich::detail::kNoneStyle()});
    } else {
        size_t i = 0;
        while (i < joined.size()) {
            const Style* st = jstyle[i];
            size_t k = i + 1;
            while (k < joined.size() && jstyle[k] == st) ++k;
            Style s = st ? *st : rich::detail::kNoneStyle();
            out.push_back({joined.substr(i, k - i), std::move(s)});
            i = k;
        }
    }
    if (!end_sv.empty())
        out.push_back({std::string(end_sv), rich::detail::kNoneStyle()});
    return out;
}

} // namespace md

// ────────────────────────────────────────────────────────────────────────────
// Token stream (markdown-it shape) + parser.
namespace parse_detail {

struct Tok {
    std::string type;
    std::string tag;
    int nesting{0}; // 1 open, -1 close, 0 self-closing
    std::string content;
    std::string info;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<Tok> children;

    const std::string* attr(const char* key) const {
        for (const auto& [k, v] : attrs)
            if (k == key) return &v;
        return nullptr;
    }
};

inline bool is_blank(std::string_view l) {
    for (char c : l)
        if (c != ' ' && c != '\t') return false;
    return true;
}
inline size_t lead_spaces(std::string_view l) {
    size_t i = 0;
    while (i < l.size() && l[i] == ' ') ++i;
    return i;
}
inline std::string_view strip_trail(std::string_view l) {
    size_t e = l.size();
    while (e > 0 && (l[e - 1] == ' ' || l[e - 1] == '\t')) --e;
    return l.substr(0, e);
}

// ── inline ─────────────────────────────────────────────────────────────────
inline bool is_escapable(char c) {
    static const char* p = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    return std::strchr(p, c) != nullptr;
}

// Find the closing run of delimiter char c starting at `from`: the next run
// whose length equals `open_len` (corpus-parity of markdown-it's delimiter
// matching: a longer run cannot close a shorter opener).
inline size_t find_close(std::string_view s, size_t from, char c, size_t open_len) {
    size_t j = from;
    while (j < s.size()) {
        if (s[j] != c) { ++j; continue; }
        size_t run = 0;
        while (j + run < s.size() && s[j + run] == c) ++run;
        if (run == open_len) return j;
        j += run;
    }
    return std::string_view::npos;
}

// ── link destination / title (markdown-it helpers/parse_link_*) ────────────
inline std::string unescape_all(std::string_view s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && is_escapable(s[i + 1])) {
            out += s[i + 1];
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}
struct DestRes {
    bool ok{false};
    size_t pos{0};
    std::string str;
};
inline DestRes parse_link_dest(std::string_view s, size_t pos, size_t max) {
    if (pos < max && s[pos] == '<') {
        size_t i = pos + 1;
        for (; i < max;) {
            const char ch = s[i];
            if (ch == '\n' || ch == '<') return DestRes{};
            if (ch == '>')
                return DestRes{true, i + 1,
                               unescape_all(s.substr(pos + 1, i - pos - 1))};
            if (ch == '\\' && i + 1 < max) {
                i += 2;
                continue;
            }
            ++i;
        }
        return DestRes{}; // no closing '>'
    }
    int level = 0;
    const size_t start = pos;
    for (; pos < max;) {
        const unsigned char c = static_cast<unsigned char>(s[pos]);
        if (c == ' ' || c == '\t' || c < 0x20 || c == 0x7F) break;
        if (c == '\\') {
            if (pos + 1 < max && s[pos + 1] == ' ') break;
            pos += 2;
            continue;
        }
        if (c == '(') {
            if (++level > 32) return DestRes{};
        } else if (c == ')') {
            if (level == 0) break;
            --level;
        }
        ++pos;
    }
    if (pos == start || level != 0) return DestRes{};
    return DestRes{true, pos, unescape_all(s.substr(start, pos - start))};
}
struct TitleRes {
    bool ok{false};
    size_t pos{0};
    std::string str;
};
inline TitleRes parse_link_title(std::string_view s, size_t pos, size_t max) {
    if (pos >= max) return TitleRes{};
    const char marker = s[pos];
    if (marker != '"' && marker != '\'' && marker != '(') return TitleRes{};
    const char close = (marker == '(') ? ')' : marker;
    size_t i = pos + 1;
    for (; i < max;) {
        if (s[i] == close)
            return TitleRes{true, i + 1,
                            unescape_all(s.substr(pos + 1, i - pos - 1))};
        if (s[i] == '(' && close == ')') return TitleRes{};
        if (s[i] == '\\' && i + 1 < max) ++i;
        ++i;
    }
    return TitleRes{}; // no closing marker (may continue on next line)
}
// markdown-it normalizeLink = mdurl.encode(mdurl.format(mdurl.parse(url,
// slashes_denote_host=True))): parse strips the URL (case preserved),
// format is identity for common shapes, encode passes through a-zA-Z0-9
// and ";/?:@&=+$,-_.!~*'()#", keeps valid %xx sequences, percent-encodes
// other ASCII, UTF-8 percent-encodes the rest.
inline std::string normalize_link(std::string_view url) {
    // mdurl.parse strips the URL before parsing ("  http://foo.com  ").
    size_t a = 0, b = url.size();
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    while (a < b && ws(url[a])) ++a;
    while (b > a && ws(url[b - 1])) --b;
    std::string s(url.substr(a, b - a));
    auto hex = [](unsigned char c) -> std::string {
        const char* d = "0123456789ABCDEF";
        std::string r(3, '%');
        r[1] = d[c >> 4];
        r[2] = d[c & 0xF];
        return r;
    };
    const char* exc = ";/?:@&=+$,-_.!~*'()#";
    std::string out;
    out.reserve(s.size());
    for (size_t k = 0; k < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[k]);
        if (c == '%' && k + 2 < s.size()) {
            const char n1 = s[k + 1], n2 = s[k + 2];
            auto hd = [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
            };
            if (hd(n1) && hd(n2)) {
                out.append(s.substr(k, 3));
                k += 3;
                continue;
            }
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            std::string_view(exc).find(static_cast<char>(c)) != std::string_view::npos) {
            out += static_cast<char>(c);
            ++k;
            continue;
        }
        if (c < 0x80) {
            out += hex(c);
            ++k;
            continue;
        }
        out += hex(c); // UTF-8 byte pass
        ++k;
    }
    return out;
}
// markdown-it common.normalize_url.validateLink: reject vbscript:/
// javascript:/file:/data: unless data:image/{gif,png,jpeg,webp};
inline bool validate_link(std::string_view url) {
    size_t a = 0, b = url.size();
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    while (a < b && ws(url[a])) ++a;
    while (b > a && ws(url[b - 1])) --b;
    auto lc = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    auto starts_with = [&](const char* p) {
        const size_t n = std::char_traits<char>::length(p);
        if (b - a < n) return false;
        for (size_t k = 0; k < n; ++k)
            if (lc(url[a + k]) != p[k]) return false;
        return true;
    };
    const bool bad = starts_with("vbscript:") || starts_with("javascript:") ||
                     starts_with("file:") || starts_with("data:");
    if (!bad) return true;
    return starts_with("data:image/gif;") || starts_with("data:image/png;") ||
           starts_with("data:image/jpeg;") || starts_with("data:image/webp;");
}
// markdown-it common.utils.normalizeReference: trim, collapse whitespace,
// lowercase.
inline std::string normalize_ref(std::string_view s) {
    std::string out;
    bool in_ws = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_ws = true;
            continue;
        }
        if (in_ws && !out.empty()) out += ' ';
        in_ws = false;
        out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return out;
}
// markdown-it table.escapedSplit: split on '|' honoring '\|' escapes.
inline std::vector<std::string> escaped_split(std::string_view s) {
    std::vector<std::string> out;
    size_t last = 0;
    bool esc = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (esc) {
            esc = false;
            continue;
        }
        if (s[i] == '\\') {
            esc = true;
            continue;
        }
        if (s[i] == '|') {
            out.push_back(std::string(s.substr(last, i - last)));
            last = i + 1;
        }
    }
    out.push_back(std::string(s.substr(last)));
    return out;
}

inline void parse_inline(std::string_view s, std::vector<Tok>& out,
                         const std::map<std::string, std::string>* refs) {
    size_t pos = 0;
    std::string buf;
    auto flush = [&]() {
        if (!buf.empty()) {
            out.push_back(Tok{"text", "span", 0, buf});
            buf.clear();
        }
    };
    auto push_emph = [&](char c, size_t len, const char* name) {
        flush();
        const size_t close = find_close(s, pos + len, c, len);
        if (close == std::string_view::npos) return false;
        // Flat markdown-it shape: open, inner tokens, close as siblings.
        out.push_back(Tok{std::string(name) + "_open", name, 1});
        parse_inline(s.substr(pos + len, close - pos - len), out, refs);
        out.push_back(Tok{std::string(name) + "_close", name, -1});
        pos = close + len;
        return true;
    };
    while (pos < s.size()) {
        const char c = s[pos];
        if (c == '\n') {
            // markdown-it newline rule: trailing spaces before the break are
            // consumed by the break itself (two+ → hard break, one → soft).
            size_t sp = 0;
            while (sp < buf.size() && buf[buf.size() - 1 - sp] == ' ') ++sp;
            buf.erase(buf.size() - sp);
            flush();
            out.push_back(Tok{sp >= 2 ? "hardbreak" : "softbreak", "br"});
            ++pos;
            // Skip heading spaces of the next line.
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
            continue;
        }
        if (c == '\\' && pos + 1 < s.size() && is_escapable(s[pos + 1])) {
            buf += s[pos + 1];
            pos += 2;
            continue;
        }
        if (c == '`') {
            size_t run = 0;
            while (pos + run < s.size() && s[pos + run] == '`') ++run;
            size_t j = pos + run;
            size_t close = std::string_view::npos;
            while (j < s.size()) {
                if (s[j] != '`') { ++j; continue; }
                size_t r2 = 0;
                while (j + r2 < s.size() && s[j + r2] == '`') ++r2;
                if (r2 == run) { close = j; break; }
                j += r2;
            }
            if (close != std::string_view::npos) {
                flush();
                out.push_back(Tok{"code_inline", "code", 0,
                                  std::string(s.substr(pos + run, close - pos - run))});
                pos = close + run;
            } else {
                buf.append(s.substr(pos, run));
                pos += run;
            }
            continue;
        }
        if (c == '!' && pos + 1 < s.size() && s[pos + 1] == '[') {
            const size_t rb = s.find(']', pos + 2);
            if (rb != std::string_view::npos && rb + 1 < s.size() && s[rb + 1] == '(') {
                const size_t cp = s.find(')', rb + 2);
                if (cp != std::string_view::npos) {
                    flush();
                    Tok img;
                    img.type = "image";
                    img.tag = "img";
                    img.content = std::string(s.substr(pos + 2, rb - pos - 2));
                    img.attrs.push_back({"src",
                                         std::string(s.substr(rb + 2, cp - rb - 2))});
                    out.push_back(std::move(img));
                    pos = cp + 1;
                    continue;
                }
            }
            buf += '!';
            ++pos;
            continue;
        }
        if (c == '[') {
            int depth = 1;
            size_t j = pos + 1;
            while (j < s.size() && depth > 0) {
                if (s[j] == '[') ++depth;
                else if (s[j] == ']') --depth;
                ++j;
            }
            const bool balanced = (depth == 0);
            bool done = false;
            const auto emit_link = [&](const std::string& href) {
                flush();
                // Flat markdown-it shape: open, inner tokens, close.
                Tok open;
                open.type = "link_open";
                open.tag = "a";
                open.nesting = 1;
                open.attrs.push_back({"href", href});
                out.push_back(std::move(open));
                parse_inline(s.substr(pos + 1, j - pos - 2), out, refs);
                out.push_back(Tok{"link_close", "a", -1});
            };
            if (balanced && j < s.size() && s[j] == '(') {
                // Inline link: [label](  <dest>  "title"  ). An empty
                // destination is valid ([a]() → link with href ""); a missing
                // ')' falls back to reference parsing.
                size_t ip = j + 1;
                while (ip < s.size() &&
                       (s[ip] == ' ' || s[ip] == '\t' || s[ip] == '\n'))
                    ++ip;
                if (ip < s.size()) {
                    std::string href;
                    const auto dr = parse_link_dest(s, ip, s.size());
                    if (dr.ok) {
                        // markdown-it link rule: normalize, then validate.
                        // A failing validation aborts the inline link (pos
                        // stays at the dest, so the ')' check fails below
                        // and reference parsing is tried).
                        const std::string norm = normalize_link(dr.str);
                        if (validate_link(norm)) {
                            href = norm;
                            ip = dr.pos;
                        }
                    }
                    const size_t ts = ip;
                    while (ip < s.size() &&
                           (s[ip] == ' ' || s[ip] == '\t' || s[ip] == '\n'))
                        ++ip;
                    if (ts != ip) {
                        const auto tr = parse_link_title(s, ip, s.size());
                        if (tr.ok) ip = tr.pos;
                    }
                    while (ip < s.size() &&
                           (s[ip] == ' ' || s[ip] == '\t' || s[ip] == '\n'))
                        ++ip;
                    if (ip < s.size() && s[ip] == ')') {
                        emit_link(href);
                        pos = ip + 1;
                        done = true;
                    }
                }
            }
            if (!done && refs != nullptr) {
                // Link reference: [label][ref], [label][], or [label].
                // j already points one past ']'.
                std::string label;
                size_t ref_end = j; // collapsed / shortcut form
                if (balanced && j < s.size() && s[j] == '[') {
                    int d2 = 1;
                    size_t k = j + 1;
                    while (k < s.size() && d2 > 0) {
                        if (s[k] == '[') ++d2;
                        else if (s[k] == ']') --d2;
                        ++k;
                    }
                    if (d2 == 0) {
                        label = std::string(s.substr(j + 1, k - j - 2));
                        ref_end = k;
                    }
                }
                if (label.empty())
                    label = std::string(s.substr(pos + 1, j - pos - 2));
                const auto it = refs->find(normalize_ref(label));
                if (it != refs->end()) {
                    emit_link(it->second);
                    pos = ref_end;
                    done = true;
                }
            }
            if (!done) {
                buf += '[';
                ++pos;
            }
            continue;
        }
        if (c == '*' || c == '_') {
            const bool dbl = pos + 1 < s.size() && s[pos + 1] == c;
            if (push_emph(c, dbl ? 2 : 1, dbl ? "strong" : "em")) continue;
            buf += c;
            ++pos;
            continue;
        }
        if (c == '~') {
            if (pos + 1 < s.size() && s[pos + 1] == '~') {
                const size_t close = s.find("~~", pos + 2);
                if (close != std::string_view::npos) {
                    flush();
                    // Flat markdown-it shape: open, inner tokens, close.
                    out.push_back(Tok{"s_open", "s", 1});
                    parse_inline(s.substr(pos + 2, close - pos - 2), out, refs);
                    out.push_back(Tok{"s_close", "s", -1});
                    pos = close + 2;
                    continue;
                }
            }
            buf += '~';
            ++pos;
            continue;
        }
        if (c == '<') {
            if (s.compare(pos, 5, "<kbd>") == 0) {
                flush();
                out.push_back(Tok{"html_inline", "kbd", 0, "<kbd>"});
                pos += 5;
                continue;
            }
            if (s.compare(pos, 6, "</kbd>") == 0) {
                flush();
                out.push_back(Tok{"html_inline", "/kbd", 0, "</kbd>"});
                pos += 6;
                continue;
            }
            buf += c;
            ++pos;
            continue;
        }
        buf += c;
        ++pos;
    }
    flush();
}

// ── block ──────────────────────────────────────────────────────────────────
struct Parser {
    std::vector<std::string> lines;
    size_t pos{0};
    // Document-wide link reference definitions ([label]: url). Shared with
    // nested sub-parsers (markdown-it keeps one env.references per document).
    std::map<std::string, std::string>* refs{nullptr};
    // markdown-it blkIndent for the current parse call; all "4 spaces"
    // thresholds (code, lazy continuation) are relative to it.
    size_t min_ind{0};

    bool bullet_at(std::string_view lc, size_t& marker_len) const {
        if (lc.empty()) return false;
        if (lc[0] != '-' && lc[0] != '+' && lc[0] != '*') return false;
        if (lc.size() > 1 && lc[1] != ' ') return false; // marker may end the line
        marker_len = 2;
        return true;
    }
    bool ordered_at(std::string_view lc, size_t& marker_len, int& number) const {
        size_t d = 0;
        while (d < lc.size() && d < 9 && lc[d] >= '0' && lc[d] <= '9') ++d;
        if (d == 0 || d + 1 > lc.size()) return false;
        if (lc[d] != '.' && lc[d] != ')') return false;
        if (d + 1 < lc.size() && lc[d + 1] != ' ') return false;
        number = 0;
        for (size_t k = 0; k < d; ++k) number = number * 10 + (lc[k] - '0');
        marker_len = d + 1;
        return true;
    }
    bool heading_at(std::string_view lc, int& level, std::string& text) const {
        size_t h = 0;
        while (h < lc.size() && lc[h] == '#') ++h;
        if (h < 1 || h > 6 || h >= lc.size() || lc[h] != ' ') return false;
        level = static_cast<int>(h);
        text = std::string(strip_trail(lc.substr(h + 1)));
        while (!text.empty() && text.back() == '#') text.pop_back();
        text = std::string(strip_trail(text));
        return true;
    }
    bool hr_at(std::string_view lc) const {
        if (lc.empty()) return false;
        const char c = lc[0];
        if (c != '-' && c != '*' && c != '_') return false;
        size_t n = 0;
        for (char ch : lc) {
            if (ch == c) ++n;
            else if (ch != ' ') return false;
        }
        return n >= 3;
    }
    bool fence_at(std::string_view lc, char& ch, size_t& n, std::string& info) const {
        if (lc.size() < 3) return false;
        ch = lc[0];
        if (ch != '`' && ch != '~') return false;
        n = 0;
        while (n < lc.size() && lc[n] == ch) ++n;
        if (n < 3) return false;
        info = std::string(strip_trail(lc.substr(n)));
        if (ch == '`' && info.find('`') != std::string::npos) return false;
        return true;
    }
    bool fence_close(std::string_view lc, char ch, size_t n) const {
        size_t k = 0;
        while (k < lc.size() && lc[k] == ch) ++k;
        if (k < n) return false;
        for (size_t i = k; i < lc.size(); ++i)
            if (lc[i] != ' ') return false;
        return true;
    }

    // markdown-it table rule: the header line is `lines[hline]`; validates the
    // separator line beneath it. Fills `aligns` ("left"/"center"/"right"/"" per
    // real cell) and `head_cells` (stripped, '\|'-aware) when valid.
    bool table_at(size_t hline, std::vector<std::string>* aligns,
                  std::vector<std::string>* head_cells) const {
        if (hline + 1 >= lines.size()) return false;
        const size_t h_ind = lead_spaces(lines[hline]);
        if (h_ind < min_ind || h_ind - min_ind >= 4) return false;
        const std::string& l = lines[hline + 1];
        const size_t sep_ind = lead_spaces(l);
        if (sep_ind < min_ind || sep_ind - min_ind >= 4) return false;
        const std::string_view t =
            std::string_view(l).substr(sep_ind);
        if (t.size() < 2) return false;
        // First char '|', '-', ':'; second '|', '-', ':' or space; '-' must not
        // be followed by a space (list ambiguity).
        if (t[0] != '|' && t[0] != '-' && t[0] != ':') return false;
        if (t[1] != '|' && t[1] != '-' && t[1] != ':' && t[1] != ' ')
            return false;
        if (t[0] == '-' && t[1] == ' ') return false;
        for (size_t i = 2; i < t.size(); ++i) {
            const char ch = t[i];
            if (ch != '|' && ch != '-' && ch != ':' && ch != ' ') return false;
        }
        // Plain '|' split; empty cells only allowed as the first/last column.
        if (aligns) aligns->clear();
        {
            size_t i = 0;
            for (;;) {
                const size_t pipe = t.find('|', i);
                std::string_view c =
                    (pipe == std::string_view::npos)
                        ? t.substr(i)
                        : t.substr(i, pipe - i);
                size_t a = 0, b = c.size();
                while (a < b && (c[a] == ' ' || c[a] == '\t')) ++a;
                while (b > a && (c[b - 1] == ' ' || c[b - 1] == '\t')) --b;
                const bool empty = (a == b);
                if (!empty) {
                    std::string_view cell = c.substr(a, b - a);
                    size_t k = 0;
                    const bool lc_ = (k < cell.size() && cell[k] == ':');
                    if (lc_) ++k;
                    size_t dashes = 0;
                    while (k < cell.size() && cell[k] == '-') { ++k; ++dashes; }
                    const bool rc_ = (k < cell.size() && cell[k] == ':');
                    if (rc_) ++k;
                    if (dashes < 1 || k != cell.size()) return false;
                    if (aligns) {
                        if (lc_ && rc_)
                            aligns->push_back("center");
                        else if (rc_)
                            aligns->push_back("right");
                        else if (lc_)
                            aligns->push_back("left");
                        else
                            aligns->push_back("");
                    }
                } else if (i != 0 && pipe != std::string_view::npos) {
                    return false; // empty cell in between
                }
                // else: leading/trailing empty column — allowed
                if (pipe == std::string_view::npos) break;
                i = pipe + 1;
            }
        }
        // Header: must contain '|'; cell count must match the separator's.
        const std::string& hl = lines[hline];
        std::string_view hv = strip_trail(std::string_view(hl).substr(lead_spaces(hl)));
        size_t h0 = 0;
        while (h0 < hv.size() && (hv[h0] == ' ' || hv[h0] == '\t')) ++h0;
        hv = hv.substr(h0);
        if (hv.find('|') == std::string_view::npos) return false;
        auto hc = escaped_split(hv);
        if (!hc.empty() && hc.front().empty()) hc.erase(hc.begin());
        if (!hc.empty() && hc.back().empty()) hc.pop_back();
        if (hc.empty() || (aligns != nullptr && hc.size() != aligns->size()))
            return false;
        if (head_cells)
            for (auto& c : hc) {
                size_t a = 0, b = c.size();
                while (a < b && (c[a] == ' ' || c[a] == '\t')) ++a;
                while (b > a && (c[b - 1] == ' ' || c[b - 1] == '\t')) --b;
                c = c.substr(a, b - a);
            }
        if (head_cells) *head_cells = std::move(hc);
        return true;
    }
    // markdown-it block `reference` rule (single line): [label]: dest "title".
    // Consumes the definition into `refs`; returns true when it parsed.
    bool refdef_at(std::string_view lc, std::string& href) {
        if (lc.empty() || lc[0] != '[') return false;
        size_t label_end = std::string_view::npos;
        for (size_t i = 1; i < lc.size(); ++i) {
            const char ch = lc[i];
            if (ch == '[') return false;
            if (ch == ']') {
                label_end = i;
                break;
            }
            if (ch == '\\' && i + 1 < lc.size()) ++i;
        }
        if (label_end == std::string_view::npos || label_end + 1 >= lc.size() ||
            lc[label_end + 1] != ':')
            return false;
        size_t p = label_end + 2;
        while (p < lc.size() && (lc[p] == ' ' || lc[p] == '\t')) ++p;
        const auto dr = parse_link_dest(lc, p, lc.size());
        if (!dr.ok) return false;
        size_t q = dr.pos;
        const size_t tstart = q;
        while (q < lc.size() && (lc[q] == ' ' || lc[q] == '\t')) ++q;
        if (tstart != q) {
            const auto tr = parse_link_title(lc, q, lc.size());
            if (tr.ok) q = tr.pos;
        }
        while (q < lc.size() && (lc[q] == ' ' || lc[q] == '\t')) ++q;
        if (q < lc.size()) return false; // garbage after dest/title
        const std::string key = normalize_ref(lc.substr(1, label_end - 1));
        if (key.empty()) return false;
        href = normalize_link(dr.str);
        if (!validate_link(href)) return false; // def rejected (markdown-it)
        if (refs != nullptr && refs->count(key) == 0) (*refs)[key] = href;
        return true;
    }
    // Paragraph terminator set (markdown-it getRules("paragraph")): table,
    // fence, blockquote, hr, list, heading. `l` is the line index to test.
    bool paragraph_terminates(size_t l) const {
        const std::string& line = lines[l];
        const size_t li = lead_spaces(line);
        // A would-be code block is a lazy paragraph continuation, not a
        // terminator. Lines shallower than min_ind still terminate (the
        // sub-parser then stops at them via the min-indent check).
        if (li >= min_ind && li - min_ind >= 4) return false;
        const std::string_view lc = std::string_view(line).substr(li);
        if (lc.empty()) return false;
        if (lc[0] == '>') return true;
        char f2 = 0;
        size_t f2n = 0;
        std::string fi;
        if (fence_at(lc, f2, f2n, fi)) return true;
        int lv = 0;
        std::string ht;
        if (heading_at(lc, lv, ht)) return true;
        if (hr_at(lc)) return true;
        size_t m = 0;
        int n2 = 0;
        if (bullet_at(lc, m) || ordered_at(lc, m, n2)) return true;
        if (lc.find('|') != std::string_view::npos)
            return table_at(l, nullptr, nullptr);
        return false;
    }
    // markdown-it lheading rule: scan forward from the paragraph start (pos)
    // for a setext underline ('=' → h1, '-' → h2).
    bool setext_scan(int& level, size_t& end) const {
        size_t nl = pos + 1;
        while (nl < lines.size()) {
            const std::string& l = lines[nl];
            if (is_blank(l)) break;
            const size_t li = lead_spaces(l);
            if (li >= min_ind && li - min_ind > 3) {
                ++nl;
                continue; // lazy continuation
            }
            if (li >= min_ind) {
                const std::string_view lc = std::string_view(l).substr(li);
                if (!lc.empty() && (lc[0] == '=' || lc[0] == '-')) {
                    size_t k = 1;
                    while (k < lc.size() && lc[k] == lc[0]) ++k;
                    while (k < lc.size() && (lc[k] == ' ' || lc[k] == '\t')) ++k;
                    if (k >= lc.size()) {
                        level = (lc[0] == '=') ? 1 : 2;
                        end = nl;
                        return true;
                    }
                }
            }
            if (paragraph_terminates(nl)) break;
            ++nl;
        }
        return false;
    }

    // Table body terminator set (markdown-it getRules("blockquote")):
    // fence, blockquote, hr, list, heading — not table.
    bool table_row_terminates(size_t l) const {
        const std::string& line = lines[l];
        const size_t li = lead_spaces(line);
        if (li >= min_ind && li - min_ind >= 4) return false;
        const std::string_view lc = std::string_view(line).substr(li);
        if (lc.empty()) return false;
        if (lc[0] == '>') return true;
        char f2 = 0;
        size_t f2n = 0;
        std::string fi;
        if (fence_at(lc, f2, f2n, fi)) return true;
        int lv = 0;
        std::string ht;
        if (heading_at(lc, lv, ht)) return true;
        if (hr_at(lc)) return true;
        size_t m = 0;
        int n2 = 0;
        return bullet_at(lc, m) || ordered_at(lc, m, n2);
    }
    void emit_table(size_t hline, const std::vector<std::string>& aligns,
                    const std::vector<std::string>& head,
                    std::vector<Tok>& out) {
        out.push_back(Tok{"table_open", "table", 1});
        {
            Tok thead{"thead_open", "thead", 1};
            Tok tr{"tr_open", "tr", 1};
            for (size_t i = 0; i < head.size(); ++i) {
                Tok th{"th_open", "th", 1};
                if (i < aligns.size() && !aligns[i].empty())
                    th.attrs.push_back({"style", "text-align:" + aligns[i]});
                Tok inl{"inline", "", 0, head[i]};
                tr.children.push_back(std::move(th));
                tr.children.push_back(std::move(inl));
                tr.children.push_back(Tok{"th_close", "th", -1});
            }
            thead.children.push_back(std::move(tr));
            thead.children.push_back(Tok{"tr_close", "tr", -1});
            out.push_back(std::move(thead));
            out.push_back(Tok{"thead_close", "thead", -1});
        }
        Tok tbody{"tbody_open", "tbody", 1};
        bool any_body = false;
        size_t row = hline + 2;
        while (row < lines.size()) {
            const std::string& l = lines[row];
            if (is_blank(l)) break;
            const size_t li = lead_spaces(l);
            if (li < min_ind || li - min_ind >= 4) break;
            if (table_row_terminates(row)) break;
            std::string_view lt =
                strip_trail(std::string_view(l).substr(li));
            size_t a = 0;
            while (a < lt.size() && (lt[a] == ' ' || lt[a] == '\t')) ++a;
            lt = lt.substr(a);
            if (lt.empty()) break;
            if (!any_body) any_body = true;
            auto cells = escaped_split(lt);
            if (!cells.empty() && cells.front().empty())
                cells.erase(cells.begin());
            if (!cells.empty() && cells.back().empty()) cells.pop_back();
            Tok r{"tr_open", "tr", 1};
            for (size_t i = 0; i < head.size(); ++i) {
                Tok td{"td_open", "td", 1};
                if (i < aligns.size() && !aligns[i].empty())
                    td.attrs.push_back({"style", "text-align:" + aligns[i]});
                std::string cell;
                if (i < cells.size()) {
                    cell = cells[i];
                    size_t c0 = 0, c1 = cell.size();
                    while (c0 < c1 && (cell[c0] == ' ' || cell[c0] == '\t'))
                        ++c0;
                    while (c1 > c0 && (cell[c1 - 1] == ' ' || cell[c1 - 1] == '\t'))
                        --c1;
                    cell = cell.substr(c0, c1 - c0);
                }
                Tok inl{"inline", "", 0, cell};
                r.children.push_back(std::move(td));
                r.children.push_back(std::move(inl));
                r.children.push_back(Tok{"td_close", "td", -1});
            }
            tbody.children.push_back(std::move(r));
            tbody.children.push_back(Tok{"tr_close", "tr", -1});
            ++row;
        }
        if (any_body) {
            out.push_back(std::move(tbody));
            out.push_back(Tok{"tbody_close", "tbody", -1});
        }
        out.push_back(Tok{"table_close", "table", -1});
        pos = row;
    }
    void parse_blocks(size_t min_indent, std::vector<Tok>& out) {
        min_ind = min_indent;
        while (pos < lines.size()) {
            const std::string& line = lines[pos];
            if (is_blank(line)) { ++pos; continue; }
            const size_t ind = lead_spaces(line);
            if (ind < min_indent) return; // nested-call termination (item/bq)
            const std::string_view content = std::string_view(line).substr(ind);
            const bool rel_code = (ind - min_indent >= 4);

            // table (markdown-it tries it first)
            if (!rel_code && content.find('|') != std::string_view::npos) {
                std::vector<std::string> aligns, head;
                if (table_at(pos, &aligns, &head)) {
                    emit_table(pos, aligns, head, out);
                    continue;
                }
            }
            // indented code block (before fence: an indented fence is code)
            if (rel_code) {
                const size_t code_col = min_indent + 4;
                std::string code;
                while (pos < lines.size()) {
                    const std::string& l = lines[pos];
                    if (is_blank(l)) {
                        size_t k = pos + 1;
                        while (k < lines.size() && is_blank(lines[k])) ++k;
                        if (k < lines.size() && lead_spaces(lines[k]) >= code_col) {
                            code += "\n";
                            ++pos;
                            continue;
                        }
                        break;
                    }
                    if (lead_spaces(l) < code_col) break;
                    code += l.substr(std::min<size_t>(code_col, lead_spaces(l)));
                    code += "\n";
                    ++pos;
                }
                out.push_back(Tok{"code_block", "code", 0, std::move(code)});
                continue;
            }
            // fence
            char fch = 0;
            size_t fn = 0;
            std::string finfo;
            if (fence_at(content, fch, fn, finfo)) {
                ++pos;
                std::string code;
                const size_t fence_ind = ind; // CommonMark: strip ≤ fence indent (max 3)
                for (;;) {
                    if (pos >= lines.size()) break;
                    const std::string_view l = lines[pos];
                    const size_t li = lead_spaces(l);
                    if (li <= fence_ind + 3 && fence_close(l.substr(li), fch, fn)) {
                        ++pos;
                        break;
                    }
                    const size_t strip_max = std::min<size_t>(fence_ind, 3);
                    const size_t strip = std::min(li, strip_max);
                    code += l.substr(strip);
                    code += "\n";
                    ++pos;
                }
                out.push_back(Tok{"fence", "code", 0, std::move(code), finfo});
                continue;
            }
            // reference definition ([label]: url "title") — consumes the line
            if (!rel_code) {
                std::string href;
                if (refdef_at(content, href)) {
                    ++pos;
                    continue;
                }
            }
            // heading (ATX)
            int level = 0;
            std::string htext;
            if (!rel_code && heading_at(content, level, htext)) {
                // markdown-it shape: heading_open has no children; the inline
                // token (with the parsed children) is a sibling.
                Tok open{"heading_open", "h" + std::to_string(level), 1};
                Tok inl{"inline", "", 0, htext};
                out.push_back(std::move(open));
                out.push_back(std::move(inl));
                out.push_back(Tok{"heading_close", "h" + std::to_string(level), -1});
                ++pos;
                continue;
            }
            // hr
            if (!rel_code && hr_at(content)) {
                out.push_back(Tok{"hr", "hr"});
                ++pos;
                continue;
            }
            // blockquote
            if (!content.empty() && content[0] == '>') {
                std::vector<std::string> inner;
                while (pos < lines.size()) {
                    const std::string_view l = lines[pos];
                    const size_t li = lead_spaces(l);
                    if (li > ind || li >= 4) break;
                    const std::string_view lc = l.substr(li);
                    if (lc.empty() || lc[0] != '>') break;
                    std::string_view rest = lc.substr(1);
                    if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
                    inner.push_back(std::string(rest));
                    ++pos;
                }
                Tok open{"blockquote_open", "blockquote", 1};
                {
                    Parser sub;
                    sub.lines = std::move(inner);
                    sub.pos = 0;
                    sub.refs = refs;
                    sub.parse_blocks(0, open.children);
                }
                out.push_back(std::move(open));
                out.push_back(Tok{"blockquote_close", "blockquote", -1});
                continue;
            }
            // lists
            size_t marker_len = 0;
            int ord_num = 0;
            const bool bullet = bullet_at(content, marker_len);
            const bool ordered = !bullet && ordered_at(content, marker_len, ord_num);
            if ((bullet || ordered) && !rel_code) {
                Tok list_open{ordered ? "ordered_list_open" : "bullet_list_open",
                             ordered ? "ol" : "ul", 1};
                if (ordered) list_open.attrs.push_back({"start", std::to_string(ord_num)});
                out.push_back(std::move(list_open));
                const char item_marker = ordered ? content[marker_len - 1] : content[0];
                for (;;) {
                    // Item marker line (the outer checks below re-validate pos).
                    const std::string& il = lines[pos];
                    const size_t iind = lead_spaces(il);
                    const std::string_view ilc = std::string_view(il).substr(iind);
                    size_t ml = 0;
                    int num = 0;
                    bullet ? bullet_at(ilc, ml) : ordered_at(ilc, ml, num);
                    // markdown-it list item geometry:
                    //   initial       = column just after the marker
                    //   content_start = column after the marker's spaces
                    //   blkIndent     = initial + min(spaces_after_marker, 4),
                    //                   capped to 1 when > 4 spaces
                    const size_t initial = iind + (bullet ? 1 : ml);
                    size_t cstart = initial;
                    while (cstart < il.size() && il[cstart] == ' ') ++cstart;
                    size_t iam = (cstart >= il.size()) ? 1 : (cstart - initial);
                    if (iam > 4) iam = 1;
                    const size_t item_blk = initial + iam;
                    // Re-parse the item's lines as a sub-document (markdown-it
                    // runs the block tokenizer recursively with blkIndent =
                    // item indent); the marker prefix becomes spaces so the
                    // sub-parser sees the content at its true column.
                    std::vector<std::string> sub_lines(lines);
                    sub_lines[pos] =
                        std::string(cstart, ' ') + std::string(il.substr(cstart));
                    Tok li_tok{"list_item_open", "li", 1};
                    if (ordered) li_tok.info = std::to_string(num);
                    {
                        Parser sub;
                        sub.lines = std::move(sub_lines);
                        sub.pos = pos;
                        sub.refs = refs;
                        sub.parse_blocks(item_blk, li_tok.children);
                        pos = sub.pos;
                    }
                    out.push_back(std::move(li_tok));
                    out.push_back(Tok{"list_item_close", "li", -1});
                    // List loop: terminate or find the next item.
                    if (pos >= lines.size()) break;
                    {
                        const std::string& nl = lines[pos]; // non-blank (sub skips blanks)
                        const size_t ni = lead_spaces(nl);
                        if (ni < min_indent) break;
                        if (ni - min_indent >= 4) break; // code block ends the list
                        const std::string_view nlc = std::string_view(nl).substr(ni);
                        char f2 = 0;
                        size_t f2n = 0;
                        std::string fi;
                        if (nlc[0] == '>' || fence_at(nlc, f2, f2n, fi) ||
                            hr_at(nlc))
                            break; // list terminators: fence, blockquote, hr
                        size_t nml = 0;
                        int nnum = 0;
                        const bool nitem =
                            bullet ? bullet_at(nlc, nml) : ordered_at(nlc, nml, nnum);
                        if (!nitem) break;
                        const char nmc = ordered ? nlc[nml - 1] : nlc[0];
                        if (nmc != item_marker) break; // marker char change → new list
                    }
                }
                out.push_back(Tok{ordered ? "ordered_list_close" : "bullet_list_close",
                                  ordered ? "ol" : "ul", -1});
                continue;
            }
            // setext heading (lheading): paragraph line(s) + '=' or '-' line.
            // Tried after lists (markdown-it order: list before lheading), so
            // a list marker line wins over a setext marker on the same line.
            if (!rel_code) {
                int sl = 0;
                size_t send = pos;
                if (setext_scan(sl, send)) {
                    std::string stext;
                    for (size_t k = pos; k < send; ++k) {
                        if (k > pos) stext += "\n";
                        stext += lines[k];
                    }
                    stext = std::string(strip_trail(stext));
                    size_t b0 = 0;
                    while (b0 < stext.size() &&
                           (stext[b0] == ' ' || stext[b0] == '\t'))
                        ++b0;
                    stext = stext.substr(b0);
                    Tok open{"heading_open", "h" + std::to_string(sl), 1};
                    Tok inl{"inline", "", 0, stext};
                    out.push_back(std::move(open));
                    out.push_back(std::move(inl));
                    out.push_back(Tok{"heading_close", "h" + std::to_string(sl), -1});
                    pos = send + 1;
                    continue;
                }
            }
            // paragraph (else)
            {
                std::vector<std::string> para{std::string(content)};
                ++pos;
                for (;;) {
                    if (pos >= lines.size() || is_blank(lines[pos])) break;
                    const size_t li = lead_spaces(lines[pos]);
                    if (li >= min_indent && li - min_indent > 3) {
                        // Lazy continuation (would be a code block on its own).
                        para.push_back(
                            std::string(std::string_view(lines[pos]).substr(li)));
                        ++pos;
                        continue;
                    }
                    if (paragraph_terminates(pos)) break;
                    para.push_back(std::string(
                        std::string_view(lines[pos]).substr(li)));
                    ++pos;
                }
                std::string joined;
                for (size_t k = 0; k < para.size(); ++k) {
                    if (k) joined += "\n";
                    joined += para[k];
                }
                Tok p_open{"paragraph_open", "p", 1};
                Tok inl{"inline", "", 0, joined};
                out.push_back(std::move(p_open));
                out.push_back(std::move(inl));
                out.push_back(Tok{"paragraph_close", "p", -1});
            }
        }
    }
};

// Deferred inline pass: markdown-it tokenizes inline content only after the
// whole block stream is built (so reference links see every definition,
// including forward ones).
inline void parse_inlines(std::vector<Tok>& toks,
                          const std::map<std::string, std::string>& refs) {
    for (auto& t : toks) {
        if (t.type == "inline" && !t.content.empty() && t.children.empty())
            parse_inline(t.content, t.children, &refs);
        parse_inlines(t.children, refs);
    }
}

inline std::vector<Tok> parse_blocks(std::string_view doc) {
    std::vector<std::string> lines;
    size_t i = 0;
    while (i < doc.size()) {
        const size_t nl = doc.find('\n', i);
        std::string l = (nl == std::string_view::npos)
                            ? std::string(doc.substr(i))
                            : std::string(doc.substr(i, nl - i));
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(std::move(l));
        if (nl == std::string_view::npos) break;
        i = nl + 1;
    }
    std::map<std::string, std::string> refs;
    Parser p;
    p.lines = std::move(lines);
    p.refs = &refs;
    std::vector<Tok> out;
    p.parse_blocks(0, out);
    parse_inlines(out, refs);
    return out;
}

inline std::vector<Tok> flatten(const std::vector<Tok>& tokens) {
    // Produce the flat markdown-it stream: every open/close/leaf token as a
    // sibling. Only `inline` is a pure container (recurse without emitting);
    // open tokens that carry nested blocks (li, blockquote, thead, tr,
    // tbody) are emitted AND their children are flattened after them.
    std::vector<Tok> out;
    std::function<void(const std::vector<Tok>&)> rec = [&](const std::vector<Tok>& toks) {
        for (const auto& t : toks) {
            if (t.type == "inline" && !t.children.empty()) {
                rec(t.children);
            } else {
                out.push_back(t);
                if (!t.children.empty()) rec(t.children);
            }
        }
    };
    rec(tokens);
    return out;
}

} // namespace parse_detail

using Token = parse_detail::Tok;

// ────────────────────────────────────────────────────────────────────────────
// Markdown theme styles (rich/default_styles.py, markdown.* entries).
namespace theme_detail {

inline const Style& md_style(const std::string& name) {
    // Colors are SGR codes (see Color::from_ansi + sgr_to_palette); repr
    // shows the derived palette number (e.g. SGR 35 → magenta number=5).
    static const std::map<std::string, Style> table = {
        {"markdown.paragraph", Style{}},
        {"markdown.em", Style{.italic = true, .explicit_bits = 0x04}},
        {"markdown.strong", Style{.bold = true, .explicit_bits = 0x01}},
        {"markdown.code", Style{.bold = true,
                                .color = Color::from_ansi(36),
                                .bg_color = Color::from_ansi(30),
                                .explicit_bits = 0x01}},
        {"markdown.code_block", Style{.color = Color::from_ansi(36),
                                      .bg_color = Color::from_ansi(30)}},
        {"markdown.block_quote", Style{.color = Color::from_ansi(35)}},
        {"markdown.list", Style{.color = Color::from_ansi(36)}},
        {"markdown.item", Style{}},
        {"markdown.item.bullet", Style{.bold = true, .explicit_bits = 0x01}},
        {"markdown.item.number", Style{.color = Color::from_ansi(36)}},
        {"markdown.hr", Style{.dim = true, .explicit_bits = 0x02}},
        {"markdown.h1", Style{.bold = true, .underline = true, .explicit_bits = 0x09}},
        {"markdown.h2", Style{.underline = true,
                              .color = Color::from_ansi(35),
                              .explicit_bits = 0x08}},
        {"markdown.h3", Style{.bold = true,
                              .color = Color::from_ansi(35),
                              .explicit_bits = 0x01}},
        {"markdown.h4", Style{.italic = true,
                              .color = Color::from_ansi(35),
                              .explicit_bits = 0x04}},
        {"markdown.h5", Style{.italic = true, .explicit_bits = 0x04}},
        {"markdown.h6", Style{.dim = true, .explicit_bits = 0x02}},
        {"markdown.link", Style{.color = Color::from_ansi(94)}},
        {"markdown.link_url", Style{.underline = true,
                                    .color = Color::from_ansi(34),
                                    .explicit_bits = 0x08}},
        {"markdown.s", Style{.strike = true, .explicit_bits = 0x20}},
        {"markdown.table.border", Style{.color = Color::from_ansi(36)}},
        {"markdown.kbd", Style{.bold = true,
                               .color = Color::from_ansi(93),
                               .explicit_bits = 0x01}},
    };
    // markdown.table.header needs an explicit bold=False — built separately.
    static Style table_header;
    static const bool th_init = [] {
        table_header.color = Color::from_ansi(36);
        table_header.explicit_bits = 0x01; // bold explicitly false
        return true;
    }();
    (void)th_init;
    static const Style kEmpty = Style{}; // map miss → falsy empty style
    if (name == "markdown.table.header") return table_header;
    auto it = table.find(name);
    return it != table.end() ? it->second : kEmpty;
}

} // namespace theme_detail

// ────────────────────────────────────────────────────────────────────────────
// Elements.
struct El;
struct Ctx;

struct El {
    virtual ~El() = default;
    bool new_line = true;
    virtual void on_enter(Ctx& c) { (void)c; }
    virtual void on_leave(Ctx& c) { (void)c; }
    virtual void on_text(Ctx& c, std::string_view t) { (void)c; (void)t; }
    virtual bool on_child_close(El* child) { (void)child; return true; }
    virtual std::vector<Segment> render(Ctx& c) const { (void)c; return {}; }
};

// Context + render loop. Defined before the concrete elements so their
// on_enter/on_text/on_leave bodies can call into it.
struct Ctx {
    const Console* con;
    Options opt;
    bool hyperlinks;
    std::vector<Style> style_stack{Style{}};
    std::vector<std::unique_ptr<El>> stack;

    const Style& cur() const { return style_stack.back(); }
    void enter(const Style& s) { style_stack.push_back(style_stack.back() + s); }
    void leave() { style_stack.pop_back(); }
    void on_text(std::string_view t) {
        if (!stack.empty()) stack.back()->on_text(*this, t);
    }
    // Context copy rendering at a narrower width (children of quotes and
    // lists); element tree and style stack are not copied.
    Ctx with_width(size_t w) const {
        Ctx c2;
        c2.con = con;
        c2.opt = opt;
        c2.opt.width = w;
        c2.hyperlinks = hyperlinks;
        return c2;
    }
};

struct ElUnknown : El {};

struct ElParagraph : El {
    md::Text text;
    md::Text::J justify{md::Text::J::Left};
    void on_enter(Ctx& c) override { c.enter(theme_detail::md_style("markdown.paragraph")); }
    void on_leave(Ctx& c) override { c.leave(); }
    void on_text(Ctx& c, std::string_view t) override { text.append(t, &c.cur()); }
    std::vector<Segment> render(Ctx& c) const override {
        auto t = text;
        t.set_justify(justify);
        return t.render(c.opt.width);
    }
};

struct ElHeading : El {
    md::Text text;
    int level{1};
    void on_enter(Ctx& c) override {
        c.enter(theme_detail::md_style("markdown.h" + std::to_string(level)));
    }
    void on_leave(Ctx& c) override { c.leave(); }
    void on_text(Ctx& c, std::string_view t) override { text.append(t, &c.cur()); }
    std::vector<Segment> render(Ctx& c) const override {
        auto t = text;
        t.set_justify((level == 1) ? md::Text::J::Center : md::Text::J::Left);
        return t.render(c.opt.width);
    }
};

struct ElCodeBlock : El {
    md::Text text;
    std::string lexer{"text"};
    void on_enter(Ctx& c) override {
        c.enter(theme_detail::md_style("markdown.code_block"));
    }
    void on_leave(Ctx& c) override { c.leave(); }
    void on_text(Ctx& c, std::string_view t) override { text.append(t, &c.cur()); }
    std::vector<Segment> render(Ctx& c) const override {
        std::string code = text.plain_ref();
        while (!code.empty() &&
               (code.back() == ' ' || code.back() == '\t' || code.back() == '\n' ||
                code.back() == '\r'))
            code.pop_back();
        highlight::SyntaxOptions so{};
        so.word_wrap = true;
        so.padding = 1;
        highlight::Syntax s{std::move(code), lexer, "monokai", so};
        return s.render(*c.con, c.opt);
    }
};

struct ElBlockQuote : El {
    std::vector<std::unique_ptr<El>> children;
    Style style;
    void on_enter(Ctx& c) override {
        c.enter(theme_detail::md_style("markdown.block_quote"));
        style = c.cur();
    }
    void on_leave(Ctx& c) override { c.leave(); }
    bool on_child_close(El* child) override {
        children.push_back(std::unique_ptr<El>(child));
        return false;
    }
    std::vector<Segment> render(Ctx& c) const override;
};

struct ElHRule : El {
    ElHRule() { new_line = false; }
    std::vector<Segment> render(Ctx& c) const override {
        const Style dim = theme_detail::md_style("markdown.hr");
        std::vector<Segment> out;
        out.push_back({std::string(c.opt.width, '-'), dim});
        out.push_back({"\n", rich::detail::kNoneStyle()});
        out.push_back({"", rich::detail::kNoneStyle()}); // Text() body
        out.push_back({"\n", rich::detail::kNoneStyle()}); // Text end
        return out;
    }
};

struct ElListItem;
struct ElList : El {
    bool bullet{true};
    int start{1};
    std::vector<std::unique_ptr<ElListItem>> items;
    bool on_child_close(El* child) override; // after ElListItem is complete
    std::vector<Segment> render(Ctx& c) const override;
};

struct ElListItem : El {
    md::Text text;
    Style style;
    std::vector<std::unique_ptr<El>> children;
    void on_enter(Ctx& c) override {
        c.enter(theme_detail::md_style("markdown.item"));
        style = c.cur();
    }
    void on_leave(Ctx& c) override { c.leave(); }
    void on_text(Ctx& c, std::string_view t) override { text.append(t, &c.cur()); }
    bool on_child_close(El* child) override {
        children.push_back(std::unique_ptr<El>(child));
        return false;
    }
};

inline bool ElList::on_child_close(El* child) {
    items.push_back(std::unique_ptr<ElListItem>(static_cast<ElListItem*>(child)));
    return false;
}

struct ElLink : El {
    md::Text text;
    std::string href;
    void on_text(Ctx& c, std::string_view t) override { text.append(t, &c.cur()); }
};

struct ElImage : El {
    ElImage() { new_line = false; }
    std::string destination;
    bool hyperlinks{true};
    md::Text text;
    std::string link;
    void on_enter(Ctx& c) override {
        link = c.cur().link;
        c.enter(theme_detail::md_style("none")); // Style{}
    }
    void on_leave(Ctx& c) override { c.leave(); }
    void on_text(Ctx& c, std::string_view t) override { text.append(t, &c.cur()); }
    std::vector<Segment> render(Ctx& c) const override;
};

struct ElTableData : El {
    md::Text content;
    md::Text::J justify{md::Text::J::Default};
    void on_text(Ctx& c, std::string_view t) override { content.append(t, &c.cur()); }
};

struct ElTableRow : El {
    std::vector<std::unique_ptr<ElTableData>> cells;
    bool on_child_close(El* child) override {
        cells.push_back(std::unique_ptr<ElTableData>(static_cast<ElTableData*>(child)));
        return false;
    }
};

struct ElTableBody : El {
    std::vector<std::unique_ptr<ElTableRow>> rows;
    bool on_child_close(El* child) override {
        rows.push_back(std::unique_ptr<ElTableRow>(static_cast<ElTableRow*>(child)));
        return false;
    }
};

struct ElTableHead : El {
    std::unique_ptr<ElTableRow> row;
    bool on_child_close(El* child) override {
        row.reset(static_cast<ElTableRow*>(child));
        return false;
    }
};

struct ElTable : El {
    std::unique_ptr<ElTableHead> header;
    std::unique_ptr<ElTableBody> body;
    bool on_child_close(El* child) override {
        if (auto* h = dynamic_cast<ElTableHead*>(child))
            header.reset(h);
        else if (auto* b = dynamic_cast<ElTableBody*>(child))
            body.reset(b);
        return false;
    }
    std::vector<Segment> render(Ctx& c) const override;
};

// Pre-rendered segment blocks fed back into Console::render_lines
// (equivalent of Python's Renderables-of-elements).
struct BareSegments {
    std::vector<Segment> segs;
    std::vector<Segment> render(const Console&, const Options&) const { return segs; }
    std::pair<size_t, size_t> measure(const Console&, const Options&) const {
        size_t w = 0;
        for (const auto& s : segs) w += unicode::string_cell_len(s.text);
        return {1, w};
    }
};

// ADL hooks must live in the same namespace as BareSegments.
inline std::vector<Segment> render_item(const BareSegments& b, const Console& c,
                                        const Options& o) {
    return b.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const BareSegments& b, const Console& c,
                                              const Options& o) {
    return b.measure(c, o);
}

class Markdown {
public:
    Markdown(std::string doc, bool hyperlinks = true)
        : tokens_(parse_detail::parse_blocks(doc)), hyperlinks_(hyperlinks) {}

    std::vector<Segment> render(const Console& c, const Options& opt) const {
        Options o = opt;
        o.height = 0; // options.update(height=None)
        Ctx ctx{&c, o, hyperlinks_};
        std::vector<Segment> out;
        bool new_line = false;
        const auto flat = parse_detail::flatten(tokens_);
        for (const auto& tok : flat) {
            const std::string& node_type = tok.type;
            const std::string& tag = tok.tag;
            const bool entering = tok.nesting == 1;
            const bool exiting = tok.nesting == -1;
            const bool self_closing = tok.nesting == 0;

            if (node_type == "text") {
                ctx.on_text(tok.content);
            } else if (node_type == "hardbreak") {
                ctx.on_text("\n");
            } else if (node_type == "softbreak") {
                ctx.on_text(" ");
            } else if (node_type == "link_open") {
                const std::string* href = tok.attr("href");
                const std::string url = href ? *href : "";
                if (hyperlinks_) {
                    Style ls = theme_detail::md_style("markdown.link_url");
                    ls.link = url;
                    ctx.enter(ls);
                } else {
                    auto el = std::make_unique<ElLink>();
                    el->href = url;
                    ctx.stack.push_back(std::move(el));
                }
            } else if (node_type == "html_inline") {
                if (tok.content == "<kbd>") {
                    ctx.enter(theme_detail::md_style("markdown.kbd"));
                } else if (tok.content == "</kbd>") {
                    ctx.leave();
                } else {
                    continue;
                }
            } else if (node_type == "link_close") {
                if (hyperlinks_) {
                    ctx.leave();
                } else {
                    auto el = std::move(ctx.stack.back());
                    ctx.stack.pop_back();
                    auto* link = static_cast<ElLink*>(el.get());
                    ctx.enter(theme_detail::md_style("markdown.link"));
                    ctx.on_text(link->text.plain());
                    ctx.leave();
                    ctx.on_text(" (");
                    ctx.enter(theme_detail::md_style("markdown.link_url"));
                    ctx.on_text(link->href);
                    ctx.leave();
                    ctx.on_text(")");
                }
            } else if ((tag == "em" || tag == "strong" || tag == "code" || tag == "s") &&
                       node_type != "fence" && node_type != "code_block") {
                if (entering) {
                    ctx.enter(theme_detail::md_style("markdown." + tag));
                } else if (exiting) {
                    ctx.leave();
                } else {
                    ctx.enter(theme_detail::md_style("markdown." + tag));
                    if (!tok.content.empty()) ctx.on_text(tok.content);
                    ctx.leave();
                }
            } else {
                // Block elements.
                El* element = make_element(tok);
                std::unique_ptr<El> owned(element);
                if (entering || self_closing) {
                    ctx.stack.push_back(std::move(owned));
                    ctx.stack.back()->on_enter(ctx);
                }
                if (exiting) {
                    owned = std::move(ctx.stack.back());
                    ctx.stack.pop_back();
                    El* el = owned.get();
                    if (!ctx.stack.empty())
                        owned.release(); // on_child_close transfers ownership
                    const bool should_render =
                        ctx.stack.empty() || ctx.stack.back()->on_child_close(el);
                    if (should_render) {
                        if (new_line)
                            out.push_back({"\n", rich::detail::kNoneStyle()});
                        auto segs = el->render(ctx);
                        out.insert(out.end(), std::make_move_iterator(segs.begin()),
                                   std::make_move_iterator(segs.end()));
                    }
                    el->on_leave(ctx);
                    new_line = el->new_line;
                } else if (self_closing) {
                    owned = std::move(ctx.stack.back());
                    ctx.stack.pop_back();
                    El* el = owned.get();
                    if (!ctx.stack.empty())
                        owned.release(); // on_child_close transfers ownership
                    if (!tok.content.empty()) el->on_text(ctx, tok.content);
                    const bool should_render =
                        ctx.stack.empty() || ctx.stack.back()->on_child_close(el);
                    if (should_render) {
                        if (new_line && node_type != "inline")
                            out.push_back({"\n", rich::detail::kNoneStyle()});
                        auto segs = el->render(ctx);
                        out.insert(out.end(), std::make_move_iterator(segs.begin()),
                                   std::make_move_iterator(segs.end()));
                    }
                    el->on_leave(ctx);
                    new_line = el->new_line;
                }
            }
        }
        return out;
    }

private:
    std::vector<Token> tokens_;
    bool hyperlinks_;

    El* make_element(const Token& tok) const {
        const std::string& t = tok.type;
        if (t == "paragraph_open") return new ElParagraph();
        if (t == "heading_open") {
            auto* e = new ElHeading();
            e->level = tok.tag.size() >= 2 ? std::stoi(tok.tag.substr(1)) : 1;
            return e;
        }
        if (t == "fence" || t == "code_block") {
            auto* e = new ElCodeBlock();
            const std::string& info = tok.info;
            const size_t sp = info.find(' ');
            const std::string lex = info.substr(0, sp);
            e->lexer = lex.empty() ? "text" : lex;
            return e;
        }
        if (t == "blockquote_open") return new ElBlockQuote();
        if (t == "hr") return new ElHRule();
        if (t == "bullet_list_open" || t == "ordered_list_open") {
            auto* e = new ElList();
            e->bullet = (t == "bullet_list_open");
            const std::string* st = tok.attr("start");
            e->start = st ? std::stoi(*st) : 1;
            return e;
        }
        if (t == "list_item_open") return new ElListItem();
        if (t == "image") {
            auto* e = new ElImage();
            const std::string* src = tok.attr("src");
            e->destination = src ? *src : "";
            e->hyperlinks = hyperlinks_;
            return e;
        }
        if (t == "table_open") return new ElTable();
        if (t == "thead_open") return new ElTableHead();
        if (t == "tbody_open") return new ElTableBody();
        if (t == "tr_open") return new ElTableRow();
        if (t == "th_open" || t == "td_open") {
            auto* e = new ElTableData();
            const std::string* style = tok.attr("style");
            if (style) {
                if (style->find("text-align:right") != std::string::npos)
                    e->justify = md::Text::J::Right;
                else if (style->find("text-align:center") != std::string::npos)
                    e->justify = md::Text::J::Center;
                else if (style->find("text-align:left") != std::string::npos)
                    e->justify = md::Text::J::Left;
            }
            return e;
        }
        return new ElUnknown();
    }
};

// ── element renders (need Ctx) ─────────────────────────────────────────────
inline std::vector<Segment> ElBlockQuote::render(Ctx& c) const {
    Options o = c.opt;
    o.width = o.width >= 4 ? o.width - 4 : 0;
    Ctx ci = c.with_width(o.width);
    std::vector<AnyRenderable> rs;
    for (const auto& ch : children)
        rs.push_back(AnyRenderable(BareSegments{ch->render(ci)}));
    auto lines = c.con->render_lines(rs, o, &style, true);
    const char* bar = "\xE2\x96\x8C "; // ▌
    std::vector<Segment> out;
    for (const auto& line : lines) {
        out.push_back({bar, style});
        for (const auto& s : line) out.push_back(s);
        out.push_back({"\n", rich::detail::kNoneStyle()});
    }
    return out;
}

inline std::vector<Segment> ElList::render(Ctx& c) const {
    std::vector<Segment> out;
    if (bullet) {
        const Style bstyle = theme_detail::md_style("markdown.item.bullet");
        Options o = c.opt;
        o.width = o.width >= 3 ? o.width - 3 : 0;
        Ctx ci = c.with_width(o.width);
        for (const auto& item : items) {
            std::vector<AnyRenderable> rs;
            for (const auto& ch : item->children)
                rs.push_back(AnyRenderable(BareSegments{ch->render(ci)}));
            auto lines = c.con->render_lines(rs, o, &item->style, true);
            bool first = true;
            for (const auto& line : lines) {
                out.push_back({first ? " \xE2\x80\xA2 " : "   ", bstyle}); // " • "
                for (const auto& s : line) out.push_back(s);
                out.push_back({"\n", rich::detail::kNoneStyle()});
                first = false;
            }
        }
    } else {
        const Style nstyle = theme_detail::md_style("markdown.item.number");
        const int last_number = start + static_cast<int>(items.size());
        const size_t number_width = std::to_string(last_number).size() + 2;
        Options o = c.opt;
        o.width = o.width >= number_width ? o.width - number_width : 0;
        Ctx ci = c.with_width(o.width);
        for (size_t i = 0; i < items.size(); ++i) {
            std::string numeral = std::to_string(start + static_cast<int>(i));
            numeral.insert(0, (number_width - 1) - numeral.size(), ' ');
            numeral += ' ';
            std::string pad(number_width, ' ');
            std::vector<AnyRenderable> rs;
            for (const auto& ch : items[i]->children)
                rs.push_back(AnyRenderable(BareSegments{ch->render(ci)}));
            auto lines = c.con->render_lines(rs, o, &items[i]->style, true);
            bool first = true;
            for (const auto& line : lines) {
                out.push_back({first ? numeral : pad, nstyle});
                for (const auto& s : line) out.push_back(s);
                out.push_back({"\n", rich::detail::kNoneStyle()});
                first = false;
            }
        }
    }
    return out;
}

inline std::vector<Segment> ElImage::render(Ctx& c) const {
    md::Text title;
    if (!text.empty()) {
        title = text;
    } else {
        std::string d = destination;
        while (!d.empty() && d.front() == '/') d.erase(d.begin());
        const auto sl = d.rfind('/');
        const std::string name = (sl == std::string::npos) ? d : d.substr(sl + 1);
        title = md::Text(name);
    }
    if (hyperlinks) {
        Style ls;
        ls.link = link.empty() ? destination : link;
        title.stylize(ls);
    }
    md::Text out;
    out.append("\xF0\x9F\x8C\x86 ", nullptr); // 🌆
    out.append_text(title);
    out.append(" ", nullptr);
    return out.render(c.opt.width, /*end_sv*/ "");
}

inline std::vector<Segment> ElTable::render(Ctx& c) const {
    const Style border = theme_detail::md_style("markdown.table.border");
    const Style header_span = theme_detail::md_style("markdown.table.header");
    static const Style table_header_apply{.bold = true}; // theme "table.header"
    std::vector<Segment> out;
    if (!header || !header->row) return out;
    const auto& hcells = header->row->cells;
    const size_t n = hcells.size();
    if (n == 0) return out;

    // content width per column = max cell text width (header + body)
    std::vector<size_t> cw(n, 0);
    for (size_t i = 0; i < n; ++i)
        cw[i] = unicode::string_cell_len(hcells[i]->content.plain_ref());
    if (body)
        for (const auto& row : body->rows)
            for (size_t i = 0; i < n && i < row->cells.size(); ++i)
                cw[i] = std::max(cw[i], unicode::string_cell_len(row->cells[i]->content.plain_ref()));

    size_t total = 2; // edges
    for (size_t i = 0; i < n; ++i) {
        total += cw[i] + (i < n - 1 ? 1 : 0); // content + right pad
        if (i < n - 1) ++total;               // divider
    }

    out.push_back({std::string(total, ' '), border});
    out.push_back({"\n", rich::detail::kNoneStyle()});

    auto emit_row = [&](const std::vector<std::unique_ptr<ElTableData>>& row_cells,
                        bool is_header) {
        out.push_back({" ", border});
        for (size_t i = 0; i < n; ++i) {
            const ElTableData* cell = (i < row_cells.size()) ? row_cells[i].get() : nullptr;
            std::vector<Segment> segs;
            if (cell) {
                auto t = cell->content;
                if (is_header) t.stylize(header_span);
                t.set_justify(cell->justify);
                segs = t.render(cw[i], /*end_sv*/ "");
                if (cell->justify == md::Text::J::Default) {
                    size_t len = 0;
                    for (const auto& s : segs) len += unicode::string_cell_len(s.text);
                    if (len < cw[i])
                        segs.push_back({std::string(cw[i] - len, ' '),
                                        rich::detail::kNoneStyle()});
                }
            }
            if (is_header)
                for (auto& s : segs) s.style = table_header_apply + s.style;
            for (const auto& s : segs) out.push_back(s);
            if (i < n - 1) {
                // right pad (Table Padding, style "" → Style{}) then divider
                if (is_header)
                    out.push_back({" ", table_header_apply + Style{}});
                else
                    out.push_back({" ", Style{}});
                out.push_back({" ", border});
            }
        }
        out.push_back({" ", border});
        out.push_back({"\n", rich::detail::kNoneStyle()});
    };

    emit_row(hcells, true);

    // head border: edge + per column (content + right pad) + dividers, all ─
    static const char kDash[3] = {'\xE2', '\x94', '\x80'}; // ─
    std::string hborder = " ";
    for (size_t i = 0; i < n; ++i) {
        const size_t run = cw[i] + (i < n - 1 ? 1 : 0);
        for (size_t k = 0; k < run; ++k) hborder.append(kDash, 3);
        if (i < n - 1) hborder.append(kDash, 3); // divider cell
    }
    hborder.push_back(' ');
    out.push_back({hborder, border});
    out.push_back({"\n", rich::detail::kNoneStyle()});

    if (body)
        for (const auto& row : body->rows) emit_row(row->cells, false);

    out.push_back({std::string(total, ' '), border});
    out.push_back({"\n", rich::detail::kNoneStyle()});
    return out;
}

// ────────────────────────────────────────────────────────────────────────────
// Renderable wrapper.
struct MarkdownRenderable {
    std::string doc;
    bool hyperlinks{true};

    std::vector<Segment> render(const Console& c, const Options& o) const {
        Markdown md(doc, hyperlinks);
        return md.render(c, o);
    }
    std::pair<size_t, size_t> measure(const Console&, const Options&) const {
        size_t longest = 0;
        std::string_view v = doc;
        size_t i = 0;
        while (i < v.size()) {
            const size_t nl = v.find('\n', i);
            const std::string_view line =
                v.substr(i, (nl == std::string_view::npos) ? std::string_view::npos
                                                           : nl - i);
            longest = std::max(longest, unicode::string_cell_len(line));
            if (nl == std::string_view::npos) break;
            i = nl + 1;
        }
        return {1, std::max<size_t>(1, longest)};
    }
};

inline std::vector<Segment> render_item(const MarkdownRenderable& m, const Console& c,
                                        const Options& o) {
    return m.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const MarkdownRenderable& m, const Console& c,
                                              const Options& o) {
    return m.measure(c, o);
}

// console.print(rich::markdown::markdown(doc)) / markdown(doc, false)
inline AnyRenderable markdown(std::string_view doc, bool hyperlinks = true) {
    return AnyRenderable(MarkdownRenderable{std::string(doc), hyperlinks});
}

} // namespace rich::markdown
