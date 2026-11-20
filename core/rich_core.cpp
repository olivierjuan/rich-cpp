#include "rich_core.h"
namespace rich {
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
} // namespace

void Console::print(const AnyRenderable& ar) {
    init_terminal();
    auto segments = ar.render(*this, Options{});
    for (const auto& s : segments) {
        if (s.style.bold) os_ << "\x1b[1m";
        if (s.style.red) os_ << "\x1b[31m";
        if (s.style.green) os_ << "\x1b[32m";
        os_ << s.text;
        if (s.style.bold || s.style.red || s.style.green) {
            os_ << "\x1b[0m";
        }
    }
    os_ << '\n';
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
                else if (tag == "/bold" || tag == "/") current_style.bold = false;
                else if (tag == "/red") current_style.red = false;
                else if (tag == "/green") current_style.green = false;
                pos = end + 1;
                continue;
            }
        }
        size_t next_tag = sv.find('[', pos);
        std::string text(sv.substr(pos, next_tag - pos));
        if (!text.empty()) {
            segments.push_back({std::move(text), current_style});
        }
        pos = next_tag;
    }
    return segments;
}

} // namespace rich
