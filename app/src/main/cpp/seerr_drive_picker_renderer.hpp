#pragma once

#include "seerr_drive_picker_screen.hpp"

#include <cstddef>
#include <string>

template <typename ColorLike>
struct SeerrDrivePickerRenderStyle {
    float headlineScale = 0.0f;
    float cornerMedium = 0.0f;
    float focusScale = 1.0f;
    ColorLike text{};
    ColorLike muted{};
    ColorLike secondaryText{};
    ColorLike focus{};
    ColorLike focusSoft{};
    ColorLike panelElevated{};
    ColorLike error{};
};

template <typename RendererLike, typename ColorLike, typename DrawListItem, typename FitText, typename DrawCentered,
          typename RenderEmpty>
void renderSeerrDrivePickerScreen(RendererLike& renderer, const SeerrDrivePickerViewModel& model,
                                  const SeerrDrivePickerRenderStyle<ColorLike>& style, DrawListItem&& drawListItem,
                                  FitText&& fitText, DrawCentered&& drawCentered, RenderEmpty&& renderEmpty) {
    renderer.text(80.0f, 56.0f, style.headlineScale, "Choose storage", style.text, 760.0f);
    renderer.text(82.0f, 125.0f, 1.55f, fitText(model.subtitle, 1.55f, 1450.0f, 1), style.muted, 1450.0f);

    if (model.rows.empty()) {
        renderEmpty("No storage targets", "Back returns to search.");
        return;
    }

    for (std::size_t slot = 0; slot < model.rows.size(); ++slot) {
        const SeerrDrivePickerRow& row = model.rows[slot].row;
        const bool focused = model.rows[slot].focused;
        const float x = 120.0f;
        const float y = 220.0f + static_cast<float>(slot) * 145.0f;
        constexpr float width = 1680.0f;
        drawListItem(x, y, width, 116.0f, focused, style.cornerMedium, style.focusScale);

        const float iconX = x + 34.0f;
        const float iconY = y + 30.0f;
        renderer.roundedRect(iconX, iconY, 58.0f, 52.0f, 10.0f, focused ? style.focusSoft : style.panelElevated);
        renderer.roundedRect(iconX + 10.0f, iconY + 11.0f, 38.0f, 7.0f, 3.5f,
                             focused ? style.focus : style.muted);
        renderer.roundedRect(iconX + 40.0f, iconY + 33.0f, 7.0f, 7.0f, 3.5f,
                             focused ? style.focus : style.secondaryText);

        renderer.textVerticallyCentered(x + 120.0f, y + 8.0f, 62.0f, 2.05f,
                                        fitText(row.name, 2.05f, 760.0f, 1),
                                        focused ? style.text : style.secondaryText, 760.0f);

        const float barX = 1085.0f;
        const float barY = y + 48.0f;
        constexpr float barWidth = 500.0f;
        if (row.hasCapacity) {
            renderer.roundedRect(barX, barY, barWidth, 14.0f, 7.0f, style.panelElevated);
            renderer.text(x + 120.0f, y + 78.0f, 1.38f, row.secondaryText, style.muted, 760.0f);
            renderer.roundedRect(barX, barY, barWidth * static_cast<float>(row.usedPercent) / 100.0f, 14.0f, 7.0f,
                                 row.nearFull ? style.error : (focused ? style.focus : style.secondaryText));
        } else {
            renderer.text(x + 120.0f, y + 78.0f, 1.38f, fitText(row.secondaryText, 1.38f, 920.0f, 1),
                          style.muted, 920.0f);
        }
        drawCentered(1510.0f, y + 26.0f, 250.0f, 58.0f, 1.60f, row.statusText,
                     row.nearFull ? style.error : (focused ? style.text : style.secondaryText), 8.0f, 3.0f);
    }

    drawCentered(590.0f, 970.0f, 740.0f, 48.0f, 1.45f,
                 "Up / Down selects   ·   OK requests   ·   Back cancels", style.muted, 12.0f, 4.0f);
}
