#pragma once

#include "audio_policy.hpp"
#include "media_player_policy.hpp"
#include "playback_continuation.hpp"
#include "playback_queue.hpp"
#include "playback_session.hpp"
#include "playback_telemetry.hpp"
#include "playback_track_selection.hpp"
#include "playback_transition.hpp"
#include "player_tracks.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PlaybackContinuationAction {
    None,
    PlayQueueIndex,
    AutoplayNext,
    ShowStillWatching,
    Stop,
};

enum class PlaybackTrackLabelKind {
    Audio,
    Subtitle,
};

struct PlaybackTickPlan {
    bool refreshTelemetry = false;
    bool requestMediaSegments = false;
    bool reportPlaybackStart = false;
    bool reportProgress = false;
    bool requestNextEpisode = false;
};

struct PlaybackContinuationPlan {
    PlaybackContinuationAction action = PlaybackContinuationAction::None;
    int queueIndex = -1;
    bool repeatCurrentQueueItem = false;
    bool resetAutoplayChain = false;
};

struct PlaybackReleasePlan {
    int64_t reportTicks = 0;
    int64_t cachedPositionTicks = 0;
    bool markPlayed = false;
    bool reportStop = false;
};

struct PlaybackReleaseContext {
    PlaybackReleasePlan plan;
    JellyfinItem item;
    PlaybackTarget target;
};

struct PlaybackProgressPlan {
    int64_t ticks = 0;
    bool report = false;
    bool paused = false;
};

struct PlaybackProgressContext {
    PlaybackProgressPlan plan;
    JellyfinItem item;
    PlaybackTarget target;
};

struct PlaybackStartContext {
    int64_t ticks = 0;
    JellyfinItem item;
    PlaybackTarget target;
};

enum class PlaybackPlayerStartMode {
    Resolved,
    WindowRestore,
};

struct PlaybackPlayerStartContext {
    std::string url;
    int startPositionMs = 0;
    int audioOrdinal = -1;
    int subtitleStreamIndex = kSubtitleOffIndex;
    int subtitleOrdinal = -1;
    std::string externalSubtitleUrl;
};

struct PlaybackLanguagePreferences {
    std::optional<std::string> audio;
    std::optional<std::string> subtitle;
};

struct PlaybackSubtitleCycleContext {
    PlaybackSubtitleCyclePlan plan;
    int audioStreamIndex = -1;
    bool busy = false;
};

struct PlaybackSubtitleLoadContext {
    std::string itemId;
    std::string mediaSourceId;
    int requestedSubtitleStreamIndex = -1;
    std::vector<JellyfinSubtitleStream> candidates;
};

struct PlaybackTelemetryReadPlan {
    bool read = false;
    bool probeDuration = false;
    int knownDurationMs = 0;
};

struct PlaybackPreparePlan {
    int64_t elapsedMs = 0;
    bool transcoding = false;
    bool timedOut = false;
    bool retryWithTranscodeFallback = false;
};

struct PlaybackWindowRestorePlan {
    bool restore = false;
    bool preservePlayer = false;
    bool resumePlayback = false;
    bool pauseAfterRestart = false;
    float videoFrameRate = 0.0f;
};

struct PlaybackWindowSuspendPlan {
    bool suspend = false;
    bool resumePlayback = false;
};

struct PlaybackFallbackPlan {
    int64_t resumeTicks = 0;
    bool retry = false;
    bool reportPrevious = false;
    bool useOfferedTarget = false;
    bool offeredDirectStream = false;
    bool forceServerStream = false;
    bool forceTranscode = false;
};

struct PlaybackFallbackAttempt {
    PlaybackFallbackPlan plan;
    JellyfinItem item;
    PlaybackTarget failedTarget;
    int audioStreamIndex = -1;
    int subtitleStreamIndex = kSubtitleOffIndex;
};

struct PlaybackSubtitleFallbackPlan {
    bool retry = false;
    int failedSubtitleStreamIndex = kSubtitleOffIndex;
    int audioStreamIndex = -1;
};

struct PlaybackStreamRestartPlan {
    JellyfinItem item;
    PlaybackTarget previousTarget;
    bool reportPrevious = false;
};

struct PlaybackTransitionPlan {
    int startPositionMs = 0;
    int durationMs = 0;
    int selectedAudioServerIndex = -1;
    int selectedSubtitleServerIndex = -1;
    bool pauseAfterRestart = false;
    bool resetContinuation = false;
    bool resetMediaSegments = false;
};

struct PlaybackNextEpisodeRequest {
    std::string seriesId;
    std::string currentItemId;
};

struct PlaybackAdjacentEpisodeRequest {
    std::string currentItemId;
    std::string seriesId;
    int currentSeason = 0;
    int currentEpisode = 0;
};

inline std::string playbackSummary(const PlaybackTarget& target, const JellyfinItem& item) {
    std::string summary = playbackMethodName(target.playMethod);
    if (!item.videoCodec.empty()) summary += " / " + item.videoCodec;
    if (item.videoWidth > 0 && item.videoHeight > 0) {
        summary += " / " + std::to_string(item.videoWidth) + "X" + std::to_string(item.videoHeight);
    }
    return summary;
}

inline PlaybackTickPlan planPlaybackTick(bool playbackEnded, bool playbackPlaying, int positionMs,
                                         std::string_view itemType, const PlaybackSessionState& sessionState,
                                         const PlaybackTelemetryState& telemetryState,
                                         const PlaybackContinuationState& continuationState,
                                         PlaybackTelemetryState::TimePoint now) {
    PlaybackTickPlan plan;
    plan.refreshTelemetry = !playbackEnded;
    plan.requestMediaSegments = !sessionState.mediaSegmentsRequested();
    plan.reportPlaybackStart = !telemetryState.playbackStartReported();
    plan.reportProgress = telemetryState.progressReportDue(now, playbackPlaying);
    plan.requestNextEpisode = !continuationState.nextEpisodeRequested() && itemType == "Episode" && positionMs >= 30000;
    return plan;
}

inline PlaybackReleasePlan planPlaybackRelease(bool requestedStopReport, bool completed, bool playbackStartReported,
                                               bool sessionValid, const JellyfinItem& item,
                                               const PlaybackTarget& target, int positionMs) {
    PlaybackReleasePlan plan;
    plan.reportTicks = completed && item.runtimeTicks > 0 ? item.runtimeTicks : playbackTicksFromPositionMs(positionMs);
    plan.cachedPositionTicks = completed ? 0 : plan.reportTicks;
    plan.markPlayed = completed;
    plan.reportStop =
        requestedStopReport && playbackStartReported && sessionValid && !item.id.empty() && !target.url.empty();
    return plan;
}

inline PlaybackProgressPlan planPlaybackProgress(bool playerScreenActive, bool jellyfinSessionValid,
                                                 bool playbackStartReported, bool targetAvailable, bool immediate,
                                                 bool preparing, bool paused, int positionMs) {
    PlaybackProgressPlan plan;
    plan.report = playerScreenActive && jellyfinSessionValid && playbackStartReported && targetAvailable &&
                  (immediate || !preparing);
    if (!plan.report) return plan;
    plan.ticks = playbackTicksFromPositionMs(positionMs);
    plan.paused = paused;
    return plan;
}

inline PlaybackPreparePlan planPlaybackPrepare(bool transcoding, PlaybackMethod method, int64_t elapsedMs) {
    PlaybackPreparePlan plan;
    plan.elapsedMs = elapsedMs;
    plan.transcoding = transcoding;
    plan.timedOut = playbackPrepareTimedOut(transcoding, elapsedMs);
    plan.retryWithTranscodeFallback = plan.timedOut && method != PlaybackMethod::Transcode;
    return plan;
}

inline PlaybackWindowRestorePlan planPlaybackWindowRestore(bool playerScreenActive, bool windowRestorePending,
                                                           bool rendererReady, bool targetAvailable,
                                                           bool rendererContextReused, bool videoSurfaceReady,
                                                           bool playerReusable, bool resumeRequested) {
    PlaybackWindowRestorePlan plan;
    plan.restore = playerScreenActive && windowRestorePending && rendererReady && targetAvailable;
    if (!plan.restore) return plan;
    plan.preservePlayer = rendererContextReused && videoSurfaceReady && playerReusable;
    plan.resumePlayback = resumeRequested;
    plan.pauseAfterRestart = !plan.preservePlayer && !resumeRequested;
    return plan;
}

inline PlaybackWindowSuspendPlan planPlaybackWindowSuspend(bool playerScreenActive, bool targetAvailable,
                                                           bool playerPlayingOrPreparing) {
    PlaybackWindowSuspendPlan plan;
    plan.suspend = playerScreenActive && targetAvailable;
    plan.resumePlayback = plan.suspend && playerPlayingOrPreparing;
    return plan;
}

inline bool shouldPausePlaybackForFocusLoss(bool playerScreenActive, bool playerPlayingOrPreparing) {
    return playerScreenActive && playerPlayingOrPreparing;
}

inline PlaybackFallbackPlan planPlaybackFallback(bool fallbackAttempted, PlaybackMethod currentMethod,
                                                 bool sessionValid, std::string_view itemId,
                                                 std::string_view currentUrl, std::string_view fallbackUrl,
                                                 bool playbackStartReported, int positionMs, bool preferServerStream) {
    PlaybackFallbackPlan plan;
    if (fallbackAttempted || currentMethod == PlaybackMethod::Transcode || !sessionValid || itemId.empty()) {
        return plan;
    }

    plan.retry = true;
    plan.resumeTicks = playbackTicksFromPositionMs(positionMs);
    plan.reportPrevious = playbackStartReported && !currentUrl.empty();
    plan.useOfferedTarget = !fallbackUrl.empty();
    if (plan.useOfferedTarget) {
        plan.offeredDirectStream = preferServerStream && transcodingUrlRepresentsDirectStream(fallbackUrl);
    } else if (preferServerStream) {
        plan.forceServerStream = true;
    } else {
        plan.forceTranscode = true;
    }
    return plan;
}

inline PlaybackSubtitleFallbackPlan planPlaybackSubtitleFallback(const JellyfinItem& item, const PlaybackTarget& target,
                                                                 const PlayerTrackState& trackState) {
    PlaybackSubtitleFallbackPlan plan;
    plan.failedSubtitleStreamIndex = trackState.selectedSubtitleServerIndex();
    plan.audioStreamIndex = trackState.selectedAudioServerIndex();
    const auto selectedSubtitle =
        std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return subtitle.index == plan.failedSubtitleStreamIndex;
        });
    const bool subtitleRequiresServerTranscode =
        selectedSubtitle != item.subtitles.end() &&
        subtitleStrategy(selectedSubtitle->codec) == SubtitleStrategy::ServerTranscode;
    plan.retry = shouldRetryFailedSubtitleTranscode(target.playMethod == PlaybackMethod::Transcode,
                                                    plan.failedSubtitleStreamIndex, subtitleRequiresServerTranscode);
    return plan;
}

inline std::string uppercasePlaybackTrackLabel(std::string label) {
    std::transform(label.begin(), label.end(), label.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return label;
}

inline std::string playbackTrackLabel(const JellyfinItem& item, const PlayerTrackState& tracks,
                                      PlaybackTrackLabelKind kind) {
    if (kind == PlaybackTrackLabelKind::Audio) {
        if (item.audios.empty()) return "DEFAULT";
        const auto selected =
            std::find_if(item.audios.begin(), item.audios.end(), [&](const JellyfinAudioStream& audio) {
                return audio.index == tracks.selectedAudioServerIndex();
            });
        const auto audio = selected == item.audios.end() ? item.audios.begin() : selected;
        std::string label = uppercasePlaybackTrackLabel(audio->language.empty() ? "AUDIO" : audio->language);
        if (item.audios.size() > 1) {
            label += " " + std::to_string(std::distance(item.audios.begin(), audio) + 1) + "/" +
                     std::to_string(item.audios.size());
        }
        return label;
    }

    if (tracks.subtitleBusy()) return "LOADING";
    if (tracks.selectedSubtitleServerIndex() >= 0) {
        const auto selected =
            std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
                return subtitle.index == tracks.selectedSubtitleServerIndex();
            });
        if (selected != item.subtitles.end()) {
            return uppercasePlaybackTrackLabel(selected->language.empty() ? "ON" : selected->language);
        }
    }
    if (tracks.subtitleCues().empty()) return "OFF";
    if (!tracks.subtitleEnabled()) return "OFF";

    std::string label =
        uppercasePlaybackTrackLabel(tracks.subtitleLanguage().empty() ? "ON" : tracks.subtitleLanguage());
    const auto subtitle =
        std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& candidate) {
            return candidate.index == tracks.activeSubtitleServerIndex();
        });
    if (subtitle != item.subtitles.end() && item.subtitles.size() > 1) {
        label += " " + std::to_string(std::distance(item.subtitles.begin(), subtitle) + 1) + "/" +
                 std::to_string(item.subtitles.size());
    }
    return label;
}

inline PlaybackTarget offeredPlaybackFallbackTarget(PlaybackTarget target, const PlaybackFallbackPlan& plan) {
    if (!plan.retry || !plan.useOfferedTarget) return target;
    target.url = target.fallbackTranscodeUrl;
    target.fallbackTranscodeUrl.clear();
    target.transcoding = true;
    target.playMethod = plan.offeredDirectStream ? PlaybackMethod::DirectStream : PlaybackMethod::Transcode;
    target.startTicks = plan.resumeTicks;
    return target;
}

inline PlaybackTransitionPlan planPlaybackTransition(const PlaybackTarget& target, const JellyfinItem& item,
                                                     bool streamRestart, bool restartPaused, int audioStreamIndex) {
    PlaybackTransitionPlan plan;
    plan.startPositionMs = playbackPositionMsFromTicks(target.startTicks);
    plan.durationMs = playbackPositionMsFromTicks(item.runtimeTicks);
    plan.selectedAudioServerIndex = audioStreamIndex >= 0 ? audioStreamIndex : target.audioStreamIndex;
    if (plan.selectedAudioServerIndex < 0 && !item.audios.empty()) {
        const auto preferred = std::find_if(item.audios.begin(), item.audios.end(),
                                            [](const JellyfinAudioStream& audio) { return audio.isDefault; });
        plan.selectedAudioServerIndex = preferred == item.audios.end() ? item.audios.front().index : preferred->index;
    }
    plan.selectedSubtitleServerIndex = target.subtitleStreamIndex;
    plan.pauseAfterRestart = streamRestart && restartPaused;
    plan.resetContinuation = !streamRestart;
    plan.resetMediaSegments = !streamRestart;
    return plan;
}

inline PlaybackContinuationPlan planPlaybackContinuation(bool playbackEnded, int positionMs, int durationMs,
                                                         const PlaybackQueueState& queueState,
                                                         const PlaybackContinuationState& continuationState,
                                                         bool autoplayNext, int stillWatchingAfter) {
    PlaybackContinuationPlan plan;
    const bool playbackComplete = playbackEnded || (durationMs > 1000 && positionMs >= durationMs - 1000);
    if (!playbackComplete) return plan;

    if (queueState.currentIndex() >= 0 && queueState.repeatMode() != QueueRepeatMode::Off) {
        const int next = queueState.nextIndex(false);
        if (next >= 0) {
            plan.action = PlaybackContinuationAction::PlayQueueIndex;
            plan.queueIndex = next;
            plan.repeatCurrentQueueItem = next == queueState.currentIndex();
        } else {
            plan.action = PlaybackContinuationAction::Stop;
        }
        return plan;
    }

    if (continuationState.nextItem()) {
        plan.action =
            shouldAutoplayNextEpisode(autoplayNext, continuationState.autoplayChainCount(), stillWatchingAfter)
                ? PlaybackContinuationAction::AutoplayNext
                : PlaybackContinuationAction::ShowStillWatching;
        return plan;
    }

    plan.action = PlaybackContinuationAction::Stop;
    plan.resetAutoplayChain = true;
    return plan;
}

class PlaybackCoordinator {
public:
    using Clock = PlaybackTelemetryState::Clock;
    using TimePoint = PlaybackTelemetryState::TimePoint;

    [[nodiscard]] PlaybackSessionState& session() { return sessionState_; }

    [[nodiscard]] const PlaybackSessionState& session() const { return sessionState_; }

    [[nodiscard]] bool activeItemAvailable() const { return !sessionState_.activeItem().id.empty(); }

    [[nodiscard]] bool activeTargetAvailable() const { return !sessionState_.activeTarget().url.empty(); }

    [[nodiscard]] bool activeTargetUsesDirectPlay() const {
        return sessionState_.activeTarget().playMethod == PlaybackMethod::DirectPlay;
    }

    [[nodiscard]] PlaybackTelemetryState& telemetry() { return telemetryState_; }

    [[nodiscard]] const PlaybackTelemetryState& telemetry() const { return telemetryState_; }

    [[nodiscard]] PlaybackContinuationState& continuation() { return continuationState_; }

    [[nodiscard]] const PlaybackContinuationState& continuation() const { return continuationState_; }

    [[nodiscard]] PlaybackTransitionState& transition() { return transitionState_; }

    [[nodiscard]] const PlaybackTransitionState& transition() const { return transitionState_; }

    [[nodiscard]] PlayerTrackState& tracks() { return trackState_; }

    [[nodiscard]] const PlayerTrackState& tracks() const { return trackState_; }

    void setZoomMode(VideoZoomMode mode) { sessionState_.setZoomMode(mode); }

    void recordExternalPlayback(std::string_view playerLabel) {
        sessionState_.setLastPlaybackSummary("EXTERNAL / " + std::string(playerLabel));
    }

    void clearActivePlayback() { sessionState_.clearActive(); }

    void activate(const JellyfinItem& item, const PlaybackTarget& target, TimePoint now) {
        sessionState_.setActive(item, target);
        sessionState_.setLastPlaybackSummary(playbackSummary(target, item));
        sessionState_.resetFallbackAttempted();
        telemetryState_.beginPlayback(now);
    }

    [[nodiscard]] PlaybackReleasePlan releasePlan(bool requestedStopReport, bool completed, bool jellyfinSessionValid,
                                                  int positionMs) const {
        return planPlaybackRelease(requestedStopReport, completed, telemetryState_.playbackStartReported(),
                                   jellyfinSessionValid, sessionState_.activeItem(), sessionState_.activeTarget(),
                                   positionMs);
    }

    [[nodiscard]] PlaybackReleaseContext releaseContext(bool requestedStopReport, bool completed,
                                                        bool jellyfinSessionValid, int positionMs) const {
        return PlaybackReleaseContext{
            .plan = releasePlan(requestedStopReport, completed, jellyfinSessionValid, positionMs),
            .item = sessionState_.activeItem(),
            .target = sessionState_.activeTarget(),
        };
    }

    void finishRelease() {
        sessionState_.clearPreparing();
        telemetryState_.clearPlaybackStartReported();
        sessionState_.clearActive();
        telemetryState_.resetReadIntervals();
        continuationState_.clearNextEpisode();
        sessionState_.resetMediaSegments();
        transitionState_.setFallbackResolving(false);
        trackState_.resetPlayback();
    }

    void markPlaybackStopReported() { sessionState_.requestHomeRefresh(); }

    [[nodiscard]] bool consumeHomeRefreshRequest() { return sessionState_.takeHomeRefreshRequest(); }

    void finishStop() {
        trackState_.clearLanguagePreferences();
        transitionState_.setLoading(false);
    }

    void resetSession() {
        continuationState_.reset();
        transitionState_.reset();
        sessionState_.reset();
        telemetryState_.reset();
        trackState_.resetSession();
    }

    void dismissStillWatchingPrompt() { continuationState_.setStillWatchingPrompt(false); }

    void resetAutoplayChain() { continuationState_.resetAutoplayChain(); }

    void resetContinuationPrompt() {
        resetAutoplayChain();
        dismissStillWatchingPrompt();
    }

    void showStillWatchingPrompt() {
        continuationState_.resetAutoplayChain();
        continuationState_.setStillWatchingPrompt(true);
    }

    void beginUserPlayback(bool continuingPlaybackChain) {
        resetContinuationPrompt();
        if (!continuingPlaybackChain) trackState_.clearLanguagePreferences();
    }

    void beginPlaybackResolution(bool showTransitionLoading) {
        transitionState_.setLoading(showTransitionLoading);
        dismissStillWatchingPrompt();
    }

    void beginAutoplayResolution() {
        continuationState_.incrementAutoplayChain();
        beginPlaybackResolution(true);
    }

    void finishPlaybackResolution() { transitionState_.setLoading(false); }

    void stageResolvedPlayback(PlaybackTarget target, JellyfinItem item) {
        transitionState_.stage(std::move(target), std::move(item));
    }

    [[nodiscard]] std::optional<PendingPlaybackTransition> takePendingTransition() { return transitionState_.take(); }

    void setPauseAfterRestart(bool pause) { transitionState_.setPauseAfterRestart(pause); }

    [[nodiscard]] bool consumePauseAfterRestart(bool playbackPlaying) {
        if (!playbackPlaying || !transitionState_.pauseAfterRestart()) return false;
        transitionState_.clearPauseAfterRestart();
        return true;
    }

    [[nodiscard]] bool transitionLoading() const { return transitionState_.loading(); }

    [[nodiscard]] bool fallbackResolving() const { return transitionState_.fallbackResolving(); }

    [[nodiscard]] PlaybackTransitionPlan activateTransition(const JellyfinItem& item, const PlaybackTarget& target,
                                                            bool streamRestart, bool restartPaused,
                                                            int audioStreamIndex, VideoZoomMode zoomMode,
                                                            TimePoint now) {
        const PlaybackTransitionPlan plan =
            planPlaybackTransition(target, item, streamRestart, restartPaused, audioStreamIndex);
        transitionState_.setPauseAfterRestart(plan.pauseAfterRestart);
        activate(item, target, now);
        sessionState_.setZoomMode(zoomMode);
        if (plan.resetContinuation) continuationState_.clearNextEpisode();
        trackState_.resetPlayback();
        trackState_.setSelectedAudioServerIndex(plan.selectedAudioServerIndex);
        trackState_.setSelectedSubtitleServerIndex(plan.selectedSubtitleServerIndex);
        if (plan.resetMediaSegments) sessionState_.resetMediaSegments();
        transitionState_.setLoading(false);
        return plan;
    }

    void syncQueueContinuation(const PlaybackQueueState& queueState) {
        const int next = queueState.nextIndex(false);
        if (const auto* item = queueState.itemAt(next))
            continuationState_.setNextItem(*item);
        else
            continuationState_.clearNextItem();
    }

    bool useQueueContinuation(const PlaybackQueueState& queueState) {
        if (queueState.currentIndex() < 0 || queueState.currentIndex() >= queueState.size()) return false;
        continuationState_.markNextEpisodeRequested();
        syncQueueContinuation(queueState);
        return true;
    }

    [[nodiscard]] std::optional<std::string> beginMediaSegmentsRequest(TimePoint now) {
        const std::string& itemId = sessionState_.activeItem().id;
        if (itemId.empty() || !sessionState_.beginMediaSegmentsRequest(now)) return std::nullopt;
        return itemId;
    }

    bool completeMediaSegmentsRequest(std::string_view expectedItemId, std::vector<JellyfinMediaSegment> segments) {
        if (sessionState_.activeItem().id != expectedItemId) return false;
        sessionState_.setMediaSegments(std::move(segments));
        return true;
    }

    bool failMediaSegmentsRequest(std::string_view expectedItemId, TimePoint now) {
        if (sessionState_.activeItem().id != expectedItemId) return false;
        sessionState_.mediaSegmentsRequestFailed(now);
        return true;
    }

    [[nodiscard]] std::optional<PlaybackNextEpisodeRequest>
    beginNextEpisodeRequest(PlaybackContinuationState::TimePoint now) {
        const JellyfinItem& item = sessionState_.activeItem();
        if (item.type != "Episode" || item.seriesId.empty() || item.id.empty() ||
            !continuationState_.beginNextEpisodeRequest(now)) {
            return std::nullopt;
        }
        return PlaybackNextEpisodeRequest{
            .seriesId = item.seriesId,
            .currentItemId = item.id,
        };
    }

    bool completeNextEpisodeRequest(std::string_view expectedItemId, JellyfinItem item) {
        if (sessionState_.activeItem().id != expectedItemId) return false;
        continuationState_.setNextItem(std::move(item));
        return true;
    }

    bool failNextEpisodeRequest(std::string_view expectedItemId, PlaybackContinuationState::TimePoint now) {
        if (sessionState_.activeItem().id != expectedItemId) return false;
        continuationState_.nextEpisodeRequestFailed(now);
        return true;
    }

    void failNextEpisodeSubmission(PlaybackContinuationState::TimePoint now) {
        continuationState_.nextEpisodeRequestFailed(now);
    }

    [[nodiscard]] std::optional<PlaybackAdjacentEpisodeRequest> beginAdjacentEpisodeLookup() {
        const JellyfinItem& item = sessionState_.activeItem();
        if (item.type != "Episode" || item.seriesId.empty() || item.id.empty() ||
            !continuationState_.beginAdjacentEpisodeLookup()) {
            return std::nullopt;
        }
        return PlaybackAdjacentEpisodeRequest{
            .currentItemId = item.id,
            .seriesId = item.seriesId,
            .currentSeason = item.parentIndexNumber,
            .currentEpisode = item.indexNumber,
        };
    }

    bool finishAdjacentEpisodeLookup(std::string_view expectedItemId) {
        continuationState_.finishAdjacentEpisodeLookup();
        return sessionState_.activeItem().id == expectedItemId;
    }

    [[nodiscard]] PlaybackTickPlan tickPlan(bool playbackEnded, bool playbackPlaying, int positionMs,
                                            TimePoint now) const {
        return planPlaybackTick(playbackEnded, playbackPlaying, positionMs, sessionState_.activeItem().type,
                                sessionState_, telemetryState_, continuationState_, now);
    }

    [[nodiscard]] PlaybackTickPlan consumeTickPlan(bool playbackEnded, bool playbackPlaying, int positionMs,
                                                   TimePoint now) {
        PlaybackTickPlan plan = tickPlan(playbackEnded, playbackPlaying, positionMs, now);
        if (plan.reportPlaybackStart && !telemetryState_.markPlaybackStartReported()) {
            plan.reportPlaybackStart = false;
        }
        if (plan.reportProgress) telemetryState_.markProgressReport(now);
        return plan;
    }

    [[nodiscard]] PlaybackStartContext playbackStartContext(int positionMs) const {
        return PlaybackStartContext{
            .ticks = playbackTicksFromPositionMs(positionMs),
            .item = sessionState_.activeItem(),
            .target = sessionState_.activeTarget(),
        };
    }

    [[nodiscard]] PlaybackPlayerStartContext
    playerStartContext(PlaybackPlayerStartMode mode = PlaybackPlayerStartMode::Resolved) const {
        const auto& item = sessionState_.activeItem();
        const auto& target = sessionState_.activeTarget();
        return PlaybackPlayerStartContext{
            .url = target.url,
            .startPositionMs = initialPlayerSeekMs(target.startTicks),
            .audioOrdinal = playerAudioOrdinal(target, item),
            .subtitleStreamIndex =
                mode == PlaybackPlayerStartMode::WindowRestore && target.playMethod == PlaybackMethod::DirectPlay
                    ? target.subtitleStreamIndex
                    : playerSubtitleStreamIndex(target, item),
            .subtitleOrdinal = playerSubtitleOrdinal(target, item),
            .externalSubtitleUrl = directExternalSubtitleUrl(target, item),
        };
    }

    [[nodiscard]] PlaybackTelemetryReadPlan consumeTelemetryRead(TimePoint now, bool force, int currentDurationMs) {
        if (!telemetryState_.shouldReadPlayback(now, force)) return {};
        PlaybackTelemetryReadPlan plan{.read = true};
        if (sessionState_.activeItem().runtimeTicks > 0) {
            plan.knownDurationMs = playbackPositionMsFromTicks(sessionState_.activeItem().runtimeTicks);
        } else if ((force || currentDurationMs <= 0) && telemetryState_.shouldProbeDuration(now, force)) {
            plan.probeDuration = true;
            telemetryState_.markDurationProbe(now);
        }
        telemetryState_.markPlaybackRead(now);
        return plan;
    }

    [[nodiscard]] PlaybackContinuationPlan continuationPlan(bool playbackEnded, int positionMs, int durationMs,
                                                            const PlaybackQueueState& queueState, bool autoplayNext,
                                                            int stillWatchingAfter) const {
        return planPlaybackContinuation(playbackEnded, positionMs, durationMs, queueState, continuationState_,
                                        autoplayNext, stillWatchingAfter);
    }

    [[nodiscard]] PlaybackProgressPlan progressPlan(bool playerScreenActive, bool jellyfinSessionValid, bool immediate,
                                                    bool preparing, bool paused, int positionMs) const {
        return planPlaybackProgress(playerScreenActive, jellyfinSessionValid, telemetryState_.playbackStartReported(),
                                    !sessionState_.activeTarget().url.empty(), immediate, preparing, paused,
                                    positionMs);
    }

    [[nodiscard]] PlaybackProgressContext progressContext(bool playerScreenActive, bool jellyfinSessionValid,
                                                         bool immediate, bool preparing, bool paused,
                                                         int positionMs) const {
        return PlaybackProgressContext{
            .plan = progressPlan(playerScreenActive, jellyfinSessionValid, immediate, preparing, paused, positionMs),
            .item = sessionState_.activeItem(),
            .target = sessionState_.activeTarget(),
        };
    }

    void beginPreparing(TimePoint now) { sessionState_.beginPreparing(now); }

    [[nodiscard]] PlaybackPreparePlan preparePlan(TimePoint now) {
        return planPlaybackPrepare(sessionState_.activeTarget().transcoding, sessionState_.activeTarget().playMethod,
                                   sessionState_.preparingElapsedMs(now));
    }

    void finishPreparing() { sessionState_.clearPreparing(); }

    [[nodiscard]] PlaybackWindowRestorePlan windowRestorePlan(bool playerScreenActive, bool windowRestorePending,
                                                              bool rendererReady, bool rendererContextReused,
                                                              bool videoSurfaceReady, bool playerReusable,
                                                              bool resumeRequested) const {
        PlaybackWindowRestorePlan plan =
            planPlaybackWindowRestore(playerScreenActive, windowRestorePending, rendererReady,
                                      activeTargetAvailable(), rendererContextReused, videoSurfaceReady,
                                      playerReusable, resumeRequested);
        if (plan.restore) plan.videoFrameRate = sessionState_.activeItem().videoFrameRate;
        return plan;
    }

    [[nodiscard]] PlaybackWindowSuspendPlan windowSuspendPlan(bool playerScreenActive,
                                                              bool playerPlayingOrPreparing) const {
        return planPlaybackWindowSuspend(playerScreenActive, activeTargetAvailable(), playerPlayingOrPreparing);
    }

    [[nodiscard]] PlaybackSubtitleFallbackPlan subtitleFallbackPlan() const {
        return planPlaybackSubtitleFallback(sessionState_.activeItem(), sessionState_.activeTarget(), trackState_);
    }

    [[nodiscard]] std::string trackLabel(PlaybackTrackLabelKind kind) const {
        return playbackTrackLabel(sessionState_.activeItem(), trackState_, kind);
    }

    [[nodiscard]] PlaybackLanguagePreferences languagePreferences() const {
        return {
            .audio = trackState_.audioLanguagePreference(),
            .subtitle = trackState_.subtitleLanguagePreference(),
        };
    }

    [[nodiscard]] PlaybackAudioCyclePlan audioTrackCyclePlan(const PlaybackTrackSelectionPolicy& policy) const {
        return planPlaybackAudioTrackCycle(sessionState_.activeItem(), trackState_.selectedAudioServerIndex(),
                                           trackState_.selectedSubtitleServerIndex(),
                                           sessionState_.activeTarget().playMethod, policy);
    }

    [[nodiscard]] PlaybackSubtitleCycleContext
    subtitleTrackCycleContext(const std::vector<std::string>& allowedLanguages) const {
        return {
            .plan = planPlaybackSubtitleTrackCycle(sessionState_.activeItem(),
                                                   trackState_.selectedSubtitleServerIndex(),
                                                   sessionState_.activeTarget().playMethod, allowedLanguages),
            .audioStreamIndex = trackState_.selectedAudioServerIndex(),
            .busy = trackState_.subtitleBusy(),
        };
    }

    [[nodiscard]] std::optional<PlaybackSubtitleLoadContext>
    beginSubtitleLoadContext(const JellyfinSubtitleStream& requested,
                             const std::vector<std::string>& allowedLanguages) {
        if (requested.index < 0 || !trackState_.beginSubtitleWork()) return std::nullopt;
        const auto& item = sessionState_.activeItem();
        return PlaybackSubtitleLoadContext{
            .itemId = item.id,
            .mediaSourceId = item.mediaSourceId,
            .requestedSubtitleStreamIndex = requested.index,
            .candidates = playbackSubtitleLoadCandidates(item, requested, allowedLanguages),
        };
    }

    void rememberAudioLanguagePreference(int streamIndex) {
        const auto& audios = sessionState_.activeItem().audios;
        const auto selected = std::find_if(
            audios.begin(), audios.end(), [&](const JellyfinAudioStream& audio) { return audio.index == streamIndex; });
        if (selected != audios.end() && !selected->language.empty()) {
            trackState_.setAudioLanguagePreference(normalizeAudioLanguage(selected->language));
        } else {
            trackState_.setAudioLanguagePreference(std::nullopt);
        }
    }

    void rememberSubtitleLanguagePreference(int streamIndex) {
        if (streamIndex < 0) {
            trackState_.setSubtitleLanguagePreference(std::string{});
            return;
        }
        const auto& subtitles = sessionState_.activeItem().subtitles;
        const auto selected =
            std::find_if(subtitles.begin(), subtitles.end(),
                         [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == streamIndex; });
        if (selected != subtitles.end() && !selected->language.empty()) {
            trackState_.setSubtitleLanguagePreference(normalizeSubtitleLanguage(selected->language));
        } else {
            trackState_.setSubtitleLanguagePreference(std::nullopt);
        }
    }

    void selectAudioStream(int streamIndex) {
        trackState_.setSelectedAudioServerIndex(streamIndex);
        sessionState_.activeTarget().audioStreamIndex = streamIndex;
    }

    void selectSubtitleStream(int streamIndex) {
        trackState_.setSelectedSubtitleServerIndex(streamIndex);
        sessionState_.activeTarget().subtitleStreamIndex = streamIndex;
    }

    void disableSubtitleRendering() {
        selectSubtitleStream(kSubtitleOffIndex);
        trackState_.setSubtitleEnabled(false);
    }

    void prepareNativeSubtitleLoad(int streamIndex) {
        selectSubtitleStream(streamIndex);
        trackState_.setSubtitleEnabled(false);
    }

    [[nodiscard]] bool subtitleLoadMatches(std::string_view itemId, int requestedStreamIndex) const {
        return sessionState_.activeItem().id == itemId &&
               trackState_.selectedSubtitleServerIndex() == requestedStreamIndex;
    }

    void failSubtitleLoad() { trackState_.failSelectedSubtitle(); }

    void completeSubtitleLoad(int streamIndex, std::string language, std::vector<SubtitleCue> cues) {
        trackState_.endSubtitleWork();
        selectSubtitleStream(streamIndex);
        trackState_.applySubtitle(streamIndex, std::move(language), std::move(cues));
    }

    [[nodiscard]] std::optional<PlaybackStreamRestartPlan> beginStreamRestart(int positionMs) {
        if (sessionState_.activeItem().id.empty() || !trackState_.beginSubtitleWork()) return std::nullopt;
        PlaybackStreamRestartPlan plan;
        plan.item = sessionState_.activeItem();
        plan.item.positionTicks = playbackTicksFromPositionMs(std::max(0, positionMs));
        plan.previousTarget = sessionState_.activeTarget();
        plan.reportPrevious = telemetryState_.playbackStartReported() && !plan.previousTarget.url.empty();
        transitionState_.setLoading(true);
        telemetryState_.clearPlaybackStartReported();
        return plan;
    }

    void finishStreamRestartRequest() {
        trackState_.endSubtitleWork();
        transitionState_.setLoading(false);
    }

    [[nodiscard]] bool completeStreamRestartRequest(std::string_view expectedItemId) {
        finishStreamRestartRequest();
        return sessionState_.activeItem().id == expectedItemId;
    }

    void stageStreamRestart(PlaybackTarget target, JellyfinItem item, bool restartPaused, int audioStreamIndex) {
        transitionState_.stage(std::move(target), std::move(item), true, restartPaused, audioStreamIndex);
    }

    [[nodiscard]] PlaybackFallbackPlan fallbackPlan(bool jellyfinSessionValid, int positionMs,
                                                    bool preferServerStream) const {
        return planPlaybackFallback(sessionState_.fallbackAttempted(), sessionState_.activeTarget().playMethod,
                                    jellyfinSessionValid, sessionState_.activeItem().id,
                                    sessionState_.activeTarget().url, sessionState_.activeTarget().fallbackTranscodeUrl,
                                    telemetryState_.playbackStartReported(), positionMs, preferServerStream);
    }

    [[nodiscard]] std::optional<PlaybackFallbackAttempt>
    fallbackAttempt(bool jellyfinSessionValid, int positionMs, bool preferServerStream) const {
        PlaybackFallbackAttempt attempt;
        attempt.plan = fallbackPlan(jellyfinSessionValid, positionMs, preferServerStream);
        if (!attempt.plan.retry) return std::nullopt;
        attempt.item = sessionState_.activeItem();
        attempt.item.positionTicks = attempt.plan.resumeTicks;
        attempt.failedTarget = sessionState_.activeTarget();
        attempt.audioStreamIndex = trackState_.selectedAudioServerIndex();
        attempt.subtitleStreamIndex = trackState_.selectedSubtitleServerIndex();
        return attempt;
    }

    void beginFallback() {
        sessionState_.clearPreparing();
        telemetryState_.clearPlaybackStartReported();
        sessionState_.markFallbackAttempted();
        telemetryState_.resetReadIntervals();
    }

    void useOfferedFallback(const PlaybackFallbackPlan& plan) {
        sessionState_.activeTarget() = offeredPlaybackFallbackTarget(sessionState_.activeTarget(), plan);
    }

    void beginFallbackResolution() { transitionState_.setFallbackResolving(true); }

    void finishFallbackResolution() { transitionState_.setFallbackResolving(false); }

    [[nodiscard]] bool completeFallbackResolution(std::string_view expectedItemId) {
        finishFallbackResolution();
        return sessionState_.activeItem().id == expectedItemId;
    }

    void stageResolvedFallback(PlaybackTarget target, JellyfinItem item, int audioStreamIndex) {
        transitionState_.setFallbackResolving(false);
        transitionState_.stage(std::move(target), std::move(item), true, false, audioStreamIndex);
    }

private:
    PlaybackSessionState sessionState_;
    PlaybackTelemetryState telemetryState_;
    PlaybackContinuationState continuationState_;
    PlaybackTransitionState transitionState_;
    PlayerTrackState trackState_;
};
