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

inline bool playerVideoHasBars(const PlayerVideoRenderState& state) {
    if (state.zoomMode != VideoZoomMode::Fit) return false;
    const PlayerVideoBounds bounds = playerVideoBounds(state);
    return bounds.x > 0.5f || bounds.y > 0.5f || bounds.x + bounds.width < state.logicalWidth - 0.5f ||
           bounds.y + bounds.height < state.logicalHeight - 0.5f;
}

template <typename RendererLike, typename ColorLike>
void renderPlayerAmbientBars(RendererLike& renderer, const PlayerVideoRenderState& state, ColorLike color) {
    if (!playerVideoHasBars(state)) return;
    const PlayerVideoBounds bounds = playerVideoBounds(state);
    if (bounds.x > 0.0f) renderer.rect(0.0f, 0.0f, bounds.x, state.logicalHeight, color);
    const float right = bounds.x + bounds.width;
    if (right < state.logicalWidth) {
        renderer.rect(right, 0.0f, state.logicalWidth - right, state.logicalHeight, color);
    }
    if (bounds.y > 0.0f) renderer.rect(0.0f, 0.0f, state.logicalWidth, bounds.y, color);
    const float bottom = bounds.y + bounds.height;
    if (bottom < state.logicalHeight) {
        renderer.rect(0.0f, bottom, state.logicalWidth, state.logicalHeight - bottom, color);
    }
}

template <typename RendererLike, typename TransformLike>
void renderPlayerVideo(RendererLike& renderer, const PlayerVideoRenderState& state, const TransformLike& transform) {
    const PlayerVideoBounds bounds = playerVideoBounds(state);
    renderer.externalImage(state.texture, bounds.x, bounds.y, bounds.width, bounds.height, transform);
}
