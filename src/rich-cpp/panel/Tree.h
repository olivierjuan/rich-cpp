#pragma once
#include <rich_core.h>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace rich::tree {

// Python rich.tree.Tree port. Per-node `style` (label, theme default "tree")
// and `guide_style` (guide glyphs, theme default "tree.line") are combined
// down the tree (ancestor + own); the `expanded` flag hides a node's
// children; `hide_root` suppresses the root label line.
//
// Guide glyph weight follows the resolved guide style exactly like Python
// make_guide: bold → heavy (┃/┣━━/┗━━), underline2 → double (║/╠══/╚══),
// otherwise normal (│/├──/└──). Prefix guide segments are emitted through
// Python Segment.apply_style with post_style=Style(bold=False, underline2=False)
// (explicit off), so ancestor/current prefix guides lose their bold/underline2
// SGR even when the glyph was chosen by those attributes.
class Tree {
public:
    struct Options {
        std::optional<std::string> style;        // nullopt: inherit (root → "tree")
        std::optional<std::string> guide_style;  // nullopt: inherit (root → "tree.line")
        bool expanded{true};
        bool hide_root{false};
    };

    explicit Tree(std::string label, Options opts = {})
        : label_(std::move(label)), opts_(std::move(opts)) {
        if (!opts_.style) opts_.style = std::string("tree");
        if (!opts_.guide_style) opts_.guide_style = std::string("tree.line");
    }

    // Python add(): style/guide_style inherit from the parent unless given.
    Tree& add(std::string label, Options opts = {}) {
        if (!opts.style) opts.style = opts_.style;
        if (!opts.guide_style) opts.guide_style = opts_.guide_style;
        auto node = std::make_shared<Tree>(std::move(label), std::move(opts));
        children_.push_back(std::move(node));
        return *children_.back();
    }

    Tree& add_child(Tree child) {
        if (!child.opts_.style) child.opts_.style = opts_.style;
        if (!child.opts_.guide_style) child.opts_.guide_style = opts_.guide_style;
        auto node = std::make_shared<Tree>(std::move(child));
        children_.push_back(std::move(node));
        return *children_.back();
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        auto resolve = [&con](const std::string& name) {
            try {
                return con.get_style(name, "");
            } catch (...) {
                return rich::Style{};
            }
        };
        // Python TREE_GUIDES rows: 0 normal, 1 bold (heavy), 2 underline2 (double);
        // columns: SPACE, CONTINUE, FORK, END.
        static const char* kGuides[3][4] = {
            {"    ",
             "\xe2\x94\x82   ",
             "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ",
             "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "},
            {"    ",
             "\xe2\x94\x83   ",
             "\xe2\x94\xa3\xe2\x94\x81\xe2\x94\x81 ",
             "\xe2\x94\x97\xe2\x94\x81\xe2\x94\x81 "},
            {"    ",
             "\xe2\x95\x91   ",
             "\xe2\x95\xa0\xe2\x95\x90\xe2\x95\x90 ",
             "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90 "},
        };
        enum { SPACE = 0, CONTINUE = 1, FORK = 2, END = 3 };
        auto make_guide = [&](int index, const rich::Style& st) {
            int guide = st.bold ? 1 : (st.underline2 ? 2 : 0);
            return rich::Segment{kGuides[guide][index], st};
        };
        // Python remove_guide_styles = Style(bold=False, underline2=False):
        // explicit off, bold (bit 0) and underline2 (bit 9) only.
        rich::Style remove_guide;
        remove_guide.explicit_bits = (1u << 0) | (1u << 9);

        struct Frame {
            const Tree* node;
            bool is_root_frame;
            size_t i;
        };
        auto item_count = [](const Frame& f) { return f.is_root_frame ? 1 : f.node->children_.size(); };

        std::vector<Frame> stack{{this, true, 0}};
        std::vector<rich::Segment> levels{make_guide(CONTINUE, resolve(*opts_.guide_style))};
        std::vector<rich::Style> style_stack{resolve(*opts_.style)};
        std::vector<rich::Style> guide_stack{resolve(*opts_.guide_style)};

        std::vector<rich::Segment> out;
        rich::Segment new_line{"\n", detail::kNoneStyle()};
        const size_t prefix_start = opts_.hide_root ? 2 : 1;

        while (!stack.empty()) {
            Frame f = stack.back();
            stack.pop_back();
            if (f.i >= item_count(f)) {
                // Python StopIteration: pop the level slot; the parent slot
                // becomes FORK so later siblings at that depth fork.
                levels.pop_back();
                if (!levels.empty()) {
                    rich::Style gs = levels.back().style.truthy() ? levels.back().style : rich::Style{};
                    levels.back() = make_guide(FORK, gs);
                    guide_stack.pop_back();
                    style_stack.pop_back();
                }
                continue;
            }
            stack.push_back(Frame{f.node, f.is_root_frame, f.i + 1});
            const Tree* node = f.is_root_frame ? f.node : f.node->children_[f.i].get();
            const bool last = (f.i + 1 == item_count(f));
            if (last)
                levels.back() = make_guide(END, levels.back().style.truthy() ? levels.back().style
                                                                             : rich::Style{});
            rich::Style node_guide = guide_stack.back() + resolve(*node->opts_.guide_style);
            rich::Style node_style = style_stack.back() + resolve(*node->opts_.style);

            std::vector<rich::Segment> prefix;
            if (levels.size() > prefix_start)
                prefix.assign(levels.begin() + static_cast<std::ptrdiff_t>(prefix_start), levels.end());
            size_t prefix_w = 0;
            for (const auto& s : prefix) prefix_w += unicode::string_cell_len(s.text);
            rich::Options lopt{.show_border = false,
                               .width = (opt.width > prefix_w) ? opt.width - prefix_w : 0,
                               .height = 0};
            auto lines = con.render_lines({rich::AnyRenderable(rich::StyledText{node->label_})}, lopt,
                                          &node_style, false);

            if (!(node == this && opts_.hide_root)) {
                bool first = true;
                for (auto& line : lines) {
                    if (!prefix.empty()) {
                        rich::Style bg = (node_style.bg_color.type == rich::Color::Type::None)
                                             ? rich::Style{}
                                             : rich::Style{.bg_color = node_style.bg_color};
                        for (auto& s : prefix)
                            out.push_back({s.text, bg + s.style + remove_guide});
                    }
                    for (auto& s : line) out.push_back(std::move(s));
                    out.push_back(new_line);
                    if (first && !prefix.empty())
                        prefix.back() = make_guide(last ? SPACE : CONTINUE,
                                                   prefix.back().style.truthy() ? prefix.back().style
                                                                                : rich::Style{});
                    first = false;
                }
            }

            if (node->opts_.expanded && !node->children_.empty()) {
                levels.back() = make_guide(last ? SPACE : CONTINUE,
                                           levels.back().style.truthy() ? levels.back().style
                                                                       : rich::Style{});
                levels.push_back(make_guide(node->children_.size() == 1 ? END : FORK, node_guide));
                style_stack.push_back(resolve(*node->opts_.style));
                guide_stack.push_back(resolve(*node->opts_.guide_style));
                stack.push_back(Frame{node, false, 0});
            }
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        // Python __rich_measure__: max over nodes of (label width + 4 * level),
        // descending only into expanded nodes.
        size_t minimum = 0, maximum = 0;
        struct M {
            const Tree* n;
            size_t level;
        };
        std::vector<M> stack{{this, 0}};
        while (!stack.empty()) {
            M m = stack.back();
            stack.pop_back();
            auto lm = measure_item(rich::StyledText{m.n->label_}, con, opt);
            minimum = std::max(minimum, lm.first + m.level * 4);
            maximum = std::max(maximum, lm.second + m.level * 4);
            if (m.n->opts_.expanded)
                for (auto& c : m.n->children_) stack.push_back(M{c.get(), m.level + 1});
        }
        return {minimum, maximum};
    }

private:
    std::string label_;
    Options opts_;
    std::vector<std::shared_ptr<Tree>> children_;
};

inline std::vector<rich::Segment> render_item(const Tree& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Tree& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::tree
