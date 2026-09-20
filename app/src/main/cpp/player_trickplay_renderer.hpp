#pragma once

#include "jellyfin_types.hpp"
#include "trickplay_policy.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>

template <typename ColorLike> struct PlayerTrickplayRenderStyle {
    float previewRadius = 0.0f;
    ColorLike backdrop{};
    ColorLike focus{};
    ColorLike text{};
};

struct PlayerTrickplayRenderState {
    uint32_t texture = 0;
    const TrickplayFrame& frame;
    const JellyfinTrickplayInfo& info;
    int decodedWidth = 0;
    int decodedHeight = 0;
    int positionMs = 0;
    int durationMs = 0;
    float logicalWidth = 0.0f;
    std::string_view positionLabel;
};

template <typename RendererLike, typename ColorLike, typename DrawLeftAligned>
bool renderPlayerTrickplay(RendererLike& renderer, const PlayerTrickplayRenderState& state,
                           const PlayerTrickplayRenderStyle<ColorLike>& style, DrawLeftAligned&& drawLeftAligned) {
    if (state.texture == 0 || !state.frame.valid() || !state.info.valid()) return false;

    constexpr float previewWidth = 420.0f;
    if (state.logicalWidth < previewWidth) return false;
    const float previewHeight = std::clamp(
        previewWidth * static_cast<float>(state.info.height) / static_cast<float>(state.info.width), 180.0f, 270.0f);
    const double progress =
        state.durationMs > 0 ? std::clamp(static_cast<double>(state.positionMs) / state.durationMs, 0.0, 1.0) : 0.5;
    const float centerX = 155.0f + static_cast<float>(1610.0 * progress);
    const float horizontalMargin = std::min(80.0f, (state.logicalWidth - previewWidth) * 0.5f);
    const float maxX = state.logicalWidth - horizontalMargin - previewWidth;
    const float x = std::clamp(centerX - previewWidth * 0.5f, horizontalMargin, maxX);
    constexpr float y = 555.0f;

    const TrickplayUvRegion uv =
        trickplayUvRegion(state.frame, state.info.width, state.info.height, state.decodedWidth, state.decodedHeight);
    if (!uv.valid()) return false;

    renderer.roundedRect(x - 7.0f, y - 7.0f, previewWidth + 14.0f, previewHeight + 58.0f, style.previewRadius + 7.0f,
                         style.backdrop);
    renderer.roundedOutline(x - 7.0f, y - 7.0f, previewWidth + 14.0f, previewHeight + 58.0f, style.previewRadius + 7.0f,
                            4.0f, style.focus);
    renderer.roundedImageRegion(state.texture, x, y, previewWidth, previewHeight, style.previewRadius, uv.u0, uv.v0,
                                uv.u1, uv.v1);
    drawLeftAligned(x + 14.0f, y + previewHeight + 7.0f, previewWidth - 28.0f, 44.0f, 1.65f, state.positionLabel,
                    style.text);
    return true;
}
