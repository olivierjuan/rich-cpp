#include "rich_core.h"
#include "Svg.h"
#include "Html.h"
#include "Emoji.h"
#include <sstream>
#include <cstdlib>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <io.h>
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#endif

namespace rich {
std::string emoji_replace(std::string_view text) { return emoji::replace(text); }
} // namespace rich

namespace rich {

namespace {
void init_terminal() {
#ifdef _WIN32
    static const bool initialized = []() {
        SetConsoleOutputCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        return true;
    }();
    (void)initialized;
#endif
}

bool no_color_enabled() {
    const char* nc = std::getenv("NO_COLOR");
    return nc != nullptr && nc[0] != '\0';
}

size_t detect_terminal_width() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            if (width > 0) return static_cast<size_t>(width);
        }
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return static_cast<size_t>(ws.ws_col);
    }
#endif
    return 80;
}

size_t detect_terminal_height() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            int height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            if (height > 0) return static_cast<size_t>(height);
        }
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return static_cast<size_t>(ws.ws_row);
    }
#endif
    return 25;
}

std::string style_to_ansi(const Style& s) {
    if (no_color_enabled()) return "";

    std::string out;
    if (s.bold) out += "\x1b[1m";
    if (s.dim) out += "\x1b[2m";
    if (s.italic) out += "\x1b[3m";
    if (s.underline) out += "\x1b[4m";
    if (s.reverse) out += "\x1b[7m";

    if (s.color.type == Color::Type::Standard) {
        out += "\x1b[" + std::to_string(s.color.ansi) + "m";
    } else if (s.color.type == Color::Type::Rgb) {
        out += "\x1b[38;2;" + std::to_string(s.color.r) + ";" +
               std::to_string(s.color.g) + ";" + std::to_string(s.color.b) + "m";
    }

    if (s.bg_color.type == Color::Type::Standard) {
        out += "\x1b[" + std::to_string(s.bg_color.ansi + 10) + "m";
    } else if (s.bg_color.type == Color::Type::Rgb) {
        out += "\x1b[48;2;" + std::to_string(s.bg_color.r) + ";" +
               std::to_string(s.bg_color.g) + ";" + std::to_string(s.bg_color.b) + "m";
    }
    return out;
}

bool has_style(const Style& s) {
    if (no_color_enabled()) return false;
    return s.bold || s.dim || s.italic || s.underline || s.reverse ||
           s.color.type != Color::Type::None || s.bg_color.type != Color::Type::None;
}

std::string repeat_utf8_core(std::string_view glyph, size_t count) {
    std::string res;
    res.reserve(glyph.size() * count);
    for (size_t i = 0; i < count; ++i) res.append(glyph);
    return res;
}
} // namespace

namespace {
// Crop sv to at most `take` cells (never splitting a codepoint); mirrors
// rich.cells.set_cell_size in the crop direction.
std::string crop_cells(std::string_view sv, size_t take) {
    std::string cut;
    if (take == 0) return cut;
    cut.reserve(sv.size());
    size_t cells = 0, i = 0;
    while (i < sv.size() && cells < take) {
        size_t j = i;
        char32_t cp = unicode::decode_utf8(sv, j);
        size_t w = static_cast<size_t>(unicode::get_character_cell_size(cp));
        if (cells + w > take) break;
        cut.append(sv.substr(i, j - i));
        cells += w;
        i = j;
    }
    return cut;
}

// Port of rich.segment.Segment.split_and_crop_lines(segments, length,
// pad=False, include_new_lines=True), flattened into one stream:
// every '\n' becomes a standalone style=None segment and each line is
// cropped to `length` cells at cell boundaries (no padding).
std::vector<Segment> split_and_crop_lines(std::vector<Segment>&& segments, size_t length) {
    auto adjust = [&](const std::vector<Segment>& line) {
        std::vector<Segment> out;
        size_t line_length = 0;
        for (const auto& s : line) line_length += unicode::string_cell_len(s.text);
        if (line_length <= length) return std::vector<Segment>(line);
        size_t cur = 0;
        for (const auto& s : line) {
            size_t sl = unicode::string_cell_len(s.text);
            if (cur + sl < length) {
                out.push_back(s);
                cur += sl;
            } else {
                out.push_back({crop_cells(s.text, length - cur), s.style});
                break;
            }
        }
        return out;
    };
    std::vector<Segment> out;
    std::vector<Segment> line;
    for (auto& s : segments) {
        if (s.text.find('\n') == std::string::npos) {
            line.push_back(std::move(s));
            continue;
        }
        std::string_view sv(s.text);
        size_t start = 0;
        for (;;) {
            size_t nl = sv.find('\n', start);
            if (nl == std::string::npos) {
                if (start < sv.size()) line.push_back({std::string(sv.substr(start)), s.style});
                break;
            }
            if (nl > start) line.push_back({std::string(sv.substr(start, nl - start)), s.style});
            auto cropped = adjust(line);
            line.clear();
            out.insert(out.end(), std::make_move_iterator(cropped.begin()), std::make_move_iterator(cropped.end()));
            out.push_back({"\n", detail::kNoneStyle()});
            start = nl + 1;
        }
    }
    if (!line.empty()) {
        auto cropped = adjust(line);
        out.insert(out.end(), std::make_move_iterator(cropped.begin()), std::make_move_iterator(cropped.end()));
    }
    return out;
}
} // namespace

void Console::print(const AnyRenderable& ar) {
    init_terminal();
    size_t active_w = (options_.width > 0) ? options_.width : detect_terminal_width();
    // height stays 0 (= unset), mirroring Python options.height=None on plain print.
    // Layouts query Console::height() instead.
    Options opt{.width = active_w};
    auto segments = ar.render(*this, opt);
    // Python print(): renderable segments pass through
    // Segment.split_and_crop_lines(width, pad=False); each '\n' becomes a
    // standalone style=None segment, over-wide lines crop at cell boundaries.
    // (Renderables carry their own trailing newline, cf. Text end="\n".)
    auto out = split_and_crop_lines(std::move(segments), active_w);

    for (const auto& s : out) {
        bool styled = has_style(s.style);
        if (styled) os_ << style_to_ansi(s.style);
        os_ << s.text;
        if (styled) os_ << "\x1b[0m";

        if (options_.record) {
            record_buffer_ += s.text;
            record_segments_.push_back(s);
        }
    }
}

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    size_t w = (options_.width > 0) ? options_.width : detect_terminal_width();
    print(AnyRenderable(Rule{std::string(title), w}));
}

std::string Console::export_text() const {
    return record_buffer_;
}

std::string Console::export_html(bool inline_styles) const {
    // Python requires record=True; when not recording the buffer is empty and
    // we render an (empty) document rather than crash.
    const std::vector<Segment>* segs = &record_segments_;
    if (!options_.record) {
        static const std::vector<Segment> empty;
        segs = &empty;
    }
    return html::render_html(*segs, inline_styles);
}

// Python style=None sentinel: color fields unset (Type::None) with ansi=200
// as an out-of-band marker. Rendered identically to a default style; only
// Segment-repr hashing (export_svg unique_id) observes the difference.
const Style& detail::kNoneStyle() {
    static const Style st = Style{.bold = false, .dim = false, .italic = false,
                                  .underline = false, .reverse = false,
                                  .color = Color{.type = Color::Type::None, .ansi = 200},
                                  .bg_color = Color{}};
    return st;
}

std::string Console::export_svg(const std::string& title, const std::string& unique_id_in) const {
    using namespace svg;
    constexpr double kFontAspect = 0.61;
    const double char_height = 20;
    const double char_width = char_height * kFontAspect;   // 12.2
    const double line_height = char_height * 1.22;         // 24.4

    const int margin_top = 1, margin_right = 1, margin_bottom = 1, margin_left = 1;
    const int padding_top = 40, padding_right = 8, padding_bottom = 8, padding_left = 8;
    const int padding_width = padding_left + padding_right;
    const int padding_height = padding_top + padding_bottom;
    const int margin_width = margin_left + margin_right;
    const int margin_height = margin_top + margin_bottom;

    // Deterministic unique_id: adler32 over Python-parity segment reprs + title.
    std::string unique_id = unique_id_in;
    if (unique_id.empty()) {
        std::string repr_data;
        for (const auto& s : record_segments_) repr_data += py_repr_segment(s);
        repr_data += title;
        unique_id = "terminal-" + std::to_string(adler32(repr_data));
    }

    const size_t width = (options_.width > 0) ? options_.width : 80;
    auto lines = svg::split_lines(record_segments_, width);

    std::vector<std::string> text_backgrounds, text_group;
    std::vector<std::pair<std::string, int>> classes; // (css, no)
    auto class_no = [&](const std::string& rules) -> int {
        for (const auto& [css, no] : classes)
            if (css == rules) return no;
        int no = static_cast<int>(classes.size()) + 1;
        classes.emplace_back(rules, no);
        return no;
    };

    for (size_t y = 0; y < lines.size(); ++y) {
        double x = 0;
        for (const auto& seg : lines[y]) {
            const Style& st = seg.style;
            const bool none_style = svg::is_none_style(st);
            Triplet color = (none_style || st.color.type == Color::Type::None) ? theme_foreground() : truecolor(st.color, true);
            Triplet bgcolor = (none_style || st.bg_color.type == Color::Type::None) ? theme_background() : truecolor(st.bg_color, false);
            if (st.reverse) std::swap(color, bgcolor);
            if (st.dim) color = blend(color, bgcolor, 0.4);

            std::string rules = "fill: " + hex(color);
            if (st.bold) rules += ";font-weight: bold";
            if (st.italic) rules += ";font-style: italic;";
            if (st.underline) rules += ";text-decoration: underline;";
            int cls = class_no(rules);
            std::string class_name = "r" + std::to_string(cls);

            bool has_background = false;
            std::string background;
            if (st.reverse) {
                has_background = true;
                background = hex((none_style || st.color.type == Color::Type::None) ? theme_foreground() : truecolor(st.color, true));
            } else {
                has_background = !none_style && st.bg_color.type != Color::Type::None;
                background = hex(bgcolor);
            }

            size_t text_cells = cell_len(seg.text);
            if (has_background) {
                text_backgrounds.push_back(
                    "<rect fill=\"" + background + "\" x=\"" + fmt_g(x * char_width) +
                    "\" y=\"" + fmt_g(y * line_height + 1.5) +
                    "\" width=\"" + fmt_g(char_width * text_cells) +
                    "\" height=\"" + fmt_g(line_height + 0.25) +
                    "\" shape-rendering=\"crispEdges\"/>");
            }

            // Skip all-space runs (Python parity).
            bool all_spaces = true;
            for (char ch : seg.text)
                if (ch != ' ') { all_spaces = false; break; }
            if (!all_spaces && !seg.text.empty()) {
                text_group.push_back(
                    "<text class=\"" + unique_id + "-" + class_name +
                    "\" x=\"" + fmt_g(x * char_width) +
                    "\" y=\"" + fmt_g(y * line_height + char_height) +
                    "\" textLength=\"" + fmt_g(char_width * py_len(seg.text)) +
                    "\" clip-path=\"url(#" + unique_id + "-line-" + std::to_string(y) +
                    ")\">" + escape_svg_text(seg.text) + "</text>");
            }
            x += text_cells;
        }
    }

    // Per-line clipPaths — Python parity: range(y) excludes the last line.
    std::string lines_svg;
    for (size_t line_no = 0; line_no + 1 < lines.size() || (lines.size() == 1 && line_no == 0 && false); ++line_no) {
        double offset = line_no * line_height + 1.5;
        lines_svg += "<clipPath id=\"" + unique_id + "-line-" + std::to_string(line_no) + "\">\n";
        lines_svg += "    <rect x=\"0\" y=\"" + fmt_g(offset) + "\" width=\"" + fmt_g(char_width * width) +
                     "\" height=\"" + fmt_g(line_height + 0.25) + "\"/>\n";
        lines_svg += "            </clipPath>";
        if (line_no + 2 < lines.size()) lines_svg += "\n";
    }

    std::string styles;
    for (size_t i = 0; i < classes.size(); ++i) {
        styles += "." + unique_id + "-r" + std::to_string(classes[i].second) + " { " + classes[i].first + " }";
        if (i + 1 < classes.size()) styles += "\n";
    }

    double terminal_width_chrome = std::ceil(width * char_width + padding_width);
    double terminal_height_chrome = lines.size() * line_height + padding_height;
    double terminal_width_clip = char_width * width - 1;
    double terminal_height_clip = lines.size() * line_height - 1;

    std::string chrome =
        "<rect fill=\"" + hex(theme_background()) + "\" stroke=\"rgba(255,255,255,0.35)\" stroke-width=\"1\"" +
        " x=\"" + std::to_string(margin_left) + "\" y=\"" + std::to_string(margin_top) +
        "\" width=\"" + fmt_g(terminal_width_chrome) + "\" height=\"" + fmt_g(terminal_height_chrome) +
        "\" rx=\"8\"/>";
    if (!title.empty()) {
        chrome += "<text class=\"" + unique_id + "-title\" fill=\"" + hex(theme_foreground()) +
                  "\" text-anchor=\"middle\" x=\"" + fmt_g(terminal_width_chrome / 2) +
                  "\" y=\"" + std::to_string(margin_top + static_cast<int>(char_height) + 6) + "\">" +
                  escape_svg_text(title) + "</text>";
    }
    chrome += "\n            <g transform=\"translate(26,22)\">\n"
              "            <circle cx=\"0\" cy=\"0\" r=\"7\" fill=\"#ff5f57\"/>\n"
              "            <circle cx=\"22\" cy=\"0\" r=\"7\" fill=\"#febc2e\"/>\n"
              "            <circle cx=\"44\" cy=\"0\" r=\"7\" fill=\"#28c840\"/>\n"
              "            </g>\n        ";

    std::string backgrounds, matrix;
    for (const auto& b : text_backgrounds) backgrounds += b;
    for (const auto& t : text_group) matrix += t;

    // CONSOLE_SVG_FORMAT template (verbatim from rich/_export_format.py).
    std::string out;
    out += "<svg class=\"rich-terminal\" viewBox=\"0 0 " + fmt_g(terminal_width_chrome + margin_width) +
           " " + fmt_g(terminal_height_chrome + margin_height) + "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    out += "    <!-- Generated with Rich https://www.textualize.io -->\n";
    out += "    <style>\n\n";
    out += "    @font-face {\n";
    out += "        font-family: \"Fira Code\";\n";
    out += "        src: local(\"FiraCode-Regular\"),\n";
    out += "                url(\"https://cdnjs.cloudflare.com/ajax/libs/firacode/6.2.0/woff2/FiraCode-Regular.woff2\") format(\"woff2\"),\n";
    out += "                url(\"https://cdnjs.cloudflare.com/ajax/libs/firacode/6.2.0/woff/FiraCode-Regular.woff\") format(\"woff\");\n";
    out += "        font-style: normal;\n";
    out += "        font-weight: 400;\n";
    out += "    }\n";
    out += "    @font-face {\n";
    out += "        font-family: \"Fira Code\";\n";
    out += "        src: local(\"FiraCode-Bold\"),\n";
    out += "                url(\"https://cdnjs.cloudflare.com/ajax/libs/firacode/6.2.0/woff2/FiraCode-Bold.woff2\") format(\"woff2\"),\n";
    out += "                url(\"https://cdnjs.cloudflare.com/ajax/libs/firacode/6.2.0/woff/FiraCode-Bold.woff\") format(\"woff\");\n";
    out += "        font-style: bold;\n";
    out += "        font-weight: 700;\n";
    out += "    }\n\n";
    out += "    ." + unique_id + "-matrix {\n";
    out += "        font-family: Fira Code, monospace;\n";
    out += "        font-size: " + fmt_g(char_height) + "px;\n";
    out += "        line-height: " + fmt_g(line_height) + "px;\n";
    out += "        font-variant-east-asian: full-width;\n";
    out += "    }\n\n";
    out += "    ." + unique_id + "-title {\n";
    out += "        font-size: 18px;\n";
    out += "        font-weight: bold;\n";
    out += "        font-family: arial;\n";
    out += "    }\n\n";
    out += "    " + styles + "\n";
    out += "    </style>\n\n";
    out += "    <defs>\n";
    out += "    <clipPath id=\"" + unique_id + "-clip-terminal\">\n";
    out += "      <rect x=\"0\" y=\"0\" width=\"" + py_str_float(terminal_width_clip) +
           "\" height=\"" + py_str_float(terminal_height_clip) + "\" />\n";
    out += "    </clipPath>\n";
    out += "    " + lines_svg + "\n";
    out += "    </defs>\n\n";
    out += "    " + chrome + "\n";
    out += "    <g transform=\"translate(" + std::to_string(margin_left + padding_left) + ", " +
           std::to_string(margin_top + padding_top) + ")\" clip-path=\"url(#" + unique_id + "-clip-terminal)\">\n";
    out += "    " + backgrounds + "\n";
    out += "    <g class=\"" + unique_id + "-matrix\">\n";
    out += "    " + matrix + "\n";
    out += "    </g>\n";
    out += "    </g>\n";
    out += "</svg>\n";
    return out;
}

std::vector<Segment> render_item(const Rule& r, const Console& c, const Options& opt) {
    size_t target_width = (r.width > 0) ? r.width : ((opt.width > 0) ? opt.width : 80);
    std::vector<Segment> out;
    // Python Rule default style="rule.line" -> bright_green; title style="rule.text" -> Style.null().
    Style dash_style{.color = Color::from_ansi(92)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width) + "\n", dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 2) ? (target_width - title_len - 2) / 2 : 2;
    size_t right = (target_width >= side + title_len + 2) ? (target_width - side - title_len - 2) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{}});
    out.push_back({" " + repeat_utf8_core("─", right) + "\n", dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule& r, const Console&, const Options& opt) {
    size_t w = (r.width > 0) ? r.width : opt.width;
    return {w, 1};
}

namespace {
bool apply_tag(std::string_view tag, Style& st) {
    if (tag == "bold" || tag == "b") { st.bold = true; return true; }
    if (tag == "dim" || tag == "d") { st.dim = true; return true; }
    if (tag == "italic" || tag == "i") { st.italic = true; return true; }
    if (tag == "underline" || tag == "u") { st.underline = true; return true; }
    if (tag == "reverse") { st.reverse = true; return true; }
    if (tag == "red") { st.color = Color::from_ansi(31); return true; }
    if (tag == "green") { st.color = Color::from_ansi(32); return true; }
    if (tag == "yellow") { st.color = Color::from_ansi(33); return true; }
    if (tag == "blue") { st.color = Color::from_ansi(34); return true; }
    if (tag == "magenta") { st.color = Color::from_ansi(35); return true; }
    if (tag == "cyan") { st.color = Color::from_ansi(36); return true; }
    if (tag == "white") { st.color = Color::from_ansi(37); return true; }
    if (tag == "black") { st.color = Color::from_ansi(30); return true; }
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "bright_black") { st.color = Color::from_ansi(90); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on yellow") { st.bg_color = Color::from_ansi(43); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on magenta") { st.bg_color = Color::from_ansi(45); return true; }
    if (tag == "on cyan") { st.bg_color = Color::from_ansi(46); return true; }
    if (tag == "on white") { st.bg_color = Color::from_ansi(47); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    if (tag == "on bright_red") { st.bg_color = Color::from_ansi(101); return true; }
    if (tag == "on bright_green") { st.bg_color = Color::from_ansi(102); return true; }
    if (tag == "on bright_yellow") { st.bg_color = Color::from_ansi(103); return true; }
    if (tag == "on bright_blue") { st.bg_color = Color::from_ansi(104); return true; }
    if (tag == "on bright_magenta") { st.bg_color = Color::from_ansi(105); return true; }
    if (tag == "on bright_cyan") { st.bg_color = Color::from_ansi(106); return true; }
    if (tag == "on bright_white") { st.bg_color = Color::from_ansi(107); return true; }
    if (tag == "on bright_black") { st.bg_color = Color::from_ansi(100); return true; }
    return false;
}

// Next tag word; merges the two-word "on <color>" form into one token
// (Python StyleSyntax treats "on blue" as a single background spec).
std::string_view next_tag_word(std::string_view tag, size_t& tstart) {
    size_t tend = tag.find(' ', tstart);
    if (tend == std::string_view::npos) tend = tag.size();
    std::string_view word = tag.substr(tstart, tend - tstart);
    if (word == "on" && tend < tag.size()) {
        size_t w2end = tag.find(' ', tend + 1);
        if (w2end == std::string_view::npos) w2end = tag.size();
        word = tag.substr(tstart, w2end - tstart);
        tstart = w2end + 1;
    } else {
        tstart = tend + 1;
    }
    return word;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        if (apply_tag(next_tag_word(tag, tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
    }
    return any_known;
}
} // namespace

std::vector<Segment> parse_markup(std::string_view sv) {
    std::vector<Segment> segments;
    std::vector<Style> style_stack;
    style_stack.push_back(Style{});

    size_t pos = 0;
    while (pos < sv.size()) {
        if (sv[pos] == '[') {
            if (pos + 1 < sv.size() && sv[pos + 1] == '[') {
                segments.push_back({"[", style_stack.back()});
                pos += 2;
                continue;
            }

            size_t end = sv.find(']', pos);
            if (end != std::string_view::npos) {
                std::string_view tag = sv.substr(pos + 1, end - pos - 1);
                if (is_known_tag(tag)) {
                    if (tag == "/" || tag == "/all") {
                        if (style_stack.size() > 1) style_stack.pop_back();
                        else style_stack.back() = Style{};
                    } else if (!tag.empty() && tag[0] == '/') {
                        if (style_stack.size() > 1) style_stack.pop_back();
                    } else {
                        Style next_st = style_stack.back();
                        size_t tstart = 0;
                        while (tstart < tag.size()) {
                            apply_tag(next_tag_word(tag, tstart), next_st);
                        }
                        style_stack.push_back(next_st);
                    }
                    pos = end + 1;
                    continue;
                }
            }

            segments.push_back({"[", style_stack.back()});
            pos += 1;
            continue;
        }

        size_t next_bracket = sv.find('[', pos);
        if (next_bracket == std::string_view::npos) {
            segments.push_back({emoji_replace(sv.substr(pos)), style_stack.back()});
            break;
        }
        segments.push_back({emoji_replace(sv.substr(pos, next_bracket - pos)), style_stack.back()});
        pos = next_bracket;
    }
    return segments;
}

} // namespace rich
