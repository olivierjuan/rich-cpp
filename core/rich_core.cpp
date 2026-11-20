#include "rich_core.h"
namespace rich {
#include <sstream>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

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

std::string style_to_ansi(const Style& s) {
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

void Console::print(const AnyRenderable& ar) {
    init_terminal();
    Options opt{.width = options_.width};
    auto segments = ar.render(*this, opt);

    for (const auto& s : segments) {
        bool styled = has_style(s.style);
        if (styled) os_ << style_to_ansi(s.style);
        os_ << s.text;
        if (styled) os_ << "\x1b[0m";

        if (options_.record) {
            record_buffer_ += s.text;
        }
    }
    // Check if the output already ends with newline
    bool ends_nl = !segments.empty() && !segments.back().text.empty() && segments.back().text.back() == '\n';
    if (!ends_nl) {
        os_ << '\n';
        if (options_.record) {
            record_buffer_ += '\n';
}

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
}

namespace {
void apply_tag(std::string_view tag, Style& st) {
    if (tag == "bold" || tag == "b") st.bold = true;
    else if (tag == "dim" || tag == "d") st.dim = true;
    else if (tag == "italic" || tag == "i") st.italic = true;
    else if (tag == "underline" || tag == "u") st.underline = true;
    else if (tag == "reverse") st.reverse = true;
    else if (tag == "red") st.color = Color::from_ansi(31);
    else if (tag == "green") st.color = Color::from_ansi(32);
    else if (tag == "yellow") st.color = Color::from_ansi(33);
    else if (tag == "blue") st.color = Color::from_ansi(34);
    else if (tag == "magenta") st.color = Color::from_ansi(35);
    else if (tag == "cyan") st.color = Color::from_ansi(36);
    else if (tag == "white") st.color = Color::from_ansi(37);
    else if (tag == "bright_red") st.color = Color::from_ansi(91);
    else if (tag == "bright_green") st.color = Color::from_ansi(92);
    else if (tag == "bright_yellow") st.color = Color::from_ansi(93);
    else if (tag == "bright_blue") st.color = Color::from_ansi(94);
    else if (tag == "bright_magenta") st.color = Color::from_ansi(95);
    else if (tag == "bright_cyan") st.color = Color::from_ansi(96);
    else if (tag == "bright_white") st.color = Color::from_ansi(97);
    else if (tag == "on red") st.bg_color = Color::from_ansi(41);
    else if (tag == "on green") st.bg_color = Color::from_ansi(42);
    else if (tag == "on blue") st.bg_color = Color::from_ansi(44);
    else if (tag == "on black") st.bg_color = Color::from_ansi(40);
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title)}));
}

std::vector<Segment> render_item(const Rule& r, const Console&, const Options&) {
    const size_t target_width = 80;
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width), dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    size_t side = (target_width > title_len + 4) ? (target_width - title_len - 4) / 2 : 2;
    size_t right = target_width > (side + title_len + 4) ? (target_width - side - title_len - 4) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
    out.push_back({" " + repeat_utf8_core("─", right), dash_style});
    return out;
}

std::pair<size_t, size_t> measure_item(const Rule&, const Console&, const Options&) {
    return {80, 1};
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
    }
    return any_known;
}
} // namespace

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    print(AnyRenderable(Rule{std::string(title), options_.width}));
}

std::string Console::export_text() const {
    return record_buffer_;
}

std::vector<Segment> render_item(const Rule& r, const Console& c, const Options& opt) {
    size_t target_width = (r.width > 0) ? r.width : ((opt.width > 0) ? opt.width : 80);
    std::vector<Segment> out;
    Style dash_style{.dim = true, .color = Color::from_ansi(90)};

    if (r.title.empty()) {
        out.push_back({repeat_utf8_core("─", target_width) + "\n", dash_style});
        return out;
    }

    size_t title_len = r.title.size();
    // Match Python Rich rule: centered title flanked by space
    // side_width = (target_width - title_len - 2) / 2
    size_t side = (target_width > title_len + 2) ? (target_width - title_len - 2) / 2 : 2;
    size_t right = (target_width >= side + title_len + 2) ? (target_width - side - title_len - 2) : side;

    out.push_back({repeat_utf8_core("─", side) + " ", dash_style});
    out.push_back({r.title, Style{.bold = true, .color = Color::from_ansi(97)}});
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
    if (tag == "bright_red") { st.color = Color::from_ansi(91); return true; }
    if (tag == "bright_green") { st.color = Color::from_ansi(92); return true; }
    if (tag == "bright_yellow") { st.color = Color::from_ansi(93); return true; }
    if (tag == "bright_blue") { st.color = Color::from_ansi(94); return true; }
    if (tag == "bright_magenta") { st.color = Color::from_ansi(95); return true; }
    if (tag == "bright_cyan") { st.color = Color::from_ansi(96); return true; }
    if (tag == "bright_white") { st.color = Color::from_ansi(97); return true; }
    if (tag == "on red") { st.bg_color = Color::from_ansi(41); return true; }
    if (tag == "on green") { st.bg_color = Color::from_ansi(42); return true; }
    if (tag == "on blue") { st.bg_color = Color::from_ansi(44); return true; }
    if (tag == "on black") { st.bg_color = Color::from_ansi(40); return true; }
    return false;
}

bool is_known_tag(std::string_view tag) {
    if (tag == "/" || tag == "/all") return true;
    if (!tag.empty() && tag[0] == '/') return true;
    Style dummy;
    size_t tstart = 0;
    bool any_known = false;
    while (tstart < tag.size()) {
        size_t tend = tag.find(' ', tstart);
        if (tend == std::string_view::npos) tend = tag.size();
        if (apply_tag(tag.substr(tstart, tend - tstart), dummy)) {
            any_known = true;
        } else {
            return false;
        }
        tstart = tend + 1;
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
                            size_t tend = tag.find(' ', tstart);
                            if (tend == std::string_view::npos) tend = tag.size();
                            apply_tag(tag.substr(tstart, tend - tstart), next_st);
                            tstart = tend + 1;
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
            segments.push_back({std::string(sv.substr(pos)), style_stack.back()});
            break;
        }
        segments.push_back({std::string(sv.substr(pos, next_bracket - pos)), style_stack.back()});
        pos = next_bracket;
    }
    return segments;
}

} // namespace rich
