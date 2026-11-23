#pragma once
#include <string_view>

namespace rich::box {

// Glyphs ported verbatim from rich 15.0.0 rich/box.py (Box.__init__
// field layout: top / head / head_row / mid / row / foot_row / foot /
// bottom; the center column of each 4-char line is structural and
// omitted, like Python keeps it only for parsing). All 19 variants.
struct Box {
    // top
    std::string_view top_left;
    std::string_view top;
    std::string_view top_divider;
    std::string_view top_right;

    // head
    std::string_view head_left;
    std::string_view head_vertical;
    std::string_view head_right;

    // head_row
    std::string_view head_row_left;
    std::string_view head_row_horizontal;
    std::string_view head_row_cross;
    std::string_view head_row_right;

    // mid
    std::string_view mid_left;
    std::string_view mid_vertical;
    std::string_view mid_right;

    // row
    std::string_view row_left;
    std::string_view row_horizontal;
    std::string_view row_cross;
    std::string_view row_right;

    // foot_row
    std::string_view foot_row_left;
    std::string_view foot_row_horizontal;
    std::string_view foot_row_cross;
    std::string_view foot_row_right;

    // foot
    std::string_view foot_left;
    std::string_view foot_vertical;
    std::string_view foot_right;

    // bottom
    std::string_view bottom_left;
    std::string_view bottom;
    std::string_view bottom_divider;
    std::string_view bottom_right;
};

inline constexpr Box ASCII = {
    "+", "-", "-", "+",
    "|", "|", "|",
    "|", "-", "+", "|",
    "|", "|", "|",
    "|", "-", "+", "|",
    "|", "-", "+", "|",
    "|", "|", "|",
    "+", "-", "-", "+",
};

inline constexpr Box ASCII2 = {
    "+", "-", "+", "+",
    "|", "|", "|",
    "+", "-", "+", "+",
    "|", "|", "|",
    "+", "-", "+", "+",
    "+", "-", "+", "+",
    "|", "|", "|",
    "+", "-", "+", "+",
};

inline constexpr Box ASCII_DOUBLE_HEAD = {
    "+", "-", "+", "+",
    "|", "|", "|",
    "+", "=", "+", "+",
    "|", "|", "|",
    "+", "-", "+", "+",
    "+", "-", "+", "+",
    "|", "|", "|",
    "+", "-", "+", "+",
};

inline constexpr Box SQUARE = {
    "┌", "─", "┬", "┐",
    "│", "│", "│",
    "├", "─", "┼", "┤",
    "│", "│", "│",
    "├", "─", "┼", "┤",
    "├", "─", "┼", "┤",
    "│", "│", "│",
    "└", "─", "┴", "┘",
};

inline constexpr Box SQUARE_DOUBLE_HEAD = {
    "┌", "─", "┬", "┐",
    "│", "│", "│",
    "╞", "═", "╪", "╡",
    "│", "│", "│",
    "├", "─", "┼", "┤",
    "├", "─", "┼", "┤",
    "│", "│", "│",
    "└", "─", "┴", "┘",
};

inline constexpr Box MINIMAL = {
    " ", " ", "╷", " ",
    " ", "│", " ",
    "╶", "─", "┼", "╴",
    " ", "│", " ",
    "╶", "─", "┼", "╴",
    "╶", "─", "┼", "╴",
    " ", "│", " ",
    " ", " ", "╵", " ",
};

inline constexpr Box MINIMAL_HEAVY_HEAD = {
    " ", " ", "╷", " ",
    " ", "│", " ",
    "╺", "━", "┿", "╸",
    " ", "│", " ",
    "╶", "─", "┼", "╴",
    "╶", "─", "┼", "╴",
    " ", "│", " ",
    " ", " ", "╵", " ",
};

inline constexpr Box MINIMAL_DOUBLE_HEAD = {
    " ", " ", "╷", " ",
    " ", "│", " ",
    " ", "═", "╪", " ",
    " ", "│", " ",
    " ", "─", "┼", " ",
    " ", "─", "┼", " ",
    " ", "│", " ",
    " ", " ", "╵", " ",
};

inline constexpr Box SIMPLE = {
    " ", " ", " ", " ",
    " ", " ", " ",
    " ", "─", "─", " ",
    " ", " ", " ",
    " ", " ", " ", " ",
    " ", "─", "─", " ",
    " ", " ", " ",
    " ", " ", " ", " ",
};

inline constexpr Box SIMPLE_HEAD = {
    " ", " ", " ", " ",
    " ", " ", " ",
    " ", "─", "─", " ",
    " ", " ", " ",
    " ", " ", " ", " ",
    " ", " ", " ", " ",
    " ", " ", " ",
    " ", " ", " ", " ",
};

inline constexpr Box SIMPLE_HEAVY = {
    " ", " ", " ", " ",
    " ", " ", " ",
    " ", "━", "━", " ",
    " ", " ", " ",
    " ", " ", " ", " ",
    " ", "━", "━", " ",
    " ", " ", " ",
    " ", " ", " ", " ",
};

inline constexpr Box HORIZONTALS = {
    " ", "─", "─", " ",
    " ", " ", " ",
    " ", "─", "─", " ",
    " ", " ", " ",
    " ", "─", "─", " ",
    " ", "─", "─", " ",
    " ", " ", " ",
    " ", "─", "─", " ",
};

inline constexpr Box ROUNDED = {
    "╭", "─", "┬", "╮",
    "│", "│", "│",
    "├", "─", "┼", "┤",
    "│", "│", "│",
    "├", "─", "┼", "┤",
    "├", "─", "┼", "┤",
    "│", "│", "│",
    "╰", "─", "┴", "╯",
};

inline constexpr Box HEAVY = {
    "┏", "━", "┳", "┓",
    "┃", "┃", "┃",
    "┣", "━", "╋", "┫",
    "┃", "┃", "┃",
    "┣", "━", "╋", "┫",
    "┣", "━", "╋", "┫",
    "┃", "┃", "┃",
    "┗", "━", "┻", "┛",
};

inline constexpr Box HEAVY_EDGE = {
    "┏", "━", "┯", "┓",
    "┃", "│", "┃",
    "┠", "─", "┼", "┨",
    "┃", "│", "┃",
    "┠", "─", "┼", "┨",
    "┠", "─", "┼", "┨",
    "┃", "│", "┃",
    "┗", "━", "┷", "┛",
};

inline constexpr Box HEAVY_HEAD = {
    "┏", "━", "┳", "┓",
    "┃", "┃", "┃",
    "┡", "━", "╇", "┩",
    "│", "│", "│",
    "├", "─", "┼", "┤",
    "├", "─", "┼", "┤",
    "│", "│", "│",
    "└", "─", "┴", "┘",
};

inline constexpr Box DOUBLE = {
    "╔", "═", "╦", "╗",
    "║", "║", "║",
    "╠", "═", "╬", "╣",
    "║", "║", "║",
    "╠", "═", "╬", "╣",
    "╠", "═", "╬", "╣",
    "║", "║", "║",
    "╚", "═", "╩", "╝",
};

inline constexpr Box DOUBLE_EDGE = {
    "╔", "═", "╤", "╗",
    "║", "│", "║",
    "╟", "─", "┼", "╢",
    "║", "│", "║",
    "╟", "─", "┼", "╢",
    "╟", "─", "┼", "╢",
    "║", "│", "║",
    "╚", "═", "╧", "╝",
};

inline constexpr Box MARKDOWN = {
    " ", " ", " ", " ",
    "|", "|", "|",
    "|", "-", "|", "|",
    "|", "|", "|",
    "|", "-", "|", "|",
    "|", "-", "|", "|",
    "|", "|", "|",
    " ", " ", " ", " ",
};

} // namespace rich::box
