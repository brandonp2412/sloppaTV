#pragma once

#include "player_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string>
#include <string_view>

template <typename ColorLike> struct PlayerControlsRenderStyle {
    ColorLike text{};
};

struct PlayerControlsRenderState {
    bool paused = false;
    PlayerControl selection = PlayerControl::PlayPause;
    float logicalWidth = 1920.0f;
    std::string audioTrackLabel;
    std::string subtitleTrackLabel;
};

template <typename RendererLike, typename ColorLike, typename DrawButton, typename FitScale, typename MaterialLabel>
void renderPlayerControls(RendererLike& renderer, const PlayerControlsRenderState& state,
                          const PlayerControlsRenderStyle<ColorLike>& style, DrawButton&& drawButton,
                          FitScale&& fitScale, MaterialLabel&& materialLabel) {
    constexpr std::array<float, 5> controlWidths{112.0f, 112.0f, 112.0f, 330.0f, 370.0f};
    constexpr float controlHeight = 66.0f;
    constexpr float controlGap = 18.0f;
    constexpr float controlY = 925.0f;
    const float controlGroupWidth = std::accumulate(controlWidths.begin(), controlWidths.end(), 0.0f) +
                                    controlGap * static_cast<float>(controlWidths.size() - 1);
    float x = (state.logicalWidth - controlGroupWidth) * 0.5f;

    for (std::size_t i = 0; i < controlWidths.size(); ++i) {
        const bool selected = i == static_cast<std::size_t>(state.selection);
        const auto bounds = drawButton(x, controlY, controlWidths[i], controlHeight, selected, i == 1);
        const float iconCenterX = std::round(bounds[0] + bounds[2] * 0.5f);
        const float iconCenterY = std::round(bounds[1] + bounds[3] * 0.5f);

        if (i == 0 || i == 2) {
            const bool forward = i == 2;
            const float center = iconCenterX;
            if (forward) {
                renderer.triangle(center - 12.0f, iconCenterY - 14.0f, center - 12.0f, iconCenterY + 14.0f,
                                  center + 10.0f, iconCenterY, style.text);
                renderer.roundedRect(center + 13.0f, iconCenterY - 14.0f, 4.0f, 28.0f, 2.0f, style.text);
            } else {
                renderer.roundedRect(center - 17.0f, iconCenterY - 14.0f, 4.0f, 28.0f, 2.0f, style.text);
                renderer.triangle(center + 12.0f, iconCenterY - 14.0f, center + 12.0f, iconCenterY + 14.0f,
                                  center - 10.0f, iconCenterY, style.text);
            }
        } else if (i == 1) {
            if (state.paused) {
                const float playLeft = iconCenterX - 22.0f / 3.0f;
                renderer.triangle(playLeft, iconCenterY - 13.0f, playLeft, iconCenterY + 13.0f, playLeft + 22.0f,
                                  iconCenterY, style.text);
            } else {
                const float pauseLeft = iconCenterX - 10.0f;
                renderer.roundedRect(pauseLeft, iconCenterY - 13.0f, 7.0f, 26.0f, 3.0f, style.text);
                renderer.roundedRect(pauseLeft + 13.0f, iconCenterY - 13.0f, 7.0f, 26.0f, 3.0f, style.text);
            }
        } else if (i == 3) {
            constexpr float iconWidth = 36.0f;
            constexpr float gap = 14.0f;
            const std::string label = "Audio  " + std::string(materialLabel(state.audioTrackLabel));
            const float textAvailableWidth = std::max(1.0f, bounds[2] - 78.0f);
            const float labelScale = fitScale(1.45f, label, textAvailableWidth, bounds[3] - 8.0f);
            const float textWidth = renderer.textWidth(labelScale, label);
            const float groupWidth = iconWidth + gap + textWidth;
            const float iconX = bounds[0] + (bounds[2] - groupWidth) * 0.5f;
            renderer.roundedRect(iconX, iconCenterY - 8.0f, 8.0f, 16.0f, 2.0f, style.text);
            renderer.triangle(iconX + 8.0f, iconCenterY - 8.0f, iconX + 8.0f, iconCenterY + 8.0f, iconX + 20.0f,
                              iconCenterY + 15.0f, style.text);
            renderer.roundedRect(iconX + 24.0f, iconCenterY - 10.0f, 4.0f, 20.0f, 2.0f, style.text);
            renderer.roundedRect(iconX + 31.0f, iconCenterY - 15.0f, 4.0f, 30.0f, 2.0f, style.text);
            renderer.textVerticallyCentered(iconX + iconWidth + gap, bounds[1], bounds[3], labelScale, label,
                                            style.text);
        } else {
            constexpr float iconWidth = 42.0f;
            constexpr float gap = 14.0f;
            const std::string label = "Subtitles  " + std::string(materialLabel(state.subtitleTrackLabel));
            const float textAvailableWidth = std::max(1.0f, bounds[2] - 86.0f);
            const float labelScale = fitScale(1.45f, label, textAvailableWidth, bounds[3] - 8.0f);
            const float textWidth = renderer.textWidth(labelScale, label);
            const float groupWidth = iconWidth + gap + textWidth;
            const float iconX = bounds[0] + (bounds[2] - groupWidth) * 0.5f;
            renderer.roundedOutline(iconX, iconCenterY - 13.0f, 38.0f, 26.0f, 6.0f, 2.0f, style.text);
            renderer.textCentered(iconX, iconCenterY - 13.0f, 38.0f, 26.0f, 0.82f, "CC", style.text);
            renderer.textVerticallyCentered(iconX + iconWidth + gap, bounds[1], bounds[3], labelScale, label,
                                            style.text);
        }

        x += controlWidths[i] + controlGap;
    }
}
