#pragma once

#include "jellyfin_types.hpp"
#include "ui_policy.hpp"

#include <cstddef>
#include <string>
#include <string_view>

template <typename ColorLike>
struct CastRenderStyle {
    float cornerSmall = 0.0f;
    float labelScale = 0.0f;
    float supportingScale = 0.0f;
    ColorLike panelAlt{};
    ColorLike focus{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike tertiary{};
};

template <typename RendererLike, typename ColorLike, typename RenderHeader, typename RenderEmpty,
          typename FocusedBounds, typename DrawPersonArtwork, typename DrawFocusHalo, typename FitText,
          typename DrawCentered>
void renderCastScreen(RendererLike& renderer, std::string_view detailName, const std::vector<JellyfinPerson>& people,
                      int selection, int uiTextSize, const CastRenderStyle<ColorLike>& style,
                      RenderHeader&& renderHeader, RenderEmpty&& renderEmpty, FocusedBounds&& focusedBounds,
                      DrawPersonArtwork&& drawPersonArtwork, DrawFocusHalo&& drawFocusHalo, FitText&& fitText,
                      DrawCentered&& drawCentered) {
    const std::string heading = detailName.empty() ? "Cast" : std::string(detailName) + " | Cast";
    renderHeader(heading);
    if (people.empty()) {
        renderEmpty("No cast information", "Jellyfin has no cast information for this title.");
        return;
    }

    constexpr int columns = mediaGridColumns();
    constexpr float slotWidth = mediaCardWidth();
    constexpr float xGap = 32.0f;
    constexpr float imageWidth = 190.0f;
    constexpr float imageHeight = 285.0f;
    const float rowStep = castRowStep(uiTextSize);
    const int firstRow = mediaFirstVisibleRow(selection, 2);
    for (int index = firstRow * columns; index < static_cast<int>(people.size()); ++index) {
        const int row = index / columns - firstRow;
        const int col = index % columns;
        if (row >= 2) break;

        const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
        const float y = 195.0f + static_cast<float>(row) * rowStep;
        const bool focused = index == selection;
        const float imageX = x + (slotWidth - imageWidth) * 0.5f;
        const auto bounds = focusedBounds(imageX, y, imageWidth, imageHeight, focused);
        const float cardRadius = style.cornerSmall * bounds[3] / imageHeight;
        renderer.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, style.panelAlt);

        const auto& person = people[static_cast<std::size_t>(index)];
        drawPersonArtwork(person, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius);
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], style.focus, cardRadius);

        const float personTitleY = y + imageHeight + 24.0f;
        renderer.text(imageX, personTitleY, style.labelScale,
                      fitText(person.name, style.labelScale, imageWidth, 1), style.text, imageWidth);
        if (!person.role.empty()) {
            const float roleY = personTitleY + 11.0f * style.labelScale * uiTextScale(uiTextSize) + 4.0f;
            renderer.text(imageX, roleY, style.supportingScale,
                          fitText(person.role, style.supportingScale, imageWidth, 1), style.muted, imageWidth);
        }
    }

    drawCentered(500.0f, 1032.0f, 920.0f, 40.0f, 1.55f,
                 "Press OK to explore titles featuring this person", style.tertiary, 12.0f, 2.0f);
}
