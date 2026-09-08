#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

template <typename Measure>
std::string fitTextLinesMeasured(
    std::string_view value,
    float maxWidth,
    int maxLines,
    Measure&& measure
) {
    if (value.empty() || maxWidth <= 0.0f || maxLines <= 0) return {};
    auto ellipsize = [&](std::string line) {
        line.append("...");
        while (line.size() > 3 && measure(line) > maxWidth) {
            line.erase(line.end() - 4);
        }
        return line;
    };

    const float spaceWidth = measure(" ");
    std::string current;
    std::string fitted;
    current.reserve(std::min<size_t>(value.size(), 256));
    fitted.reserve(value.size() + 4);
    float currentWidth = 0.0f;
    int line = 1;
    size_t position = 0;
    while (position < value.size()) {
        while (position < value.size()
            && std::isspace(static_cast<unsigned char>(value[position]))) ++position;
        if (position >= value.size()) break;
        const size_t start = position;
        while (position < value.size()
            && !std::isspace(static_cast<unsigned char>(value[position]))) ++position;
        const std::string_view word(value.data() + start, position - start);
        const float wordWidth = measure(word);
        const size_t previousSize = current.size();
        const float previousWidth = currentWidth;
        if (previousSize > 0) current.push_back(' ');
        current.append(word);
        currentWidth = previousWidth + (previousSize > 0 ? spaceWidth : 0.0f) + wordWidth;
        if (currentWidth <= maxWidth) continue;

        current.resize(previousSize);
        currentWidth = previousWidth;
        if (current.empty()) {
            current = ellipsize(std::string(word));
            currentWidth = measure(current);
        }
        if (line >= maxLines) {
            if (!fitted.empty()) fitted += '\n';
            fitted += ellipsize(current);
            return fitted;
        }
        if (!fitted.empty()) fitted += '\n';
        fitted += current;
        current.assign(word);
        currentWidth = wordWidth;
        ++line;
    }
    if (!current.empty()) {
        if (!fitted.empty()) fitted += '\n';
        fitted += currentWidth <= maxWidth ? current : ellipsize(current);
    }
    return fitted;
}
