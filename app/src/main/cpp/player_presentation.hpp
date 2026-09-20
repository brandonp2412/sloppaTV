#pragma once

#include "app_settings.hpp"
#include "artwork_provider.hpp"
#include "media_display_text.hpp"
#include "media_player.hpp"
#include "playback_coordinator.hpp"
#include "player_controls_renderer.hpp"
#include "player_header_renderer.hpp"
#include "player_next_up_renderer.hpp"
#include "player_progress_renderer.hpp"
#include "player_screen.hpp"
#include "player_seek_feedback_renderer.hpp"
#include "player_skip_button_renderer.hpp"
#include "player_status_renderer.hpp"
#include "player_subtitle_renderer.hpp"
#include "player_trickplay_renderer.hpp"
#include "player_video_renderer.hpp"
#include "renderer.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "text_layout.hpp"
#include "trickplay_policy.hpp"
#include "trickplay_preview.hpp"
#include "ui_components.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"
#include "ui_theme.hpp"
#include "video_surface.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <ctime>
#include <string>
#include <string_view>

inline std::string mediaSegmentSkipLabel(const JellyfinMediaSegment& segment) {
    if (segment.type == "Intro") return "Skip intro";
    if (segment.type == "Outro") return "Skip credits";
    if (segment.type == "Recap") return "Skip recap";
    if (segment.type == "Preview") return "Skip preview";
    if (segment.type == "Commercial") return "Skip commercial";
    return "Skip";
}

template <typename ArtworkLike> class PlayerPresentationUi {
public:
    PlayerPresentationUi(Renderer& renderer, ArtworkLike& artwork, const JellyfinSession& session,
                         const AppSettings& settings, std::chrono::steady_clock::time_point lastInteraction)
        : renderer_(renderer), artwork_(artwork), session_(session), settings_(settings),
          lastInteraction_(lastInteraction) {}

    [[nodiscard]] std::string fitText(std::string_view value, float scale, float maxWidth, int maxLines) const {
        return fitRenderedTextLines(renderer_, value, scale, maxWidth, maxLines);
    }

    [[nodiscard]] float fittedSingleLineScale(float scale, std::string_view value, float width, float height) const {
        float fittedScale = scale;
        const float measuredWidth = renderer_.textWidth(fittedScale, value);
        const float visualHeight = 10.0f * fittedScale * uiTextScale(settings_.uiTextSize);
        float fit = 1.0f;
        if (measuredWidth > width && measuredWidth > 0.0f) fit = std::min(fit, width / measuredWidth);
        if (visualHeight > height && visualHeight > 0.0f) fit = std::min(fit, height / visualHeight);
        return fittedScale * fit;
    }

    void drawRightAlignedSingleLine(float right, float y, float scale, std::string_view value, Color color,
                                    float maxWidth) {
        float fittedScale = scale;
        float width = renderer_.textWidth(fittedScale, value);
        if (maxWidth > 0.0f && width > maxWidth && width > 0.0f) {
            fittedScale *= maxWidth / width;
            width = renderer_.textWidth(fittedScale, value);
        }
        renderer_.text(right - width, y, fittedScale, value, color);
    }

    void drawLeftAlignedSingleLineFit(float x, float y, float width, float height, float scale, std::string_view value,
                                      Color color) {
        const float fittedScale = fittedSingleLineScale(scale, value, width, height);
        renderer_.textVerticallyCentered(x, y, height, fittedScale, fitText(value, fittedScale, width, 1), color,
                                         width);
    }

    [[nodiscard]] std::array<float, 4> drawButtonSurface(float x, float y, float width, float height, bool focused,
                                                         bool primary = false) {
        const auto bounds = focusedBounds(x, y, width, height, focused, materialButtonFocusScale());
        const float radius = std::min(material_tv::cornerLarge, height * 0.5f);
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        const Color surface = primary
                                  ? material_tv::primaryContainer
                                  : (focused ? material_tv::surfaceContainerHighest : material_tv::surfaceContainer);
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, surface);
        if (!focused && !primary) {
            renderer_.roundedOutline(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, 1.5f,
                                     material_tv::outline);
        }
        if (focused) {
            material_tv::focusRing(renderer_, bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius,
                                   material_tv::primary);
        }
        return bounds;
    }

    void drawModalSurface(float x, float y, float width, float height, float radius) {
        material_tv::dialog(renderer_, x, y, width, height, radius);
    }

    [[nodiscard]] bool drawHomeArtwork(const JellyfinItem& item, float x, float y, float width, float height,
                                       float radius) {
        ArtworkEntry* entry = artwork_.homeTexture(session_, item, isSeerrItem(item), renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, width, height, radius);
        return true;
    }

    void drawArtworkPlaceholder(const JellyfinItem& item, float x, float y, float width, float height, float radius) {
        renderer_.roundedRect(x, y, width, height, radius, material_tv::surfaceContainer);
        renderer_.roundedOutline(x, y, width, height, radius, 1.0f, material_tv::outline);
        const std::string& source = item.type == "Episode" && !item.seriesName.empty() ? item.seriesName : item.name;
        std::string initial = "?";
        const auto first =
            std::find_if(source.begin(), source.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
        if (first != source.end()) {
            initial.assign(1, static_cast<char>(std::toupper(static_cast<unsigned char>(*first))));
        }
        const float scale = height < 100.0f ? 2.0f : (height < 200.0f ? 3.0f : 4.2f);
        renderer_.textCentered(x, y, width, height, scale, initial, material_tv::onSurfaceVariant);
    }

private:
    [[nodiscard]] std::array<float, 4> focusedBounds(float x, float y, float width, float height, bool focused,
                                                     float scale) const {
        if (!focused) return {x, y, width, height};
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastInteraction_)
                .count();
        float animatedScale = scale;
        if (elapsedMs >= 0 && elapsedMs < 150) {
            const float t = std::clamp(static_cast<float>(elapsedMs) / 150.0f, 0.0f, 1.0f);
            const float remaining = 1.0f - t;
            const float eased = 1.0f - remaining * remaining * remaining;
            animatedScale = 1.0f + (scale - 1.0f) * eased;
        }
        const float scaledWidth = width * animatedScale;
        const float scaledHeight = height * animatedScale;
        return {
            x - (scaledWidth - width) * 0.5f,
            y - (scaledHeight - height) * 0.5f,
            scaledWidth,
            scaledHeight,
        };
    }

    void drawCoverTexture(const ArtworkEntry& entry, float x, float y, float width, float height, float radius) {
        const int sourceWidth = entry.sourceWidth > 0 ? entry.sourceWidth : entry.decoded.width;
        const int sourceHeight = entry.sourceHeight > 0 ? entry.sourceHeight : entry.decoded.height;
        if (entry.texture == 0 || sourceWidth <= 0 || sourceHeight <= 0 || width <= 0.0f || height <= 0.0f) return;
        const float sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
        const float targetAspect = width / height;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        if (sourceAspect > targetAspect) {
            const float visible = targetAspect / sourceAspect;
            u0 = (1.0f - visible) * 0.5f;
            u1 = u0 + visible;
        } else if (sourceAspect < targetAspect) {
            const float visible = sourceAspect / targetAspect;
            v0 = (1.0f - visible) * 0.5f;
            v1 = v0 + visible;
        }
        renderer_.roundedImageRegion(entry.texture, x, y, width, height, radius, u0, v0, u1, v1);
    }

    Renderer& renderer_;
    ArtworkLike& artwork_;
    const JellyfinSession& session_;
    const AppSettings& settings_;
    std::chrono::steady_clock::time_point lastInteraction_;
};

template <typename ArtworkLike>
bool renderPlayerTrickplayPresentation(Renderer& renderer, PlayerPresentationUi<ArtworkLike>& ui,
                                       PlaybackCoordinator& playbackCoordinator, PlayerScreenState& playerScreenState,
                                       TrickplayPreviewState& trickplayState) {
    if (!trickplayState.visible(std::chrono::steady_clock::now(), playbackCoordinator.session().activeItem().id) ||
        !playbackCoordinator.session().activeItem().trickplay.valid()) {
        return false;
    }
    const auto& info = playbackCoordinator.session().activeItem().trickplay;
    const TrickplayFrame frame = trickplayFrameForPosition(trickplayState.positionMs(), info.intervalMs,
                                                           info.thumbnailCount, info.tileWidth, info.tileHeight);
    if (!frame.valid() || frame.tileIndex != trickplayState.tileIndex()) return false;
    if (trickplayState.texture() == 0 || trickplayState.textureGeneration() != renderer.generation()) {
        const auto& decoded = trickplayState.decoded();
        trickplayState.setTexture(renderer.createTexture(decoded.width, decoded.height, decoded.rgba.data()),
                                  renderer.generation());
    }
    if (trickplayState.texture() == 0) return false;

    const auto& decoded = trickplayState.decoded();
    const std::string positionLabel = formatPlaybackTime(trickplayState.positionMs());
    return renderPlayerTrickplay(
        renderer,
        PlayerTrickplayRenderState{
            .texture = trickplayState.texture(),
            .frame = frame,
            .info = info,
            .decodedWidth = decoded.width,
            .decodedHeight = decoded.height,
            .positionMs = trickplayState.positionMs(),
            .durationMs = playerScreenState.durationMs(),
            .logicalWidth = Renderer::logicalWidth(),
            .positionLabel = positionLabel,
        },
        PlayerTrickplayRenderStyle<Color>{
            .previewRadius = material_tv::cornerSmall,
            .backdrop = Color{0.0f, 0.0f, 0.0f, 0.90f},
            .focus = material_tv::primary,
            .text = material_tv::onSurface,
        },
        [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
            ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
        });
}

template <typename ArtworkLike>
void renderPlayerPresentation(Renderer& renderer, NativeMediaPlayer& player, VideoSurface& videoSurface,
                              PlaybackCoordinator& playbackCoordinator, PlayerScreenState& playerScreenState,
                              TrickplayPreviewState& trickplayState, const AppSettings& settings,
                              const JellyfinSession& session, ArtworkLike& artwork,
                              std::chrono::steady_clock::time_point lastInteraction) {
    PlayerPresentationUi<ArtworkLike> ui(renderer, artwork, session, settings, lastInteraction);
    const PlayerStatus status = player.status();
    std::string videoError;
    if (videoSurface.ready()) {
        videoSurface.update(videoError);
        renderPlayerVideo(renderer,
                          PlayerVideoRenderState{
                              .texture = videoSurface.texture(),
                              .sourceWidth = player.videoWidth(),
                              .sourceHeight = player.videoHeight(),
                              .zoomMode = playbackCoordinator.session().zoomMode(),
                              .logicalWidth = Renderer::logicalWidth(),
                              .logicalHeight = Renderer::logicalHeight(),
                          },
                          videoSurface.transform());
    }

    const auto now = std::chrono::steady_clock::now();
    const int remainingMs = playerScreenState.durationMs() > 0
                                ? std::max(0, playerScreenState.durationMs() - playerScreenState.positionMs())
                                : 0;
    const auto skipSegment = playbackCoordinator.activeSkippableSegment(playerScreenState.positionMs());
    const bool userOverlayVisible = playerScreenState.overlayVisible(now);
    const bool showNextUp = shouldShowNextUpCard(playbackCoordinator.continuation().nextItem().has_value(), remainingMs,
                                                 userOverlayVisible, skipSegment != nullptr);
    const bool showOverlay = status == PlayerStatus::Preparing || status == PlayerStatus::Paused ||
                             playbackCoordinator.transitionLoading() || playbackCoordinator.fallbackResolving() ||
                             userOverlayVisible;
    if (showOverlay) {
        renderer.verticalGradient(0.0f, 0.0f, 1920.0f, 250.0f, Color{0.0f, 0.0f, 0.0f, 0.74f},
                                  Color{0.0f, 0.0f, 0.0f, 0.0f});
        renderer.verticalGradient(0.0f, 650.0f, 1920.0f, 430.0f, Color{0.0f, 0.0f, 0.0f, 0.0f},
                                  Color{0.0f, 0.0f, 0.0f, 0.90f});
    }

    std::string subtitleText = player.subtitleText();
    if (const SubtitleCue* cue = playbackCoordinator.activeSubtitleCue(playerScreenState.positionMs())) {
        subtitleText = cue->text;
    }
    if (!subtitleText.empty()) {
        const float textScale = subtitleTextScale(settings.subtitleSize);
        renderPlayerSubtitle(renderer,
                             PlayerSubtitleRenderState{
                                 .text = subtitleText,
                                 .boxMaxWidth = subtitleBoxMaxWidth(skipSegment != nullptr),
                                 .textScale = textScale,
                                 .lineHeight = 11.0f * textScale * uiTextScale(settings.uiTextSize),
                                 .logicalWidth = Renderer::logicalWidth(),
                                 .bottomY = subtitleBottomY(showOverlay, playerScreenState.controlsActive(now),
                                                            settings.subtitlePosition, skipSegment != nullptr),
                                 .showBackground = settings.subtitleBackground,
                             },
                             PlayerSubtitleRenderStyle<Color>{
                                 .cornerRadius = material_tv::cornerMedium,
                                 .text = material_tv::onSurface,
                                 .background = Color{0.0f, 0.0f, 0.0f, 0.80f},
                                 .outline = Color{0.0f, 0.0f, 0.0f, 0.92f},
                             },
                             [&ui](std::string value, float scale, float maxWidth, int maxLines) {
                                 return ui.fitText(value, scale, maxWidth, maxLines);
                             });
    }

    if (skipSegment) {
        const std::string skipLabel = mediaSegmentSkipLabel(*skipSegment);
        renderPlayerSkipButton(
            renderer,
            PlayerSkipButtonRenderState{
                .label = skipLabel,
                .y = skipButtonY(showOverlay),
            },
            PlayerSkipButtonRenderStyle<Color>{.text = material_tv::onSurface},
            [&ui](float x, float y, float width, float height, bool focused, bool primary) {
                return ui.drawButtonSurface(x, y, width, height, focused, primary);
            },
            [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
                return ui.fitText(value, scale, maxWidth, maxLines);
            });
    }

    if (playerScreenState.seekFeedbackVisible(now)) {
        renderPlayerSeekFeedback(renderer,
                                 PlayerSeekFeedbackRenderState{
                                     .seconds = playerScreenState.seekFeedbackSeconds(),
                                     .fade = playerScreenState.seekFeedbackAlpha(now),
                                 },
                                 [](float r, float g, float b, float a) { return Color{r, g, b, a}; });
    }
    if (!showOverlay) return;

    const auto nextItem = playbackCoordinator.continuation().nextItem();
    if (showNextUp && nextItem) {
        renderPlayerNextUp(
            PlayerNextUpRenderState{
                .item = *nextItem,
                .remainingMs = remainingMs,
            },
            PlayerNextUpRenderStyle<Color>{
                .panelCornerRadius = material_tv::cornerMedium,
                .artworkCornerRadius = material_tv::cornerExtraSmall,
                .focus = material_tv::primary,
                .text = material_tv::onSurface,
                .muted = material_tv::onSurfaceVariant,
            },
            [&ui](float x, float y, float width, float height, float radius) {
                ui.drawModalSurface(x, y, width, height, radius);
            },
            [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                return ui.drawHomeArtwork(item, x, y, width, height, radius);
            },
            [&ui](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                ui.drawArtworkPlaceholder(item, x, y, width, height, radius);
            },
            [&ui](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                ui.drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            },
            [](const JellyfinItem& item) { return episodeLabel(item); });
    }

    const auto& activeItem = playbackCoordinator.session().activeItem();
    const std::string heading = activeItem.seriesName.empty() ? activeItem.name : activeItem.seriesName;
    const std::string playerEpisodeNumber = episodeNumberLabel(activeItem);
    const std::string secondary =
        activeItem.seriesName.empty()
            ? episodeLabel(activeItem)
            : playerEpisodeNumber + (activeItem.name.empty() ? "" : "  |  " + activeItem.name);
    renderPlayerHeader(
        renderer,
        PlayerHeaderRenderState{
            .heading = heading,
            .secondary = secondary,
            .showNextUp = showNextUp,
            .headlineScale = material_tv::type::headline,
            .secondaryY = 42.0f + 11.0f * material_tv::type::headline * uiTextScale(settings.uiTextSize) + 8.0f,
        },
        PlayerHeaderRenderStyle<Color>{
            .text = material_tv::onSurface,
            .muted = material_tv::onSurfaceVariant,
        },
        [&ui](std::string_view value, float scale, float maxWidth, int maxLines) {
            return ui.fitText(value, scale, maxWidth, maxLines);
        });

    const int position = playerScreenState.positionMs();
    const int duration = playerScreenState.durationMs();
    std::string clockText;
    std::string finishLabel;
    if (settings.showClock) {
        const std::time_t wallNow = std::time(nullptr);
        clockText = formatLocalClock(wallNow, settings.clock24Hour);
        if (remainingMs > 0 && status == PlayerStatus::Playing && !skipSegment) {
            const std::time_t finishAt = wallNow + static_cast<std::time_t>((remainingMs + 999) / 1000);
            finishLabel = "Ends " + formatLocalClock(finishAt, settings.clock24Hour);
        }
    }
    const std::string state =
        playbackCoordinator.fallbackResolving()
            ? "Retrying playback"
            : (playbackCoordinator.transitionLoading() ? (activeItem.id.empty() ? "Loading episode" : "Switching track")
                                                       : (status == PlayerStatus::Preparing ? "Loading" : ""));
    renderPlayerStatus(renderer,
                       PlayerStatusRenderState{
                           .clockText = clockText,
                           .finishText = finishLabel,
                           .statusText = state,
                       },
                       PlayerStatusRenderStyle<Color>{
                           .muted = material_tv::onSurfaceVariant,
                           .secondary = material_tv::onSurfaceSecondary,
                       },
                       [&ui](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
                           ui.drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
                       });

    const std::string positionText = formatPlaybackTime(position);
    const std::string durationText = formatPlaybackTime(duration);
    renderPlayerProgress(renderer,
                         PlayerProgressRenderState{
                             .positionMs = position,
                             .durationMs = duration,
                             .skipButtonVisible = skipSegment != nullptr,
                             .positionText = positionText,
                             .durationText = durationText,
                         },
                         PlayerProgressRenderStyle<Color>{
                             .text = material_tv::onSurface,
                             .track = material_tv::track,
                             .focus = material_tv::primary,
                         },
                         [&ui](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
                             ui.drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
                         });

    renderPlayerTrickplayPresentation(renderer, ui, playbackCoordinator, playerScreenState, trickplayState);

    if (playerScreenState.controlsActive(now)) {
        renderPlayerControls(
            renderer,
            PlayerControlsRenderState{
                .paused = status == PlayerStatus::Paused,
                .selection = playerScreenState.controlSelection(),
                .logicalWidth = Renderer::logicalWidth(),
                .audioTrackLabel = playbackCoordinator.trackLabel(PlaybackTrackLabelKind::Audio),
                .subtitleTrackLabel = playbackCoordinator.trackLabel(PlaybackTrackLabelKind::Subtitle),
            },
            PlayerControlsRenderStyle<Color>{.text = material_tv::onSurface},
            [&ui](float x, float y, float width, float height, bool focused, bool primary) {
                return ui.drawButtonSurface(x, y, width, height, focused, primary);
            },
            [&ui](float scale, std::string_view value, float width, float height) {
                return ui.fittedSingleLineScale(scale, value, width, height);
            },
            [](std::string_view value) { return materialLabel(value); });
    }
}
