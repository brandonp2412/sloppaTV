#pragma once

#include "details_screen.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

struct DetailsRenderConfig {
    bool showClock = false;
    bool clock24Hour = true;
    bool showWatchedIndicators = true;
    int uiTextSize = 1;
    bool stillWatchingPrompt = false;
    bool overlayOpen = false;
};

template <typename ColorLike> struct DetailsRenderStyle {
    float canvasWidth = 1920.0f;
    float canvasHeight = 1080.0f;
    float cornerLarge = 0.0f;
    float cornerExtraSmall = 0.0f;
    float cardFocusScale = 1.0f;
    float labelScale = 0.0f;
    ColorLike background{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike secondaryText{};
    ColorLike focus{};
    ColorLike panelElevated{};
    ColorLike outline{};
    ColorLike track{};
    ColorLike backdropHorizontalStart{};
    ColorLike backdropHorizontalEnd{};
    ColorLike backdropVerticalStart{};
    ColorLike backdropVerticalEnd{};
};

template <typename RendererLike, typename ColorLike, typename DrawBackdrop, typename DrawRightAligned,
          typename FormatClock, typename DrawCentered, typename DrawLogo, typename FitTextLines,
          typename EpisodeNumberLabel, typename EpisodeLabel, typename FormatPlaybackTime, typename DrawChip,
          typename MaterialLabel, typename DrawButtonSurface, typename FocusedBounds, typename DrawHomeArtwork,
          typename DrawArtworkPlaceholder, typename DrawFocusHalo>
void renderDetailsScreen(RendererLike& renderer, const JellyfinItem& detail, const DetailsScreenState& state,
                         std::span<const std::string> actions, const DetailsRenderConfig& config,
                         const DetailsRenderStyle<ColorLike>& style, DrawBackdrop&& drawBackdrop,
                         DrawRightAligned&& drawRightAligned, FormatClock&& formatClock, DrawCentered&& drawCentered,
                         DrawLogo&& drawLogo, FitTextLines&& fitTextLines, EpisodeNumberLabel&& episodeNumberLabel,
                         EpisodeLabel&& episodeLabel, FormatPlaybackTime&& formatPlaybackTime, DrawChip&& drawChip,
                         MaterialLabel&& materialLabel, DrawButtonSurface&& drawButtonSurface,
                         FocusedBounds&& focusedBounds, DrawHomeArtwork&& drawHomeArtwork,
                         DrawArtworkPlaceholder&& drawArtworkPlaceholder, DrawFocusHalo&& drawFocusHalo) {
    const bool backdropVisible = drawBackdrop(detail, 0.84f);
    if (backdropVisible) {
        renderer.horizontalGradient(0.0f, 0.0f, 1350.0f, style.canvasHeight, style.backdropHorizontalStart,
                                    style.backdropHorizontalEnd);
        renderer.verticalGradient(0.0f, 0.0f, style.canvasWidth, style.canvasHeight, style.backdropVerticalStart,
                                  style.backdropVerticalEnd);
    } else {
        renderer.rect(0.0f, 0.0f, style.canvasWidth, style.canvasHeight, style.background);
    }

    if (config.showClock) {
        drawRightAligned(1840.0f, 50.0f, 2.10f, formatClock(config.clock24Hour), style.muted, 210.0f);
    }

    if (config.stillWatchingPrompt) {
        constexpr float promptX = 820.0f;
        constexpr float promptWidth = 800.0f;
        renderer.roundedRect(promptX, 54.0f, promptWidth, 54.0f, style.cornerLarge, style.panelElevated);
        renderer.roundedOutline(promptX, 54.0f, promptWidth, 54.0f, style.cornerLarge, 1.5f, style.outline);
        drawCentered(promptX, 54.0f, promptWidth, 54.0f, 1.95f, "Still watching? Press OK to continue", style.text,
                     20.0f, 4.0f);
    }

    constexpr float contentX = 72.0f;
    constexpr float contentWidth = 920.0f;
    const bool episode = detail.type == "Episode";
    const std::string mainTitle = episode && !detail.seriesName.empty() ? detail.seriesName : detail.name;
    const bool hasLogo = drawLogo(detail, contentX, 132.0f, 700.0f, 138.0f);
    if (!hasLogo) {
        renderer.text(contentX, 142.0f, 6.0f,
                      fitTextLines(mainTitle.empty() ? "Loading…" : mainTitle, 6.0f, contentWidth, 1), style.text,
                      contentWidth);
    }

    const std::string episodeNumber = episodeNumberLabel(detail);
    const std::string secondary =
        episode ? (detail.name.empty() || detail.name == detail.seriesName
                       ? episodeNumber
                       : episodeNumber + (episodeNumber.empty() ? "" : "  |  ") + detail.name)
                : episodeLabel(detail);
    const float uiScale = uiTextScale(config.uiTextSize);
    const float titleBottom = hasLogo ? 270.0f : 142.0f + 10.0f * 6.0f * uiScale;
    const float secondaryY = std::max(286.0f, titleBottom + 12.0f);
    if (!secondary.empty()) {
        renderer.text(contentX, secondaryY, 2.80f, fitTextLines(secondary, 2.80f, contentWidth, 1),
                      style.secondaryText, contentWidth);
    }

    std::vector<std::string> metadata;
    if (detail.productionYear > 0) metadata.emplace_back(std::to_string(detail.productionYear));
    if (!detail.officialRating.empty()) metadata.emplace_back(detail.officialRating);
    if (detail.runtimeTicks > 0) {
        metadata.emplace_back(formatPlaybackTime(static_cast<int>(detail.runtimeTicks / 10000)));
    }
    if (detail.communityRating >= 0.0f) {
        std::ostringstream rating;
        rating << std::fixed << std::setprecision(1) << detail.communityRating << "/10";
        metadata.emplace_back(rating.str());
    }
    if (!detail.genres.empty()) metadata.emplace_back(detail.genres.front());

    constexpr float metadataY = 380.0f;
    float metadataX = contentX;
    for (const auto& value : metadata) {
        const float available = contentX + contentWidth - metadataX;
        if (available < 72.0f) break;
        const float width = drawChip(metadataX, metadataY, value, false, 1.38f, 42.0f, available);
        metadataX += width + 10.0f;
    }

    constexpr float overviewY = 438.0f;
    const int overviewLines = config.uiTextSize > 0 ? 2 : 3;
    if (!detail.overview.empty()) {
        renderer.text(contentX, overviewY, 2.35f,
                      fitTextLines(detail.overview, 2.35f, contentWidth, overviewLines), style.secondaryText,
                      contentWidth);
    }

    constexpr float stateY = 600.0f;
    float stateX = contentX;
    if (detail.favorite) {
        stateX += drawChip(stateX, stateY, "Favorite", true, 1.42f, 42.0f, 180.0f) + 10.0f;
    }
    if (config.showWatchedIndicators && detail.played) {
        drawChip(stateX, stateY, "Watched", true, 1.42f, 42.0f, 180.0f);
    }

    constexpr float actionY = 658.0f;
    constexpr float actionGap = 18.0f;
    constexpr float actionRightInset = 72.0f;
    auto desiredActionWidth = [&](const std::string& action) {
        return std::round(
            std::max(145.0f, renderer.textWidth(style.labelScale, materialLabel(action)) + 56.0f));
    };
    float desiredActionWidths = 0.0f;
    for (const auto& action : actions) desiredActionWidths += desiredActionWidth(action);
    const float actionGaps = actions.empty() ? 0.0f : actionGap * static_cast<float>(actions.size() - 1);
    const float availableActionWidths =
        std::max(1.0f, style.canvasWidth - contentX - actionRightInset - actionGaps);
    const float actionWidthScale = desiredActionWidths > availableActionWidths && desiredActionWidths > 0.0f
                                       ? availableActionWidths / desiredActionWidths
                                       : 1.0f;
    float actionX = contentX;
    for (std::size_t index = 0; index < actions.size(); ++index) {
        const bool focused = !config.overlayOpen && !state.similarFocused() && !state.episodeContextFocused() &&
                             state.actionSelection() == static_cast<int>(index);
        const float width = std::round(desiredActionWidth(actions[index]) * actionWidthScale);
        const bool primaryAction = index == 0;
        const auto bounds = drawButtonSurface(actionX, actionY, width, 64.0f, focused, primaryAction);
        drawCentered(bounds[0], bounds[1], bounds[2], bounds[3], 1.80f, materialLabel(actions[index]),
                     primaryAction || focused ? style.text : style.secondaryText, 18.0f, 6.0f);
        actionX += width + actionGap;
    }

    if (detail.positionTicks > 0 && detail.runtimeTicks > 0) {
        const double fraction =
            std::clamp(static_cast<double>(detail.positionTicks) / static_cast<double>(detail.runtimeTicks), 0.0, 1.0);
        renderer.roundedRect(contentX, 744.0f, 560.0f, 4.0f, 2.0f, style.track);
        renderer.roundedRect(contentX, 744.0f, static_cast<float>(560.0 * fraction), 4.0f, 2.0f, style.focus);
    }

    if (episode && state.hasEpisodeSeriesContext()) {
        renderer.text(72.0f, 752.0f, 2.30f, "Show & seasons",
                      state.episodeContextFocused() ? style.text : style.secondaryText, 520.0f);
        const int count = state.episodeContextCount();
        constexpr int visible = 5;
        const int maxStart = std::max(0, count - visible);
        const int start = std::clamp(state.episodeContextSelection() - 1, 0, maxStart);
        constexpr float buttonWidth = 320.0f;
        constexpr float buttonHeight = 72.0f;
        constexpr float buttonGap = 26.0f;
        constexpr float rowY = 825.0f;
        for (int slot = 0; slot < visible; ++slot) {
            const int index = start + slot;
            if (index >= count) break;
            const float x = 72.0f + static_cast<float>(slot) * (buttonWidth + buttonGap);
            const bool focused =
                !config.overlayOpen && state.episodeContextFocused() && state.episodeContextSelection() == index;
            const auto bounds = drawButtonSurface(x, rowY, buttonWidth, buttonHeight, focused, index == 0);
            std::string label = "GO TO SHOW";
            if (index > 0 && static_cast<std::size_t>(index - 1) < state.seasons().size()) {
                const auto& season = state.seasons()[static_cast<std::size_t>(index - 1)];
                label = season.name.empty() ? "SEASON " + std::to_string(index) : season.name;
            }
            drawCentered(bounds[0], bounds[1], bounds[2], bounds[3], 1.75f, materialLabel(label),
                         index == 0 || focused ? style.text : style.secondaryText, 18.0f, 5.0f);
        }
        return;
    }

    const auto& similarItems = state.similar();
    if (similarItems.empty()) return;

    renderer.text(72.0f, 752.0f, 2.30f, "More like this",
                  state.similarFocused() ? style.text : style.secondaryText, 440.0f);
    constexpr int visible = 5;
    const int maxStart = std::max(0, static_cast<int>(similarItems.size()) - visible);
    const int start = std::clamp(state.similarSelection() - 1, 0, maxStart);
    constexpr float cardWidth = 320.0f;
    constexpr float cardHeight = 144.0f;
    constexpr float cardGap = 26.0f;
    for (int slot = 0; slot < visible; ++slot) {
        const int index = start + slot;
        if (index >= static_cast<int>(similarItems.size())) break;
        const auto& similar = similarItems[static_cast<std::size_t>(index)];
        const float x = 72.0f + static_cast<float>(slot) * (cardWidth + cardGap);
        constexpr float y = 825.0f;
        const bool focused = !config.overlayOpen && state.similarFocused() && index == state.similarSelection();
        const auto bounds = focusedBounds(x, y, cardWidth, cardHeight, focused, style.cardFocusScale);
        const float cardRadius = style.cornerExtraSmall * bounds[3] / cardHeight;
        if (!drawHomeArtwork(similar, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius)) {
            drawArtworkPlaceholder(similar, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius);
        }
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], style.focus, cardRadius);
        renderer.text(x + 2.0f, y + cardHeight + 22.0f, 2.10f,
                      fitTextLines(similar.name, 2.10f, cardWidth - 10.0f, 1),
                      focused ? style.text : style.secondaryText, cardWidth - 10.0f);
    }
}
