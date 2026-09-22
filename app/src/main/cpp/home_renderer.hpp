#pragma once

#include "home_screen.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

struct HomeRenderConfig {
    int backdropMode = 0;
    bool showClock = false;
    bool clock24Hour = true;
    int uiTextSize = 1;
    bool loading = false;
};

struct HomeSlideState {
    int fromFirst = 0;
    int toFirst = 0;
    std::chrono::steady_clock::time_point started{};
};

template <typename ColorLike> struct HomeRenderStyle {
    float canvasWidth = 1920.0f;
    float canvasHeight = 1080.0f;
    float cornerLarge = 0.0f;
    float buttonFocusScale = 1.0f;
    ColorLike background{};
    ColorLike backdropScrim{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike clockMuted{};
    ColorLike brandGold{};
    ColorLike focusSoft{};
    ColorLike panelElevated{};
    ColorLike panelAlt{};
    ColorLike focus{};
};

template <typename RendererLike, typename ColorLike, typename DrawBackdrop, typename DrawBrandMark,
          typename DrawTabSurface, typename DrawCentered, typename FocusedBounds, typename DrawProfileArtwork,
          typename DrawFocusHalo, typename DrawRightAligned, typename FormatClock, typename RenderEmpty,
          typename RenderHomeRow, typename Now>
void renderHomeScreen(RendererLike& renderer, const std::vector<JellyfinHomeRow>& rows, const HomeScreenState& state,
                      const JellyfinSession& session, const HomeRenderConfig& config, const HomeSlideState& slide,
                      const HomeRenderStyle<ColorLike>& style, DrawBackdrop&& drawBackdrop,
                      DrawBrandMark&& drawBrandMark, DrawTabSurface&& drawTabSurface, DrawCentered&& drawCentered,
                      FocusedBounds&& focusedBounds, DrawProfileArtwork&& drawProfileArtwork,
                      DrawFocusHalo&& drawFocusHalo, DrawRightAligned&& drawRightAligned, FormatClock&& formatClock,
                      RenderEmpty&& renderEmpty, RenderHomeRow&& renderHomeRow, Now&& now) {
    bool backdropVisible = false;
    if (config.backdropMode > 0 && !rows.empty()) {
        const int backdropRow =
            std::clamp(state.row() >= 0 ? state.row() : state.firstVisibleRow(), 0, static_cast<int>(rows.size()) - 1);
        const auto& row = rows[static_cast<std::size_t>(backdropRow)];
        if (!row.items.empty() && backdropRow < static_cast<int>(state.selectionCount())) {
            const int selection = state.selection(backdropRow, static_cast<int>(row.items.size()));
            backdropVisible = drawBackdrop(row.items[static_cast<std::size_t>(selection)], 0.24f);
        }
    }
    if (!backdropVisible) {
        renderer.rect(0.0f, 0.0f, style.canvasWidth, style.canvasHeight, style.background);
    } else {
        renderer.rect(0.0f, 0.0f, style.canvasWidth, style.canvasHeight, style.backdropScrim);
    }

    const bool toolbarFocused = state.row() < 0;
    const bool hasBrandMark = drawBrandMark(72.0f, 27.0f, 72.0f);
    renderer.text(hasBrandMark ? 160.0f : 72.0f, 42.0f, 3.0f, "sloppaTV", style.text, 430.0f);

    const std::array<std::string_view, 3> navLabels{"Home", "Search", "Settings"};
    const std::array<int, 3> navIndices{1, 2, 3};
    const std::array<float, 3> navMinWidths{138.0f, 158.0f, 178.0f};
    float navX = 960.0f;
    for (std::size_t index = 0; index < navLabels.size(); ++index) {
        const bool focused = toolbarFocused && state.navIndex() == navIndices[index];
        const bool active = navIndices[index] == 1;
        const float navWidth =
            std::round(std::max(navMinWidths[index], renderer.textWidth(2.0f, navLabels[index]) + 44.0f));
        if (focused) {
            const auto bounds = drawTabSurface(navX, 40.0f, navWidth, 54.0f, true, active);
            drawCentered(bounds[0], bounds[1], bounds[2], bounds[3], 2.0f, navLabels[index], style.text, 12.0f, 4.0f);
        } else {
            if (active) renderer.roundedRect(navX, 40.0f, navWidth, 54.0f, style.cornerLarge, style.focusSoft);
            drawCentered(navX, 40.0f, navWidth, 54.0f, 2.0f, navLabels[index], active ? style.text : style.muted, 12.0f,
                         4.0f);
        }
        navX += navWidth + 12.0f;
    }

    constexpr float profileX = 1660.0f;
    constexpr float profileY = 36.0f;
    constexpr float profileSize = 62.0f;
    const bool profileFocused = toolbarFocused && state.navIndex() == 0;
    const auto profileBounds =
        focusedBounds(profileX, profileY, profileSize, profileSize, profileFocused, style.buttonFocusScale);
    const float profileRadius = profileBounds[3] * 0.5f;
    renderer.roundedRect(profileBounds[0], profileBounds[1], profileBounds[2], profileBounds[3], profileRadius,
                         profileFocused ? style.panelElevated : style.panelAlt);
    if (!drawProfileArtwork(session, profileBounds[0], profileBounds[1], profileBounds[2])) {
        const std::string initial =
            session.username.empty()
                ? "U"
                : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(session.username.front()))));
        drawCentered(profileBounds[0], profileBounds[1], profileBounds[2], profileBounds[3], 2.35f, initial, style.text,
                     6.0f, 6.0f);
    }
    if (profileFocused) {
        drawFocusHalo(profileBounds[0], profileBounds[1], profileBounds[2], profileBounds[3], style.focus,
                      profileRadius);
    }

    if (config.showClock) {
        drawRightAligned(1900.0f, 53.0f, 2.10f, formatClock(config.clock24Hour), style.clockMuted, 150.0f);
    }

    if (rows.empty()) {
        renderEmpty(config.loading ? "Loading your library" : "Your library is empty",
                    config.loading ? "Connecting to your Jellyfin server"
                                   : "Check your server connection and available libraries.");
        return;
    }

    const int firstVisibleRow =
        homeFirstVisibleRow(state.firstVisibleRow(), state.row(), static_cast<int>(rows.size()), 2);
    int renderFirstRow = firstVisibleRow;
    float slideOffset = 0.0f;
    int renderRowCount = 2;
    const auto current = now();
    constexpr auto slideDuration = std::chrono::milliseconds(220);
    const float rowStep = homeRowStep(config.uiTextSize);
    if (slide.started != std::chrono::steady_clock::time_point{} && slide.toFirst == firstVisibleRow &&
        current < slide.started + slideDuration) {
        const float elapsed =
            static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(current - slide.started).count());
        const float progress = std::clamp(elapsed / 220.0f, 0.0f, 1.0f);
        const float eased = progress * progress * (3.0f - 2.0f * progress);
        renderRowCount = 3;
        if (slide.toFirst > slide.fromFirst) {
            renderFirstRow = slide.fromFirst;
            slideOffset = -rowStep * eased;
        } else {
            renderFirstRow = slide.toFirst;
            slideOffset = -rowStep * (1.0f - eased);
        }
    }

    for (int visible = 0; visible < renderRowCount; ++visible) {
        const int row = renderFirstRow + visible;
        if (row < 0 || row >= static_cast<int>(rows.size())) continue;
        renderHomeRow(rows[static_cast<std::size_t>(row)].title, rows[static_cast<std::size_t>(row)].items, row,
                      150.0f + static_cast<float>(visible) * rowStep + slideOffset);
    }
}
