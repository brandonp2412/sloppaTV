#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <string>

template <typename ColorLike> struct PlayerNextUpRenderStyle {
    float panelCornerRadius = 0.0f;
    float artworkCornerRadius = 0.0f;
    ColorLike focus{};
    ColorLike text{};
    ColorLike muted{};
};

struct PlayerNextUpRenderState {
    const JellyfinItem& item;
    int remainingMs = 0;
};

template <typename ColorLike, typename DrawModal, typename DrawArtwork, typename DrawPlaceholder,
          typename DrawLeftAligned, typename EpisodeLabel>
void renderPlayerNextUp(const PlayerNextUpRenderState& state, const PlayerNextUpRenderStyle<ColorLike>& style,
                        DrawModal&& drawModal, DrawArtwork&& drawArtwork, DrawPlaceholder&& drawPlaceholder,
                        DrawLeftAligned&& drawLeftAligned, EpisodeLabel&& episodeLabel) {
    constexpr float panelX = 1195.0f;
    constexpr float panelY = 185.0f;
    constexpr float panelWidth = 625.0f;
    constexpr float panelHeight = 205.0f;
    constexpr float contentY = 214.0f;
    constexpr float artworkX = 1210.0f;
    constexpr float artworkWidth = 240.0f;
    constexpr float artworkHeight = 146.0f;
    constexpr float textX = 1478.0f;
    constexpr float textWidth = 312.0f;

    drawModal(panelX, panelY, panelWidth, panelHeight, style.panelCornerRadius);
    if (!drawArtwork(state.item, artworkX, contentY, artworkWidth, artworkHeight, style.artworkCornerRadius)) {
        drawPlaceholder(state.item, artworkX, contentY, artworkWidth, artworkHeight, style.artworkCornerRadius);
    }

    const std::string nextHeading = "Up next  |  " + std::to_string(std::max(0, state.remainingMs / 1000)) + "s";
    drawLeftAligned(textX, contentY, textWidth, 34.0f, 1.55f, nextHeading, style.focus);
    drawLeftAligned(textX, contentY + 38.0f, textWidth, 58.0f, 2.05f, state.item.name, style.text);

    const std::string nextLabel = episodeLabel(state.item);
    if (!nextLabel.empty()) {
        drawLeftAligned(textX, contentY + 100.0f, textWidth, 46.0f, 1.45f, nextLabel, style.muted);
    }
}
