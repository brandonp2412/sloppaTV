#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

template <typename Measure>
std::string ellipsizeTextMeasured(
    std::string_view value,
    float maxWidth,
    size_t minimumPrefix,
    Measure&& measure
) {
    minimumPrefix = std::min(minimumPrefix, value.size());
    size_t low = minimumPrefix;
    size_t high = value.size();
    std::string candidate;
    candidate.reserve(value.size() + 3);
    const auto fits = [&](size_t length) {
        candidate.assign(value.substr(0, length));
        candidate.append("...");
        return measure(candidate) <= maxWidth;
    };
    if (!fits(low)) return candidate;
    while (low < high) {
        const size_t middle = low + (high - low + 1) / 2;
        if (fits(middle)) low = middle;
        else high = middle - 1;
    }
    candidate.assign(value.substr(0, low));
    candidate.append("...");
    return candidate;
}

template <typename Measure>
std::string fitSingleLineMeasured(std::string_view value, float maxWidth, Measure&& measure) {
    if (measure(value) <= maxWidth) return std::string(value);
    return ellipsizeTextMeasured(value, maxWidth, std::min<size_t>(4, value.size()), std::forward<Measure>(measure));
}

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
