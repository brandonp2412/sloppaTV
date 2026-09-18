#pragma once

#include "diagnostics_screen.hpp"

#include <cstddef>
#include <initializer_list>
#include <string_view>

template <typename ColorLike> struct DiagnosticsRenderStyle {
    float cornerLarge = 0.0f;
    ColorLike panelAlt{};
    ColorLike outline{};
    ColorLike divider{};
    ColorLike tertiary{};
    ColorLike text{};
    ColorLike muted{};
};

template <typename RendererLike, typename ColorLike, typename RenderHeader, typename DrawChip, typename DrawLeftAligned,
          typename DrawCentered>
void renderDiagnosticsScreen(RendererLike& renderer, const DiagnosticsScreenData& data,
                             const DiagnosticsRenderStyle<ColorLike>& style, RenderHeader&& renderHeader,
                             DrawChip&& drawChip, DrawLeftAligned&& drawLeftAligned, DrawCentered&& drawCentered) {
    renderHeader("Diagnostics");
    const auto rows = diagnosticsRows(data);

    auto renderPanel = [&](float x, float y, float width, float height, std::string_view title,
                           std::initializer_list<int> indices) {
        renderer.roundedRect(x, y, width, height, style.cornerLarge, style.panelAlt);
        renderer.roundedOutline(x, y, width, height, style.cornerLarge, 1.0f, style.outline);
        (void)drawChip(x + 26.0f, y + 22.0f, title, true, 1.45f, 42.0f, width - 52.0f);

        float rowY = y + 86.0f;
        size_t row = 0;
        for (const int index : indices) {
            if (index < 0 || index >= static_cast<int>(rows.size())) continue;
            if (row > 0) renderer.rect(x + 28.0f, rowY - 13.0f, width - 56.0f, 1.0f, style.divider);
            drawLeftAligned(x + 28.0f, rowY - 5.0f, 260.0f, 46.0f, 1.45f,
                            rows[static_cast<size_t>(index)].first, style.tertiary);
            drawLeftAligned(x + 300.0f, rowY - 5.0f, width - 330.0f, 46.0f, 1.75f,
                            rows[static_cast<size_t>(index)].second, style.text);
            rowY += 58.0f;
            ++row;
        }
    };

    renderPanel(85.0f, 175.0f, 840.0f, 315.0f, "App & server", {0, 1, 2, 3});
    renderPanel(995.0f, 175.0f, 840.0f, 315.0f, "Video & display", {4, 7, 8});
    renderPanel(85.0f, 515.0f, 840.0f, 250.0f, "Audio", {5, 6});
    renderPanel(995.0f, 515.0f, 840.0f, 250.0f, "Last playback", {9});
    drawCentered(560.0f, 944.0f, 800.0f, 52.0f, 1.75f, "Back or OK returns to settings", style.muted, 12.0f, 4.0f);
}
