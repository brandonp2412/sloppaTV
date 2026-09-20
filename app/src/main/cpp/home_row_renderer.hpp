#pragma once

#include "home_screen.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct HomeRowFadeState {
    int itemIndex = -1;
    std::string itemId;
    std::chrono::steady_clock::time_point started{};
    std::chrono::steady_clock::time_point now{};
};

template <typename ColorLike> struct HomeRowRenderStyle {
    float cornerSmall = 0.0f;
    float cardFocusScale = 1.0f;
    ColorLike text{};
    ColorLike secondaryText{};
    ColorLike muted{};
    ColorLike track{};
    ColorLike focus{};
};

template <typename RendererLike, typename ColorLike, typename FocusedBounds, typename DrawArtwork,
          typename DrawPlaceholder, typename DrawHalo, typename FitText, typename DrawChip, typename IsExternalItem,
          typename EpisodeNumberLabel>
void renderHomeRowContent(RendererLike& renderer, std::string_view title, const std::vector<JellyfinItem>& items,
                          int row, float top, const HomeScreenState& homeState, int uiTextSize,
                          const HomeRowFadeState& fadeState, const HomeRowRenderStyle<ColorLike>& style,
                          FocusedBounds&& focusedBounds, DrawArtwork&& drawArtwork, DrawPlaceholder&& drawPlaceholder,
                          DrawHalo&& drawHalo, FitText&& fitText, DrawChip&& drawChip, IsExternalItem&& isExternalItem,
                          EpisodeNumberLabel&& episodeNumberLabel) {
    if (items.empty()) return;

    const int selected = homeState.selection(row, static_cast<int>(items.size()));
    const bool rowFocused = homeState.row() == row;
    const float imageOffset = homeRowImageOffset(uiTextSize);

    if (title == "My Media") {
        renderer.text(72.0f, top, 3.05f, "My media", rowFocused ? style.text : style.secondaryText, 420.0f);
        constexpr float cardW = 360.0f;
        constexpr float cardH = 193.0f;
        constexpr float gap = 24.0f;
        const float imageY = top + imageOffset;
        float x = 72.0f;
        const int start = homeState.firstVisibleItem(row, static_cast<int>(items.size()), 4);
        for (int index = start; index < static_cast<int>(items.size()); ++index) {
            if (x + cardW > 1885.0f && index > start) break;
            const bool focused = rowFocused && index == selected;
            const auto bounds = focusedBounds(x, imageY, cardW, cardH, focused, style.cardFocusScale);
            const float cardRadius = style.cornerSmall * bounds[3] / cardH;
            const auto& item = items[static_cast<std::size_t>(index)];
            const bool hasArtwork = drawArtwork(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, 1.0f);
            if (!hasArtwork) {
                drawPlaceholder(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, 1.0f);
            }
            if (focused) drawHalo(bounds[0], bounds[1], bounds[2], bounds[3], style.focus, cardRadius);
            renderer.text(x + 4.0f, imageY + cardH + 24.0f, 2.05f, fitText(item.name, 2.05f, cardW - 8.0f, 1),
                          focused ? style.text : style.secondaryText, cardW - 8.0f);
            x += cardW + gap;
        }
        return;
    }

    renderer.text(72.0f, top, 3.05f, fitText(title, 3.05f, 900.0f, 1), rowFocused ? style.text : style.secondaryText,
                  900.0f);
    const int start = homeState.firstVisibleItem(row, static_cast<int>(items.size()), 5);
    constexpr float cardH = 202.0f;
    constexpr float cardW = 350.0f;
    constexpr float gap = 18.0f;
    const float imageY = top + imageOffset;
    float x = 72.0f;

    auto singleLine = [&](std::string_view value, float scale, float width) { return fitText(value, scale, width, 1); };

    for (int index = start; index < static_cast<int>(items.size()); ++index) {
        if (x + cardW > 1908.0f && index > start) break;
        const auto& item = items[static_cast<std::size_t>(index)];
        float itemAlpha = 1.0f;
        if (title == "Next Up" && index == fadeState.itemIndex && item.id == fadeState.itemId &&
            fadeState.started != std::chrono::steady_clock::time_point{}) {
            const auto fadeElapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(fadeState.now - fadeState.started).count();
            const float progress = std::clamp(static_cast<float>(fadeElapsed) / 300.0f, 0.0f, 1.0f);
            itemAlpha = progress * progress * (3.0f - 2.0f * progress);
        }
        const auto faded = [itemAlpha](ColorLike color) {
            color.a *= itemAlpha;
            return color;
        };
        const bool focused = rowFocused && index == selected;
        const auto bounds = focusedBounds(x, imageY, cardW, cardH, focused, style.cardFocusScale);
        const float cardRadius = style.cornerSmall * bounds[3] / cardH;
        const bool hasArtwork = drawArtwork(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, itemAlpha);
        if (!hasArtwork) {
            drawPlaceholder(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, itemAlpha);
        }
        if (item.externalProgressPercent >= 0) {
            const double progress = std::clamp(static_cast<double>(item.externalProgressPercent) / 100.0, 0.0, 1.0);
            renderer.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f, bounds[2] - 16.0f, 4.0f, 2.0f,
                                 faded(style.track));
            renderer.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f,
                                 static_cast<float>((bounds[2] - 16.0f) * progress), 4.0f, 2.0f, faded(style.focus));
        } else if (item.positionTicks > 0 && item.runtimeTicks > 0) {
            const double progress =
                std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
            renderer.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f, bounds[2] - 16.0f, 4.0f, 2.0f,
                                 faded(style.track));
            renderer.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f,
                                 static_cast<float>((bounds[2] - 16.0f) * progress), 4.0f, 2.0f, faded(style.focus));
        }
        if (focused) drawHalo(bounds[0], bounds[1], bounds[2], bounds[3], faded(style.focus), cardRadius);

        std::string primary = item.type == "Episode" && !item.seriesName.empty() ? item.seriesName : item.name;
        primary = singleLine(primary, 2.45f, cardW - 18.0f);
        const float titleY = imageY + cardH + 22.0f;
        renderer.text(x + 2.0f, titleY, 2.45f, primary, faded(focused ? style.text : style.secondaryText),
                      cardW - 4.0f);
        if (isExternalItem(item) && !item.externalStatus.empty()) {
            const float secondaryY = titleY + 11.0f * 2.45f * uiTextScale(uiTextSize) + 4.0f;
            if (item.externalProgressPercent >= 0 && !item.externalProgressLabel.empty()) {
                const std::string percentLabel = std::to_string(item.externalProgressPercent) + "%";
                const float percentWidth = std::ceil(renderer.textWidth(1.16f, percentLabel));
                const float etaWidth =
                    item.externalProgressEta.empty()
                        ? 0.0f
                        : std::round(
                              std::clamp(renderer.textWidth(1.08f, materialLabel(item.externalProgressEta)) + 34.0f,
                                         72.0f, 210.0f));
                constexpr float metadataGap = 12.0f;
                constexpr float statusToMetadataGap = 22.0f;
                const float rightEdge = x + cardW - 2.0f;
                const float etaX = rightEdge - etaWidth;
                const float percentX =
                    item.externalProgressEta.empty() ? rightEdge - percentWidth : etaX - metadataGap - percentWidth;
                const float statusWidth = std::max(60.0f, percentX - (x + 2.0f) - statusToMetadataGap);
                renderer.text(x + 2.0f, secondaryY, 1.48f, singleLine(item.externalProgressLabel, 1.48f, statusWidth),
                              faded(style.muted), statusWidth);
                renderer.textVerticallyCentered(percentX, secondaryY - 3.0f, 30.0f, 1.16f, percentLabel,
                                                style.secondaryText, percentWidth);
                if (!item.externalProgressEta.empty()) {
                    drawChip(etaX, secondaryY - 3.0f, item.externalProgressEta, false, 1.08f, 30.0f, 210.0f);
                }
            } else {
                renderer.text(x + 2.0f, secondaryY, 1.58f, singleLine(item.externalStatus, 1.58f, cardW - 4.0f),
                              faded(style.muted), cardW - 4.0f);
            }
        } else if (item.type == "Episode") {
            std::string episode = episodeNumberLabel(item);
            if (!item.name.empty() && item.name != item.seriesName) {
                if (!episode.empty()) episode += "  |  ";
                episode += item.name;
            }
            if (!episode.empty()) {
                const float secondaryY = titleY + 11.0f * 2.45f * uiTextScale(uiTextSize) + 4.0f;
                renderer.text(x + 2.0f, secondaryY, 1.58f, singleLine(episode, 1.58f, cardW - 4.0f), faded(style.muted),
                              cardW - 4.0f);
            }
        }
        x += cardW + gap;
    }
}
