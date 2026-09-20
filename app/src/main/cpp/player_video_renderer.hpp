#pragma once

#include "app_settings.hpp"

#include <algorithm>
#include <cstdint>

struct PlayerVideoRenderState {
    uint32_t texture = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    VideoZoomMode zoomMode = VideoZoomMode::Fit;
    float logicalWidth = 0.0f;
    float logicalHeight = 0.0f;
};

struct PlayerVideoBounds {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

inline PlayerVideoBounds playerVideoBounds(const PlayerVideoRenderState& state) {
    PlayerVideoBounds bounds{
        .x = 0.0f,
        .y = 0.0f,
        .width = state.logicalWidth,
        .height = state.logicalHeight,
    };
    if (state.sourceWidth <= 0 || state.sourceHeight <= 0 || state.zoomMode == VideoZoomMode::Stretch) return bounds;

    const float widthScale = state.logicalWidth / static_cast<float>(state.sourceWidth);
    const float heightScale = state.logicalHeight / static_cast<float>(state.sourceHeight);
    const float scale =
        state.zoomMode == VideoZoomMode::Fit ? std::min(widthScale, heightScale) : std::max(widthScale, heightScale);
    bounds.width = static_cast<float>(state.sourceWidth) * scale;
    bounds.height = static_cast<float>(state.sourceHeight) * scale;
    bounds.x = (state.logicalWidth - bounds.width) * 0.5f;
    bounds.y = (state.logicalHeight - bounds.height) * 0.5f;
    return bounds;
}

template <typename RendererLike, typename TransformLike>
void renderPlayerVideo(RendererLike& renderer, const PlayerVideoRenderState& state, const TransformLike& transform) {
    const PlayerVideoBounds bounds = playerVideoBounds(state);
    renderer.externalImage(state.texture, bounds.x, bounds.y, bounds.width, bounds.height, transform);
}
