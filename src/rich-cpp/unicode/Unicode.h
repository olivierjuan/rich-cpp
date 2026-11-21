#pragma once
#include <rich_export.h>
#include <string_view>

namespace rich::unicode {

// Returns the number of columns required to display the character.
// Uses a simplified approach suitable for basic C++ porting.
RICH_EXPORT int cell_width(char32_t cp);

// Returns the width of a string in cells.
RICH_EXPORT size_t string_width(std::string_view sv);

} // namespace rich::unicode
