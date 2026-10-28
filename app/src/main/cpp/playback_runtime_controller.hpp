#pragma once

#include "app_settings.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "media_session.hpp"
#include "media_session_policy.hpp"
#include "playback_continuation_executor.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_resolver.hpp"
#include "playback_stream_executor.hpp"
#include "player_screen.hpp"
#include "request_epoch.hpp"
#include "subtitle_load_executor.hpp"
#include "subtitle_policy.hpp"
#include "video_surface.hpp"

#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <mutex>
#include <limits>
#include <string>
#include <utility>
#include <vector>

enum class PlaybackRuntimeNotice {
    None,
    SubtitleStartFailed,
};

enum class PlaybackHostEffectType {
    None,
    StopPlayback,
    OpenQueue,
    PlayAdjacentEpisode,
    SeekWithTrickplay,
    PlayQueueIndex,
    QueueAutoplayNext,
    ShowStillWatching,
    SubtitleStartFailed,
};

struct PlaybackHostEffect {
    PlaybackHostEffectType type = PlaybackHostEffectType::None;
    int positionMs = 0;
    int queueIndex = -1;
    int episodeDirection = 0;
    bool restartCurrent = false;
    bool replacingCompleted = false;
    bool completed = false;
    bool resetAutoplayChain = false;
    std::optional<JellyfinItem> item;

    static PlaybackHostEffect simple(PlaybackHostEffectType type) {
        PlaybackHostEffect effect;
        effect.type = type;
        return effect;
    }

    static PlaybackHostEffect adjacentEpisode(int direction) {
        PlaybackHostEffect effect = simple(PlaybackHostEffectType::PlayAdjacentEpisode);
        effect.episodeDirection = direction;
        return effect;
    }

    static PlaybackHostEffect seekWithTrickplay(int positionMs) {
        PlaybackHostEffect effect = simple(PlaybackHostEffectType::SeekWithTrickplay);
        effect.positionMs = positionMs;
        return effect;
    }

    static PlaybackHostEffect playQueueIndex(int index, bool restartCurrent = false, bool replacingCompleted = false) {
        PlaybackHostEffect effect = simple(PlaybackHostEffectType::PlayQueueIndex);
        effect.queueIndex = index;
        effect.restartCurrent = restartCurrent;
        effect.replacingCompleted = replacingCompleted;
        return effect;
    }

    static PlaybackHostEffect withItem(PlaybackHostEffectType type, std::optional<JellyfinItem> item) {
        PlaybackHostEffect effect = simple(type);
        effect.item = std::move(item);
        return effect;
    }

    static PlaybackHostEffect stop(bool completed = false, bool resetAutoplayChain = false) {
        PlaybackHostEffect effect = simple(PlaybackHostEffectType::StopPlayback);
        effect.completed = completed;
        effect.resetAutoplayChain = resetAutoplayChain;
        return effect;
    }
};

class PlaybackRuntimeController {
public:
    PlaybackRuntimeController(PlaybackCoordinator& coordinator, PlayerScreenState& screenState,
                              NativeMediaPlayer& player, VideoSurface& videoSurface, JellyfinSession& session,
                              AppSettings& settings, RequestEpoch& playbackEpoch, bool& loading, std::string& error,
                              const std::string& dataPath, const JellyfinServerInfo* serverInfo = nullptr)
        : coordinator_(coordinator), screenState_(screenState), player_(player), videoSurface_(videoSurface),
          session_(session), settings_(settings), playbackEpoch_(playbackEpoch),
#ifndef SLOPPATV_BENCHMARK
          loading_(loading),
#endif
          error_(error), dataPath_(dataPath), serverInfo_(serverInfo) {
#ifdef SLOPPATV_BENCHMARK
        (void)loading;
#endif
    }

    [[nodiscard]] PlaybackTrackSelectionPolicy trackSelectionPolicy() const {
        return {
            .autoSubtitles = settings_.autoSubtitles,
            .autoSubtitleLanguage = settings_.autoSubtitleLanguage,
            .autoSubtitleSourceLanguage = settings_.autoSubtitleSourceLanguage,
            .allowedSubtitleLanguages = settings_.subtitleLanguages,
            .server = session_.server,
            .userId = session_.userId,
            .seriesTrackPreferences = settings_.seriesTrackPreferences,
        };
    }

    [[nodiscard]] bool isRemuxServer() const { return serverInfo_ && serverInfo_->isRemux; }

    [[nodiscard]] int serverAudioChannels() const {
        return ::serverAudioChannels(serverInfo_ && serverInfo_->isRemux, settings_.maxAudioChannels);
    }

    [[nodiscard]] PlaybackResolutionOptions resolutionOptions() const {
        const PlaybackLanguagePreferences preferences = coordinator_.languagePreferences();
        return {
            .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
            .maxAudioChannels = serverAudioChannels(),
            .isRemux = isRemuxServer(),
            .overrides = playbackOverridesFor(settings_),
            .preferredMediaSources = settings_.preferredMediaSources,
            .audioLanguagePreference = preferences.audio,
            .subtitleLanguagePreference = preferences.subtitle,
            .trackPolicy = trackSelectionPolicy(),
        };
    }

    [[nodiscard]] PlaybackDecodeMode activeDecodeMode() const {
        const std::string& itemId = coordinator_.session().activeItem().id;
        return !itemId.empty() && itemId == softwareDecodeItemId_ ? PlaybackDecodeMode::Software
                                                                  : PlaybackDecodeMode::Hardware;
    }

    void refreshTelemetry(bool force = false) {
        const auto now = std::chrono::steady_clock::now();
        const PlaybackTelemetryReadPlan plan = coordinator_.consumeTelemetryRead(now, force, screenState_.durationMs());
        if (!plan.read) return;
        screenState_.applyObservedPosition(coordinator_.logicalPositionFromPlayerPosition(player_.positionMs()), now);
        if (plan.knownDurationMs > 0) {
            screenState_.setDurationMs(plan.knownDurationMs);
        } else if (plan.probeDuration) {
            const int duration = player_.durationMs();
            if (duration > 0) screenState_.setDurationMs(duration);
        }
    }

    template <typename TelemetryExecutor>
    void reportProgress(TelemetryExecutor& telemetry, bool playerScreenActive, bool immediate) {
        const PlayerStatus status = player_.status();
        const PlaybackProgressContext progress = coordinator_.progressContext(
            playerScreenActive, session_.valid(), immediate, status == PlayerStatus::Preparing,
            status == PlayerStatus::Paused, screenState_.positionMs());
        if (!progress.plan.report) return;
        telemetry.reportProgress(session_, progress.item, progress.target, progress.plan.ticks, progress.plan.paused);
    }

    template <typename SubtitleExecutor>
    bool loadSubtitle(SubtitleExecutor& subtitles, const JellyfinSubtitleStream& subtitle,
                      const std::string& deliveryUrl = {}) {
        if (!session_.valid()) return true;
        auto context = coordinator_.beginSubtitleLoadContext(subtitle, settings_.subtitleLanguages);
        if (!context) return true;
        const uint64_t generation = playbackEpoch_.snapshot();
        if (subtitles.load(session_, SubtitleLoadRequest{
                                         .generation = generation,
                                         .itemId = std::move(context->itemId),
                                         .mediaSourceId = std::move(context->mediaSourceId),
                                         .requestedSubtitleIndex = context->requestedSubtitleStreamIndex,
                                         .candidates = std::move(context->candidates),
                                         .deliveryUrl = deliveryUrl,
                                         .dataPath = dataPath_,
                                     })) {
            return true;
        }
        coordinator_.failSubtitleLoad();
        return false;
    }

    template <typename StreamExecutor>
    bool restartAt(StreamExecutor& stream, int positionMs, int audioStreamIndex, int subtitleStreamIndex,
                   const std::string& replacementMediaSourceId = {}) {
        if (!session_.valid()) return false;
        const int targetPositionMs = std::max(0, positionMs);
        const bool wasPaused = player_.status() == PlayerStatus::Paused;
        auto restartPlan = coordinator_.beginStreamRestart(targetPositionMs);
        if (!restartPlan) return false;

        JellyfinItem item = std::move(restartPlan->item);
        if (!replacementMediaSourceId.empty()) item.mediaSourceId = replacementMediaSourceId;
        __android_log_print(ANDROID_LOG_INFO, "SloppaTV",
                            "Playback restart item=%s title=%s series=%s S%dE%d preferredSource=%s atMs=%d",
                            item.id.c_str(), item.name.c_str(), item.seriesId.c_str(), item.parentIndexNumber,
                            item.indexNumber, item.mediaSourceId.empty() ? "(auto)" : item.mediaSourceId.c_str(),
                            targetPositionMs);
        const PlaybackTarget previousTarget = std::move(restartPlan->previousTarget);
        const bool shouldReportPrevious = restartPlan->reportPrevious;
        const uint64_t generation = playbackEpoch_.begin();

        screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(10));
        screenState_.setPositionMs(targetPositionMs);
        player_.stop();
        videoSurface_.release();

        if (stream.restart(session_, std::move(item), previousTarget, shouldReportPrevious,
                           PlaybackStreamResolutionOptions{
                               .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
                               .maxAudioChannels = serverAudioChannels(),
                               .overrides = playbackOverridesFor(settings_),
                               .audioStreamIndex = audioStreamIndex,
                               .subtitleStreamIndex = subtitleStreamIndex,
                           },
                           wasPaused, generation))
            return true;
        playbackEpoch_.invalidate();
        coordinator_.finishStreamRestartRequest();
        error_ = "STREAM RESTART COULD NOT BE STARTED";
        return false;
    }

    template <typename StreamExecutor, typename TelemetryExecutor>
    void cycleAudioTrack(StreamExecutor& stream, TelemetryExecutor& telemetry, bool playerScreenActive,
                         std::optional<int> requestedIndex = std::nullopt) {
        const PlaybackAudioCyclePlan plan =
            requestedIndex ? coordinator_.beginAudioTrackSelection(*requestedIndex, trackSelectionPolicy())
                           : coordinator_.beginAudioTrackCycle(trackSelectionPolicy());
        if (!plan.available) {
            error_ = "ONLY ONE AUDIO TRACK";
            return;
        }

        refreshTelemetry(true);
        const int switchPositionMs = screenState_.positionMs();
        if (plan.tryEmbeddedSwitch && player_.selectEmbeddedAudioStream(plan.audioStreamIndex, plan.audioOrdinal)) {
            coordinator_.selectAudioStream(plan.audioStreamIndex);
            screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(4));
            reportProgress(telemetry, playerScreenActive, false);
            return;
        }
        static_cast<void>(restartAt(stream, switchPositionMs, plan.audioStreamIndex, plan.subtitleStreamIndex));
    }

    template <typename SubtitleExecutor, typename StreamExecutor, typename TelemetryExecutor>
    PlaybackRuntimeNotice cycleSubtitleTrack(SubtitleExecutor& subtitles, StreamExecutor& stream,
                                             TelemetryExecutor& telemetry, bool playerScreenActive,
                                             std::optional<int> requestedIndex = std::nullopt) {
        const PlaybackSubtitleCycleContext cycle =
            requestedIndex ? coordinator_.beginSubtitleTrackSelection(*requestedIndex, settings_.subtitleLanguages)
                           : coordinator_.beginSubtitleTrackCycle(settings_.subtitleLanguages);
        const PlaybackSubtitleCyclePlan& plan = cycle.plan;
        if (cycle.busy) {
            if (plan.action == PlaybackSubtitleCycleAction::NoSubtitles) error_ = "NO SUBTITLE TRACKS";
            return PlaybackRuntimeNotice::None;
        }
        if (plan.action == PlaybackSubtitleCycleAction::NoSubtitles) {
            error_ = "NO SUBTITLE TRACKS";
            return PlaybackRuntimeNotice::None;
        }
        if (plan.action == PlaybackSubtitleCycleAction::NoAllowedTracks) {
            error_ = "NO ALLOWED SUBTITLE TRACKS";
            return PlaybackRuntimeNotice::None;
        }

        if (plan.action == PlaybackSubtitleCycleAction::DisableInPlayer && player_.disableSubtitles()) {
            coordinator_.disableSubtitleRendering();
            screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(4));
            reportProgress(telemetry, playerScreenActive, false);
            return PlaybackRuntimeNotice::None;
        }

        if (plan.directPlayStream) {
            const JellyfinSubtitleStream& selected = *plan.directPlayStream;
            const PlaybackMethod activeMethod = coordinator_.session().activeTarget().playMethod;
            const bool canCompositeEmbeddedBitmap =
                activeDecodeMode() == PlaybackDecodeMode::Software && selected.discoveredByPlayer &&
                activeMethod == PlaybackMethod::DirectPlay &&
                canSwitchEmbeddedSubtitleInPlayer(plan.strategy, selected.isExternal);
            if ((usePlayerDiscoveredTextTrack(selected, activeMethod) || canCompositeEmbeddedBitmap) &&
                player_.selectEmbeddedSubtitleStream(selected.index)) {
                coordinator_.selectSubtitleStream(selected.index);
                screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(4));
                error_.clear();
                reportProgress(telemetry, playerScreenActive, false);
                return PlaybackRuntimeNotice::None;
            }
            __android_log_print(ANDROID_LOG_INFO, "SloppaTV",
                                "Selecting subtitle stream=%d codec=%s external=%d strategy=%d",
                                plan.subtitleStreamIndex, selected.codec.c_str(), selected.isExternal ? 1 : 0,
                                static_cast<int>(plan.strategy));
            if (plan.action == PlaybackSubtitleCycleAction::LoadNative) {
                player_.disableSubtitles();
                coordinator_.prepareNativeSubtitleLoad(plan.subtitleStreamIndex);
                if (!loadSubtitle(subtitles, selected, selected.deliveryUrl))
                    return PlaybackRuntimeNotice::SubtitleStartFailed;
                screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(4));
                reportProgress(telemetry, playerScreenActive, false);
                return PlaybackRuntimeNotice::None;
            }
            if (plan.strategy == SubtitleStrategy::ClientEmbedded) {
                __android_log_print(ANDROID_LOG_INFO, "SloppaTV",
                                    "Bitmap subtitle stream=%d requires server burn-in with mediacodec_embed",
                                    plan.subtitleStreamIndex);
            }
        }

        refreshTelemetry(true);
        const std::string currentMediaSourceId = coordinator_.session().activeTarget().mediaSourceId;
        if (!restartAt(stream, screenState_.positionMs(), cycle.audioStreamIndex, plan.subtitleStreamIndex,
                       currentMediaSourceId))
            return PlaybackRuntimeNotice::SubtitleStartFailed;
        return PlaybackRuntimeNotice::None;
    }

    template <typename StreamExecutor> bool seekTo(StreamExecutor& stream, int positionMs) {
        const int targetMs = std::max(0, positionMs);
        if (coordinator_.seekRequiresStreamRestart(targetMs)) {
            // The server stream begins at the resume offset; mpv cannot seek before its first frame.
            restartAt(stream, targetMs, coordinator_.tracks().selectedAudioServerIndex(),
                      coordinator_.tracks().selectedSubtitleServerIndex());
            return false; // Stream restart reports progress once its new timeline is ready.
        }
        player_.seekTo(coordinator_.playerPositionFromLogicalPosition(targetMs));
        screenState_.beginSeek(targetMs, std::chrono::steady_clock::now());
        return true;
    }

    template <typename StreamExecutor, typename TelemetryExecutor>
    bool skipActiveMediaSegment(StreamExecutor& stream, TelemetryExecutor& telemetry, bool playerScreenActive) {
        const auto* segment = coordinator_.activeSkippableSegment(screenState_.positionMs());
        const auto& activeItem = coordinator_.session().activeItem();
        if (segment && canDisableSkipForSegmentType(segment->type) &&
            skipSegmentsDisabledForSeries(settings_, activeItem.seriesId)) {
            return false;
        }
        const auto targetMs = coordinator_.activeSkippableSegmentEndMs(screenState_.positionMs());
        if (!targetMs) return false;
        if (seekTo(stream, *targetMs)) reportProgress(telemetry, playerScreenActive, false);
        return true;
    }

    void startResolvedTarget(int positionOverrideMs = -1) {
        const auto now = std::chrono::steady_clock::now();
        const PlaybackPlayerStartContext start = coordinator_.playerStartContext();
        const int logicalStartPositionMs = positionOverrideMs >= 0 ? positionOverrideMs : start.logicalStartPositionMs;
        const int playerStartPositionMs = positionOverrideMs >= 0
                                              ? coordinator_.playerPositionFromLogicalPosition(logicalStartPositionMs)
                                              : start.playerStartPositionMs;
        coordinator_.beginPreparing(now);
        embeddedSubtitleDiscoveryDone_ = false;
        embeddedSubtitleDiscoveryStarted_ = {};
        embeddedSubtitleDiscoveryLastPoll_ = {};
        player_.startAsync(start.url, videoSurface_.surface(), playerStartPositionMs, settings_.playbackBufferPreset,
                           start.audioOrdinal, start.subtitleStreamIndex, start.subtitleOrdinal,
                           start.externalSubtitleUrl, activeDecodeMode(), settings_.maxAudioChannels, isRemuxServer());
        initialResumeTargetMs_ = coordinator_.activeTargetUsesDirectPlay() ? logicalStartPositionMs : 0;
        initialResumeRetryMs_ = 0;
        if (logicalStartPositionMs > 0) screenState_.beginInitialPosition(logicalStartPositionMs, now);
    }

    bool retryWithSoftwareDecode(bool rendererReady) {
        if (!rendererReady || !player_.hardwareDecoderFailed()) return false;
        const std::string itemId = coordinator_.session().activeItem().id;
        if (itemId.empty() || itemId == softwareDecodeItemId_) return false;

        const int retryPositionMs = std::max(0, screenState_.positionMs());
        player_.stop();
        videoSurface_.release();
        std::string surfaceError;
        if (!videoSurface_.create(surfaceError)) {
            error_ = surfaceError.empty() ? "SOFTWARE DECODE FALLBACK SURFACE IS NOT AVAILABLE" : surfaceError;
            return false;
        }

        softwareDecodeItemId_ = itemId;
        __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                            "MediaCodec failed to start; retrying current item with software video decoding");
        screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(5));
        startResolvedTarget(retryPositionMs);
        return true;
    }

    template <typename StreamExecutor> bool retryWithoutSubtitle(StreamExecutor& stream) {
        const PlaybackSubtitleFallbackPlan plan = coordinator_.subtitleFallbackPlan();
        if (!plan.retry) return false;
        __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                            "Subtitle-selected transcode failed; retrying item without subtitles (stream %d)",
                            plan.failedSubtitleStreamIndex);
        return restartAt(stream, screenState_.positionMs(), plan.audioStreamIndex, kSubtitleOffIndex);
    }

    template <typename StreamExecutor>
    bool retryWithFallback(StreamExecutor& stream, bool rendererReady, bool preferServerStream = false) {
#ifdef SLOPPATV_BENCHMARK
        (void)stream;
        (void)rendererReady;
        (void)preferServerStream;
        __android_log_print(ANDROID_LOG_WARN, "SloppaTV", "Benchmark build refusing Jellyfin server playback fallback");
        return false;
#else
        auto fallbackAttempt =
            coordinator_.fallbackAttempt(session_.valid(), screenState_.positionMs(), preferServerStream);
        if (!fallbackAttempt) return false;

        const PlaybackFallbackPlan fallbackPlan = fallbackAttempt->plan;
        const PlaybackTarget failedTarget = std::move(fallbackAttempt->failedTarget);
        JellyfinItem item = std::move(fallbackAttempt->item);
        const int64_t resumeTicks = fallbackPlan.resumeTicks;
        const bool shouldReportPrevious = fallbackPlan.reportPrevious;
        const int audioStreamIndex = fallbackAttempt->audioStreamIndex;
        const int subtitleStreamIndex = fallbackAttempt->subtitleStreamIndex;

        player_.stop();
        videoSurface_.release();
        coordinator_.beginFallback();
        screenState_.setPositionMs(playbackPositionMsFromTicks(resumeTicks));
        screenState_.setDurationMs(playbackPositionMsFromTicks(item.runtimeTicks));

        if (fallbackPlan.useOfferedTarget) {
            if (shouldReportPrevious) {
                stream.reportPreviousStop(session_, item, failedTarget, resumeTicks, "stop-after-failure");
            }
            coordinator_.useOfferedFallback(fallbackPlan);
            const bool directStreamFallback = fallbackPlan.offeredDirectStream;

            std::string surfaceError;
            if (!rendererReady || !videoSurface_.create(surfaceError)) {
                error_ = surfaceError.empty() ? "VIDEO FALLBACK SURFACE IS NOT AVAILABLE" : surfaceError;
                return false;
            }
            __android_log_print(ANDROID_LOG_WARN, "SloppaTV", "Direct play failed; using offered Jellyfin %s fallback",
                                directStreamFallback ? "direct-stream" : "transcode");
            screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(5));
            startResolvedTarget();
            return true;
        }

        PlaybackOverrides fallbackOverrides = playbackOverridesFor(settings_);
        fallbackOverrides.forceServerStream = fallbackPlan.forceServerStream;
        fallbackOverrides.forceTranscode = fallbackPlan.forceTranscode;
        const uint64_t generation = playbackEpoch_.begin();
        loading_ = true;
        coordinator_.beginFallbackResolution();
        screenState_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(10));
        __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                            "Direct play failed without fallback URL; forcing Jellyfin %s negotiation",
                            preferServerStream ? "server-stream" : "transcode");

        const bool submitted =
            stream.resolveFallback(session_, std::move(item), failedTarget, shouldReportPrevious, resumeTicks,
                                   PlaybackStreamResolutionOptions{
                                       .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
                                       .maxAudioChannels = serverAudioChannels(),
                                       .overrides = fallbackOverrides,
                                       .audioStreamIndex = audioStreamIndex,
                                       .subtitleStreamIndex = subtitleStreamIndex,
                                   },
                                   generation);
        if (!submitted) {
            playbackEpoch_.invalidate();
            loading_ = false;
            coordinator_.finishFallbackResolution();
            error_ = "TRANSCODE FALLBACK COULD NOT BE STARTED";
            return false;
        }
        return true;
#endif
    }

    template <typename ContinuationExecutor> void requestMediaSegments(ContinuationExecutor& continuation) {
        if (!session_.valid()) return;
        const auto request = coordinator_.beginMediaSegmentsRequest(std::chrono::steady_clock::now());
        if (!request) return;
        if (!continuation.requestMediaSegments(session_, *request)) {
            coordinator_.failMediaSegmentsRequest(*request, std::chrono::steady_clock::now());
        }
    }

    template <typename ContinuationExecutor>
    void requestNextEpisode(ContinuationExecutor& continuation, const PlaybackQueueState& queue) {
        const PlaybackNextEpisodePlan plan =
            coordinator_.beginNextEpisodePlan(queue, session_.valid(), std::chrono::steady_clock::now());
        if (!plan.request) return;
        if (!continuation.requestNextEpisode(session_, plan.request->seriesId, plan.request->currentItemId)) {
            coordinator_.failNextEpisodeSubmission(std::chrono::steady_clock::now());
        }
    }

    template <typename SubtitleExecutor, typename StreamExecutor, typename TelemetryExecutor>
    PlaybackHostEffect handlePlayerInput(PlayerScreenInput input, int repeatCount, SubtitleExecutor& subtitles,
                                         StreamExecutor& stream, TelemetryExecutor& telemetry,
                                         bool playerScreenActive = true) {
        const auto now = std::chrono::steady_clock::now();
        const PlayerScreenCommand command = screenState_.handleInput(input, now);
        switch (command.type) {
        case PlayerScreenCommandType::None:
            return {};
        case PlayerScreenCommandType::StopPlayback:
            return PlaybackHostEffect::stop();
        case PlayerScreenCommandType::OpenQueue:
            return PlaybackHostEffect::simple(PlaybackHostEffectType::OpenQueue);
        case PlayerScreenCommandType::PreviousEpisode:
            return PlaybackHostEffect::adjacentEpisode(-1);
        case PlayerScreenCommandType::NextEpisode:
            return PlaybackHostEffect::adjacentEpisode(1);
        case PlayerScreenCommandType::ActivatePlayback:
            if (!shouldHandlePauseToggle(repeatCount)) return {};
            if (skipActiveMediaSegment(stream, telemetry, playerScreenActive)) return {};
            [[fallthrough]];
        case PlayerScreenCommandType::TogglePause:
            if (!shouldHandlePauseToggle(repeatCount)) return {};
            player_.togglePause();
            reportProgress(telemetry, playerScreenActive, true);
            return {};
        case PlayerScreenCommandType::Play:
            if (!shouldHandlePauseToggle(repeatCount)) return {};
            player_.play();
            reportProgress(telemetry, playerScreenActive, true);
            return {};
        case PlayerScreenCommandType::Pause:
            if (!shouldHandlePauseToggle(repeatCount)) return {};
            player_.pause();
            reportProgress(telemetry, playerScreenActive, true);
            return {};
        case PlayerScreenCommandType::OpenAudioPicker: {
            std::vector<PlayerTrackOption> options;
            const auto& item = coordinator_.session().activeItem();
            const int selected = coordinator_.tracks().selectedAudioServerIndex();
            for (const auto& track : item.audios) {
                std::string label = track.title.empty() ? track.language : track.title;
                if (label.empty()) label = "Audio " + std::to_string(track.index);
                if (!track.codec.empty() && track.title.empty()) label += " (" + track.codec + ")";
                options.push_back({track.index, std::move(label), track.index == selected});
            }
            if (options.empty()) {
                error_ = "NO AUDIO TRACKS";
                return {};
            }
            screenState_.openTrackPicker(PlayerTrackPicker::Audio, std::move(options), now);
            return {};
        }
        case PlayerScreenCommandType::OpenSubtitlePicker: {
            std::vector<PlayerTrackOption> options;
            const int selected = coordinator_.tracks().selectedSubtitleServerIndex();
            options.push_back({-1, "Off", selected < 0});
            for (const auto& track : coordinator_.session().activeItem().subtitles) {
                if (!playbackSubtitleAllowed(track, settings_.subtitleLanguages)) continue;
                std::string label = track.title.empty() ? track.language : track.title;
                if (label.empty()) label = "Subtitle " + std::to_string(track.index);
                if (track.forced) label += " (forced)";
                options.push_back({track.index, std::move(label), track.index == selected});
            }
            screenState_.openTrackPicker(PlayerTrackPicker::Subtitle, std::move(options), now);
            return {};
        }
        case PlayerScreenCommandType::SelectAudioTrack:
            if (command.trackIndex != coordinator_.tracks().selectedAudioServerIndex())
                cycleAudioTrack(stream, telemetry, playerScreenActive, command.trackIndex);
            return {};
        case PlayerScreenCommandType::SelectSubtitleTrack:
            if (command.trackIndex != coordinator_.tracks().selectedSubtitleServerIndex() &&
                cycleSubtitleTrack(subtitles, stream, telemetry, playerScreenActive, command.trackIndex) ==
                    PlaybackRuntimeNotice::SubtitleStartFailed)
                return PlaybackHostEffect::simple(PlaybackHostEffectType::SubtitleStartFailed);
            return {};
        case PlayerScreenCommandType::SeekBackward:
        case PlayerScreenCommandType::SeekForward: {
            const bool forward = command.type == PlayerScreenCommandType::SeekForward;
            const int64_t deltaMs =
                heldSeekDeltaMs(forward ? settings_.seekForwardSeconds : settings_.seekBackSeconds, repeatCount);
            const int targetMs = relativeSeekPositionMs(screenState_.positionMs(), forward ? deltaMs : -deltaMs,
                                                        screenState_.durationMs());
            screenState_.showSeekFeedback(static_cast<int>((forward ? deltaMs : -deltaMs) / 1000), now);
            return PlaybackHostEffect::seekWithTrickplay(targetMs);
        }
        }
        return {};
    }

    template <typename TelemetryExecutor>
    PlaybackHostEffect handleMediaSessionCommand(const MediaSessionCommand& command, PlaybackQueueState& queue,
                                                 TelemetryExecutor& telemetry, bool playerScreenActive = true) {
        switch (command.type) {
        case MediaSessionCommandType::Play:
            player_.play();
            reportProgress(telemetry, playerScreenActive, true);
            return {};
        case MediaSessionCommandType::Pause:
            player_.pause();
            reportProgress(telemetry, playerScreenActive, true);
            return {};
        case MediaSessionCommandType::Stop:
            return PlaybackHostEffect::stop();
        case MediaSessionCommandType::SeekTo: {
            const int64_t maxPosition =
                screenState_.durationMs() > 0 ? screenState_.durationMs() : std::numeric_limits<int>::max();
            return PlaybackHostEffect::seekWithTrickplay(
                static_cast<int>(std::clamp<int64_t>(command.positionMs, 0, maxPosition)));
        }
        case MediaSessionCommandType::Next:
            if (queue.currentIndex() >= 0) {
                const int next = queue.nextIndex(true);
                if (next >= 0) return PlaybackHostEffect::playQueueIndex(next);
            }
            return {};
        case MediaSessionCommandType::Previous:
            if (queue.currentIndex() > 0) {
                return PlaybackHostEffect::playQueueIndex(queue.currentIndex() - 1);
            }
            return PlaybackHostEffect::seekWithTrickplay(0);
        }
        return {};
    }

    template <typename StreamExecutor, typename TelemetryExecutor, typename ContinuationExecutor, typename MediaSession,
              typename StateMutex>
    PlaybackHostEffect tickActivePlayer(StreamExecutor& stream, TelemetryExecutor& telemetry,
                                        ContinuationExecutor& continuation, MediaSession& mediaSession,
                                        const PlaybackQueueState& queue, bool rendererReady, StateMutex& stateMutex) {
        PlayerStatus status = player_.status();
        if (status == PlayerStatus::Preparing) {
            mediaSession.updateState(MediaSessionState::Buffering, screenState_.positionMs());
            const auto now = std::chrono::steady_clock::now();
            const bool isRemux = serverInfo_ && serverInfo_->isRemux;
            const PlaybackPreparePlan preparePlan = coordinator_.preparePlan(now, isRemux);
            const std::string alternate = coordinator_.session().activeTarget().alternativeMediaSourceId;
            if (shouldRetrySlowRemuxSource(
                    isRemux, preparePlan.elapsedMs, !alternate.empty(),
                    remuxNeedsResumeReadBudget(coordinator_.session().activeTarget().startTicks))) {
                std::scoped_lock lock(stateMutex);
                const auto& item = coordinator_.session().activeItem();
                const auto& target = coordinator_.session().activeTarget();
                if (failedSourceItemId_ != item.id) {
                    failedSourceItemId_ = item.id;
                    failedSources_.clear();
                }
                if (shouldUseRemuxAlternate(target.mediaSourceId, alternate, failedSources_)) {
                    const std::string failedSource = target.mediaSourceId;
                    __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                                        "Remux source stalled item=%s title=%s S%dE%d source=%s next=%s "
                                        "elapsedMs=%lld positionMs=%d",
                                        item.id.c_str(), item.name.c_str(), item.parentIndexNumber, item.indexNumber,
                                        failedSource.c_str(), alternate.c_str(),
                                        static_cast<long long>(preparePlan.elapsedMs), screenState_.positionMs());
                    if (restartAt(stream, remuxFallbackStartPositionMs(screenState_.positionMs()), -1,
                                  kSubtitleServerDefaultIndex, alternate)) {
                        failedSources_.push_back(failedSource);
                        error_.clear();
                        return {};
                    }
                }
            }
            if (preparePlan.timedOut) {
                std::scoped_lock lock(stateMutex);
                const auto& item = coordinator_.session().activeItem();
                const auto& target = coordinator_.session().activeTarget();
                __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                                    "Playback prepare timeout item=%s title=%s S%dE%d source=%s alt=%s "
                                    "elapsedMs=%lld mode=%s positionMs=%d",
                                    item.id.c_str(), item.name.c_str(), item.parentIndexNumber, item.indexNumber,
                                    target.mediaSourceId.c_str(),
                                    target.alternativeMediaSourceId.empty() ? "(none)"
                                                                            : target.alternativeMediaSourceId.c_str(),
                                    static_cast<long long>(preparePlan.elapsedMs),
                                    preparePlan.transcoding ? "transcode" : "direct", screenState_.positionMs());
                if (preparePlan.retryWithTranscodeFallback && retryWithFallback(stream, rendererReady)) {
                    error_.clear();
                    return {};
                }
                error_ = isRemux ? "REMUX SOURCE DID NOT BECOME READY IN TIME" : "PLAYBACK TOOK TOO LONG TO START";
                return PlaybackHostEffect::stop();
            }
        } else {
            coordinator_.finishPreparing();
        }

        if (coordinator_.consumePauseAfterRestart(status == PlayerStatus::Playing)) {
            player_.togglePause();
            status = player_.status();
        }

        if (status == PlayerStatus::Error) {
            std::scoped_lock lock(stateMutex);
            const std::string playerError = player_.error();
            // A torrent may pass metadata probing yet fail the first HTTP range.
            // Try the offered source before converting the same unavailable input.
            const std::string alternate = coordinator_.session().activeTarget().alternativeMediaSourceId;
            const std::string itemId = coordinator_.session().activeItem().id;
            if (failedSourceItemId_ != itemId) {
                failedSourceItemId_ = itemId;
                failedSources_.clear();
            }
            failedSources_.push_back(coordinator_.session().activeTarget().mediaSourceId);
            if (serverInfo_ && serverInfo_->isRemux && coordinator_.activeTargetUsesDirectPlay() &&
                shouldUseRemuxAlternate(coordinator_.session().activeTarget().mediaSourceId, alternate,
                                        failedSources_) &&
                restartAt(stream, remuxFallbackStartPositionMs(screenState_.positionMs()), -1,
                          kSubtitleServerDefaultIndex, alternate)) {
                error_.clear();
                return {};
            }
            if (retryWithSoftwareDecode(rendererReady) || retryWithoutSubtitle(stream) ||
                retryWithFallback(stream, rendererReady)) {
                error_.clear();
                return {};
            }
            const auto& item = coordinator_.session().activeItem();
            const auto& target = coordinator_.session().activeTarget();
            __android_log_print(ANDROID_LOG_ERROR, "SloppaTV",
                                "Playback failed item=%s title=%s S%dE%d source=%s reason=%s", item.id.c_str(),
                                item.name.c_str(), item.parentIndexNumber, item.indexNumber,
                                target.mediaSourceId.c_str(), playerError.c_str());
            error_ = playerError;
            return PlaybackHostEffect::stop();
        }

        if (status == PlayerStatus::Playing) failedSources_.clear();

        const bool playbackEnded = status == PlayerStatus::Ended;
        if (playbackEnded && coordinator_.activeTargetUsesDirectPlay()) {
            // Sparse Remux streams can report a truncated duration on premature EOF.
            // Preserve the known episode runtime when deciding if an early seek finished playback.
            const int recoveryDurationMs = std::max(player_.durationMs(), screenState_.durationMs());
            const int recoveryTargetMs =
                screenState_.prematureSeekEndTarget(recoveryDurationMs, std::chrono::steady_clock::now());
            if (recoveryTargetMs >= 0) {
                std::scoped_lock lock(stateMutex);
                const std::string alternate = coordinator_.session().activeTarget().alternativeMediaSourceId;
                screenState_.setPositionMs(recoveryTargetMs);
                __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                                    "Stream ended prematurely after seek target=%d; retrying same episode",
                                    recoveryTargetMs);
                if ((!alternate.empty() &&
                     restartAt(stream, recoveryTargetMs, -1, kSubtitleServerDefaultIndex, alternate)) ||
                    retryWithFallback(stream, rendererReady, true)) {
                    error_.clear();
                    return {};
                }
                error_ = "Stream ended while seeking";
                return PlaybackHostEffect::stop();
            }
        }
        if (!playbackEnded && status != PlayerStatus::Playing && status != PlayerStatus::Paused) return {};
        if (status == PlayerStatus::Playing && coordinator_.activeTargetUsesDirectPlay()) {
            const int observedMs = player_.positionMs();
            const auto now = std::chrono::steady_clock::now();
            if (initialResumeRetryMs_ > 0) {
                if (postSeekPositionMatchesTarget(observedMs, initialResumeRetryMs_, 3'000)) {
                    initialResumeRetryMs_ = 0;
                } else if (screenState_.pendingSeekAppearsFailed(observedMs, now)) {
                    std::scoped_lock lock(stateMutex);
                    const int requestedMs = initialResumeRetryMs_;
                    initialResumeRetryMs_ = 0;
                    screenState_.setPositionMs(requestedMs);
                    if (retryWithFallback(stream, rendererReady, true)) {
                        error_.clear();
                        return {};
                    }
                    screenState_.setPositionMs(observedMs);
                }
            } else if (initialResumeTargetMs_ > 0) {
                if (postSeekPositionMatchesTarget(observedMs, initialResumeTargetMs_, 3'000)) {
                    initialResumeTargetMs_ = 0;
                } else if (initialResumeSeekMissed(initialResumeTargetMs_, observedMs, true)) {
                    std::scoped_lock lock(stateMutex);
                    const int requestedMs = initialResumeTargetMs_;
                    initialResumeTargetMs_ = 0;
                    __android_log_print(ANDROID_LOG_WARN, "SloppaTV",
                                        "Initial resume seek missed target=%d observed=%d; retrying after load",
                                        requestedMs, observedMs);
                    if (player_.seekable()) {
                        initialResumeRetryMs_ = requestedMs;
                        seekTo(stream, requestedMs);
                    } else {
                        screenState_.setPositionMs(requestedMs);
                        if (retryWithFallback(stream, rendererReady, true)) {
                            error_.clear();
                            return {};
                        }
                        screenState_.setPositionMs(observedMs);
                    }
                }
            }
        }
        const auto discoveryNow = std::chrono::steady_clock::now();
        if (embeddedSubtitleDiscoveryStarted_ == std::chrono::steady_clock::time_point{})
            embeddedSubtitleDiscoveryStarted_ = discoveryNow;
        if (!embeddedSubtitleDiscoveryDone_ && coordinator_.activeTargetUsesDirectPlay() && !playbackEnded &&
            discoveryNow - embeddedSubtitleDiscoveryStarted_ < std::chrono::seconds(30) &&
            (embeddedSubtitleDiscoveryLastPoll_ == std::chrono::steady_clock::time_point{} ||
             discoveryNow - embeddedSubtitleDiscoveryLastPoll_ >= std::chrono::seconds(1))) {
            embeddedSubtitleDiscoveryLastPoll_ = discoveryNow;
            if (const auto tracks = player_.embeddedSubtitleTracks()) {
                std::vector<JellyfinSubtitleStream> subtitles;
                subtitles.reserve(tracks->size());
                for (const auto& track : *tracks) {
                    subtitles.push_back({
                        .index = track.streamIndex,
                        .codec = track.codec,
                        .language = normalizeSubtitleLanguage(track.language),
                        .title = track.title,
                        .forced = track.forced,
                        .isDefault = track.isDefault,
                        .discoveredByPlayer = true,
                    });
                }
                if (!subtitles.empty()) {
                    coordinator_.session().mergeEmbeddedSubtitles(subtitles);
                    embeddedSubtitleDiscoveryDone_ = true;
                    if (!coordinator_.selectedSubtitleStream()) {
                        const auto& item = coordinator_.session().activeItem();
                        const int preferred = playbackAutoDiscoveredSubtitleIndexForItem(
                            item, coordinator_.session().activeTarget().audioStreamIndex, trackSelectionPolicy(),
                            activeDecodeMode() == PlaybackDecodeMode::Software);
                        if (preferred >= 0 && player_.selectEmbeddedSubtitleStream(preferred)) {
                            coordinator_.selectSubtitleStream(preferred);
                            __android_log_print(ANDROID_LOG_INFO, "SloppaTV",
                                                "Auto-selected discovered subtitle stream %d", preferred);
                        }
                    }
                    __android_log_print(ANDROID_LOG_INFO, "SloppaTV", "Discovered %zu embedded subtitle tracks",
                                        subtitles.size());
                }
            }
        }
        if (playbackEnded && screenState_.durationMs() > 0) {
            screenState_.setPositionMs(screenState_.durationMs());
        }

        if (!playbackEnded && coordinator_.activeTargetUsesDirectPlay()) {
            const int pendingSeekTargetMs = screenState_.pendingSeekTargetMs();
            const int recoveryTargetMs =
                pendingSeekTargetMs >= 0 ? pendingSeekTargetMs : screenState_.recentSeekTargetMs();
            if (recoveryTargetMs >= 0) {
                const auto now = std::chrono::steady_clock::now();
                const int observedPositionMs = coordinator_.logicalPositionFromPlayerPosition(player_.positionMs());
                const bool mediaSeekable = player_.seekable();
                const bool seekFailureMatured = pendingSeekTargetMs >= 0
                                                    ? screenState_.pendingSeekAppearsFailed(observedPositionMs, now)
                                                    : screenState_.recentSeekAppearsFailed(observedPositionMs, now);
                const bool failedSeek = shouldFallbackAfterUnseekableSeek(mediaSeekable, observedPositionMs,
                                                                          recoveryTargetMs, seekFailureMatured);
                if (failedSeek) {
                    std::scoped_lock lock(stateMutex);
                    screenState_.setPositionMs(recoveryTargetMs);
                    __android_log_print(
                        ANDROID_LOG_WARN, "SloppaTV",
                        "Direct-play seek failed target=%d observed=%d seekable=%d; using Jellyfin stream fallback",
                        recoveryTargetMs, observedPositionMs, mediaSeekable);
                    if (retryWithFallback(stream, rendererReady, true)) {
                        error_.clear();
                        return {};
                    }
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const PlaybackTickPlan plan = coordinator_.consumeTickPlan(playbackEnded, status == PlayerStatus::Playing,
                                                                   screenState_.positionMs(), now);
        if (plan.refreshTelemetry) {
            refreshTelemetry();
            mediaSession.updateState(status == PlayerStatus::Playing ? MediaSessionState::Playing
                                                                     : MediaSessionState::Paused,
                                     screenState_.positionMs());
        }
        if (plan.requestMediaSegments) requestMediaSegments(continuation);
        if (plan.reportPlaybackStart) {
            const PlaybackStartContext start = coordinator_.playbackStartContext(screenState_.positionMs());
            telemetry.reportStart(session_, start.item, start.target, start.ticks);
        }
        if (plan.reportProgress) reportProgress(telemetry, true, false);
        if (plan.requestNextEpisode) requestNextEpisode(continuation, queue);

        const PlaybackContinuationPlan continuationPlan =
            coordinator_.continuationPlan(playbackEnded, screenState_.positionMs(), screenState_.durationMs(), queue,
                                          settings_.autoplayNext, settings_.stillWatchingAfter);
        switch (continuationPlan.action) {
        case PlaybackContinuationAction::None:
            return {};
        case PlaybackContinuationAction::PlayQueueIndex:
            return PlaybackHostEffect::playQueueIndex(continuationPlan.queueIndex,
                                                      continuationPlan.repeatCurrentQueueItem, true);
        case PlaybackContinuationAction::AutoplayNext:
            return PlaybackHostEffect::withItem(PlaybackHostEffectType::QueueAutoplayNext, continuationPlan.nextItem);
        case PlaybackContinuationAction::ShowStillWatching:
            return PlaybackHostEffect::withItem(PlaybackHostEffectType::ShowStillWatching, continuationPlan.nextItem);
        case PlaybackContinuationAction::Stop:
            return PlaybackHostEffect::stop(true, continuationPlan.resetAutoplayChain);
        }
        return {};
    }

private:
    PlaybackCoordinator& coordinator_;
    PlayerScreenState& screenState_;
    NativeMediaPlayer& player_;
    VideoSurface& videoSurface_;
    JellyfinSession& session_;
    AppSettings& settings_;
    RequestEpoch& playbackEpoch_;
#ifndef SLOPPATV_BENCHMARK
    bool& loading_;
#endif
    std::string& error_;
    const std::string& dataPath_;
    const JellyfinServerInfo* serverInfo_;
    std::string softwareDecodeItemId_;
    std::string failedSourceItemId_;
    std::vector<std::string> failedSources_;
    int initialResumeTargetMs_ = 0;
    int initialResumeRetryMs_ = 0;
    bool embeddedSubtitleDiscoveryDone_ = false;
    std::chrono::steady_clock::time_point embeddedSubtitleDiscoveryStarted_{};
    std::chrono::steady_clock::time_point embeddedSubtitleDiscoveryLastPoll_{};
};
