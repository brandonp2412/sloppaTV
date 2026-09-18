#pragma once

#include "search_screen.hpp"
#include "seerr_storage.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

template <typename ColorLike> struct SearchRenderStyle {
    float headlineScale = 0.0f;
    float cornerSmall = 0.0f;
    float wideInputFocusScale = 1.0f;
    float cardFocusScale = 1.0f;
    ColorLike text{};
    ColorLike muted{};
    ColorLike secondaryText{};
    ColorLike focus{};
    ColorLike divider{};
    ColorLike panel{};
    ColorLike panelAlt{};
    ColorLike panelElevated{};
    ColorLike error{};
};

template <typename RendererLike, typename ColorLike, typename DrawInputSurface, typename FitTextLines,
          typename DrawLeftAligned, typename DrawCentered, typename RenderKeyboard, typename RenderEmpty,
          typename FocusedBounds, typename DrawHomeArtwork, typename DrawArtworkPlaceholder, typename DrawFocusHalo,
          typename DrawLingeringTitle, typename RenderMediaArtworkCard, typename Now, typename WithAlpha>
void renderSearchScreen(RendererLike& renderer, const SearchScreenState& state, bool seerrConfigured,
                        bool systemSearchInputActive, bool seerrSearchLoading, std::string_view seerrSearchError,
                        bool seerrStorageLoading, std::span<const SeerrStorageTarget> storageTargets,
                        const SearchRenderStyle<ColorLike>& style, DrawInputSurface&& drawInputSurface,
                        FitTextLines&& fitTextLines, DrawLeftAligned&& drawLeftAligned, DrawCentered&& drawCentered,
                        RenderKeyboard&& renderKeyboard, RenderEmpty&& renderEmpty, FocusedBounds&& focusedBounds,
                        DrawHomeArtwork&& drawHomeArtwork, DrawArtworkPlaceholder&& drawArtworkPlaceholder,
                        DrawFocusHalo&& drawFocusHalo, DrawLingeringTitle&& drawLingeringTitle,
                        RenderMediaArtworkCard&& renderMediaArtworkCard, Now&& now, WithAlpha&& withAlpha) {
    const auto& results = state.results();
    const auto& query = state.query();

    renderer.text(80.0f, 44.0f, style.headlineScale, "Search", style.text, 520.0f);
    constexpr float searchTop = 155.0f;
    constexpr float searchWidth = 1450.0f;
    const bool searchFieldFocused =
        systemSearchInputActive || (!state.keyboard() && results.empty() && !seerrSearchLoading);
    const auto searchBounds =
        drawInputSurface(72.0f, searchTop, searchWidth, 68.0f, searchFieldFocused, style.wideInputFocusScale);
    const std::string searchDisplay = query.empty() ? "Movies, shows and episodes" : query;
    renderer.textVerticallyCentered(106.0f, searchBounds[1], searchBounds[3], 2.15f,
                                    fitTextLines(searchDisplay, 2.15f, searchWidth - 68.0f, 1),
                                    query.empty() ? style.muted : style.text, searchWidth - 68.0f);
    const std::string_view searchHint =
        state.keyboard() ? "On-screen keyboard"
                         : (systemSearchInputActive ? "Typing…" : (results.empty() ? "Press OK to type" : "Up to edit"));
    drawLeftAligned(1575.0f, searchTop, 250.0f, 68.0f, 1.45f, searchHint,
                    (state.keyboard() || systemSearchInputActive) ? style.focus : style.secondaryText);

    if (state.keyboard()) {
        renderKeyboard(270.0f);
        drawCentered(560.0f, 886.0f, 800.0f, 58.0f, 1.62f, "Done runs search   |   Back closes keyboard",
                     style.muted, 16.0f, 5.0f);
        return;
    }

    if (systemSearchInputActive && results.empty() && !seerrSearchLoading) {
        drawCentered(480.0f, 300.0f, 960.0f, 64.0f, 1.75f, "Type to search Jellyfin and Seerr", style.muted,
                     16.0f, 5.0f);
        return;
    }
    if (query.empty() && results.empty()) {
        renderEmpty("Find your next favorite", "Search your library and request anything missing through Seerr.");
        return;
    }

    constexpr int columns = mediaGridColumns();
    constexpr float slotWidth = mediaCardWidth();
    constexpr float xGap = 32.0f;

    std::vector<int> semanticRows;
    if (!query.empty() || state.rowItemCount(SearchScreenState::kLibraryRow) > 0 || state.loading()) {
        semanticRows.push_back(SearchScreenState::kLibraryRow);
    }
    if (seerrConfigured) semanticRows.push_back(SearchScreenState::kSeerrRow);
    if (state.rowItemCount(SearchScreenState::kEpisodeRow) > 0) {
        semanticRows.push_back(SearchScreenState::kEpisodeRow);
    }
    if (semanticRows.empty()) {
        renderEmpty("No results found",
                    seerrConfigured ? "No Jellyfin or Seerr matches for this search."
                                    : "No Jellyfin matches. Connect Seerr in Settings to search for more.");
        return;
    }

    const int selectedSemanticRow = state.selectedRow();
    const auto selectedPosition = std::find(semanticRows.begin(), semanticRows.end(), selectedSemanticRow);
    const int selectedRowPosition =
        selectedPosition == semanticRows.end() ? 0 : static_cast<int>(std::distance(semanticRows.begin(), selectedPosition));
    const int firstSemantic =
        std::clamp(selectedRowPosition - 1, 0, std::max(0, static_cast<int>(semanticRows.size()) - 2));

    auto drawLoadingDots = [&](float x, float y) {
        const double seconds = std::chrono::duration<double>(now().time_since_epoch()).count();
        for (int index = 0; index < 3; ++index) {
            const float pulse =
                0.45f + 0.55f * static_cast<float>((std::sin(seconds * 5.0 - index * 1.2) + 1.0) * 0.5);
            const float size = 9.0f + pulse * 5.0f;
            renderer.roundedRect(x + static_cast<float>(index) * 24.0f, y + (14.0f - size) * 0.5f, size, size,
                                 size * 0.5f, withAlpha(style.focus, 0.45f + pulse * 0.55f));
        }
    };

    auto renderResultRow = [&](int semanticRow, int visibleSlot) {
        const float labelY = visibleSlot == 0 ? 258.0f : 726.0f;
        const float cardY = visibleSlot == 0 ? 314.0f : 780.0f;
        const int count = state.rowItemCount(semanticRow);
        const std::string label = semanticRow == SearchScreenState::kLibraryRow
                                      ? "In your library"
                                      : (semanticRow == SearchScreenState::kSeerrRow ? "Seerr" : "Episodes");

        if (semanticRow == SearchScreenState::kSeerrRow) {
            renderer.rect(72.0f, labelY - 22.0f, 1776.0f, 1.5f, style.divider);
            renderer.text(72.0f, labelY, 1.75f, label, style.secondaryText, 520.0f);

            float badgeRight = 1848.0f;
            int shownDrives = 0;
            std::unordered_set<std::string> shownStorage;
            for (auto it = storageTargets.rbegin(); it != storageTargets.rend() && shownDrives < 5; ++it) {
                const std::string identity = it->path + ":" + std::to_string(it->totalSpace);
                if (!shownStorage.insert(identity).second || it->totalSpace <= 0) continue;
                const int percent = std::clamp(it->usedPercent(), 0, 100);
                const std::string text = std::to_string(percent) + "%";
                const float badgeWidth = 108.0f;
                const float badgeX = badgeRight - badgeWidth;
                const float badgeY = labelY - 8.0f;
                renderer.roundedRect(badgeX, badgeY, badgeWidth, 43.0f, 15.0f, style.panelElevated);
                renderer.roundedRect(badgeX + 11.0f, badgeY + 11.0f, 29.0f, 21.0f, 5.0f, style.panel);
                renderer.roundedRect(badgeX + 16.0f, badgeY + 16.0f, 19.0f, 4.0f, 2.0f,
                                     percent >= 90 ? style.error : style.secondaryText);
                renderer.roundedRect(badgeX + 31.0f, badgeY + 25.0f, 4.0f, 4.0f, 2.0f,
                                     percent >= 90 ? style.error : style.focus);
                drawCentered(badgeX + 43.0f, badgeY, 58.0f, 43.0f, 1.32f, text,
                             percent >= 90 ? style.error : style.secondaryText, 4.0f, 2.0f);
                badgeRight = badgeX - 10.0f;
                ++shownDrives;
            }
            if (seerrStorageLoading && shownDrives == 0) {
                renderer.text(1635.0f, labelY + 1.0f, 1.30f, "Loading storage…", style.muted, 210.0f);
            }

            if (seerrSearchLoading) {
                drawLoadingDots(196.0f, labelY + 8.0f);
                renderer.text(288.0f, labelY + 1.0f, 1.45f, "Searching Seerr…", style.muted, 440.0f);
            } else if (count <= 0) {
                const std::string message = seerrSearchError.empty() ? "No additional matches" : "Seerr unavailable";
                renderer.text(196.0f, labelY + 1.0f, 1.45f, message, style.muted, 520.0f);
                return;
            }
        } else {
            renderer.text(72.0f, labelY, 1.75f, label, style.secondaryText, 520.0f);
            if (semanticRow == SearchScreenState::kLibraryRow && state.loading() && count <= 0) {
                drawLoadingDots(228.0f, labelY + 8.0f);
                renderer.text(320.0f, labelY + 1.0f, 1.45f, "Searching Jellyfin…", style.muted, 440.0f);
                return;
            }
            if (semanticRow == SearchScreenState::kLibraryRow && count <= 0) {
                renderer.text(228.0f, labelY + 1.0f, 1.45f, "No library matches", style.muted, 440.0f);
                return;
            }
        }
        if (count <= 0) return;

        const int localStart = state.firstVisibleInRow(semanticRow, columns);
        const int begin = state.rowStart(semanticRow) + localStart;
        const int end = std::min(begin + columns, state.rowStart(semanticRow) + count);
        if (semanticRow == SearchScreenState::kSeerrRow) {
            const float imageHeight = slotWidth * 0.56f;
            for (int index = begin; index < end; ++index) {
                const int col = index - begin;
                const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
                const auto& item = results[static_cast<std::size_t>(index)];
                const bool focused = !systemSearchInputActive && index == state.selection();
                const auto bounds = focusedBounds(x, cardY, slotWidth, imageHeight, focused, style.cardFocusScale);
                const float radius = style.cornerSmall * bounds[3] / imageHeight;
                renderer.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], radius, style.panelAlt);
                if (!drawHomeArtwork(item, bounds[0], bounds[1], bounds[2], bounds[3], radius)) {
                    drawArtworkPlaceholder(item, bounds[0], bounds[1], bounds[2], bounds[3], radius);
                }
                if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], style.focus, radius);
                const float titleY = cardY + imageHeight + 18.0f;
                if (focused) {
                    drawLingeringTitle(x + 2.0f, titleY, 1.95f, item.name, slotWidth - 4.0f, style.text, now());
                } else {
                    renderer.text(x + 2.0f, titleY, 1.95f,
                                  fitTextLines(item.name, 1.95f, slotWidth - 4.0f, 1), style.secondaryText,
                                  slotWidth - 4.0f);
                }
                const std::string requestState =
                    item.externalRequested ? item.externalStatus : std::string("Press OK to request");
                renderer.text(x + 2.0f, titleY + 27.0f, 1.35f,
                              fitTextLines(requestState, 1.35f, slotWidth - 4.0f, 1), style.muted,
                              slotWidth - 4.0f);
            }
            return;
        }

        const bool rowHasPortraitCards =
            std::any_of(results.begin() + begin, results.begin() + end,
                        [](const JellyfinItem& item) { return !usesLandscapeMediaCard(item.type); });
        for (int index = begin; index < end; ++index) {
            const int col = index - begin;
            const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
            renderMediaArtworkCard(results[static_cast<std::size_t>(index)], x, cardY, slotWidth,
                                   !systemSearchInputActive && index == state.selection(), true, false,
                                   rowHasPortraitCards, 1);
        }
    };

    for (int slot = 0; slot < 2 && firstSemantic + slot < static_cast<int>(semanticRows.size()); ++slot) {
        renderResultRow(semanticRows[static_cast<std::size_t>(firstSemantic) + static_cast<std::size_t>(slot)], slot);
    }
}
