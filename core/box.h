#pragma once
#include <string_view>

namespace rich::box {

struct Box {
    std::string_view top_left;
    std::string_view top;
    std::string_view top_divider;
    std::string_view top_right;

    std::string_view mid_left;
    std::string_view mid_vertical;
    std::string_view mid_right;

    std::string_view head_row_left;
    std::string_view head_row_horizontal;
    std::string_view head_row_cross;
    std::string_view head_row_right;

    std::string_view bottom_left;
    std::string_view bottom;
    std::string_view bottom_divider;
    std::string_view bottom_right;
};

inline constexpr Box ROUNDED = {
    .top_left = "╭",
    .top = "─",
    .top_divider = "┬",
    .top_right = "╮",
    .mid_left = "│",
    .mid_vertical = "│",
    .mid_right = "│",
    .head_row_left = "├",
    .head_row_horizontal = "─",
    .head_row_cross = "┼",
    .head_row_right = "┤",
    .bottom_left = "╰",
    .bottom = "─",
    .bottom_divider = "┴",
    .bottom_right = "╯",
};

inline constexpr Box SQUARE = {
    .top_left = "┌",
    .top = "─",
    .top_divider = "┬",
    .top_right = "┐",
    .mid_left = "│",
    .mid_vertical = "│",
    .mid_right = "│",
    .head_row_left = "├",
    .head_row_horizontal = "─",
    .head_row_cross = "┼",
    .head_row_right = "┤",
    .bottom_left = "└",
    .bottom = "─",
    .bottom_divider = "┴",
    .bottom_right = "┘",
};

inline constexpr Box HEAVY = {
    .top_left = "┏",
    .top = "━",
    .top_divider = "┳",
    .top_right = "┓",
    .mid_left = "┃",
    .mid_vertical = "┃",
    .mid_right = "┃",
    .head_row_left = "┣",
    .head_row_horizontal = "━",
    .head_row_cross = "╋",
    .head_row_right = "┫",
    .bottom_left = "┗",
    .bottom = "━",
    .bottom_divider = "┻",
    .bottom_right = "┛",
};

inline constexpr Box DOUBLE = {
    .top_left = "╔",
    .top = "═",
    .top_divider = "╦",
    .top_right = "╗",
    .mid_left = "║",
    .mid_vertical = "║",
    .mid_right = "║",
    .head_row_left = "╠",
    .head_row_horizontal = "═",
    .head_row_cross = "╬",
    .head_row_right = "╣",
    .bottom_left = "╚",
    .bottom = "═",
    .bottom_divider = "╩",
    .bottom_right = "╝",
};

inline constexpr Box ASCII = {
    .top_left = "+",
    .top = "-",
    .top_divider = "-",
    .top_right = "+",
    .mid_left = "|",
    .mid_vertical = "|",
    .mid_right = "|",
    .head_row_left = "|",
    .head_row_horizontal = "-",
    .head_row_cross = "+",
    .head_row_right = "|",
    .bottom_left = "+",
    .bottom = "-",
    .bottom_divider = "-",
    .bottom_right = "+",
};

} // namespace rich::box
