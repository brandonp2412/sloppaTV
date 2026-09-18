#pragma once

struct PlayerSeekFeedbackRenderState {
    int seconds = 0;
    float fade = 0.0f;
};

template <typename RendererLike, typename MakeColor>
void renderPlayerSeekFeedback(RendererLike& renderer, const PlayerSeekFeedbackRenderState& state,
                              MakeColor&& makeColor) {
    const bool forward = state.seconds > 0;
    const auto wash = makeColor(1.0f, 1.0f, 1.0f, 0.09f * state.fade);
    const auto glyph = makeColor(1.0f, 1.0f, 1.0f, 0.72f * state.fade);
    constexpr float ovalWidth = 420.0f;
    constexpr float ovalHeight = 300.0f;
    const float ovalX = forward ? 1675.0f : -175.0f;
    constexpr float ovalY = 350.0f;
    renderer.roundedRect(ovalX, ovalY, ovalWidth, ovalHeight, ovalHeight * 0.5f, wash);

    const float centerX = forward ? 1740.0f : 180.0f;
    constexpr float centerY = 500.0f;
    constexpr float arrowGap = 20.0f;
    for (int arrow = 0; arrow < 2; ++arrow) {
        const float offset = (static_cast<float>(arrow) - 0.5f) * arrowGap;
        if (forward) {
            const float x = centerX + 54.0f + offset;
            renderer.triangle(x - 8.0f, centerY - 13.0f, x - 8.0f, centerY + 13.0f, x + 9.0f, centerY, glyph);
        } else {
            const float x = centerX - 54.0f + offset;
            renderer.triangle(x + 8.0f, centerY - 13.0f, x + 8.0f, centerY + 13.0f, x - 9.0f, centerY, glyph);
        }
    }
}
