#include "rich_core.h"
namespace rich {

namespace rich {

void Console::print(const AnyRenderable& ar) {
    auto segments = ar.render(*this, Options{});
    for (const auto& s : segments) {
        // Basic ANSI application
        if (s.style.bold) os_ << "\x1b[1m";
        if (s.style.red) os_ << "\x1b[31m";
        if (s.style.green) os_ << "\x1b[32m";
        os_ << s.text;
        os_ << "\x1b[0m"; // Reset
    }
    os_ << std::endl;
}

std::vector<Segment> parse_markup(std::string_view sv) {
    std::vector<Segment> segments;
    Style current_style;
    size_t pos = 0;
    while (pos < sv.size()) {
        if (sv[pos] == '[') {
            size_t end = sv.find(']', pos);
            if (end != std::string_view::npos) {
                std::string_view tag = sv.substr(pos + 1, end - pos - 1);
                if (tag == "bold") current_style.bold = true;
                else if (tag == "red") current_style.red = true;
                else if (tag == "green") current_style.green = true;
                else if (tag == "/bold") current_style.bold = false;
                else if (tag == "/red") current_style.red = false;
                else if (tag == "/green") current_style.green = false;
                pos = end + 1;
                continue;
            }
        }
        size_t next_tag = sv.find('[', pos);
        std::string text(sv.substr(pos, next_tag - pos));
        segments.push_back({text, current_style});
        pos = next_tag;
    }
    return segments;
}

} // namespace rich