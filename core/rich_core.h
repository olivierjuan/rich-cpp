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
#include <utility>

namespace rich {

struct Style {
    bool bold{false};
    bool red{false};
    bool green{false};
    constexpr bool operator==(const Style&) const = default;
};

struct RICH_EXPORT Segment {
    std::string text;
    Style style;
};

struct Options {
    bool show_border{true};
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

class RICH_EXPORT Console {
public:
    explicit Console(std::ostream& os = std::cout) noexcept : os_(os) {}
    void print(const AnyRenderable& ar);
private:
    std::ostream& os_;
};

// Pipe composition
template <typename F>
AnyRenderable operator|(AnyRenderable lhs, F&& rhs) {
    return rhs(std::move(lhs));
}

// Text primitive
struct Text {
    std::string content;
};
inline std::vector<Segment> render_item(const Text& t, const Console&, const Options&) {
    return {{t.content, {}}};
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

} // namespace rich
