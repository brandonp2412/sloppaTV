#pragma once

#include "jellyfin_types.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

struct MediaGridRenderState {
    bool loading = false;
    int selection = 0;
    int uiTextSize = 0;
};

struct MediaGridCardPlacement {
    float x = 0.0f;
    float y = 0.0f;
    float slotWidth = 0.0f;
    bool focused = false;
    bool showState = true;
    bool preferSeriesCover = false;
    bool alignToPortraitBand = false;
    int titleLineLimit = 0;
};

template <typename RenderHeader, typename RenderEmpty, typename RenderCard>
void renderMediaGridScreen(std::string_view title, const std::vector<JellyfinItem>& items,
                           const MediaGridRenderState& state, RenderHeader&& renderHeader, RenderEmpty&& renderEmpty,
                           RenderCard&& renderCard) {
    renderHeader(title);
    if (items.empty()) {
        renderEmpty(state.loading ? "Loading titles" : "No titles available",
                    state.loading ? "Fetching titles from Jellyfin" : "Press Back to return to your library.");
        return;
    }

    constexpr int columns = mediaGridColumns();
    constexpr float slotWidth = mediaCardWidth();
    constexpr float xGap = 32.0f;
    const bool hasPortraitCards =
        std::any_of(items.begin(), items.end(), [](const JellyfinItem& item) { return !usesLandscapeMediaCard(item.type); });
    const float rowStep = searchMediaRowHeight(hasPortraitCards);
    const int firstRow = mediaFirstVisibleRow(state.selection, 2);
    for (int index = firstRow * columns; index < static_cast<int>(items.size()); ++index) {
        const int row = index / columns - firstRow;
        const int col = index % columns;
        if (row >= 2) break;

        renderCard(
            items[static_cast<std::size_t>(index)],
            MediaGridCardPlacement{
                .x = 80.0f + static_cast<float>(col) * (slotWidth + xGap),
                .y = 195.0f + static_cast<float>(row) * rowStep,
                .slotWidth = slotWidth,
                .focused = index == state.selection,
                .showState = true,
                .preferSeriesCover = hasPortraitCards,
                .alignToPortraitBand = hasPortraitCards,
                .titleLineLimit = mediaGridTitleLineLimit(row, state.uiTextSize, hasPortraitCards),
            });
    }
}
