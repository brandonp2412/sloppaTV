#pragma once

#include <algorithm>
#include <string_view>

template <typename ColorLike> struct PlayerStatusRenderStyle {
    ColorLike muted{};
    ColorLike secondary{};
};

struct PlayerStatusRenderState {
    std::string_view clockText;
    std::string_view finishText;
    std::string_view statusText;
};

template <typename RendererLike, typename ColorLike, typename DrawRightAligned>
void renderPlayerStatus(RendererLike& renderer, const PlayerStatusRenderState& state,
                        const PlayerStatusRenderStyle<ColorLike>& style, DrawRightAligned&& drawRightAligned) {
    if (!state.clockText.empty()) {
        drawRightAligned(1840.0f, 46.0f, 2.05f, state.clockText, style.muted, 210.0f);
    }
    if (!state.finishText.empty()) {
        const float finishWidth = renderer.textWidth(1.75f, state.finishText);
        renderer.text(std::max(1180.0f, 1770.0f - finishWidth), 772.0f, 1.75f, state.finishText, style.secondary,
                      590.0f);
    }
    if (!state.statusText.empty()) {
        renderer.text(80.0f, 772.0f, 2.0f, state.statusText, style.secondary, 580.0f);
    }
}
