#pragma once

#include <string>
#include <string_view>

template <typename ColorLike> struct ScreenChromeRenderStyle {
    float pageInset = 0.0f;
    float supportingScale = 0.0f;
    float headlineScale = 0.0f;
    float titleScale = 0.0f;
    float labelScale = 0.0f;
    float cornerLarge = 0.0f;
    ColorLike muted{};
    ColorLike text{};
    ColorLike panelAlt{};
};

template <typename RendererLike, typename ColorLike, typename FitText, typename DrawRightAligned, typename FormatClock>
void renderScreenHeader(RendererLike& renderer, std::string_view title, bool showClock, bool clock24Hour,
                        const ScreenChromeRenderStyle<ColorLike>& style, FitText&& fitText,
                        DrawRightAligned&& drawRightAligned, FormatClock&& formatClock) {
    renderer.text(style.pageInset, 28.0f, style.supportingScale, "sloppaTV", style.muted);
    renderer.text(style.pageInset, 76.0f, style.headlineScale, fitText(title, style.headlineScale, 1480.0f, 1), style.text,
                  1480.0f);
    if (showClock)
        drawRightAligned(1840.0f, 52.0f, 2.05f, formatClock(clock24Hour), style.muted, 210.0f);
}

template <typename RendererLike, typename ColorLike, typename DrawCentered>
void renderScreenEmptyState(RendererLike& renderer, std::string_view title, std::string_view message,
                            const ScreenChromeRenderStyle<ColorLike>& style, DrawCentered&& drawCentered) {
    constexpr float x = 440.0f;
    constexpr float width = 1040.0f;
    renderer.roundedRect(x, 350.0f, width, 250.0f, style.cornerLarge, style.panelAlt);
    drawCentered(x + 48.0f, 386.0f, width - 96.0f, 80.0f, style.titleScale, title, style.text, 10.0f, 6.0f);
    drawCentered(x + 48.0f, 480.0f, width - 96.0f, 64.0f, style.labelScale, message, style.muted, 10.0f, 5.0f);
}
