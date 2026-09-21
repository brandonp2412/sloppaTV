#pragma once

#include "player_seek_feedback_renderer.hpp"
#include "player_skip_button_renderer.hpp"
#include "player_subtitle_renderer.hpp"

#include <string_view>

struct PlayerOverlayRenderState {
    bool showOverlay = false;
    std::string_view subtitleText;
    float subtitleBoxMaxWidth = 0.0f;
    float subtitleTextScale = 1.0f;
    float subtitleLineHeight = 0.0f;
    float logicalWidth = 1920.0f;
    float subtitleBottomY = 0.0f;
    bool subtitleBackground = true;
    bool skipButtonVisible = false;
    std::string_view skipLabel;
    float skipButtonY = 0.0f;
    bool seekFeedbackVisible = false;
    int seekFeedbackSeconds = 0;
    float seekFeedbackAlpha = 0.0f;
};

template <typename ColorLike> struct PlayerOverlayRenderStyle {
    float subtitleCornerRadius = 0.0f;
    ColorLike text{};
};

template <typename RendererLike, typename ColorLike, typename FitTextLines, typename DrawButton, typename MakeColor>
void renderPlayerOverlay(RendererLike& renderer, const PlayerOverlayRenderState& state,
                         const PlayerOverlayRenderStyle<ColorLike>& style, FitTextLines&& fitTextLines,
                         DrawButton&& drawButton, MakeColor&& makeColor) {
    if (state.showOverlay) {
        renderer.verticalGradient(0.0f, 0.0f, 1920.0f, 250.0f, makeColor(0.0f, 0.0f, 0.0f, 0.74f),
                                  makeColor(0.0f, 0.0f, 0.0f, 0.0f));
        renderer.verticalGradient(0.0f, 650.0f, 1920.0f, 430.0f, makeColor(0.0f, 0.0f, 0.0f, 0.0f),
                                  makeColor(0.0f, 0.0f, 0.0f, 0.90f));
    }

    if (!state.subtitleText.empty()) {
        renderPlayerSubtitle(
            renderer,
            PlayerSubtitleRenderState{
                .text = state.subtitleText,
                .boxMaxWidth = state.subtitleBoxMaxWidth,
                .textScale = state.subtitleTextScale,
                .lineHeight = state.subtitleLineHeight,
                .logicalWidth = state.logicalWidth,
                .bottomY = state.subtitleBottomY,
                .showBackground = state.subtitleBackground,
            },
            PlayerSubtitleRenderStyle<ColorLike>{
                .cornerRadius = style.subtitleCornerRadius,
                .text = style.text,
                .background = makeColor(0.0f, 0.0f, 0.0f, 0.80f),
                .outline = makeColor(0.0f, 0.0f, 0.0f, 0.92f),
            },
            fitTextLines);
    }

    if (state.skipButtonVisible) {
        renderPlayerSkipButton(
            renderer, PlayerSkipButtonRenderState{.label = state.skipLabel, .y = state.skipButtonY},
            PlayerSkipButtonRenderStyle<ColorLike>{.text = style.text}, drawButton, fitTextLines);
    }

    if (state.seekFeedbackVisible) {
        renderPlayerSeekFeedback(
            renderer,
            PlayerSeekFeedbackRenderState{
                .seconds = state.seekFeedbackSeconds,
                .fade = state.seekFeedbackAlpha,
            },
            makeColor);
    }
}
