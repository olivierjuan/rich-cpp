#include "rich_core.h"
#include "Svg.h"
#include "Html.h"
#include "Emoji.h"
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <array>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  ifndef ENABLE_ECHO_ON
#    define ENABLE_ECHO_ON 0x0004L  // wincon.h, absent from some SDK copies
#  endif
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

// Python Style._add: right operand wins for the boolean attributes it
// explicitly set; colors/bg_color/link take "right or left" (a DEFAULT-typed
// color is truthy in Python, so it wins too); explicit-bit mask is the union.
// kNoneStyle (Python's null style) is the identity.
Style Style::operator+(const Style& o) const {
    if (svg::is_none_style(o)) return *this;
    if (svg::is_none_style(*this)) return o;
    Style r = *this;
    r.explicit_bits = static_cast<uint16_t>(explicit_bits | o.explicit_bits);
    r.bold = (o.explicit_bits & 0x0001) != 0 ? o.bold : r.bold;
    r.dim = (o.explicit_bits & 0x0002) != 0 ? o.dim : r.dim;
    r.italic = (o.explicit_bits & 0x0004) != 0 ? o.italic : r.italic;
    r.underline = (o.explicit_bits & 0x0008) != 0 ? o.underline : r.underline;
    r.reverse = (o.explicit_bits & 0x0010) != 0 ? o.reverse : r.reverse;
    r.strike = (o.explicit_bits & 0x0020) != 0 ? o.strike : r.strike;
    r.blink = (o.explicit_bits & 0x0040) != 0 ? o.blink : r.blink;
    r.blink2 = (o.explicit_bits & 0x0080) != 0 ? o.blink2 : r.blink2;
    r.conceal = (o.explicit_bits & 0x0100) != 0 ? o.conceal : r.conceal;
    r.underline2 = (o.explicit_bits & 0x0200) != 0 ? o.underline2 : r.underline2;
    r.frame = (o.explicit_bits & 0x0400) != 0 ? o.frame : r.frame;
    r.encircle = (o.explicit_bits & 0x0800) != 0 ? o.encircle : r.encircle;
    r.overline = (o.explicit_bits & 0x1000) != 0 ? o.overline : r.overline;
    if (o.color.type != Color::Type::None) r.color = o.color;
    if (o.bg_color.type != Color::Type::None) r.bg_color = o.bg_color;
    if (!o.link.empty()) {
        r.link = o.link;
        r.link_id = o.link_id;
    }
    return r;
}

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

} // namespace

// Python Color.downgrade parity (rich/color.py).
namespace {
// rich._palettes.STANDARD_PALETTE — the 16 "standard" RGB values used for
// nearest-match downgrade.
constexpr std::array<std::array<int, 3>, 16> kStandardPalette = {{
    {0, 0, 0}, {170, 0, 0}, {0, 170, 0}, {170, 85, 0}, {0, 0, 170},
    {170, 0, 170}, {0, 170, 170}, {170, 170, 170}, {85, 85, 85},
    {255, 85, 85}, {85, 255, 85}, {255, 255, 85}, {85, 85, 255},
    {255, 85, 255}, {85, 255, 255}, {255, 255, 255},
}};
// rich._palettes.EIGHT_BIT_PALETTE indices 0-15 (the cube/ramp follow:
// 16-231 = 6x6x6 with levels {0,95,135,175,215,255}, 232-255 = gray 8..238).
constexpr std::array<std::array<int, 3>, 16> kEightBitBase = {{
    {0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {128, 128, 0}, {0, 0, 128},
    {128, 0, 128}, {0, 128, 128}, {192, 192, 192}, {128, 128, 128},
    {255, 0, 0}, {0, 255, 0}, {255, 255, 0}, {0, 0, 255},
    {255, 0, 255}, {0, 255, 255}, {255, 255, 255},
}};

void eight_bit_triplet(int index, int& r, int& g, int& b) {
    if (index < 16) {
        r = kEightBitBase[index][0]; g = kEightBitBase[index][1];
        b = kEightBitBase[index][2];
        return;
    }
    if (index < 232) {
        static const int levels[6] = {0, 95, 135, 175, 215, 255};
        int i = index - 16;
        r = levels[i / 36]; g = levels[(i / 6) % 6]; b = levels[i % 6];
        return;
    }
    int v = 8 + 10 * (index - 232);
    r = g = b = v;
}

// Python colorsys.rgb_to_hls parity (exact C stdlib formulas — the `s < 0.15`
// grayscale branch of downgrade must see bit-identical floats).
void rgb_to_hls(double r, double g, double b, double& h, double& l, double& s) {
    double maxc = std::max(r, std::max(g, b));
    double minc = std::min(r, std::min(g, b));
    double summ = maxc + minc;
    double rang = maxc - minc;
    l = summ / 2.0;
    if (minc == maxc) {
        h = 0.0; s = 0.0;
        return;
    }
    if (l <= 0.5) s = rang / summ;
    else s = rang / (2.0 - summ);
    double vr = (maxc - r) / rang;
    double v;
    if (g == maxc) v = vr + (b - r) / rang;
    else if (b == maxc) v = 2.0 + vr;
    else v = 4.0 + (r - g) / rang;
    v /= 6.0;
    if (v < 0.0) v += 1.0;
    h = v;
}

// Python Palette.match parity (BT.601 perceptual distance, integer >> 8).
int palette_match(const std::array<std::array<int, 3>, 16>& palette, int r1, int g1, int b1) {
    int best = 0;
    double best_d = 1e300;
    for (int i = 0; i < 16; ++i) {
        int r2 = palette[i][0], g2 = palette[i][1], b2 = palette[i][2];
        int red_mean = (r1 + r2) / 2;
        int red = r1 - r2, green = g1 - g2, blue = b1 - b2;
        double term_r = ((512.0 + red_mean) * (double)red * red) / 256.0;
        double term_b = ((767.0 - red_mean) * (double)blue * blue) / 256.0;
        double d = std::sqrt(term_r + 4.0 * green * green + term_b);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

// Python round() = round-half-to-even (banker's).
int round_half_even(double x) { return static_cast<int>(std::nearbyint(x)); }

Color rgb_to_charcoal(uint8_t r, uint8_t g, uint8_t b) {
    double h, l, s;
    rgb_to_hls(r / 255.0, g / 255.0, b / 255.0, h, l, s);
    if (s < 0.15) {
        int gray = round_half_even(l * 25.0);
        int n = (gray == 0) ? 16 : (gray == 25 ? 231 : 231 + gray);
        return Color::charcoal(static_cast<uint8_t>(n));
    }
    auto six = [](int v) {
        double d = (v < 95) ? v / 95.0 : 1.0 + (v - 95) / 40.0;
        return round_half_even(d);
    };
    int n = 16 + 36 * six(r) + 6 * six(g) + six(b);
    return Color::charcoal(static_cast<uint8_t>(n));
}
} // namespace

void Color::charcoal_rgb(int index, int& r, int& g, int& b) { eight_bit_triplet(index, r, g, b); }

Color downgrade_color(const Color& c, ColorSystem system) {
    // Python: `if self.type in (ColorType.DEFAULT, system): return self`,
    // then EIGHT_BIT-from-truecolor, STANDARD-from-truecolor-or-eightbit,
    // WINDOWS branches, else `return self`. (ColorType/ColorSystem are
    // IntEnums compared by value: STANDARD=1, EIGHT_BIT=2, TRUECOLOR=3.)
    if (c.type == Color::Type::Default) return c;
    const bool same_system =
        (c.type == Color::Type::Standard && system == ColorSystem::Standard) ||
        (c.type == Color::Type::Charcoal && system == ColorSystem::EightBit) ||
        (c.type == Color::Type::Rgb && system == ColorSystem::Truecolor);
    if (same_system) return c;
    // Standard colors are renderable in every system -> self.
    if (c.type == Color::Type::Standard) return c;
    if (system == ColorSystem::Truecolor) return c; // everything fits
    if (system == ColorSystem::EightBit && c.type == Color::Type::Rgb)
        return rgb_to_charcoal(c.r, c.g, c.b);
    if (system == ColorSystem::Standard) {
        int r, g, b;
        if (c.type == Color::Type::Rgb) {
            r = c.r; g = c.g; b = c.b;
        } else {  // Charcoal: EIGHT_BIT_PALETTE[number]
            eight_bit_triplet(c.ansi, r, g, b);
        }
        int n = palette_match(kStandardPalette, r, g, b);
        // Standard number 0-7 -> SGR 30-37; 8-15 -> 90-97.
        return Color::from_ansi(static_cast<uint8_t>(n < 8 ? 30 + n : 90 + n));
    }
    return c;
}

// Python Color.get_ansi_codes(foreground) as one SGR fragment.
inline std::string color_sgr(const Color& c, bool foreground) {
    switch (c.type) {
    case Color::Type::Default:
        return foreground ? "39" : "49";
    case Color::Type::Standard:
        return std::to_string(foreground ? c.ansi : c.ansi + 10);
    case Color::Type::Charcoal:
        return std::string(foreground ? "38;5;" : "48;5;") + std::to_string(c.ansi);
    case Color::Type::Rgb:
    default:
        return std::string(foreground ? "38;2;" : "48;2;") + std::to_string(c.r) +
               ";" + std::to_string(c.g) + ";" + std::to_string(c.b);
    }
}

// Python Style._make_ansi_codes: attributes first (bit order 0..12, only
// set-and-explicit), then color, then background.
std::string style_sgr(const Style& s, ColorSystem system, bool strip_color) {
    struct Attr { bool value; bool explicit_set; const char* code; };
    const Attr attrs[] = {
        {s.bold, (s.explicit_bits & 0x0001) != 0, "1"},
        {s.dim, (s.explicit_bits & 0x0002) != 0, "2"},
        {s.italic, (s.explicit_bits & 0x0004) != 0, "3"},
        {s.underline, (s.explicit_bits & 0x0008) != 0, "4"},
        {s.blink, (s.explicit_bits & 0x0040) != 0, "5"},
        {s.blink2, (s.explicit_bits & 0x0080) != 0, "6"},
        {s.reverse, (s.explicit_bits & 0x0010) != 0, "7"},
        {s.conceal, (s.explicit_bits & 0x0100) != 0, "8"},
        {s.strike, (s.explicit_bits & 0x0020) != 0, "9"},
        {s.underline2, (s.explicit_bits & 0x0200) != 0, "21"},
        {s.frame, (s.explicit_bits & 0x0400) != 0, "51"},
        {s.encircle, (s.explicit_bits & 0x0800) != 0, "52"},
        {s.overline, (s.explicit_bits & 0x1000) != 0, "53"},
    };
    std::string parts;
    auto append = [&parts](const std::string& p) {
        if (!parts.empty()) parts += ';';
        parts += p;
    };
    for (const auto& a : attrs)
        if (a.value && a.explicit_set) append(a.code);
    if (!strip_color) {
        if (s.color.type != Color::Type::None)
            append(color_sgr(downgrade_color(s.color, system), true));
        if (s.bg_color.type != Color::Type::None)
            append(color_sgr(downgrade_color(s.bg_color, system), false));
    }
    if (parts.empty()) return "";
    return "\x1b[" + parts + "m";
}

namespace {
// Python Style.render link wrap: OSC 8 with a per-instance id.
inline std::string link_osc_begin(const Style& s) {
    return "\x1b]8;id=" + std::to_string(s.link_id) + ";" + s.link + "\x1b\\";
}
inline const char* link_osc_end() { return "\x1b]8;;\x1b\\"; }

// True when the style emits anything for the active color system.
// strip_color mirrors NO_COLOR (attributes keep, colors go); a null color
// system strips everything including links, so Console never calls this.
bool has_style(const Style& s, ColorSystem system, bool strip_color) {
    return !style_sgr(s, system, strip_color).empty() || !s.link.empty();
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

void Console::emit_segments(const std::vector<Segment>& segs) {
    // Python _render_buffer: auto-detected None color system (non-TTY or
    // dumb terminal) renders plain text — no SGR, no OSC 8 links; no_color
    // drops colors only (Segment.remove_color); otherwise full SGR downgraded
    // to the active color system.
    const auto cs = active_color_system();
    const bool strip_color = cs.has_value() && no_color();
    for (const auto& s : segs) {
        const bool styled = cs.has_value() && has_style(s.style, *cs, strip_color);
        const bool linked =
            cs.has_value() && !s.style.link.empty() && !options_.legacy_windows;
        if (linked) os_ << link_osc_begin(s.style);
        if (styled) os_ << style_sgr(s.style, *cs, strip_color);
        os_ << s.text;
        if (styled) os_ << "\x1b[0m";
        if (linked) os_ << link_osc_end();

        if (options_.record) {
            record_buffer_ += s.text;
            record_segments_.push_back(s);
        }
    }
}

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
    emit_segments(out);
}

namespace {
// Split a flat segment stream into physical lines at '\n'.
std::vector<std::vector<Segment>> split_physical_lines(std::vector<Segment> segs) {
    std::vector<std::vector<Segment>> lines;
    std::vector<Segment> cur;
    for (auto& s : segs) {
        size_t i = 0;
        while (i < s.text.size()) {
            const size_t nl = s.text.find('\n', i);
            if (nl == std::string::npos) {
                cur.push_back({s.text.substr(i), s.style});
                break;
            }
            if (nl > i) cur.push_back({s.text.substr(i, nl - i), s.style});
            lines.push_back(std::move(cur));
            cur.clear();
            i = nl + 1;
        }
    }
    lines.push_back(std::move(cur));
    return lines;
}

// Table overflow="fold": wrap a physical line at <= N cells, cutting at
// cell boundaries (never splitting a codepoint).
std::vector<std::vector<Segment>> fold_line(const std::vector<Segment>& line, size_t N) {
    std::vector<std::vector<Segment>> out;
    std::vector<Segment> cur;
    size_t cells = 0;
    auto flush = [&] {
        out.push_back(std::move(cur));
        cur.clear();
        cells = 0;
    };
    for (const auto& s : line) {
        size_t i = 0;
        while (i < s.text.size()) {
            if (cells >= N) flush();
            std::string cut = crop_cells(s.text.substr(i), N - cells);
            if (cut.empty()) {
                ++i;  // zero-width glyph at a boundary: drop
                continue;
            }
            cur.push_back({cut, s.style});
            cells += unicode::string_cell_len(cut);
            i += cut.size();
        }
    }
    flush();
    return out;
}
} // namespace

void Console::log(std::initializer_list<std::string_view> objects, LogOptions opts) {
    init_terminal();
    const size_t W = (options_.width > 0) ? options_.width : detect_terminal_width();

    // Python: no objects -> (NewLine(),) -> an empty message. Objects are
    // joined with " " (sep default) after markup parsing.
    std::vector<Segment> msg_segs;
    bool first = true;
    for (std::string_view obj : objects) {
        if (!first) msg_segs.push_back({std::string(" "), Style{}});
        auto parsed = parse_markup(obj);
        msg_segs.insert(msg_segs.end(), parsed.begin(), parsed.end());
        first = false;
    }

    std::string time_str;
    if (opts.show_time) {
        std::tm tm_now{};
        if (opts.time.has_value()) {
            tm_now = *opts.time;
        } else {
            const std::time_t now = std::time(nullptr);
#ifdef _WIN32
            localtime_s(&tm_now, &now);
#else
            localtime_r(&now, &tm_now);
#endif
        }
        char buf[16];
        std::strftime(buf, sizeof buf, "%H:%M:%S", &tm_now);
        time_str = std::string("[") + buf + "]";
    }
    const bool omit = opts.show_time && opts.omit_repeated_times &&
                      !time_str.empty() && last_log_time_ == time_str;
    if (opts.show_time) last_log_time_ = time_str;

    const size_t tcol = opts.show_time ? time_str.size() : 0;
    std::string path_str;
    if (opts.show_path && !opts.path.empty()) {
        path_str = opts.path;
        if (opts.line_no > 0) path_str += ":" + std::to_string(opts.line_no);
    }
    const size_t pcol = path_str.size();
    size_t N = W;
    if (opts.show_time) N -= tcol + 1;  // time cell + its styled pad space
    if (!path_str.empty()) N -= pcol + 1;
    if (N > W) N = W;

    const auto physical = split_physical_lines(std::move(msg_segs));
    const Style time_style = get_style("log.time");
    const Style path_style = get_style("log.path");

    // Table.grid(padding=(0,1)) layout, one grid row per physical message
    // line: [time cell][1][message cell (fold, padded to N)][1][path cell].
    // The time cell's trailing pad space is part of the (styled) time cell;
    // the message pad and the pre-path separator are unstyled.
    std::vector<Segment> flat;
    for (size_t li = 0; li < physical.size(); ++li) {
        const auto folded = fold_line(physical[li], N);
        for (size_t fi = 0; fi < folded.size(); ++fi) {
            const bool first_line = (li == 0 && fi == 0);
            if (opts.show_time) {
                flat.push_back({first_line && !omit ? time_str : std::string(tcol, ' '),
                                time_style});
                flat.push_back({std::string(" "), time_style});
            }
            size_t cells = 0;
            for (const auto& s : folded[fi])
                cells += unicode::string_cell_len(s.text);
            for (const auto& s : folded[fi]) flat.push_back(s);
            if (cells < N) flat.push_back({std::string(N - cells, ' '), Style{}});
            if (!path_str.empty()) {
                flat.push_back({std::string(" "), Style{}});
                flat.push_back(Segment{
                    first_line ? std::string(path_str) : std::string(pcol, ' '),
                    first_line ? path_style : Style{}});
            }
            if (fi + 1 < folded.size() || li + 1 < physical.size())
                flat.push_back({std::string("\n"), Style{}});
        }
        // Python Table rows end with a '\n' (same as print's trailing newline).
        flat.push_back({std::string("\n"), Style{}});
    }
    emit_segments(flat);
}

std::string Console::input(std::string_view prompt, bool markup, bool password) {
    init_terminal();
    if (!prompt.empty()) {
        // Python: self.print(prompt, markup=markup, emoji=emoji, end="") —
        // recorded like any print, but without the trailing newline.
        auto segs = markup ? parse_markup(prompt)
                           : std::vector<Segment>{{std::string(prompt), Style{}}};
        emit_segments(segs);
        os_.flush();
    }
    std::string line;
    auto read_line = [&line]() {
        line.clear();
        std::getline(std::cin, line);
    };
    if (password) {
#ifdef _WIN32
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        const bool can_hide =
            hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &mode) &&
            SetConsoleMode(hIn, mode & ~ENABLE_ECHO_ON);
        if (can_hide) read_line();
        if (can_hide) SetConsoleMode(hIn, mode);
        if (!can_hide) read_line();
#else
        termios saved{};
        const bool can_hide = tcgetattr(STDIN_FILENO, &saved) == 0;
        if (can_hide) {
            termios no_echo = saved;
            no_echo.c_lflag &= static_cast<tcflag_t>(~ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &no_echo);
            read_line();
            tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        } else {
            read_line();
        }
#endif
    } else {
        read_line();
    }
    // A '\r' from CRLF piped input is not part of the line.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

void Console::print(std::string_view markup_text) {
    print(AnyRenderable(StyledText{std::string(markup_text)}));
}

void Console::rule(std::string_view title) {
    size_t w = (options_.width > 0) ? options_.width : detect_terminal_width();
    print(AnyRenderable(Rule{std::string(title), w}));
}

bool Console::is_terminal() const {
    if (options_.force_terminal.has_value()) return *options_.force_terminal;
    if (&os_ == &std::cout || &os_ == &std::cerr) {
#ifdef _WIN32
        int fd = (&os_ == &std::cout) ? _fileno(stdout) : _fileno(stderr);
        return _isatty(fd) != 0;
#else
        int fd = (&os_ == &std::cout) ? fileno(stdout) : fileno(stderr);
        return isatty(fd) != 0;
#endif
    }
    // ostringstream / file streams: not a terminal (Python file.isatty()).
    return false;
}

std::optional<ColorSystem> Console::active_color_system() const {
    if (options_.color_system.has_value()) return options_.color_system;
    // Python _detect_color_system.
    const auto term = []() {
        const char* t = std::getenv("TERM");
        return std::string(t ? t : "");
    }();
    if (!is_terminal()) return std::nullopt;
    {  // dumb terminal: TERM in ("dumb", "unknown")
        auto t = term;
        for (auto& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (t == "dumb" || t == "unknown") return std::nullopt;
    }
#ifdef _WIN32
    (void)term;
    // Win10+ consoles report truecolor via VT features; older ones fall back
    // to 8-bit. We enable VT processing on init, so truecolor is the match.
    return options_.legacy_windows ? ColorSystem::Standard : ColorSystem::Truecolor;
#else
    const char* ct = std::getenv("COLORTERM");
    std::string color_term = ct ? ct : "";
    for (auto& c : color_term) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // strip()
    size_t b = color_term.find_first_not_of(" \t"), e = color_term.find_last_not_of(" \t");
    if (b != std::string::npos) color_term = color_term.substr(b, e - b + 1);
    else color_term.clear();
    if (color_term == "truecolor" || color_term == "24bit") return ColorSystem::Truecolor;
    // TERM rpartition("-") -> _TERM_COLORS.get(suffix, STANDARD)
    size_t hyphen = term.rfind('-');
    std::string colors = (hyphen == std::string::npos) ? "" : term.substr(hyphen + 1);
    if (colors == "256color" || colors == "kitty") return ColorSystem::EightBit;
    return ColorSystem::Standard;
#endif
}

bool Console::no_color() const {
    if (options_.no_color.has_value()) return *options_.no_color;
    return no_color_enabled();
}

// Python Segment.adjust_line_length: crop over-long lines at cell
// boundaries, or pad short lines with a style-`style` segment.
static std::vector<Segment> adjust_line_length(std::vector<Segment>&& line,
                                               size_t length, const Style* style,
                                               bool pad) {
    size_t line_length = 0;
    for (const auto& s : line) line_length += unicode::string_cell_len(s.text);
    if (line_length < length) {
        if (pad)
            line.push_back({std::string(length - line_length, ' '),
                            style ? *style : detail::kNoneStyle()});
        return std::move(line);
    }
    if (line_length == length) return std::move(line);
    std::vector<Segment> out;
    size_t cur = 0;
    for (auto& s : line) {
        size_t sl = unicode::string_cell_len(s.text);
        if (cur + sl < length) {
            out.push_back(std::move(s));
            cur += sl;
        } else {
            out.push_back({crop_cells(s.text, length - cur), s.style});
            break;
        }
    }
    return out;
}

std::vector<std::vector<Segment>> Console::render_lines(
    const std::vector<AnyRenderable>& renderables, const Options& opt,
    const Style* style, bool pad) const {
    std::vector<Segment> segs;
    for (const auto& r : renderables) {
        auto s = r.render(*this, opt);
        segs.insert(segs.end(), std::make_move_iterator(s.begin()),
                    std::make_move_iterator(s.end()));
    }
    if (style && style->truthy())
        for (auto& s : segs) s.style = *style + s.style;

    std::vector<std::vector<Segment>> lines;
    std::vector<Segment> line;
    for (auto& s : segs) {
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
            lines.push_back(adjust_line_length(std::move(line), opt.width, style, pad));
            line.clear();
            start = nl + 1;
        }
    }
    if (!line.empty())
        lines.push_back(adjust_line_length(std::move(line), opt.width, style, pad));
    return lines;
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
    const Style dash_style = c.get_style("rule.line");

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
// Python STYLE_ATTRIBUTES: markup words that set a style attribute.
struct AttrWord {
    const char* name;
    uint16_t explicit_bit;
};
constexpr AttrWord kAttrWords[] = {
    {"bold", 0x0001},   {"b", 0x0001},
    {"dim", 0x0002},    {"d", 0x0002},
    {"italic", 0x0004}, {"i", 0x0004},
    {"underline", 0x0008}, {"u", 0x0008},
    {"underline2", 0x0200}, {"uu", 0x0200},
    {"blink", 0x0040},
    {"blink2", 0x0080},
    {"reverse", 0x0010}, {"r", 0x0010},
    {"conceal", 0x0100}, {"c", 0x0100},
    {"strike", 0x0020},  {"s", 0x0020},
    {"frame", 0x0400},
    {"encircle", 0x0800},
    {"overline", 0x1000}, {"o", 0x1000},
};

// Python Color.parse: "default", ANSI_COLOR_NAMES (256 words), and the
// RE_COLOR forms  #rrggbb / color(N) / rgb(r,g,b) (ints, spaces allowed).
// "color(N)" with N<16 is a STANDARD color in Python (SGR 30-37/90-97),
// N>=16 is EIGHT_BIT (SGR 38;5;N).
bool parse_color_word(std::string_view w, Color& out) {
    auto lower_copy = [w]() {
        std::string s(w);
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    std::string lw = lower_copy();
    static const struct { const char* name; int ansi; } kNamed[] = {
        {"black", 30},    {"red", 31},     {"green", 32},    {"yellow", 33},
        {"blue", 34},     {"magenta", 35}, {"cyan", 36},     {"white", 37},
        {"bright_black", 90}, {"bright_red", 91}, {"bright_green", 92},
        {"bright_yellow", 93}, {"bright_blue", 94}, {"bright_magenta", 95},
        {"bright_cyan", 96}, {"bright_white", 97},
    };
    for (const auto& n : kNamed)
        if (lw == n.name) { out = Color::from_ansi(static_cast<uint8_t>(n.ansi)); return true; }
    if (lw == "default") { out = Color::dflt(); return true; }
    if (lw.size() > 7 && lw.compare(0, 6, "color(") == 0 && lw.back() == ')') {
        const std::string_view num = lw.substr(6, lw.size() - 7);
        int n = 0;
        for (char c : num) {
            if (c < '0' || c > '9') return false;
            n = n * 10 + (c - '0');
        }
        if (n > 255) return false;
        if (n < 16) {
            out = Color::from_ansi(static_cast<uint8_t>(n < 8 ? 30 + n : 90 + n - 8));
            return true;
        }
        out = Color::charcoal(static_cast<uint8_t>(n));
        return true;
    }
    if (w.size() == 7 && w[0] == '#') {
        auto hex = [](std::string_view s, int& v) {
            v = 0;
            for (char c : s) {
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else return false;
                v = v * 16 + d;
            }
            return true;
        };
        int r, g, b;
        if (!hex(w.substr(1, 2), r) || !hex(w.substr(3, 2), g) || !hex(w.substr(5, 2), b))
            return false;
        out = Color::rgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                         static_cast<uint8_t>(b));
        return true;
    }
    if (w.size() > 5 && w.compare(0, 4, "rgb(") == 0 && w.back() == ')') {
        // Python: components = color_rgb.split(","); int() each (surrounding
        // whitespace allowed, empty rejected).
        std::string inner(w.substr(4, w.size() - 5));
        std::vector<std::string> parts;
        std::string cur;
        for (char c : inner) {
            if (c == ',') {
                parts.push_back(cur);
                cur.clear();
            } else
                cur.push_back(c);
        }
        parts.push_back(cur);
        if (parts.size() != 3) return false;
        int vals[3];
        for (int p = 0; p < 3; ++p) {
            size_t b = parts[p].find_first_not_of(" \t");
            size_t e = parts[p].find_last_not_of(" \t");
            if (b == std::string::npos) return false;
            int n = 0;
            for (size_t q = b; q <= e; ++q) {
                char c = parts[p][q];
                if (c < '0' || c > '9') return false;
                n = n * 10 + (c - '0');
            }
            vals[p] = n;
        }
        if (vals[0] > 255 || vals[1] > 255 || vals[2] > 255) return false;
        out = Color::rgb(static_cast<uint8_t>(vals[0]), static_cast<uint8_t>(vals[1]),
                         static_cast<uint8_t>(vals[2]));
        return true;
    }
    return false;
}

// Applies one style word to `st` (Python: StyleStack.push(Style.parse(word))
// semantics — the word is a full style definition; only the single-word
// forms occur in practice via next_tag_word's "on X" merge).
bool apply_tag(std::string_view tag, Style& st) {
    for (const auto& a : kAttrWords) {
        if (tag == a.name) {
            st.explicit_bits = static_cast<uint16_t>(st.explicit_bits | a.explicit_bit);
            switch (a.explicit_bit) {
            case 0x0001: st.bold = true; break;
            case 0x0002: st.dim = true; break;
            case 0x0004: st.italic = true; break;
            case 0x0008: st.underline = true; break;
            case 0x0200: st.underline2 = true; break;
            case 0x0040: st.blink = true; break;
            case 0x0080: st.blink2 = true; break;
            case 0x0010: st.reverse = true; break;
            case 0x0100: st.conceal = true; break;
            case 0x0020: st.strike = true; break;
            case 0x0400: st.frame = true; break;
            case 0x0800: st.encircle = true; break;
            case 0x1000: st.overline = true; break;
            }
            return true;
        }
    }
    if (tag.rfind("on ", 0) == 0) {
        Color bg;
        return parse_color_word(tag.substr(3), bg) && (st.bg_color = bg, true);
    }
    Color fg;
    return parse_color_word(tag, fg) && (st.color = fg, true);
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
} // namespace

// =====================================================================
// Theme system (rich.theme + rich.default_styles)
// =====================================================================

// Python Style.parse on a full definition: whitespace-separated words, each
// applied in order; "on <color>" counts as one spec (background).
Style parse_style_def(std::string_view definition) {
    Style out;
    size_t pos = 0;
    auto next_word = [&definition, &pos](std::string_view& word) {
        while (pos < definition.size() && (definition[pos] == ' ' || definition[pos] == '\t'))
            pos++;
        if (pos >= definition.size()) { word = std::string_view(); return; }
        size_t end = pos;
        while (end < definition.size() && definition[end] != ' ' && definition[end] != '\t')
            end++;
        word = definition.substr(pos, end - pos);
        pos = end;
    };
    std::string_view word;
    while (next_word(word), !word.empty()) {
        if (word == "on") {
            std::string_view color;
            next_word(color);
            if (color.empty())
                throw std::invalid_argument("style: 'on' without a color");
            if (!apply_tag("on " + std::string(color), out))
                throw std::invalid_argument(std::string("style: unknown color '") +
                                            std::string(color) + "'");
            continue;
        }
        if (!apply_tag(word, out))
            throw std::invalid_argument(std::string("style: unknown style word '") +
                                        std::string(word) + "'");
    }
    return out;
}

const std::map<std::string, Style>& default_styles() {
    // Entry builder: `on_bits` = attribute bits set to True, `off_bits` =
    // additional bits explicitly set (to False) in Python's constructor —
    // tracked via Style.explicit_bits, invisible to SGR emission.
    auto mk = [](uint16_t on_bits, uint16_t off_bits = 0,
                 Color fg = Color{}, Color bg = Color{}) {
        Style s;
        s.explicit_bits = static_cast<uint16_t>(on_bits | off_bits);
        s.bold = (on_bits & 0x0001) != 0;
        s.dim = (on_bits & 0x0002) != 0;
        s.italic = (on_bits & 0x0004) != 0;
        s.underline = (on_bits & 0x0008) != 0;
        s.reverse = (on_bits & 0x0010) != 0;
        s.strike = (on_bits & 0x0020) != 0;
        s.blink = (on_bits & 0x0040) != 0;
        s.blink2 = (on_bits & 0x0080) != 0;
        s.conceal = (on_bits & 0x0100) != 0;
        s.underline2 = (on_bits & 0x0200) != 0;
        s.frame = (on_bits & 0x0400) != 0;
        s.encircle = (on_bits & 0x0800) != 0;
        s.overline = (on_bits & 0x1000) != 0;
        s.color = fg;
        s.bg_color = bg;
        return s;
    };
    const Color black = Color::from_ansi(30), red = Color::from_ansi(31);
    const Color green = Color::from_ansi(32), yellow = Color::from_ansi(33);
    const Color blue = Color::from_ansi(34), magenta = Color::from_ansi(35);
    const Color cyan = Color::from_ansi(36), white = Color::from_ansi(37);
    const Color bred = Color::from_ansi(91), bgreen = Color::from_ansi(92);
    const Color byellow = Color::from_ansi(93), bblue = Color::from_ansi(94);
    const Color bmagenta = Color::from_ansi(95), bcyan = Color::from_ansi(96);
    static const std::map<std::string, Style> styles = {
        {"none", mk(0)},
        {"reset", mk(0, 0x01FF, Color::dflt(), Color::dflt())},
        {"dim", mk(0x0002)},
        {"bright", mk(0, 0x0002)},  // Style(dim=False)
        {"bold", mk(0x0001)},
        {"strong", mk(0x0001)},
        {"code", mk(0x0011)},
        {"italic", mk(0x0004)},
        {"emphasize", mk(0x0004)},
        {"underline", mk(0x0008)},
        {"blink", mk(0x0040)},
        {"blink2", mk(0x0080)},
        {"reverse", mk(0x0010)},
        {"strike", mk(0x0020)},
        {"black", mk(0, 0, black)},
        {"red", mk(0, 0, red)},
        {"green", mk(0, 0, green)},
        {"yellow", mk(0, 0, yellow)},
        {"magenta", mk(0, 0, magenta)},
        {"cyan", mk(0, 0, cyan)},
        {"white", mk(0, 0, white)},
        {"inspect.attr", mk(0x0004, 0, yellow)},
        {"inspect.attr.dunder", mk(0x0004, 0x0002, yellow)},
        {"inspect.callable", mk(0x0001, 0, red)},
        {"inspect.async_def", mk(0x0004, 0, bcyan)},
        {"inspect.def", mk(0x0004, 0, bcyan)},
        {"inspect.class", mk(0x0004, 0, bcyan)},
        {"inspect.error", mk(0x0001, 0, red)},
        {"inspect.equals", mk(0)},
        {"inspect.help", mk(0, 0, cyan)},
        {"inspect.doc", mk(0x0002)},
        {"inspect.value.border", mk(0, 0, green)},
        {"live.ellipsis", mk(0x0001, 0, red)},
        {"layout.tree.row", mk(0, 0x0002, red)},
        {"layout.tree.column", mk(0, 0x0002, blue)},
        {"logging.keyword", mk(0x0001, 0, yellow)},
        {"logging.level.notset", mk(0x0002)},
        {"logging.level.debug", mk(0, 0, green)},
        {"logging.level.info", mk(0, 0, blue)},
        {"logging.level.warning", mk(0, 0, yellow)},
        {"logging.level.error", mk(0x0001, 0, red)},
        {"logging.level.critical", mk(0x0011, 0, red)},
        {"log.level", mk(0)},
        {"log.time", mk(0x0002, 0, cyan)},
        {"log.message", mk(0)},
        {"log.path", mk(0x0002)},
        {"repr.ellipsis", mk(0, 0, yellow)},
        {"repr.indent", mk(0x0002, 0, green)},
        {"repr.error", mk(0x0001, 0, red)},
        {"repr.str", mk(0, 0x0006, green)},
        {"repr.brace", mk(0x0001)},
        {"repr.comma", mk(0x0001)},
        {"repr.ipv4", mk(0x0001, 0, bgreen)},
        {"repr.ipv6", mk(0x0001, 0, bgreen)},
        {"repr.eui48", mk(0x0001, 0, bgreen)},
        {"repr.eui64", mk(0x0001, 0, bgreen)},
        {"repr.tag_start", mk(0x0001)},
        {"repr.tag_name", mk(0x0001, 0, bmagenta)},
        {"repr.tag_contents", mk(0, 0, Color::dflt())},
        {"repr.tag_end", mk(0x0001)},
        {"repr.attrib_name", mk(0, 0x0004, yellow)},
        {"repr.attrib_equal", mk(0x0001)},
        {"repr.attrib_value", mk(0, 0x0004, magenta)},
        {"repr.number", mk(0x0001, 0x0004, cyan)},
        {"repr.number_complex", mk(0x0001, 0x0004, cyan)},
        {"repr.bool_true", mk(0x0004, 0, bgreen)},
        {"repr.bool_false", mk(0x0004, 0, bred)},
        {"repr.none", mk(0x0004, 0, magenta)},
        {"repr.url", mk(0x0008, 0x0005, bblue)},
        {"repr.uuid", mk(0, 0x0001, byellow)},
        {"repr.call", mk(0x0001, 0, magenta)},
        {"repr.path", mk(0, 0, magenta)},
        {"repr.filename", mk(0, 0, bmagenta)},
        {"rule.line", mk(0, 0, bgreen)},
        {"rule.text", mk(0)},
        {"json.brace", mk(0x0001)},
        {"json.bool_true", mk(0x0004, 0, bgreen)},
        {"json.bool_false", mk(0x0004, 0, bred)},
        {"json.null", mk(0x0004, 0, magenta)},
        {"json.number", mk(0x0001, 0x0004, cyan)},
        {"json.str", mk(0, 0x0006, green)},
        {"json.key", mk(0x0001, 0, blue)},
        {"prompt", mk(0)},
        {"prompt.choices", mk(0x0001, 0, magenta)},
        {"prompt.default", mk(0x0001, 0, cyan)},
        {"prompt.invalid", mk(0, 0, red)},
        {"prompt.invalid.choice", mk(0, 0, red)},
        {"pretty", mk(0)},
        {"scope.border", mk(0, 0, blue)},
        {"scope.key", mk(0x0004, 0, yellow)},
        {"scope.key.special", mk(0x0004, 0x0002, yellow)},
        {"scope.equals", mk(0, 0, red)},
        {"table.header", mk(0x0001)},
        {"table.footer", mk(0x0001)},
        {"table.cell", mk(0)},
        {"table.title", mk(0x0004)},
        {"table.caption", mk(0x0004, 0x0002)},
        {"traceback.error", mk(0x0004, 0, red)},
        {"traceback.border.syntax_error", mk(0, 0, bred)},
        {"traceback.border", mk(0, 0, red)},
        {"traceback.text", mk(0)},
        {"traceback.title", mk(0x0001, 0, red)},
        {"traceback.exc_type", mk(0x0001, 0, bred)},
        {"traceback.exc_value", mk(0)},
        {"traceback.offset", mk(0x0001, 0, bred)},
        {"traceback.error_range", mk(0x0009)},
        {"traceback.note", mk(0x0001, 0, green)},
        {"traceback.group.border", mk(0, 0, magenta)},
        // Python Color.parse("grey23") = 8-bit gray ramp index 237.
        {"bar.back", mk(0, 0, Color::charcoal(237))},
        {"bar.complete", mk(0, 0, Color::rgb(249, 38, 114))},
        {"bar.finished", mk(0, 0, Color::rgb(114, 156, 31))},
        {"bar.pulse", mk(0, 0, Color::rgb(249, 38, 114))},
        {"progress.description", mk(0)},
        {"progress.filesize", mk(0, 0, green)},
        {"progress.filesize.total", mk(0, 0, green)},
        {"progress.download", mk(0, 0, green)},
        {"progress.elapsed", mk(0, 0, yellow)},
        {"progress.percentage", mk(0, 0, magenta)},
        {"progress.remaining", mk(0, 0, cyan)},
        {"progress.data.speed", mk(0, 0, red)},
        {"progress.spinner", mk(0, 0, green)},
        {"status.spinner", mk(0, 0, green)},
        {"tree", mk(0)},
        {"tree.line", mk(0)},
        {"markdown.paragraph", mk(0)},
        {"markdown.text", mk(0)},
        {"markdown.em", mk(0x0004)},
        {"markdown.emph", mk(0x0004)},
        {"markdown.strong", mk(0x0001)},
        {"markdown.code", mk(0x0001, 0, cyan, black)},
        {"markdown.code_block", mk(0, 0, cyan, black)},
        {"markdown.block_quote", mk(0, 0, magenta)},
        {"markdown.list", mk(0, 0, cyan)},
        {"markdown.item", mk(0)},
        {"markdown.item.bullet", mk(0x0001)},
        {"markdown.item.number", mk(0, 0, cyan)},
        {"markdown.hr", mk(0x0002)},
        {"markdown.h1.border", mk(0)},
        {"markdown.h1", mk(0x0009)},
        {"markdown.h2", mk(0x0008, 0, magenta)},
        {"markdown.h3", mk(0x0001, 0, magenta)},
        {"markdown.h4", mk(0x0004, 0, magenta)},
        {"markdown.h5", mk(0x0004)},
        {"markdown.h6", mk(0x0002)},
        {"markdown.h7", mk(0x0006)},
        {"markdown.link", mk(0, 0, bblue)},
        {"markdown.link_url", mk(0x0008, 0, blue)},
        {"markdown.s", mk(0x0020)},
        {"markdown.table.border", mk(0, 0, cyan)},
        {"markdown.table.header", mk(0, 0x0001, cyan)},
        {"markdown.kbd", mk(0x0001, 0, byellow)},
        {"iso8601.date", mk(0, 0, blue)},
        {"iso8601.time", mk(0, 0, magenta)},
        {"iso8601.timezone", mk(0, 0, yellow)},
    };
    return styles;
}

std::string Theme::config() const {
    std::string out = "[styles]\n";
    bool first = true;
    for (const auto& [name, style] : styles_) {
        if (!first) out += "\n";
        first = false;
        out += name;
        out += " = ";
        out += svg::py_repr_style(style);
    }
    return out;
}

Style Console::get_style(std::string_view name, std::string_view default_name) const {
    auto it = theme_stack_.back().find(std::string(name));
    if (it != theme_stack_.back().end())
        return it->second;
    try {
        return parse_style_def(name);
    } catch (const std::invalid_argument&) {
        if (default_name.empty())
            throw std::runtime_error("no style defined for '" + std::string(name) + "'");
        return get_style(default_name, "");
    }
}

void Console::push_theme(const Theme& theme, bool inherit) {
    std::map<std::string, Style> entry =
        inherit ? theme_stack_.back() : std::map<std::string, Style>{};
    for (const auto& [k, v] : theme.styles()) entry[k] = v;
    theme_stack_.push_back(std::move(entry));
}

void Console::pop_theme() {
    if (theme_stack_.size() <= 1)
        throw std::runtime_error("unable to pop base theme");
    theme_stack_.pop_back();
}

ThemeScope Console::use_theme(const Theme& theme, bool inherit) {
    push_theme(theme, inherit);
    return ThemeScope(*this);
}

ThemeScope::~ThemeScope() {
    if (console_ != nullptr) console_->pop_theme();
}

namespace {

// Python rich.markup grammar (RE_TAGS = ((\\*)\[([a-z#/@][^[]*?)])]):
//   - A tag is: optional backslashes, '[', a FIRST character in [a-z#/@],
//     lazy content (no '['), ']'. A '[' that does not begin such a tag is a
//     literal bracket; ']' outside a tag is literal. (So "[[x]" yields a
//     literal first '[' plus tag "[x]" — there is no "[[" escape token.)
//   - An odd number of backslashes before a tag escapes it (the whole match
//     is literal); an even count yields k/2 literal backslashes, then the tag.
//   - Open tags: unknown names resolve to an EMPTY style — the delimiters are
//     consumed either way (Python Style.normalize is forgiving).
//   - Close '[/name]' pops the matching open tag (searched from the top,
//     aliases canonicalized); '[/]' pops the innermost. Python raises
//     MarkupError on mismatch/empty stack; this port degrades leniently
//     (pop one level), which only diverges on inputs Python rejects.

bool is_tag_start(char c) {
    return (c >= 'a' && c <= 'z') || c == '#' || c == '/' || c == '@';
}

// Python str(Style.parse(tag)) normalization for close-tag matching:
// attributes canonicalized (b→bold, i→italic, u→underline, d→dim, r→reverse,
// c→conceal, s→strike, o→overline, uu→underline2) and reordered into
// STYLE_ATTRIBUTES bit order, then the color word (verbatim), then "on X".
// If any word is unknown, Style.parse fails and normalize falls back to
// tag.strip().lower().
std::string lower_all(std::string_view sv) {
    std::string out(sv);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string canon_tag_name(std::string_view tag) {
    static const char* kCanonOrder[] = {
        "bold", "dim", "italic", "underline", "blink", "blink2", "reverse",
        "conceal", "strike", "underline2", "frame", "encircle", "overline",
    };
    auto alias = [](std::string_view w) -> std::string_view {
        if (w == "b") return "bold";
        if (w == "i") return "italic";
        if (w == "u") return "underline";
        if (w == "d") return "dim";
        if (w == "r") return "reverse";
        if (w == "c") return "conceal";
        if (w == "s") return "strike";
        if (w == "o") return "overline";
        if (w == "uu") return "underline2";
        return w;
    };
    std::vector<std::string> attrs;  // canonical attribute names found
    std::string color, bg;
    size_t tstart = 0;
    while (tstart < tag.size()) {
        std::string_view word = next_tag_word(tag, tstart);
        std::string_view canon = alias(word);
        bool is_attr = false;
        for (const auto* name : kCanonOrder)
            if (canon == name) {
                if (std::find(attrs.begin(), attrs.end(), std::string(name)) == attrs.end())
                    attrs.push_back(name);
                is_attr = true;
                break;
            }
        if (is_attr) continue;
        if (word.size() > 3 && word.compare(0, 3, "on ") == 0)
            bg = std::string(word);  // last background wins
        else {
            Color tmp;
            if (parse_color_word(word, tmp)) color = std::string(word);  // last color wins
            else return lower_all(tag);  // Style.parse failure -> strip().lower()
        }
    }
    std::string out;
    for (const auto* name : kCanonOrder)  // attributes in canonical bit order
        if (std::find(attrs.begin(), attrs.end(), std::string(name)) != attrs.end()) {
            if (!out.empty()) out += ' ';
            out += name;
        }
    if (!color.empty()) {
        if (!out.empty()) out += ' ';
        out += color;
    }
    if (!bg.empty()) {
        if (!out.empty()) out += ' ';
        out += bg;
    }
    if (out.empty()) return lower_all(tag);
    return out;
}
} // namespace

std::vector<Segment> parse_markup(std::string_view sv) {
    struct Frame { Style style; std::string name; };
    std::vector<Segment> segments;
    std::vector<Frame> stack;
    stack.push_back({Style{}, ""});

    // True when sv[b] begins a tag (with k backslashes before it at b-k):
    // tag-start char present, and a closing ']' before any '['.
    auto tag_bounds = [&](size_t b) -> size_t {
        // b points at '['
        if (b + 1 >= sv.size() || !is_tag_start(sv[b + 1])) return std::string_view::npos;
        const size_t close = sv.find(']', b + 2);
        const size_t next_open = sv.find('[', b + 2);
        if (close == std::string_view::npos) return std::string_view::npos;
        if (next_open != std::string_view::npos && next_open < close)
            return std::string_view::npos;
        return close;
    };

    auto apply_tag_open = [&](std::string_view tag) {
        Style next_st = stack.back().style;
        size_t tstart = 0;
        while (tstart < tag.size()) apply_tag(next_tag_word(tag, tstart), next_st);
        stack.push_back({next_st, canon_tag_name(tag)});
    };
    auto apply_tag_close = [&](std::string_view tag) {
        std::string_view name = tag.substr(1);
        const size_t eq = name.find('=');
        if (eq != std::string_view::npos) name = name.substr(0, eq);
        // Trim surrounding spaces (Python: style_name.strip()).
        while (!name.empty() && name.front() == ' ') name.remove_prefix(1);
        while (!name.empty() && name.back() == ' ') name.remove_suffix(1);
        if (name.empty()) {
            if (stack.size() > 1) stack.pop_back();
            else stack.back().style = Style{};
            return;
        }
        const std::string want = canon_tag_name(name);
        for (int i = static_cast<int>(stack.size()) - 1; i >= 1; --i) {
            if (stack[i].name == want) {
                stack.resize(static_cast<size_t>(i));
                return;
            }
        }
        if (stack.size() > 1) stack.pop_back();  // lenient fallback
    };

    size_t pos = 0;
    while (pos < sv.size()) {
        // Literal chunk: run up to the next '[' or a backslash that directly
        // precedes a '[' (escapes must be decided together with the tag).
        size_t i = pos;
        while (i < sv.size() && sv[i] != '[' &&
               !(sv[i] == '\\' && i + 1 < sv.size() && sv[i + 1] == '['))
            ++i;
        if (i == pos) {
            if (i < sv.size() && sv[i] == '\\') {
                // Run of k backslashes before '[' at b = i + k.
                size_t k = 0;
                while (k < sv.size() - i && sv[i + k] == '\\') ++k;
                const size_t b = i + k;  // position of '['
                const size_t close = tag_bounds(b);
                if (close != std::string_view::npos) {
                    if (k % 2 == 1) {
                        // Escaped: the whole match (backslashes + tag) is literal.
                        segments.push_back(
                            {emoji_replace(sv.substr(i, close + 1 - i)), stack.back().style});
                        pos = close + 1;
                        continue;
                    }
                    if (k > 0)
                        segments.push_back({std::string(k / 2, '\\'), stack.back().style});
                    std::string_view tag = sv.substr(b + 1, close - b - 1);
                    if (tag[0] == '/') apply_tag_close(tag);
                    else apply_tag_open(tag);
                    pos = close + 1;
                    continue;
                }
                // Not a tag: backslashes are literal, then handle '['.
                segments.push_back({std::string(k, '\\'), stack.back().style});
                pos = b;
                continue;
            }
            // i at '[' (no preceding backslashes)
            const size_t close = tag_bounds(i);
            if (close != std::string_view::npos) {
                std::string_view tag = sv.substr(i + 1, close - i - 1);
                if (tag[0] == '/') apply_tag_close(tag);
                else apply_tag_open(tag);
                pos = close + 1;
                continue;
            }
            segments.push_back({"[", stack.back().style});
            ++pos;
            continue;
        }
        segments.push_back({emoji_replace(sv.substr(pos, i - pos)), stack.back().style});
        pos = i;
    }
    return segments;
}

} // namespace rich
