#pragma once

#include "jellyfin_types.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

struct MediaArtworkCardRenderOptions {
    bool showState = true;
    bool preferSeriesCover = false;
    bool alignToPortraitBand = false;
    int titleLineLimit = 0;
    int uiTextSize = 0;
    bool showWatchedIndicators = true;
};

template <typename ColorLike> struct MediaCardRenderStyle {
    float canvasHeight = 1080.0f;
    float cornerSmall = 0.0f;
    float cornerMedium = 0.0f;
    float cardFocusScale = 1.0f;
    float labelScale = 0.0f;
    ColorLike panel{};
    ColorLike panelAlt{};
    ColorLike panelElevated{};
    ColorLike tertiary{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike track{};
    ColorLike focus{};
    ColorLike focusSoft{};
    ColorLike outline{};
};

template <typename RendererLike, typename ColorLike, typename FocusedBounds, typename DrawArtwork,
          typename DrawPlaceholder, typename DrawHalo, typename DrawCentered, typename FitText,
          typename SecondaryLabel>
void renderMediaArtworkCardContent(RendererLike& renderer, const JellyfinItem& item, float x, float y, float slotWidth,
                                   bool focused, const MediaArtworkCardRenderOptions& options,
                                   const MediaCardRenderStyle<ColorLike>& style, FocusedBounds&& focusedBounds,
                                   DrawArtwork&& drawArtwork, DrawPlaceholder&& drawPlaceholder, DrawHalo&& drawHalo,
                                   DrawCentered&& drawCentered, FitText&& fitText, SecondaryLabel&& secondaryLabel) {
    const bool seriesCoverForEpisode = options.preferSeriesCover && item.type == "Episode" && !item.seriesId.empty() &&
                                       !item.seriesPrimaryImageTag.empty();
    const bool landscape = usesLandscapeMediaCard(item.type) && !seriesCoverForEpisode;
    const float imageWidth = landscape ? slotWidth : mediaPosterWidth();
    const float imageHeight = landscape ? 180.0f : mediaPosterHeight();
    const float artworkBandHeight = options.alignToPortraitBand ? mediaPosterHeight() : imageHeight;
    const float imageX = x + (slotWidth - imageWidth) * 0.5f;
    const float imageY = y + std::max(0.0f, (artworkBandHeight - imageHeight) * 0.5f);
    const auto bounds = focusedBounds(imageX, imageY, imageWidth, imageHeight, focused, style.cardFocusScale);
    const float cardRadius = style.cornerSmall * bounds[3] / imageHeight;

    renderer.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, style.panelAlt);
    const bool hasArtwork =
        drawArtwork(item, seriesCoverForEpisode, landscape, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius);
    if (!hasArtwork) {
        drawPlaceholder(item, bounds[0] + 1.0f, bounds[1] + 1.0f, bounds[2] - 2.0f, bounds[3] - 2.0f,
                        std::max(0.0f, cardRadius - 1.0f));
    }

    if (item.positionTicks > 0 && item.runtimeTicks > 0) {
        const double fraction =
            std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
        renderer.roundedRect(bounds[0] + 10.0f, bounds[1] + bounds[3] - 14.0f, bounds[2] - 20.0f, 5.0f, 2.5f,
                             style.track);
        renderer.roundedRect(bounds[0] + 10.0f, bounds[1] + bounds[3] - 14.0f,
                             static_cast<float>((bounds[2] - 20.0f) * fraction), 5.0f, 2.5f, style.focus);
    }

    if (options.showState && (item.favorite || (options.showWatchedIndicators && item.played))) {
        const std::string_view label = item.favorite ? "Favorite" : "Watched";
        const float badgeWidth = item.favorite ? 132.0f : 118.0f;
        const float badgeX = bounds[0] + bounds[2] - badgeWidth - 12.0f;
        const float badgeY = bounds[1] + 12.0f;
        // State is useful context, but it must not read as another focused control.
        const ColorLike badgeSurface = style.panelElevated;
        renderer.roundedRect(badgeX, badgeY, badgeWidth, 34.0f, 17.0f, badgeSurface);
        renderer.roundedOutline(badgeX, badgeY, badgeWidth, 34.0f, 17.0f, 1.0f,
                                focused && item.favorite ? style.focus : style.outline);
        drawCentered(badgeX, badgeY, badgeWidth, 34.0f, 1.12f, label,
                     focused && item.favorite ? style.text : style.muted, 10.0f, 3.0f);
    }

    if (focused) drawHalo(bounds[0], bounds[1], bounds[2], bounds[3], style.focus, cardRadius);

    const float titleY = y + artworkBandHeight + 24.0f;
    const int titleLines = options.titleLineLimit > 0 ? options.titleLineLimit : (landscape ? 1 : 2);
    const float titleWidth = landscape ? imageWidth - 4.0f : slotWidth - 4.0f;
    const float titleX = landscape ? imageX + 2.0f : x + 2.0f;
    const std::string fittedTitle = fitText(item.name, style.labelScale, titleWidth, titleLines);
    renderer.text(titleX, titleY, style.labelScale, fittedTitle, style.text, titleWidth);

    const std::string secondary = secondaryLabel(item);
    if (secondary.empty()) return;

    const int renderedTitleLines =
        fittedTitle.empty() ? 0 : 1 + static_cast<int>(std::count(fittedTitle.begin(), fittedTitle.end(), '\n'));
    const float titleLineHeight = 11.0f * style.labelScale * uiTextScale(options.uiTextSize);
    const float secondaryY = titleY + titleLineHeight * static_cast<float>(renderedTitleLines) + 3.0f;
    const float secondaryHeight = 10.0f * 1.45f * uiTextScale(options.uiTextSize);
    if (secondaryY + secondaryHeight <= style.canvasHeight - 8.0f)
        renderer.text(titleX, secondaryY, 1.45f, fitText(secondary, 1.45f, titleWidth, 1), style.muted, titleWidth);
}

template <typename RendererLike, typename ColorLike, typename FocusedBounds, typename DrawCentered, typename DrawHalo>
void renderMediaTextTileContent(RendererLike& renderer, const JellyfinItem& item, float x, float y, float width,
                                float height, bool focused, const MediaCardRenderStyle<ColorLike>& style,
                                FocusedBounds&& focusedBounds, DrawCentered&& drawCentered, DrawHalo&& drawHalo) {
    const auto bounds = focusedBounds(x, y, width, height, focused, style.cardFocusScale);
    const float tileRadius = style.cornerMedium * bounds[3] / height;
    renderer.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], tileRadius,
                         focused ? style.panelElevated : style.panel);
    drawCentered(bounds[0] + 28.0f, bounds[1] + 18.0f, bounds[2] - 56.0f, 42.0f, 1.25f, item.type, style.tertiary,
                 8.0f, 3.0f);
    drawCentered(bounds[0] + 28.0f, bounds[1] + 58.0f, bounds[2] - 56.0f, bounds[3] - 76.0f, 2.55f, item.name,
                 style.text, 10.0f, 6.0f);
    if (focused) drawHalo(bounds[0], bounds[1], bounds[2], bounds[3], style.focus, tileRadius);
}
