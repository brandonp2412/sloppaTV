#pragma once

#include "player_buffer_indicator.hpp"

#include <algorithm>
#include <string>
#include <string_view>

// This compact wait message is rendered independently of the timed playback controls.
// Subtitle downloads can outlast their initial four-second button feedback.
inline std::string playerWaitStatus(bool resolving, bool preparing, bool subtitleBusy,
                                    const PlayerBufferIndicator& buffer,
                                    PlayerBufferIndicator::TimePoint now) {
    if (resolving || preparing) return buffer.statusText(now);
    if (subtitleBusy) return buffer.interrupted() ? "Loading subtitles; " + buffer.statusText(now)
                                                 : "Loading subtitles";
    if (buffer.interrupted()) return buffer.statusText(now);
    return {};
}

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
