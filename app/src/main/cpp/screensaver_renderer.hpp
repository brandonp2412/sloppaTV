#pragma once

#include "screensaver_policy.hpp"

#include <array>
#include <cstdint>
#include <string_view>

template <typename ColorLike>
struct ScreensaverRenderStyle {
    ColorLike background{};
    ColorLike primary{};
    ColorLike text{};
    ColorLike tertiary{};
};

template <typename RendererLike, typename ColorLike, typename DrawLeftAligned, typename DrawCentered>
void renderScreensaverScreen(RendererLike& renderer, int64_t elapsedSeconds, std::string_view clock,
                             float canvasWidth, float canvasHeight, const ScreensaverRenderStyle<ColorLike>& style,
                             DrawLeftAligned&& drawLeftAligned, DrawCentered&& drawCentered) {
    renderer.rect(0.0f, 0.0f, canvasWidth, canvasHeight, style.background);
    static constexpr std::array<std::array<float, 2>, 8> positions{{
        {{170.0f, 170.0f}},
        {{1120.0f, 170.0f}},
        {{170.0f, 675.0f}},
        {{1120.0f, 675.0f}},
        {{650.0f, 245.0f}},
        {{650.0f, 635.0f}},
        {{340.0f, 410.0f}},
        {{980.0f, 410.0f}},
    }};
    const auto& position = positions[static_cast<std::size_t>(screensaverPositionSlot(elapsedSeconds))];

    drawLeftAligned(position[0], position[1], 600.0f, 104.0f, 4.2f, "sloppaTV", style.primary);
    drawLeftAligned(position[0], position[1] + 112.0f, 700.0f, 205.0f, 9.0f, clock, style.text);
    drawCentered(600.0f, 1008.0f, 720.0f, 48.0f, 1.45f, "Press any button to return", style.tertiary, 12.0f,
                 4.0f);
}
