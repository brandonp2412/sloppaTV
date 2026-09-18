#pragma once

#include <string>
#include <string_view>

template <typename ColorLike> struct PlayerHeaderRenderStyle {
    ColorLike text{};
    ColorLike muted{};
};

struct PlayerHeaderRenderState {
    std::string heading;
    std::string secondary;
    bool showNextUp = false;
    float headlineScale = 0.0f;
    float secondaryY = 0.0f;
};

template <typename RendererLike, typename ColorLike, typename FitTextLines>
void renderPlayerHeader(RendererLike& renderer, const PlayerHeaderRenderState& state,
                        const PlayerHeaderRenderStyle<ColorLike>& style, FitTextLines&& fitTextLines) {
    const std::string_view playbackHeading = state.heading.empty() ? std::string_view{"Playback"} : state.heading;
    const float headingWidth = state.showNextUp ? 1040.0f : 1460.0f;
    renderer.text(80.0f, 42.0f, state.headlineScale,
                  fitTextLines(playbackHeading, state.headlineScale, headingWidth, 1), style.text, headingWidth);

    if (state.secondary.empty() || state.secondary == state.heading) return;

    const float secondaryWidth = state.showNextUp ? 1040.0f : 1500.0f;
    renderer.text(80.0f, state.secondaryY, 2.6f, fitTextLines(state.secondary, 2.6f, secondaryWidth, 1), style.muted,
                  secondaryWidth);
}
