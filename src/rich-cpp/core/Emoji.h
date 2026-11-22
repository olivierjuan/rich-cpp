#pragma once
// Emoji shortcode replacement (port of Python Rich's rich.emoji / _emoji_replace).
//
// Python semantics (_emoji_replace.py, regex `:(\S*?)(?:(?:\-)(emoji|text))?:`):
//   - `:name:` -> table lookup of name.lowercase(); unknown codes are left verbatim.
//   - A `-emoji` / `-text` suffix before the closing colon selects the variant
//     and appends U+FE0F / U+FE0E to the matched emoji.
//   - The name part stops at the first closing ':' (no whitespace allowed inside).
// The table lives in the generated EmojiTable.h (Python-parity, 3608 entries).
#include "EmojiTable.h"

#include <string>
#include <string_view>

namespace rich::emoji {

// Binary search by shortcode; returns nullptr when absent.
inline const char* lookup(std::string_view name) {
    size_t lo = 0, hi = kEmojiTable.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        std::string_view mid_name = kEmojiTable[mid].name;
        if (mid_name < name) lo = mid + 1;
        else if (mid_name > name) hi = mid;
        else return kEmojiTable[mid].value;
    }
    return nullptr;
}

// Python _emoji_replace(text): replace known :shortcode: tokens, leave the
// rest of the text (including unknown codes) byte-for-byte unchanged.
inline std::string replace(std::string_view text) {
    static constexpr std::string_view kVariantEmoji = "\xEF\xB8\x8F"; // U+FE0F
    static constexpr std::string_view kVariantText  = "\xEF\xB8\x8E"; // U+FE0E

    std::string out;
    out.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t colon = text.find(':', pos);
        if (colon == std::string_view::npos) {
            out.append(text.substr(pos));
            break;
        }
        // Scan to the first closing ':' (name must be whitespace-free).
        size_t end = colon + 1;
        bool whitespace = false;
        while (end < text.size() && text[end] != ':') {
            unsigned char c = static_cast<unsigned char>(text[end]);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
                whitespace = true;
                break;
            }
            ++end;
        }
        if (whitespace || end == text.size()) {
            // No closing colon (or whitespace inside): copy up to and past ':'.
            out.append(text.substr(pos, colon - pos + 1));
            pos = colon + 1;
            continue;
        }
        std::string_view body = text.substr(colon + 1, end - colon - 1);
        std::string_view variant;
        if (body.size() >= 6 && body.substr(body.size() - 6) == "-emoji") {
            variant = kVariantEmoji;
            body.remove_suffix(6);
        } else if (body.size() >= 5 && body.substr(body.size() - 5) == "-text") {
            variant = kVariantText;
            body.remove_suffix(5);
        }
        // Python lowercases the name before lookup (ASCII shortcodes only).
        std::string name(body);
        for (char& c : name)
            if (c >= 'A' && c <= 'Z') c += 32;
        const char* value = lookup(name);
        if (value != nullptr) {
            out.append(text.substr(pos, colon - pos)); // text before the code
            out += value;
            out += variant;
            pos = end + 1;
        } else {
            // Unknown code: Python consumes ":code:" verbatim and resumes
            // after the closing colon.
            out.append(text.substr(pos, end - pos + 1));
            pos = end + 1;
        }
    }
    return out;
}

} // namespace rich::emoji
