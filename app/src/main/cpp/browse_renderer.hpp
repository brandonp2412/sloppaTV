#pragma once

#include "browse_screen.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

template <typename ColorLike> struct BrowseRenderStyle {
    float cornerLarge = 0.0f;
    ColorLike focusSoft{};
    ColorLike panel{};
    ColorLike outline{};
    ColorLike text{};
    ColorLike muted{};
};

template <typename RendererLike, typename ColorLike, typename RenderHeader, typename RenderEmpty, typename DrawTabSurface,
          typename DrawCentered, typename RenderTextTile, typename RenderMediaArtworkCard>
void renderBrowseScreen(RendererLike& renderer, const BrowseScreenState& state, bool loading,
                        const BrowseRenderStyle<ColorLike>& style, RenderHeader&& renderHeader,
                        RenderEmpty&& renderEmpty, DrawTabSurface&& drawTabSurface, DrawCentered&& drawCentered,
                        RenderTextTile&& renderTextTile, RenderMediaArtworkCard&& renderMediaArtworkCard) {
    renderHeader(state.heading());

    if (state.hasFilterBar()) {
        const auto labels = state.filterLabels();
        float x = 88.0f;
        for (size_t index = 0; index < labels.size(); ++index) {
            const float width = labels[index] == "COLLECTIONS" ? 235.0f : 176.0f;
            const bool focused = state.filterFocused() && static_cast<int>(index) == state.filterSelection();
            const bool active = static_cast<int>(index) == state.activeFilterSelection();
            if (focused) {
                const auto bounds = drawTabSurface(x, 190.0f, width, 58.0f, true, active);
                drawCentered(bounds[0], bounds[1], bounds[2], bounds[3], 1.65f, materialLabel(labels[index]),
                             style.text, 14.0f, 4.0f);
            } else {
                renderer.roundedRect(x, 190.0f, width, 58.0f, style.cornerLarge, active ? style.focusSoft : style.panel);
                if (!active)
                    renderer.roundedOutline(x, 190.0f, width, 58.0f, style.cornerLarge, 1.5f, style.outline);
                drawCentered(x, 190.0f, width, 58.0f, 1.65f, materialLabel(labels[index]),
                             active ? style.text : style.muted, 14.0f, 4.0f);
            }
            x += width + 10.0f;
        }
    }

    const auto& items = state.items();
    if (items.empty()) {
        renderEmpty(loading ? "Loading your library" : "No titles found",
                    loading ? "Fetching titles from Jellyfin" : "Try another filter to discover more titles.");
        return;
    }

    constexpr int columns = mediaGridColumns();
    constexpr float slotWidth = mediaCardWidth();
    constexpr float xGap = 32.0f;
    const bool syntheticPage = state.syntheticPage();
    const bool hasPortraitCards =
        std::any_of(items.begin(), items.end(), [](const JellyfinItem& item) { return !usesLandscapeMediaCard(item.type); });
    const float rowStep = syntheticPage ? 190.0f : browseMediaRowHeight(hasPortraitCards);
    const int visibleRows = browseMediaVisibleRows(syntheticPage);
    const int firstRow = mediaFirstVisibleRow(state.selection(), visibleRows);
    for (int index = firstRow * columns; index < static_cast<int>(items.size()); ++index) {
        const int row = index / columns - firstRow;
        const int col = index % columns;
        if (row >= visibleRows) break;
        const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
        const float y = 285.0f + static_cast<float>(row) * rowStep;
        const bool focused = !state.filterFocused() && index == state.selection();
        const auto& item = items[static_cast<size_t>(index)];
        if (syntheticPage)
            renderTextTile(item, x, y, slotWidth, 160.0f, focused);
        else {
            const bool showState = item.type != "BoxSet" && item.type != "CollectionFolder";
            renderMediaArtworkCard(item, x, y, slotWidth, focused, showState, false, hasPortraitCards,
                                   browseMediaTitleLineLimit(hasPortraitCards));
        }
    }
}
