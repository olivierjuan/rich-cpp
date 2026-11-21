#pragma once
#include <rich_core.h>
#include <string_view>

namespace rich::markdown {
// Minimal wrapper for md4c
RICH_EXPORT AnyRenderable parse(std::string_view md);
}
