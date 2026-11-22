#pragma once
#include "rich_export.h"
#include <string_view>
#include <string>
#include <vector>
#include <type_traits>
#include <concepts>
#include <iosfwd>
#include <memory>
#include <iostream>
#include <sstream>
#include <utility>
#include <cstdint>
#include <algorithm>

namespace rich {

struct RICH_EXPORT Color {
    enum class Type { None, Standard, Rgb };
    Type type{Type::None};
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t ansi{0};

    static constexpr Color from_ansi(uint8_t code) {
        Color c;
        c.type = Type::Standard;
        c.ansi = code;
        return c;
    }
    static constexpr Color rgb(uint8_t red, uint8_t green, uint8_t blue) {
        Color c;
        c.type = Type::Rgb;
        c.r = red; c.g = green; c.b = blue;
        return c;
    }
    constexpr bool operator==(const Color&) const = default;
};

struct RICH_EXPORT Style {
    bool bold{false};
    bool dim{false};
    bool italic{false};
    bool underline{false};
    bool reverse{false};
    bool strike{false};
    Color color;
    Color bg_color;
    // Python Style tracks which boolean attributes were *explicitly set* in
    // the constructor; repr emits them even when false (e.g.
    // Style(color='green', bold=False) -> "bold=False"). Bit 0=bold,
    // 1=dim, 2=italic, 3=underline, 4=reverse, 5=strike.
    uint8_t explicit_bits{0};
    // Hyperlink target (Python Style.link); OSC 8 on terminal render.
    std::string link;
    uint32_t link_id{0};

    constexpr bool operator==(const Style& o) const {
        return bold == o.bold && dim == o.dim && italic == o.italic &&
               underline == o.underline && reverse == o.reverse &&
               strike == o.strike && color == o.color && bg_color == o.bg_color &&
               explicit_bits == o.explicit_bits && link == o.link;
    }

    // Python bool(Style): false only for the null style (no attributes,
    // colors, explicit bits, or link).
    bool truthy() const {
        return explicit_bits != 0 || bold || dim || italic || underline ||
               reverse || strike || color.type != Color::Type::None ||
               bg_color.type != Color::Type::None || !link.empty();
    }

    // Python Style.__add__ / _add: right operand wins for the attribute bits
    // it explicitly set; colors/bg/link take `right or left`; explicit bits
    // union. A null (kNoneStyle) operand is the identity.
    Style operator+(const Style& o) const;
};

struct RICH_EXPORT Segment {
    std::string text;
    Style style;
};

namespace detail {
// Sentinel for Python's style=None (plain strings without markup tags and the
// appended newline). Distinct from an explicit empty Style, which markup gap
// pieces carry and whose repr must appear in export_svg's unique_id hash.
RICH_EXPORT const Style& kNoneStyle();
} // namespace detail

struct Options {
    bool show_border{true};
    size_t width{80};
    size_t height{0}; // 0 = not applicable / auto
};

class Console;

template<class T, class C, class Opt>
concept Renderable = requires(const T& t, const C& c, const Opt& o) {
    { render_item(t, c, o) } -> std::same_as<std::vector<Segment>>;
    { measure_item(t, c, o) } -> std::convertible_to<std::pair<size_t, size_t>>;
};

class RICH_EXPORT AnyRenderable {
public:
    AnyRenderable() = default;

    template<class T>
    requires Renderable<std::decay_t<T>, Console, Options>
    AnyRenderable(const T& t)
        : self_(std::make_shared<Model<std::decay_t<T>>>(t)) {}

    // Type-erased copy/move: copies the shared model (which owns a copy of T).
    // For non-copyable T this is only viable at wrap time; subsequent copies
    // of the AnyRenderable share the same Model instance via shared_ptr.
    AnyRenderable(const AnyRenderable&) = default;
    AnyRenderable& operator=(const AnyRenderable&) = default;
    AnyRenderable(AnyRenderable&&) noexcept = default;
    AnyRenderable& operator=(AnyRenderable&&) noexcept = default;

    std::vector<Segment> render(const Console& con, const Options& opt) const {
        return self_ ? self_->render(con, opt) : std::vector<Segment>{};
    }
    std::pair<size_t, size_t> measure(const Console& con, const Options& opt) const {
        return self_ ? self_->measure(con, opt) : std::pair<size_t, size_t>{0, 0};
    }

    bool empty() const noexcept { return !self_; }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual std::vector<Segment> render(const Console& con, const Options& opt) const = 0;
        virtual std::pair<size_t, size_t> measure(const Console& con, const Options& opt) const = 0;
    };

    template<class T>
    struct Model final : Concept {
        // Owns a copy of the wrapped value. Constructed once at wrap time;
        // AnyRenderable copies thereafter share this Model via shared_ptr.
        explicit Model(const T& t) : t_(t) {}

        std::vector<Segment> render(const Console& con, const Options& opt) const override {
            return render_item(t_, con, opt);
        }
        std::pair<size_t, size_t> measure(const Console& con, const Options& opt) const override {
            return measure_item(t_, con, opt);
        }

        T t_;
    };

    std::shared_ptr<const Concept> self_;
};

struct ConsoleOptions {
    size_t width{80};
    size_t height{0}; // 0 = auto-detect (defaults to 25 rows)
    bool record{false};
    bool legacy_windows{false};
};

class RICH_EXPORT Console {
public:
    explicit Console(std::ostream& os = std::cout, ConsoleOptions options = {}) noexcept
        : os_(os), options_(options) {}

    void print(const AnyRenderable& ar);
    void print(std::string_view markup_text);
    void rule(std::string_view title = "");

    // Write raw bytes (VT escapes, etc.) directly to the underlying stream.
    // Not recorded in export_text(); intended for live animation only.
    void write_raw(std::string_view text) {
        os_ << text;
        os_.flush();
    }

    size_t width() const noexcept { return options_.width; }
    size_t height() const noexcept { return options_.height; }
    bool legacy_windows() const noexcept { return options_.legacy_windows; }
    std::string export_text() const;
    // Terminal snapshot as SVG (Python Rich Console.export_svg parity).
    // Requires record=true. title: window caption; unique_id: deterministic
    // id prefix (default: derived from content via adler32, like Python).
    std::string export_svg(const std::string& title = "Rich",
                           const std::string& unique_id = "") const;
    // Console contents as a standalone HTML document (Python
    // Console.export_html parity, DEFAULT_TERMINAL_THEME). Requires
    // record=true. inline_styles: true -> per-span style attributes;
    // false (default) -> <style> block with r1..rN classes.
    std::string export_html(bool inline_styles = false) const;
    // Segment-level access to the recorded buffer (styles preserved).
    const std::vector<Segment>& recorded_segments() const noexcept { return record_segments_; }
    // Python Console.render_lines parity: render each renderable at `opt`,
    // apply `style` to every segment when truthy (Segment.apply_style:
    // `style + segment.style`, None-style segments take the base), split at
    // '\n', crop lines longer than opt.width at cell boundaries and pad
    // shorter lines with a pad segment (style = `style`, or the None-style
    // sentinel when `style` is null).
    std::vector<std::vector<Segment>> render_lines(
        const std::vector<AnyRenderable>& renderables, const Options& opt,
        const Style* style, bool pad = true) const;

private:
    std::ostream& os_;
    ConsoleOptions options_;
    std::string record_buffer_;
    std::vector<Segment> record_segments_;
};

// Core types above are complete here. The wrapping engine (Text.h) needs
// Segment/Style/Options/Console defined, and the render helpers below need
// the engine — so Text.h is pulled in at global scope between the two.
} // namespace rich

#include <Text.h>

namespace rich {

// Pipe composition
template <typename F>
AnyRenderable operator|(AnyRenderable lhs, F&& rhs) {
    return rhs(std::move(lhs));
}

// Text primitive
struct Text {
    std::string content;
    Style style;
};
inline std::vector<Segment> render_item(const Text& t, const Console&, const Options& opt) {
    // Python Text.__rich_console__: wrap at console width (cell-aware), join
    // lines with style-None newlines, append the end ("\n").
    size_t width = (opt.width > 0) ? opt.width : 80;
    std::vector<Segment> single{{t.content, t.style}};
    auto out = text::wrap_detail::wrap_styled(single, width, true, detail::kNoneStyle());
    out.push_back({"\n", detail::kNoneStyle()});
    return out;
}
inline std::pair<size_t, size_t> measure_item(const Text& t, const Console&, const Options&) {
    // Python Text.__rich_measure__: min = longest word's cell width,
    // max = widest line's cell width.
    size_t max_line = 0, longest_word = 0;
    {
        std::string cur;
        for (char c : t.content) {
            if (c == '\n') {
                max_line = std::max(max_line, unicode::string_cell_len(cur));
                cur.clear();
            } else cur.push_back(c);
        }
        max_line = std::max(max_line, unicode::string_cell_len(cur));
    }
    for (const auto& w : text::wrap_detail::words(t.content)) {
        std::string_view core = w.text;
        while (!core.empty() && (core.back() == ' ' || core.back() == '\t'))
            core.remove_suffix(1);
        longest_word = std::max(longest_word, unicode::string_cell_len(core));
    }
    return {std::min(longest_word, max_line), max_line};
}

// StyledText primitive
struct StyledText {
    std::string content;
};
RICH_EXPORT std::vector<Segment> parse_markup(std::string_view sv);
// Emoji shortcode replacement (":smile:" -> UTF-8 emoji), Python Rich
// _emoji_replace parity. Defined in rich_core.cpp on top of the generated
// EmojiTable.h (kept out of this header for compile times).
RICH_EXPORT std::string emoji_replace(std::string_view text);
inline std::vector<Segment> render_item(const StyledText& t, const Console&, const Options& opt) {
    // Python markup.render: a string with no '[' becomes Text(text,
    // style="") with no spans → Segment(text) carries style=None, while
    // markup strings render gap pieces with an explicit empty Style().
    // Emoji shortcodes are replaced in both paths (Python applies
    // _emoji_replace to each text chunk). The result is a Text, so the
    // segment stream is cell-wrapped at the console width with style-None
    // inter-line newlines plus the trailing end ("\n").
    size_t width = (opt.width > 0) ? opt.width : 80;
    if (t.content.find('[') == std::string::npos) {
        std::string text = emoji_replace(t.content);
        auto out = text::wrap_detail::wrap_styled({{text, detail::kNoneStyle()}}, width,
                                                  true, detail::kNoneStyle());
        out.push_back({"\n", detail::kNoneStyle()});
        return out;
    }
    auto segs = parse_markup(t.content);
    auto out = text::wrap_detail::wrap_styled(segs, width, true, detail::kNoneStyle());
    out.push_back({"\n", detail::kNoneStyle()});
    return out;
}
inline std::pair<size_t, size_t> measure_item(const StyledText& t, const Console&, const Options&) {
    // Same measurement contract as Text (MarkupText delegates to it).
    size_t max_line = 0, longest_word = 0;
    {
        std::string cur;
        for (char c : t.content) {
            if (c == '\n') {
                max_line = std::max(max_line, unicode::string_cell_len(cur));
                cur.clear();
            } else cur.push_back(c);
        }
        max_line = std::max(max_line, unicode::string_cell_len(cur));
    }
    for (const auto& w : text::wrap_detail::words(t.content)) {
        std::string_view core = w.text;
        while (!core.empty() && (core.back() == ' ' || core.back() == '\t'))
            core.remove_suffix(1);
        longest_word = std::max(longest_word, unicode::string_cell_len(core));
    }
    return {std::min(longest_word, max_line), max_line};
}

// Rule primitive
struct Rule {
    std::string title;
    size_t width{0};
};
RICH_EXPORT std::vector<Segment> render_item(const Rule& r, const Console& c, const Options& o);
RICH_EXPORT std::pair<size_t, size_t> measure_item(const Rule& r, const Console& c, const Options& o);

} // namespace rich
