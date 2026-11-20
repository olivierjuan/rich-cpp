#pragma once
#include <rich_core.h>
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace rich::tree {

class Tree {
public:
    Tree(std::string label) : label_(std::move(label)) {}

    Tree& add(std::string child_label) {
        children_.push_back(std::make_shared<Tree>(std::move(child_label)));
        return *children_.back();
    }

    Tree& add_child(Tree child) {
        children_.push_back(std::make_shared<Tree>(std::move(child)));
        return *children_.back();
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        render_node(out, "", true, true);
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console&, const rich::Options&) const {
        return {40, children_.size() + 1};
    }

private:
    void render_node(std::vector<rich::Segment>& out, const std::string& prefix, bool is_root, bool is_last) const {
        rich::Style guide_style{.dim = true, .color = rich::Color::from_ansi(90)};

        if (is_root) {
            auto segs = rich::parse_markup(label_);
            for (auto& s : segs) out.push_back(std::move(s));
            out.push_back({"\n", {}});
        } else {
            std::string branch = is_last ? "└── " : "├── ";
            out.push_back({prefix + branch, guide_style});
            auto segs = rich::parse_markup(label_);
            for (auto& s : segs) out.push_back(std::move(s));
            out.push_back({"\n", {}});
        }

        std::string child_prefix = is_root ? "" : (prefix + (is_last ? "    " : "│   "));

        for (size_t i = 0; i < children_.size(); ++i) {
            bool child_is_last = (i + 1 == children_.size());
            children_[i]->render_node(out, child_prefix, false, child_is_last);
        }
    }

    std::string label_;
    std::vector<std::shared_ptr<Tree>> children_;
};

inline std::vector<rich::Segment> render_item(const Tree& t, const rich::Console& c, const rich::Options& o) {
    return t.render(c, o);
}
inline std::pair<size_t, size_t> measure_item(const Tree& t, const rich::Console& c, const rich::Options& o) {
    return t.measure(c, o);
}

} // namespace rich::tree
