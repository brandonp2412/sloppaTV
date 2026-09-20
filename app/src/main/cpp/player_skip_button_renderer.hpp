#pragma once

#include <cmath>
#include <string>
#include <string_view>

template <typename ColorLike> struct PlayerSkipButtonRenderStyle {
    ColorLike text{};
};

struct PlayerSkipButtonRenderState {
    std::string_view label;
    float y = 0.0f;
};

template <typename RendererLike, typename ColorLike, typename DrawButton, typename FitTextLines>
void renderPlayerSkipButton(RendererLike& renderer, const PlayerSkipButtonRenderState& state,
                            const PlayerSkipButtonRenderStyle<ColorLike>& style, DrawButton&& drawButton,
                            FitTextLines&& fitTextLines) {
    const auto bounds = drawButton(1480.0f, state.y, 320.0f, 74.0f, true, true);
    constexpr float labelScale = 1.82f;
    constexpr float iconWidth = 34.0f;
    constexpr float iconGap = 14.0f;
    const std::string fittedLabel = fitTextLines(state.label, labelScale, 224.0f, 1);
    const float labelWidth = renderer.textWidth(labelScale, fittedLabel);
    const float groupWidth = iconWidth + iconGap + labelWidth;
    const float iconX = std::round(bounds[0] + (bounds[2] - groupWidth) * 0.5f);
    const float iconCenterY = std::round(bounds[1] + bounds[3] * 0.5f);

    renderer.triangle(iconX, iconCenterY - 11.0f, iconX, iconCenterY + 11.0f, iconX + 13.0f, iconCenterY, style.text);
    renderer.triangle(iconX + 11.0f, iconCenterY - 11.0f, iconX + 11.0f, iconCenterY + 11.0f, iconX + 24.0f,
                      iconCenterY, style.text);
    renderer.roundedRect(iconX + 27.0f, iconCenterY - 12.0f, 4.0f, 24.0f, 2.0f, style.text);
    renderer.textVerticallyCentered(iconX + iconWidth + iconGap, bounds[1], bounds[3], labelScale, fittedLabel,
                                    style.text, labelWidth);
}
