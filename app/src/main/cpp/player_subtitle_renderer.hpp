#pragma once

#include "unicode_text.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

inline std::string normalizePlayerSubtitleText(std::string_view input) {
    const std::string displaySafe = displayText(input, '\0');
    const auto attachesToPrevious = [](std::string_view token) {
        if (token.empty()) return false;
        return std::all_of(token.begin(), token.end(), [](unsigned char c) {
            switch (c) {
            case '!':
            case '?':
            case '.':
            case ',':
            case ';':
            case ':':
            case '%':
            case ')':
            case ']':
            case '}':
                return true;
            default:
                return false;
            }
        });
    };

    std::istringstream words(displaySafe);
    std::vector<std::string> tokens;
    std::string word;
    while (words >> word) {
        if (!tokens.empty() && attachesToPrevious(word))
            tokens.back() += word;
        else
            tokens.push_back(std::move(word));
    }

    std::string output;
    for (const auto& token : tokens) {
        if (!output.empty()) output += ' ';
        output += token;
    }
    return output;
}

template <typename ColorLike> struct PlayerSubtitleRenderStyle {
    float cornerRadius = 0.0f;
    ColorLike text{};
    ColorLike background{};
    ColorLike outline{};
};

struct PlayerSubtitleRenderState {
    std::string_view text;
    float boxMaxWidth = 0.0f;
    float textScale = 1.0f;
    float lineHeight = 0.0f;
    float logicalWidth = 1920.0f;
    float bottomY = 0.0f;
    bool showBackground = true;
};

template <typename RendererLike, typename ColorLike, typename FitTextLines>
void renderPlayerSubtitle(RendererLike& renderer, const PlayerSubtitleRenderState& state,
                          const PlayerSubtitleRenderStyle<ColorLike>& style, FitTextLines&& fitTextLines) {
    if (state.text.empty() || state.boxMaxWidth <= 0.0f) return;

    constexpr float horizontalPadding = 32.0f;
    constexpr float verticalPadding = 20.0f;
    const float textMaxWidth = std::max(0.0f, state.boxMaxWidth - horizontalPadding * 2.0f);
    const std::string subtitle =
        fitTextLines(normalizePlayerSubtitleText(state.text), state.textScale, textMaxWidth, 3);

    std::istringstream stream(subtitle);
    std::vector<std::string> lines;
    std::string line;
    float widest = 0.0f;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        widest = std::max(widest, renderer.textWidth(state.textScale, line));
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back(subtitle);

    const float minimumBoxWidth = std::min(320.0f, state.boxMaxWidth);
    const float boxWidth = std::clamp(widest + horizontalPadding * 2.0f, minimumBoxWidth, state.boxMaxWidth);
    const float boxHeight = verticalPadding * 2.0f + state.lineHeight * static_cast<float>(lines.size());
    const float boxX = (state.logicalWidth - boxWidth) * 0.5f;
    const float boxY = state.bottomY - boxHeight;

    if (state.showBackground) {
        renderer.roundedRect(boxX, boxY, boxWidth, boxHeight, style.cornerRadius, style.background);
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float width = renderer.textWidth(state.textScale, lines[i]);
        const float textX = (state.logicalWidth - width) * 0.5f;
        const float textY = boxY + verticalPadding + static_cast<float>(i) * state.lineHeight;
        renderer.outlinedText(textX, textY, state.textScale, lines[i], style.text, style.outline, 0.0f);
    }
}
