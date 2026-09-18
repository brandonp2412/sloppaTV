#pragma once

#include "unicode_text.hpp"

#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

template <typename RendererLike>
std::string fitRenderedTextLines(const RendererLike& renderer, std::string_view value, float scale, float maxWidth,
                                 int maxLines) {
    if (value.empty() || maxWidth <= 0.0f || maxLines <= 0) return {};
    const std::string displayValue = displayText(value);
    auto ellipsize = [&](std::string line) {
        while (!line.empty() && renderer.textWidth(scale, line + "...") > maxWidth) line.pop_back();
        return line + "...";
    };
    std::istringstream words(displayValue);
    std::string word;
    std::string current;
    std::string fitted;
    int line = 1;
    while (words >> word) {
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (renderer.textWidth(scale, candidate) <= maxWidth) {
            current = candidate;
            continue;
        }
        if (current.empty()) {
            if (!fitted.empty()) fitted += '\n';
            fitted += ellipsize(word);
            if (line >= maxLines) return fitted;
            ++line;
            current.clear();
            continue;
        }
        if (line >= maxLines) {
            if (!fitted.empty()) fitted += '\n';
            fitted += ellipsize(current);
            return fitted;
        }
        if (!fitted.empty()) fitted += '\n';
        fitted += current;
        current = word;
        ++line;
    }
    if (!current.empty()) {
        if (!fitted.empty()) fitted += '\n';
        fitted += renderer.textWidth(scale, current) <= maxWidth ? current : ellipsize(current);
    }
    return fitted;
}

template <typename RendererLike, typename ColorLike>
void renderLingeringTitle(RendererLike& renderer, float x, float y, float scale, std::string_view value, float maxWidth,
                          ColorLike color, std::chrono::steady_clock::time_point lastInteraction,
                          std::chrono::steady_clock::time_point now) {
    const std::string displayValue = displayText(value);
    if (displayValue.empty()) return;
    const float titleWidth = renderer.textWidth(scale, displayValue);
    if (titleWidth <= maxWidth) {
        renderer.text(x, y, scale, displayValue, color, maxWidth);
        return;
    }

    constexpr auto linger = std::chrono::milliseconds(1200);
    const auto marqueeStart = lastInteraction + linger;
    if (now < marqueeStart) {
        renderer.text(x, y, scale, fitRenderedTextLines(renderer, displayValue, scale, maxWidth, 1), color, maxWidth);
        return;
    }

    constexpr float speedPixelsPerSecond = 28.0f;
    const std::string gap = "      ";
    const float cycleWidth = titleWidth + renderer.textWidth(scale, gap);
    const float elapsedSeconds = std::chrono::duration<float>(now - marqueeStart).count();
    const float offset = std::fmod(elapsedSeconds * speedPixelsPerSecond, cycleWidth);
    const std::string track = displayValue + gap + displayValue;
    renderer.beginClipRect(x, y - 4.0f, maxWidth, 64.0f);
    renderer.text(x - offset, y, scale, track, color);
    renderer.endClipRect();
}
