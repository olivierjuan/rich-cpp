#pragma once
#include <rich_core.h>

namespace rich::highlight {
class Highlighter {
public:
    virtual ~Highlighter() = default;
    virtual AnyRenderable highlight(std::string_view text) = 0;
};
}
