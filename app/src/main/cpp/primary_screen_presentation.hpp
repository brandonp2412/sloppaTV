#pragma once

#include "browse_renderer.hpp"
#include "home_renderer.hpp"
#include "keyboard_renderer.hpp"
#include "login_renderer.hpp"
#include "media_display_text.hpp"
#include "profiles_renderer.hpp"
#include "search_renderer.hpp"
#include "ui_policy.hpp"
#include "ui_theme.hpp"

#include <chrono>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

template <typename RendererLike, typename UiLike, typename Rows>
void renderKeyboardPresentation(RendererLike& renderer, UiLike& ui, const Rows& rows, int selectedRow,
                                int selectedColumn, float top, float canvasHeight) {
    renderVirtualKeyboard(
        renderer, rows, selectedRow, selectedColumn, top,
        KeyboardRenderStyle<Color>{
            .canvasHeight = canvasHeight,
            .cornerLarge = material_tv::cornerLarge,
            .surfaceContainerHigh = material_tv::surfaceContainerHigh,
            .text = material_tv::onSurface,
        },
        [&ui](float x, float y, float width, float height, bool focused, bool primary) {
            return ui.drawButtonSurface(x, y, width, height, focused, primary);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        });
}

template <typename RendererLike, typename UiLike, typename RenderKeyboard>
void renderLoginPresentation(RendererLike& renderer, UiLike& ui, const AccountScreenState& accountState, bool loading,
                             int savedUserCount, float canvasWidth, float canvasHeight,
                             RenderKeyboard&& renderKeyboard) {
    renderLoginScreen(
        renderer, canvasWidth, canvasHeight,
        LoginRenderState{
            .quickConnectActive = accountState.quickConnectActive(),
            .quickConnectCode = accountState.quickConnectCode(),
            .loading = loading,
            .savedUserCount = savedUserCount,
            .keyboardActive = accountState.keyboardActive(),
            .loginFocus = accountState.loginFocus(),
            .fields = {accountState.field(0), accountState.field(1), accountState.field(2)},
            .discoveryStatus = accountState.discoveryStatus(),
        },
        LoginRenderStyle<Color>{
            .cornerLarge = material_tv::cornerLarge,
            .cornerMedium = material_tv::cornerMedium,
            .displayScale = material_tv::type::display,
            .bodyScale = material_tv::type::body,
            .wideInputFocusScale = materialWideInputFocusScale(),
            .background = material_tv::background,
            .surfaceContainerHigh = material_tv::surfaceContainerHigh,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .secondaryText = material_tv::onSurfaceSecondary,
            .panelElevated = material_tv::surfaceContainerHigh,
            .outline = material_tv::outline,
            .panelAlt = material_tv::surfaceContainer,
            .focusSoft = material_tv::primaryContainer,
            .tertiary = material_tv::onSurfaceDisabled,
            .focus = material_tv::primary,
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](float x, float y, float width, float height) { ui.drawModalSurface(x, y, width, height); },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        },
        [&ui](float x, float y, float width, float height, bool focused, float focusScale) {
            return ui.drawInputSurface(x, y, width, height, focused, focusScale);
        },
        [&ui](float x, float y, float width, float height, bool focused, bool primary) {
            return ui.drawButtonSurface(x, y, width, height, focused, primary);
        },
        [&ui](float x, float y, float width, float height, bool focused) {
            return ui.drawFocusedSurface(x, y, width, height, focused);
        },
        [&ui](std::string_view value, float scale, float width, int lines) {
            return ui.fitTextLines(value, scale, width, lines);
        },
        std::forward<RenderKeyboard>(renderKeyboard));
}

template <typename RendererLike, typename UiLike, typename GetSession>
void renderProfilesPresentation(RendererLike& renderer, UiLike& ui, int savedCount, int profileSelection,
                                int profileAction, GetSession&& getSession) {
    renderProfilesScreen(
        renderer, savedCount, profileSelection, profileAction,
        ProfilesRenderStyle<Color>{
            .cornerMedium = material_tv::cornerMedium,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .focus = material_tv::primary,
            .panelElevated = material_tv::surfaceContainerHigh,
            .panelAlt = material_tv::surfaceContainer,
        },
        [&ui](std::string_view title) { ui.renderHeader(title); },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        },
        [&ui](float x, float y, float width, float height, bool focused, bool primary) {
            return ui.drawFocusedSurface(x, y, width, height, focused, primary);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        std::forward<GetSession>(getSession),
        [&ui](const auto& saved, float x, float y, float size) { return ui.drawProfileArtwork(saved, x, y, size); },
        [&ui](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
            return ui.drawButtonSurface(x, y, width, height, focused, primary, destructive);
        });
}

template <typename RendererLike, typename UiLike, typename DrawBrandMark, typename RenderHomeRow>
void renderHomePresentation(RendererLike& renderer, UiLike& ui, const std::vector<JellyfinHomeRow>& rows,
                            const HomeScreenState& homeState, const JellyfinSession& session,
                            const AppSettings& settings, bool loading, int slideFromFirst, int slideToFirst,
                            std::chrono::steady_clock::time_point slideStarted, float canvasWidth, float canvasHeight,
                            DrawBrandMark&& drawBrandMark, RenderHomeRow&& renderHomeRow) {
    const Color muted = material_tv::onSurfaceVariant;
    renderHomeScreen(
        renderer, rows, homeState, session,
        HomeRenderConfig{
            .backdropMode = settings.backdropMode,
            .showClock = settings.showClock,
            .clock24Hour = settings.clock24Hour,
            .uiTextSize = settings.uiTextSize,
            .loading = loading,
        },
        HomeSlideState{
            .fromFirst = slideFromFirst,
            .toFirst = slideToFirst,
            .started = slideStarted,
        },
        HomeRenderStyle<Color>{
            .canvasWidth = canvasWidth,
            .canvasHeight = canvasHeight,
            .cornerLarge = material_tv::cornerLarge,
            .buttonFocusScale = materialButtonFocusScale(),
            .background = material_tv::background,
            .backdropScrim = Color{0.01f, 0.012f, 0.018f, 0.34f},
            .text = material_tv::onSurface,
            .muted = muted,
            .clockMuted = Color{muted.r, muted.g, muted.b, 0.82f},
            .brandGold = material_tv::tertiary,
            .focusSoft = material_tv::primaryContainer,
            .panelElevated = material_tv::surfaceContainerHigh,
            .panelAlt = material_tv::surfaceContainer,
            .focus = material_tv::primary,
        },
        [&ui](const JellyfinItem& item, float alpha) { return ui.drawBackdrop(item, alpha); },
        std::forward<DrawBrandMark>(drawBrandMark),
        [&ui](float x, float y, float width, float height, bool focused, bool selected) {
            return ui.drawTabSurface(x, y, width, height, focused, selected);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](float x, float y, float width, float height, bool focused, float focusScale) {
            return ui.focusedBounds(x, y, width, height, focused, focusScale);
        },
        [&ui](const JellyfinSession& saved, float x, float y, float size) {
            return ui.drawProfileArtwork(saved, x, y, size);
        },
        [&ui](float x, float y, float width, float height, Color color, float radius) {
            ui.drawFocusHalo(x, y, width, height, color, radius);
        },
        [&ui](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
            ui.drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
        },
        [](bool clock24Hour) { return formatLocalClock(std::time(nullptr), clock24Hour); },
        [&ui](std::string_view title, std::string_view message) { ui.renderEmptyState(title, message); },
        std::forward<RenderHomeRow>(renderHomeRow), [] { return std::chrono::steady_clock::now(); });
}

template <typename RendererLike, typename UiLike, typename RenderTextTile, typename RenderMediaArtworkCard>
void renderBrowsePresentation(RendererLike& renderer, UiLike& ui, const BrowseScreenState& browseState, bool loading,
                              RenderTextTile&& renderTextTile, RenderMediaArtworkCard&& renderMediaArtworkCard) {
    renderBrowseScreen(
        renderer, browseState, loading,
        BrowseRenderStyle<Color>{
            .cornerLarge = material_tv::cornerLarge,
            .focusSoft = material_tv::primaryContainer,
            .panel = material_tv::surface,
            .outline = material_tv::outline,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
        },
        [&ui](std::string_view heading) { ui.renderHeader(heading); },
        [&ui](std::string_view title, std::string_view message) { ui.renderEmptyState(title, message); },
        [&ui](float x, float y, float width, float height, bool focused, bool selected) {
            return ui.drawTabSurface(x, y, width, height, focused, selected);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        std::forward<RenderTextTile>(renderTextTile), std::forward<RenderMediaArtworkCard>(renderMediaArtworkCard));
}

template <typename RendererLike, typename UiLike, typename RenderKeyboard, typename DrawLingeringTitle,
          typename RenderMediaArtworkCard>
void renderSearchPresentation(RendererLike& renderer, UiLike& ui, const SearchScreenState& searchState,
                              bool seerrConfigured, bool systemSearchInputActive, bool seerrSearchLoading,
                              std::string_view seerrSearchError, bool seerrStorageLoading,
                              std::span<const SeerrStorageTarget> storageTargets, RenderKeyboard&& renderKeyboard,
                              DrawLingeringTitle&& drawLingeringTitle,
                              RenderMediaArtworkCard&& renderMediaArtworkCard) {
    renderSearchScreen(
        renderer, searchState, seerrConfigured, systemSearchInputActive, seerrSearchLoading, seerrSearchError,
        seerrStorageLoading, storageTargets,
        SearchRenderStyle<Color>{
            .headlineScale = material_tv::type::headline,
            .cornerSmall = material_tv::cornerSmall,
            .wideInputFocusScale = materialWideInputFocusScale(),
            .cardFocusScale = materialCardFocusScale(),
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .secondaryText = material_tv::onSurfaceSecondary,
            .focus = material_tv::primary,
            .divider = material_tv::outlineVariant,
            .panel = material_tv::surface,
            .panelAlt = material_tv::surfaceContainer,
            .panelElevated = material_tv::surfaceContainerHigh,
            .error = material_tv::error,
        },
        [&ui](float x, float y, float width, float height, bool focused, float focusScale) {
            return ui.drawInputSurface(x, y, width, height, focused, focusScale);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        std::forward<RenderKeyboard>(renderKeyboard),
        [&ui](std::string_view title, std::string_view message) { ui.renderEmptyState(title, message); },
        [&ui](float x, float y, float width, float height, bool focused, float focusScale) {
            return ui.focusedBounds(x, y, width, height, focused, focusScale);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
            return ui.drawHomeArtwork(item, x, y, width, height, radius);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
            ui.drawArtworkPlaceholder(item, x, y, width, height, radius);
        },
        [&ui](float x, float y, float width, float height, Color color, float radius) {
            ui.drawFocusHalo(x, y, width, height, color, radius);
        },
        std::forward<DrawLingeringTitle>(drawLingeringTitle),
        std::forward<RenderMediaArtworkCard>(renderMediaArtworkCard), [] { return std::chrono::steady_clock::now(); },
        [](Color color, float alpha) { return Color{color.r, color.g, color.b, alpha}; });
}
