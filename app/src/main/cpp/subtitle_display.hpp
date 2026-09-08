#pragma once

#include "unicode_text.hpp"

#include <algorithm>
#include <string>
#include <string_view>

inline constexpr bool subtitleDisplayWhitespace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

inline constexpr bool subtitleDisplayPunctuation(unsigned char value) {
    switch (value) {
        case '!': case '?': case '.': case ',': case ';': case ':':
        case '%': case ')': case ']': case '}':
            return true;
        default:
            return false;
    }
}

inline std::string normalizeSubtitleDisplayText(std::string_view input) {
    std::string transformed;
    std::string_view displaySafe = input;
    if (std::any_of(input.begin(), input.end(), [](unsigned char byte) { return byte >= 0x80; })) {
        transformed = displayText(input, '\0');
        displaySafe = transformed;
    }

    std::string output;
    output.reserve(displaySafe.size());
    size_t index = 0;
    while (index < displaySafe.size()) {
        while (index < displaySafe.size() && subtitleDisplayWhitespace(static_cast<unsigned char>(displaySafe[index]))) ++index;
        if (index == displaySafe.size()) break;
        const size_t start = index;
        bool attachesToPrevious = true;
        while (index < displaySafe.size() && !subtitleDisplayWhitespace(static_cast<unsigned char>(displaySafe[index]))) {
            attachesToPrevious = attachesToPrevious
                && subtitleDisplayPunctuation(static_cast<unsigned char>(displaySafe[index]));
            ++index;
        }
        if (!output.empty() && !attachesToPrevious) output.push_back(' ');
        output.append(displaySafe.substr(start, index - start));
    }
    return output;
}
