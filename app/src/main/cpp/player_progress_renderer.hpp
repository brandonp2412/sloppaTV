#pragma once

#include "ui_policy.hpp"

#include <algorithm>
#include <string_view>

template <typename ColorLike> struct PlayerProgressRenderStyle {
    ColorLike text{};
    ColorLike track{};
    ColorLike focus{};
};

struct PlayerProgressRenderState {
    int positionMs = 0;
    int durationMs = 0;
    bool skipButtonVisible = false;
    std::string_view positionText;
    std::string_view durationText;
};

template <typename RendererLike, typename ColorLike, typename DrawRightAligned>
void renderPlayerProgress(RendererLike& renderer, const PlayerProgressRenderState& state,
                          const PlayerProgressRenderStyle<ColorLike>& style, DrawRightAligned&& drawRightAligned) {
    constexpr float progressX = 150.0f;
    constexpr float progressWidth = 1620.0f;
    renderer.text(progressX, 834.0f, 2.0f, state.positionText, style.text);
    drawRightAligned(playbackDurationRightX(state.skipButtonVisible), 834.0f, 2.0f, state.durationText, style.text,
                     220.0f);

    constexpr float progressTrackY = 890.0f;
    renderer.roundedRect(progressX, progressTrackY, progressWidth, 7.0f, 3.5f, style.track);
    if (state.durationMs <= 0) return;

    const float progress =
        std::clamp(static_cast<float>(state.positionMs) / static_cast<float>(state.durationMs), 0.0f, 1.0f);
    const float progressPixels = progressWidth * progress;
    renderer.roundedRect(progressX, progressTrackY, progressPixels, 7.0f, 3.5f, style.focus);
    const float thumbCenterX = playbackProgressThumbCenterX(progressX, progressWidth, progress, 9.0f);
    renderer.roundedRect(thumbCenterX - 9.0f, progressTrackY - 5.5f, 18.0f, 18.0f, 9.0f, style.text);
}
