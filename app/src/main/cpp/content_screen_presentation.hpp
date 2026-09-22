#pragma once
#include "seerr_season_picker_renderer.hpp"

#include "home_row_renderer.hpp"
#include "media_card_renderer.hpp"
#include "primary_screen_presentation.hpp"
#include "screen_presentation.hpp"
#include "seerr_drive_picker_renderer.hpp"
#include "seerr_drive_picker_screen.hpp"
#include "text_layout.hpp"
#include "ui_theme.hpp"

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

inline MediaCardRenderStyle<Color> contentMediaCardStyle() {
    return MediaCardRenderStyle<Color>{
        .canvasHeight = Renderer::logicalHeight(),
        .cornerSmall = material_tv::cornerSmall,
        .cornerMedium = material_tv::cornerMedium,
        .cardFocusScale = materialCardFocusScale(),
        .labelScale = material_tv::type::label,
        .panel = material_tv::surface,
        .panelAlt = material_tv::surfaceContainer,
        .panelElevated = material_tv::surfaceContainerHigh,
        .tertiary = material_tv::onSurfaceDisabled,
        .text = material_tv::onSurface,
        .muted = material_tv::onSurfaceVariant,
        .track = material_tv::track,
        .focus = material_tv::primary,
        .focusSoft = material_tv::primaryContainer,
        .outline = material_tv::outline,
    };
}

template <typename UiLike>
void renderContentMediaArtworkCard(Renderer& renderer, UiLike& ui, const AppSettings& settings,
                                   const JellyfinItem& item, float x, float y, float slotWidth, bool focused,
                                   bool showState = true, bool preferSeriesCover = false,
                                   bool alignToPortraitBand = false, int titleLineLimit = 0) {
    renderMediaArtworkCardContent(
        renderer, item, x, y, slotWidth, focused,
        MediaArtworkCardRenderOptions{
            .showState = showState,
            .preferSeriesCover = preferSeriesCover,
            .alignToPortraitBand = alignToPortraitBand,
            .titleLineLimit = titleLineLimit,
            .uiTextSize = settings.uiTextSize,
            .showWatchedIndicators = settings.showWatchedIndicators,
        },
        contentMediaCardStyle(),
        [&ui](float imageX, float imageY, float imageWidth, float imageHeight, bool isFocused, float focusScale) {
            return ui.focusedBounds(imageX, imageY, imageWidth, imageHeight, isFocused, focusScale);
        },
        [&ui](const JellyfinItem& source, bool seriesCoverForEpisode, bool landscape, float imageX, float imageY,
              float imageWidth, float imageHeight, float radius) {
            JellyfinItem cover = source;
            if (seriesCoverForEpisode) {
                cover.id = source.seriesId;
                cover.imageTag = source.seriesPrimaryImageTag;
                cover.type = "Series";
            }
            return landscape ? ui.drawHomeArtwork(cover, imageX, imageY, imageWidth, imageHeight, radius)
                             : ui.drawArtwork(cover, imageX, imageY, imageWidth, imageHeight, 1.0f, radius);
        },
        [&ui](const JellyfinItem& source, float imageX, float imageY, float imageWidth, float imageHeight,
              float radius) { ui.drawArtworkPlaceholder(source, imageX, imageY, imageWidth, imageHeight, radius); },
        [&ui](float imageX, float imageY, float imageWidth, float imageHeight, Color color, float radius) {
            ui.drawFocusHalo(imageX, imageY, imageWidth, imageHeight, color, radius);
        },
        [&ui](float centeredX, float centeredY, float centeredWidth, float centeredHeight, float scale,
              std::string_view value, Color color, float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(centeredX, centeredY, centeredWidth, centeredHeight, scale, value, color,
                                         horizontalPadding, verticalPadding);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [](const JellyfinItem& source) {
            return isSeerrItem(source)
                       ? (source.externalRequested ? source.externalStatus : std::string("Press OK to request"))
                       : episodeLabel(source);
        });
}

template <typename UiLike>
void renderContentTextTile(Renderer& renderer, UiLike& ui, const JellyfinItem& item, float x, float y, float width,
                           float height, bool focused) {
    renderMediaTextTileContent(
        renderer, item, x, y, width, height, focused, contentMediaCardStyle(),
        [&ui](float tileX, float tileY, float tileWidth, float tileHeight, bool isFocused, float focusScale) {
            return ui.focusedBounds(tileX, tileY, tileWidth, tileHeight, isFocused, focusScale);
        },
        [&ui](float centeredX, float centeredY, float centeredWidth, float centeredHeight, float scale,
              std::string_view value, Color color, float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(centeredX, centeredY, centeredWidth, centeredHeight, scale, value, color,
                                         horizontalPadding, verticalPadding);
        },
        [&ui](float tileX, float tileY, float tileWidth, float tileHeight, Color color, float radius) {
            ui.drawFocusHalo(tileX, tileY, tileWidth, tileHeight, color, radius);
        });
}

template <typename UiLike>
void renderContentHomeRow(Renderer& renderer, UiLike& ui, const HomeScreenState& homeState, const AppSettings& settings,
                          int fadeIndex, const std::string& fadeItemId,
                          std::chrono::steady_clock::time_point fadeStarted, std::string_view title,
                          const std::vector<JellyfinItem>& items, int row, float top) {
    renderHomeRowContent(
        renderer, title, items, row, top, homeState, settings.uiTextSize,
        HomeRowFadeState{
            .itemIndex = fadeIndex,
            .itemId = fadeItemId,
            .started = fadeStarted,
            .now = std::chrono::steady_clock::now(),
        },
        HomeRowRenderStyle<Color>{
            .cornerSmall = material_tv::cornerSmall,
            .cardFocusScale = materialCardFocusScale(),
            .text = material_tv::onSurface,
            .secondaryText = material_tv::onSurfaceSecondary,
            .muted = material_tv::onSurfaceVariant,
            .track = material_tv::track,
            .focus = material_tv::primary,
        },
        [&ui](float x, float y, float width, float height, bool focused, float focusScale) {
            return ui.focusedBounds(x, y, width, height, focused, focusScale);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius, float alpha) {
            return ui.drawHomeArtwork(item, x, y, width, height, radius, alpha);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius, float alpha) {
            ui.drawArtworkPlaceholder(item, x, y, width, height, radius, alpha);
        },
        [&ui](float x, float y, float width, float height, Color color, float radius) {
            ui.drawFocusHalo(x, y, width, height, color, radius);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [&ui](float x, float y, const std::string& label, bool selected, float scale, float height, float maxWidth) {
            return ui.drawChip(x, y, label, selected, scale, height, maxWidth);
        },
        [](const JellyfinItem& item) { return isSeerrItem(item); },
        [](const JellyfinItem& item) { return episodeNumberLabel(item); });
}

template <typename UiLike, typename DrawBrandMark>
void renderContentHome(Renderer& renderer, UiLike& ui, const JellyfinHomeData& home, const HomeScreenState& homeState,
                       const JellyfinSession& session, const AppSettings& settings, bool loading, int slideFromFirst,
                       int slideToFirst, std::chrono::steady_clock::time_point slideStarted, int fadeIndex,
                       const std::string& fadeItemId, std::chrono::steady_clock::time_point fadeStarted,
                       DrawBrandMark&& drawBrandMark) {
    renderHomePresentation(renderer, ui, home.rows, homeState, session, settings, loading, slideFromFirst, slideToFirst,
                           slideStarted, Renderer::logicalWidth(), Renderer::logicalHeight(),
                           std::forward<DrawBrandMark>(drawBrandMark),
                           [&](std::string_view title, const std::vector<JellyfinItem>& items, int row, float top) {
                               renderContentHomeRow(renderer, ui, homeState, settings, fadeIndex, fadeItemId,
                                                    fadeStarted, title, items, row, top);
                           });
}

template <typename UiLike>
void renderContentBrowse(Renderer& renderer, UiLike& ui, const BrowseScreenState& browseState,
                         const AppSettings& settings, bool loading) {
    renderBrowsePresentation(
        renderer, ui, browseState, loading,
        [&](const JellyfinItem& item, float x, float y, float width, float height, bool focused) {
            renderContentTextTile(renderer, ui, item, x, y, width, height, focused);
        },
        [&](const JellyfinItem& item, float x, float y, float width, bool focused, bool showState,
            bool seriesCoverForEpisode, bool alignMixedHeights, int titleLineLimit) {
            renderContentMediaArtworkCard(renderer, ui, settings, item, x, y, width, focused, showState,
                                          seriesCoverForEpisode, alignMixedHeights, titleLineLimit);
        });
}

template <typename UiLike, typename RenderKeyboard>
void renderContentSearch(Renderer& renderer, UiLike& ui, const SearchScreenState& searchState,
                         const AppSettings& settings, bool seerrConfigured, bool systemSearchInputActive,
                         bool seerrSearchLoading, std::string_view seerrSearchError, bool seerrStorageLoading,
                         std::span<const SeerrStorageTarget> storageTargets,
                         std::chrono::steady_clock::time_point lastInteraction, RenderKeyboard&& renderKeyboard) {
    renderSearchPresentation(
        renderer, ui, searchState, seerrConfigured, systemSearchInputActive, seerrSearchLoading, seerrSearchError,
        seerrStorageLoading, storageTargets, std::forward<RenderKeyboard>(renderKeyboard),
        [&](float x, float y, float scale, std::string_view value, float maxWidth, Color color,
            std::chrono::steady_clock::time_point now) {
            renderLingeringTitle(renderer, x, y, scale, value, maxWidth, color, lastInteraction, now);
        },
        [&](const JellyfinItem& item, float x, float y, float width, bool focused, bool showState,
            bool seriesCoverForEpisode, bool alignMixedHeights, int titleLineLimit) {
            renderContentMediaArtworkCard(renderer, ui, settings, item, x, y, width, focused, showState,
                                          seriesCoverForEpisode, alignMixedHeights, titleLineLimit);
        });
}

template <typename UiLike>
void renderContentSeerrDrivePicker(Renderer& renderer, UiLike& ui, const SeerrDrivePickerViewModel& model) {
    renderSeerrDrivePickerScreen(
        renderer, model,
        SeerrDrivePickerRenderStyle<Color>{
            .headlineScale = material_tv::type::headline,
            .cornerMedium = material_tv::cornerMedium,
            .focusScale = materialWideListItemFocusScale(),
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .secondaryText = material_tv::onSurfaceSecondary,
            .focus = material_tv::primary,
            .focusSoft = material_tv::primaryContainer,
            .panelElevated = material_tv::surfaceContainerHigh,
            .error = material_tv::error,
        },
        [&ui](float x, float y, float width, float height, bool focused, float radius, float focusScale) {
            ui.drawListItemSurface(x, y, width, height, focused, radius, focusScale);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](const std::string& title, const std::string& message) { ui.renderEmptyState(title, message); });
}

template <typename UiLike>
void renderContentSeerrSeasonPicker(Renderer& renderer, UiLike& ui, const SeerrSeasonPickerState& state) {
    renderSeerrSeasonPicker(renderer, ui, state,
                            SeerrSeasonPickerStyle<Color>{
                                .headlineScale = material_tv::type::headline,
                                .cornerRadius = material_tv::cornerMedium,
                                .focusScale = materialWideListItemFocusScale(),
                                .text = material_tv::onSurface,
                                .muted = material_tv::onSurfaceVariant,
                            });
}

template <typename UiLike>
void renderContentMediaGrid(Renderer& renderer, UiLike& ui, const AppSettings& settings, std::string_view title,
                            const std::vector<JellyfinItem>& items, bool loading, int selection) {
    renderMediaGridPresentation(title, items, loading, selection, settings.uiTextSize, ui,
                                [&](const JellyfinItem& item, const MediaGridCardPlacement& placement) {
                                    renderContentMediaArtworkCard(
                                        renderer, ui, settings, item, placement.x, placement.y, placement.slotWidth,
                                        placement.focused, placement.showState, placement.preferSeriesCover,
                                        placement.alignToPortraitBand, placement.titleLineLimit);
                                });
}

template <typename UiLike>
void renderContentCast(Renderer& renderer, UiLike& ui, const JellyfinItem& item, const DetailsScreenState& state,
                       const AppSettings& settings) {
    renderCastPresentation(renderer, ui, item, state, settings.uiTextSize);
}

template <typename UiLike>
void renderContentPersonItems(Renderer& renderer, UiLike& ui, const DetailsScreenState& state,
                              const AppSettings& settings, bool loading) {
    const auto& person = state.selectedPerson();
    const std::string heading = person.name.empty() ? "Person" : "Featuring " + person.name;
    renderContentMediaGrid(renderer, ui, settings, heading, state.personItems(), loading, state.personItemSelection());
}

template <typename UiLike>
void renderContentSeasons(Renderer& renderer, UiLike& ui, const DetailsScreenState& state, const AppSettings& settings,
                          bool loading) {
    const auto& series = state.seriesDetail();
    renderContentMediaGrid(renderer, ui, settings, series.name.empty() ? "Seasons" : series.name + " | Seasons",
                           state.seasons(), loading, state.seasonSelection());
}

template <typename UiLike>
void renderContentEpisodes(Renderer& renderer, UiLike& ui, const DetailsScreenState& state, const AppSettings& settings,
                           bool loading) {
    const auto& series = state.seriesDetail();
    const auto& season = state.selectedSeason();
    const std::string heading = season.name.empty() ? "Episodes" : series.name + " - " + season.name;
    renderContentMediaGrid(renderer, ui, settings, heading, state.episodes(), loading, state.episodeSelection());
}

template <typename UiLike>
void renderContentDetails(Renderer& renderer, UiLike& ui, const JellyfinItem& item, const DetailsScreenState& state,
                          const std::vector<std::string>& actions, const AppSettings& settings,
                          bool stillWatchingPrompt, bool overlayOpen) {
    renderDetailsPresentation(renderer, ui, item, state, actions, settings, stillWatchingPrompt, overlayOpen,
                              Renderer::logicalWidth(), Renderer::logicalHeight());
}
