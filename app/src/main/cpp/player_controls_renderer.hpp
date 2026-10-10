#pragma once

#include "player_screen.hpp"
#include "ui_icons.hpp"

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
            ui_icons::skip(renderer, iconCenterX - 20.0f, iconCenterY - 20.0f, 40.0f, i == 2, style.text);
        } else if (i == 1) {
            renderer.icon(state.paused ? ui_icons::Glyph::Play : ui_icons::Glyph::Pause, iconCenterX - 20.0f,
                          iconCenterY - 20.0f, 40.0f, style.text);
        } else if (i == 3) {
            constexpr float iconWidth = 36.0f;
            constexpr float gap = 14.0f;
            const std::string label = "Audio  " + std::string(materialLabel(state.audioTrackLabel));
            const float textAvailableWidth = std::max(1.0f, bounds[2] - 78.0f);
            const float labelScale = fitScale(1.45f, label, textAvailableWidth, bounds[3] - 8.0f);
            const float textWidth = renderer.textWidth(labelScale, label);
            const float groupWidth = iconWidth + gap + textWidth;
            const float iconX = bounds[0] + (bounds[2] - groupWidth) * 0.5f;
            ui_icons::audio(renderer, iconX, iconCenterY - 18.0f, iconWidth, style.text);
            renderer.textVerticallyCentered(iconX + iconWidth + gap, bounds[1], bounds[3], labelScale, label,
                                            style.text);
        } else {
            constexpr float iconWidth = 36.0f;
            constexpr float gap = 14.0f;
            const std::string label = "Subtitles  " + std::string(materialLabel(state.subtitleTrackLabel));
            const float textAvailableWidth = std::max(1.0f, bounds[2] - 86.0f);
            const float labelScale = fitScale(1.45f, label, textAvailableWidth, bounds[3] - 8.0f);
            const float textWidth = renderer.textWidth(labelScale, label);
            const float groupWidth = iconWidth + gap + textWidth;
            const float iconX = bounds[0] + (bounds[2] - groupWidth) * 0.5f;
            ui_icons::subtitles(renderer, iconX, iconCenterY - 18.0f, iconWidth, style.text);
            renderer.textVerticallyCentered(iconX + iconWidth + gap, bounds[1], bounds[3], labelScale, label,
                                            style.text);
        }

        x += controlWidths[i] + controlGap;
    }
}
