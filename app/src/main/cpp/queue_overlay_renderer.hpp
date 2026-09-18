#pragma once

#include "queue_overlay_screen.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

template <typename ColorLike> struct QueueOverlayRenderStyle {
    float artworkCornerRadius = 0.0f;
    ColorLike scrim{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike tertiary{};
    ColorLike focusSoft{};
    ColorLike panelAlt{};
};

struct QueueOverlayRenderState {
    const std::vector<JellyfinItem>& items;
    int currentIndex = 0;
    int selection = 0;
    int actionSelection = 0;
    QueueRepeatMode repeatMode = QueueRepeatMode::Off;
};

template <typename RendererLike, typename ColorLike, typename DrawModal, typename DrawLeft, typename DrawCentered,
          typename DrawListItem, typename DrawArtwork, typename DrawPlaceholder, typename EpisodeLabel,
          typename DrawButton, typename DrawDisabledButton, typename MaterialLabel>
void renderQueueOverlayScreen(RendererLike& renderer, const QueueOverlayRenderState& state,
                              const QueueOverlayRenderStyle<ColorLike>& style, DrawModal&& drawModal,
                              DrawLeft&& drawLeft, DrawCentered&& drawCentered, DrawListItem&& drawListItem,
                              DrawArtwork&& drawArtwork, DrawPlaceholder&& drawPlaceholder, EpisodeLabel&& episodeLabel,
                              DrawButton&& drawButton, DrawDisabledButton&& drawDisabledButton,
                              MaterialLabel&& materialLabel) {
    const int size = static_cast<int>(state.items.size());
    if (size <= 0) return;

    const int current = std::clamp(state.currentIndex, 0, size - 1);
    const int selection = std::clamp(state.selection, current, size - 1);

    renderer.rect(0.0f, 0.0f, 1920.0f, 1080.0f, style.scrim);
    drawModal(790.0f, 28.0f, 1090.0f, 1020.0f);
    drawLeft(842.0f, 60.0f, 620.0f, 74.0f, 3.35f, "Playback queue", style.text);
    renderer.roundedRect(1555.0f, 70.0f, 255.0f, 46.0f, 18.0f, style.panelAlt);
    drawCentered(1555.0f, 70.0f, 255.0f, 46.0f, 1.35f,
                 std::to_string(queueOverlayRemainingCount(current, size)) + " remaining", style.muted, 12.0f, 4.0f);

    constexpr int visibleRows = 5;
    const int first = queueOverlayFirstVisible(selection, current, size, visibleRows);
    for (int slot = 0; slot < visibleRows; ++slot) {
        const int index = first + slot;
        if (index >= size) break;

        const float y = 160.0f + static_cast<float>(slot) * 108.0f;
        const bool selected = index == selection;
        const auto& item = state.items[static_cast<size_t>(index)];
        const auto bounds = drawListItem(830.0f, y, 990.0f, 90.0f, selected);
        if (!drawArtwork(item, bounds[0] + 12.0f, bounds[1] + 10.0f, 124.0f, 70.0f, style.artworkCornerRadius)) {
            drawPlaceholder(item, bounds[0] + 12.0f, bounds[1] + 10.0f, 124.0f, 70.0f, style.artworkCornerRadius);
        }

        const QueueOverlayMarker marker = queueOverlayMarker(index, current);
        renderer.roundedRect(bounds[0] + 154.0f, bounds[1] + 24.0f, marker.width, 40.0f, 16.0f,
                             marker.current ? style.focusSoft : style.panelAlt);
        drawCentered(bounds[0] + 154.0f, bounds[1] + 24.0f, marker.width, 40.0f, 1.20f, marker.label,
                     marker.current ? style.text : style.muted, 8.0f, 3.0f);
        drawLeft(bounds[0] + 300.0f, bounds[1] + 5.0f, 610.0f, 46.0f, 1.85f, item.name, style.text);

        const std::string secondary = episodeLabel(item);
        if (!secondary.empty()) {
            drawLeft(bounds[0] + 300.0f, bounds[1] + 50.0f, 610.0f, 36.0f, 1.25f, secondary, style.muted);
        }
    }

    const auto actions = queueOverlayActionLabels(state.repeatMode);
    for (size_t i = 0; i < actions.size(); ++i) {
        const bool firstActionRow = i < 4;
        const int column = firstActionRow ? static_cast<int>(i) : static_cast<int>(i) - 4;
        const float width = firstActionRow ? 230.0f : 310.0f;
        const float x = 835.0f + static_cast<float>(column) * (width + 16.0f);
        const float y = firstActionRow ? 715.0f : 805.0f;
        const bool focused = state.actionSelection == static_cast<int>(i);
        const bool available = queueOverlayActionEnabled(static_cast<int>(i), selection, current, size);
        std::array<float, 4> actionBounds{x, y, width, 68.0f};
        if (available)
            actionBounds = drawButton(x, y, width, 68.0f, focused, focused, i == 4);
        else
            drawDisabledButton(x, y, width, 68.0f);
        drawCentered(actionBounds[0], actionBounds[1], actionBounds[2], actionBounds[3], 1.45f,
                     materialLabel(actions[i]), available ? style.text : style.tertiary, 14.0f, 5.0f);
    }

    drawCentered(875.0f, 905.0f, 920.0f, 52.0f, 1.50f,
                 "Up / Down selects item   |   Left / Right chooses action   |   OK applies", style.muted, 14.0f, 4.0f);
    drawCentered(1170.0f, 958.0f, 500.0f, 48.0f, 1.40f, "Back closes queue", style.muted, 12.0f, 4.0f);
}
