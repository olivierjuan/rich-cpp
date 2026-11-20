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
namespace rich {
#include <sstream>
#include <utility>
#include <cstdint>

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
    Color color;
    Color bg_color;

    constexpr bool operator==(const Style&) const = default;
};

struct RICH_EXPORT Segment {
    std::string text;
    Style style;
};

struct Options {
    bool show_border{true};
    size_t width{80};
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
    AnyRenderable(T&& t)
        : self_(std::make_shared<Model<std::decay_t<T>>>(std::forward<T>(t))) {}

    std::vector<Segment> render(const Console& con, const Options& opt) const {
        return self_ ? self_->render(con, opt) : std::vector<Segment>{};
    }
    std::pair<size_t, size_t> measure(const Console& con, const Options& opt) const {
        return self_ ? self_->measure(con, opt) : std::pair<size_t, size_t>{0, 0};
    }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual std::vector<Segment> render(const Console& con, const Options& opt) const = 0;
        virtual std::pair<size_t, size_t> measure(const Console& con, const Options& opt) const = 0;
    };

    template<class T>
    struct Model final : Concept {
        template<typename U>
        explicit Model(U&& t) : t_(std::forward<U>(t)) {}

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

    size_t width() const noexcept { return options_.width; }
    std::string export_text() const;

private:
    std::ostream& os_;
    ConsoleOptions options_;
    std::string record_buffer_;
};

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
inline std::vector<Segment> render_item(const Text& t, const Console&, const Options&) {
    return {{t.content, t.style}};
}
inline std::pair<size_t, size_t> measure_item(const Text& t, const Console&, const Options&) {
    return {t.content.size(), 1};
}

// StyledText primitive
struct StyledText {
    std::string content;
};
RICH_EXPORT std::vector<Segment> parse_markup(std::string_view sv);
inline std::vector<Segment> render_item(const StyledText& t, const Console&, const Options&) {
    return parse_markup(t.content);
}
inline std::pair<size_t, size_t> measure_item(const StyledText& t, const Console&, const Options&) {
    return {t.content.size(), 1};
}

// Rule primitive
struct Rule {
    std::string title;
    size_t width{0};
};
RICH_EXPORT std::vector<Segment> render_item(const Rule& r, const Console& c, const Options& o);
RICH_EXPORT std::pair<size_t, size_t> measure_item(const Rule& r, const Console& c, const Options& o);

} // namespace rich
