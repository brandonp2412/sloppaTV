#pragma once

#include "cast_renderer.hpp"
#include "diagnostics_renderer.hpp"
#include "details_renderer.hpp"
#include "media_display_text.hpp"
#include "media_grid_renderer.hpp"
#include "item_menu_renderer.hpp"
#include "queue_overlay_renderer.hpp"
#include "screensaver_renderer.hpp"
#include "settings_renderer.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"
#include "ui_theme.hpp"

#include <chrono>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

template <typename RendererLike, typename UiLike>
void renderSettingsPresentation(RendererLike& renderer, UiLike& ui, const SettingsScreenState& screen,
                                const AppSettings& settings, int maxAudioOutputChannels,
                                std::string_view externalPlayer, std::string_view username,
                                bool systemSettingsInputActive, bool seerrApiKeyTyping) {
    renderSettingsScreen(
        renderer,
        SettingsRenderState{
            .screen = screen,
            .settings = settings,
            .maxAudioOutputChannels = maxAudioOutputChannels,
            .externalPlayer = externalPlayer,
            .username = username,
            .systemSettingsInputActive = systemSettingsInputActive,
            .seerrApiKeyTyping = seerrApiKeyTyping,
        },
        SettingsRenderStyle<Color>{
            .headlineScale = material_tv::type::headline,
            .cornerMedium = material_tv::cornerMedium,
            .cornerSmall = material_tv::cornerSmall,
            .wideInputFocusScale = materialWideInputFocusScale(),
            .wideListItemFocusScale = materialWideListItemFocusScale(),
            .listItemFocusScale = materialListItemFocusScale(),
            .text = material_tv::onSurface,
            .secondaryText = material_tv::onSurfaceSecondary,
            .muted = material_tv::onSurfaceVariant,
            .focus = material_tv::primary,
            .focusSoft = material_tv::primaryContainer,
            .panelAlt = material_tv::surfaceContainer,
            .outline = material_tv::outline,
            .scrim = material_tv::scrim,
        },
        [&ui](float x, float y, float width, float height, bool focused, float focusScale) {
            return ui.drawInputSurface(x, y, width, height, focused, focusScale);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [&ui](std::string_view title, std::string_view message) { ui.renderEmptyState(title, message); },
        [&ui](float x, float y, float width, float height, bool focused, float radius, float focusScale) {
            return ui.drawListItemSurface(x, y, width, height, focused, radius, focusScale);
        },
        [](std::string_view value) { return std::string(materialLabel(value)); },
        [&ui](float x, float y, bool on, bool focused) { ui.drawSwitch(x, y, on, focused); },
        [&ui](float x, float y, std::string_view label, bool selected, float scale, float height, float maxWidth) {
            return ui.drawChip(x, y, label, selected, scale, height, maxWidth);
        },
        [&ui](float x, float y, float width, float height) { ui.drawModalSurface(x, y, width, height); });
}

template <typename RendererLike, typename UiLike, typename CodecSupportLike>
void renderDiagnosticsPresentation(RendererLike& renderer, UiLike& ui, const CodecSupportLike& codecs,
                                   std::string_view appVersion, std::string_view sessionServer,
                                   std::string_view serverName, std::string_view serverVersion, bool serverLoading,
                                   std::string_view lastPlaybackSummary) {
    std::vector<std::string> hdr;
    if (codecs.displayHdr10) hdr.emplace_back("HDR10");
    if (codecs.displayHdr10Plus) hdr.emplace_back("HDR10+");
    if (codecs.displayDolbyVision) hdr.emplace_back("DOLBY VISION");
    if (codecs.displayHlg) hdr.emplace_back("HLG");

    std::string architecture = "UNKNOWN";
#if defined(__aarch64__)
    architecture = "ARM64";
#elif defined(__arm__)
    architecture = "ARM32";
#elif defined(__x86_64__)
    architecture = "X86_64";
#endif

    renderDiagnosticsScreen(
        renderer,
        DiagnosticsScreenData{
            .appVersion = std::string(appVersion),
            .architecture = std::move(architecture),
            .sessionServer = std::string(sessionServer),
            .serverName = std::string(serverName),
            .serverVersion = std::string(serverVersion),
            .serverLoading = serverLoading,
            .videoCodecs = codecs.jellyfinVideoCodecs(),
            .audioCodecs = codecs.jellyfinAudioCodecs(codecs.maxAudioOutputChannels),
            .maxAudioOutputChannels = codecs.maxAudioOutputChannels,
            .maxHevcWidth = codecs.maxHevcWidth,
            .maxHevcHeight = codecs.maxHevcHeight,
            .hdrFormats = std::move(hdr),
            .lastPlaybackSummary = std::string(lastPlaybackSummary),
        },
        DiagnosticsRenderStyle<Color>{
            .cornerLarge = material_tv::cornerLarge,
            .panelAlt = material_tv::surfaceContainer,
            .outline = material_tv::outline,
            .divider = material_tv::outlineVariant,
            .tertiary = material_tv::onSurfaceDisabled,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
        },
        [&ui](std::string_view title) { ui.renderHeader(title); },
        [&ui](float x, float y, std::string_view label, bool selected, float scale, float height, float maxWidth) {
            return ui.drawChip(x, y, label, selected, scale, height, maxWidth);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        });
}

template <typename RendererLike, typename UiLike>
void renderQueueOverlayPresentation(RendererLike& renderer, UiLike& ui, PlaybackQueueState& queueState) {
    if (queueState.empty()) return;
    queueState.setSelection(queueState.selection());
    renderQueueOverlayScreen(
        renderer,
        QueueOverlayRenderState{
            .items = queueState.items(),
            .currentIndex = queueState.currentIndex(),
            .selection = queueState.selection(),
            .actionSelection = queueState.actionSelection(),
            .repeatMode = queueState.repeatMode(),
        },
        QueueOverlayRenderStyle<Color>{
            .artworkCornerRadius = material_tv::cornerExtraSmall,
            .scrim = material_tv::scrim,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .tertiary = material_tv::onSurfaceDisabled,
            .focusSoft = material_tv::primaryContainer,
            .panelAlt = material_tv::surfaceContainer,
        },
        [&ui](float x, float y, float width, float height) { ui.drawModalSurface(x, y, width, height); },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](float x, float y, float width, float height, bool focused) {
            return ui.drawListItemSurface(x, y, width, height, focused);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
            return ui.drawHomeArtwork(item, x, y, width, height, radius);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
            ui.drawArtworkPlaceholder(item, x, y, width, height, radius);
        },
        [](const JellyfinItem& item) { return episodeLabel(item); },
        [&ui](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
            return ui.drawButtonSurface(x, y, width, height, focused, primary, destructive);
        },
        [&ui](float x, float y, float width, float height) { ui.drawDisabledButtonSurface(x, y, width, height); },
        [](std::string_view value) { return std::string(materialLabel(value)); });
}

template <typename RendererLike, typename UiLike>
void renderScreensaverPresentation(RendererLike& renderer, UiLike& ui, bool clock24Hour, float canvasWidth,
                                   float canvasHeight) {
    const int64_t elapsedSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const std::string clock = formatLocalClock(std::time(nullptr), clock24Hour);
    renderScreensaverScreen(
        renderer, elapsedSeconds, clock, canvasWidth, canvasHeight,
        ScreensaverRenderStyle<Color>{
            .background = Color{0.006f, 0.008f, 0.012f, 1.0f},
            .primary = material_tv::primary,
            .text = material_tv::onSurface,
            .tertiary = material_tv::onSurfaceDisabled,
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        });
}

template <typename RendererLike, typename UiLike>
void renderItemMenuPresentation(RendererLike& renderer, UiLike& ui, const JellyfinItem& detail,
                                const DetailsScreenState& detailsState, const std::vector<std::string>& actions,
                                float canvasWidth, float canvasHeight) {
    renderItemMenuScreen(
        renderer, canvasWidth, canvasHeight,
        ItemMenuRenderState{
            .deleteConfirmation = detailsState.deleteConfirmation(),
            .deleteConfirmationSelection = detailsState.deleteConfirmationSelection(),
            .itemMenuSelection = detailsState.itemMenuSelection(),
            .seerrRequest = isSeerrItem(detail),
            .itemName = detail.name,
            .itemType = detail.type,
            .externalStatus = detail.externalStatus,
            .externalProgressPercent = detail.externalProgressPercent,
            .externalProgressLabel = detail.externalProgressLabel,
            .externalProgressEta = detail.externalProgressEta,
        },
        actions,
        ItemMenuRenderStyle<Color>{
            .cornerLarge = material_tv::cornerLarge,
            .scrim = material_tv::scrim,
            .error = material_tv::error,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .tertiary = material_tv::onSurfaceDisabled,
            .focus = material_tv::primary,
            .secondaryText = material_tv::onSurfaceSecondary,
            .divider = material_tv::outlineVariant,
        },
        [&ui](float x, float y, float width, float height) { ui.drawModalSurface(x, y, width, height); },
        [&ui](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
            return ui.drawButtonSurface(x, y, width, height, focused, primary, destructive);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](std::string_view value, float scale, float width, int lines) {
            return ui.fitTextLines(value, scale, width, lines);
        },
        [&ui](float x, float y, std::string_view label, bool selected, float scale, float height, float maxWidth) {
            return ui.drawChip(x, y, label, selected, scale, height, maxWidth);
        },
        [&ui](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
            return ui.drawFocusedSurface(x, y, width, height, focused, primary, destructive);
        },
        [](std::string_view value) { return materialLabel(value); });
}

template <typename UiLike, typename RenderCard>
void renderMediaGridPresentation(std::string_view title, const std::vector<JellyfinItem>& items, bool loading,
                                 int selection, int uiTextSize, UiLike& ui, RenderCard&& renderCard) {
    renderMediaGridScreen(
        std::string(title), items,
        MediaGridRenderState{
            .loading = loading,
            .selection = selection,
            .uiTextSize = uiTextSize,
        },
        [&ui](std::string_view heading) { ui.renderHeader(heading); },
        [&ui](std::string_view emptyTitle, std::string_view message) { ui.renderEmptyState(emptyTitle, message); },
        std::forward<RenderCard>(renderCard));
}

template <typename RendererLike, typename UiLike>
void renderDetailsPresentation(RendererLike& renderer, UiLike& ui, const JellyfinItem& detail,
                               const DetailsScreenState& detailsState, const std::vector<std::string>& actions,
                               const AppSettings& settings, bool stillWatchingPrompt, bool overlayOpen,
                               float canvasWidth, float canvasHeight) {
    renderDetailsScreen(
        renderer, detail, detailsState, actions,
        DetailsRenderConfig{
            .showClock = settings.showClock,
            .clock24Hour = settings.clock24Hour,
            .showWatchedIndicators = settings.showWatchedIndicators,
            .uiTextSize = settings.uiTextSize,
            .stillWatchingPrompt = stillWatchingPrompt,
            .overlayOpen = overlayOpen,
        },
        DetailsRenderStyle<Color>{
            .canvasWidth = canvasWidth,
            .canvasHeight = canvasHeight,
            .cornerLarge = material_tv::cornerLarge,
            .cornerExtraSmall = material_tv::cornerExtraSmall,
            .cardFocusScale = materialCardFocusScale(),
            .labelScale = material_tv::type::label,
            .background = material_tv::background,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .secondaryText = material_tv::onSurfaceSecondary,
            .focus = material_tv::primary,
            .panelElevated = material_tv::surfaceContainerHigh,
            .outline = material_tv::outline,
            .track = material_tv::track,
            .backdropHorizontalStart = Color{0.0f, 0.0f, 0.0f, 0.92f},
            .backdropHorizontalEnd = Color{0.0f, 0.0f, 0.0f, 0.03f},
            .backdropVerticalStart = Color{0.0f, 0.0f, 0.0f, 0.08f},
            .backdropVerticalEnd = Color{0.0f, 0.0f, 0.0f, 0.92f},
        },
        [&ui](const JellyfinItem& item, float alpha) { return ui.drawBackdrop(item, alpha); },
        [&ui](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
            ui.drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
        },
        [](bool clock24Hour) { return formatLocalClock(std::time(nullptr), clock24Hour); },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        },
        [&ui](const JellyfinItem& item, float x, float y, float width, float height) {
            return ui.drawLogo(item, x, y, width, height);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [](const JellyfinItem& item) { return episodeNumberLabel(item); },
        [](const JellyfinItem& item) { return episodeLabel(item); },
        [](int milliseconds) { return formatPlaybackTime(milliseconds); },
        [&ui](float x, float y, std::string_view value, bool active, float scale, float height, float maxWidth) {
            return ui.drawChip(x, y, value, active, scale, height, maxWidth);
        },
        [](std::string_view value) { return materialLabel(value); },
        [&ui](float x, float y, float width, float height, bool focused, bool primary) {
            return ui.drawButtonSurface(x, y, width, height, focused, primary);
        },
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
        });
}

template <typename RendererLike, typename UiLike>
void renderCastPresentation(RendererLike& renderer, UiLike& ui, const JellyfinItem& detail,
                            const DetailsScreenState& detailsState, int uiTextSize) {
    renderCastScreen(
        renderer, detail.name, detail.people, detailsState.castSelection(), uiTextSize,
        CastRenderStyle<Color>{
            .cornerSmall = material_tv::cornerSmall,
            .labelScale = material_tv::type::label,
            .supportingScale = material_tv::type::supporting,
            .panelAlt = material_tv::surfaceContainer,
            .focus = material_tv::primary,
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
            .tertiary = material_tv::onSurfaceDisabled,
        },
        [&ui](std::string_view heading) { ui.renderHeader(heading); },
        [&ui](std::string_view title, std::string_view message) { ui.renderEmptyState(title, message); },
        [&ui](float x, float y, float width, float height, bool focused) {
            return ui.focusedBounds(x, y, width, height, focused);
        },
        [&ui](const JellyfinPerson& person, float x, float y, float width, float height, float radius) {
            JellyfinItem artworkItem;
            artworkItem.id = person.id;
            artworkItem.name = person.name;
            artworkItem.type = "Person";
            artworkItem.imageTag = person.imageTag;
            if (!ui.drawArtwork(artworkItem, x, y, width, height, 1.0f, radius))
                ui.drawArtworkPlaceholder(artworkItem, x, y, width, height, radius);
        },
        [&ui](float x, float y, float width, float height, Color color, float radius) {
            ui.drawFocusHalo(x, y, width, height, color, radius);
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitTextLines(value, scale, maxWidth, maxLines);
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color,
              float horizontalPadding, float verticalPadding) {
            ui.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
        });
}
