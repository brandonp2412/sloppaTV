#pragma once

#include "ui_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

template <typename ColorLike> struct PlayerProgressRenderStyle {
    ColorLike text{};
    ColorLike track{};
    ColorLike focus{};
};

struct PlayerProgressRenderState {
    int positionMs = 0;
    int durationMs = 0;
    int bufferAheadMs = 0;
    bool skipButtonVisible = false;
    bool startupLoading = false;
    std::optional<float> startupProgress = std::nullopt;
    std::int64_t animationTimeMs = 0;
    std::string_view positionText{};
    std::string_view durationText{};
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
    if (state.startupLoading) {
        // Only this small section is animated; the seek thumb is hidden.
        constexpr float startupWidth = 150.0f;
        constexpr float pi = 3.14159265358979323846f;
        const float phase = static_cast<float>(state.animationTimeMs % 1400) / 1400.0f;
        const float pulse = 0.5f + 0.5f * std::sin(2.0f * pi * phase);
        if (!state.startupProgress) {
            renderer.roundedRect(progressX, progressTrackY, 45.0f + 75.0f * pulse, 7.0f, 3.5f, style.focus);
        } else {
            const float filled = startupWidth * std::clamp(*state.startupProgress, 0.0f, 1.0f);
            if (filled > 0.0f) renderer.roundedRect(progressX, progressTrackY, filled, 7.0f, 3.5f, style.focus);
            // Glint remains inside verified contiguous bytes, never fake progress.
            if (filled > 0.0f) {
                const float glintWidth = std::min(filled, 8.0f + 14.0f * pulse);
                renderer.roundedRect(progressX + filled - glintWidth, progressTrackY, glintWidth, 7.0f, 3.5f,
                                     style.text);
            } else {
                renderer.roundedRect(progressX, progressTrackY, 12.0f + 12.0f * pulse, 7.0f, 3.5f, style.focus);
            }
        }
        return;
    }
    if (state.durationMs <= 0) return;

    const float progress =
        std::clamp(static_cast<float>(state.positionMs) / static_cast<float>(state.durationMs), 0.0f, 1.0f);
    const float progressPixels = progressWidth * progress;
    const float bufferedEnd = std::clamp(static_cast<float>(state.positionMs + std::max(0, state.bufferAheadMs)) /
                                             static_cast<float>(state.durationMs),
                                         progress, 1.0f);
    const float bufferedPixels = progressWidth * bufferedEnd;
    if (bufferedPixels > progressPixels)
        renderer.roundedRect(progressX + progressPixels, progressTrackY, bufferedPixels - progressPixels, 7.0f, 3.5f,
                             style.text);
    renderer.roundedRect(progressX, progressTrackY, progressPixels, 7.0f, 3.5f, style.focus);
    const float thumbCenterX = playbackProgressThumbCenterX(progressX, progressWidth, progress, 9.0f);
    renderer.roundedRect(thumbCenterX - 9.0f, progressTrackY - 5.5f, 18.0f, 18.0f, 9.0f, style.text);
}
