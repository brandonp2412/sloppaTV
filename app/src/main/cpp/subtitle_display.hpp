#pragma once

#include "unicode_text.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

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

inline void splitSubtitleDisplayLines(std::string_view text, std::vector<std::string>& lines) {
    lines.clear();
    size_t start = 0;
    while (start < text.size()) {
        const size_t end = text.find('\n', start);
        const size_t length = (end == std::string_view::npos ? text.size() : end) - start;
        if (length > 0) lines.emplace_back(text.substr(start, length));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    if (lines.empty()) lines.emplace_back(text);
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
