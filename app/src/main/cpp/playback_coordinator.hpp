#pragma once

#include "media_player_policy.hpp"
#include "playback_continuation.hpp"
#include "playback_queue.hpp"
#include "playback_session.hpp"
#include "playback_telemetry.hpp"

#include <algorithm>
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

struct PlaybackFallbackPlan {
    int64_t resumeTicks = 0;
    bool retry = false;
    bool reportPrevious = false;
    bool useOfferedTarget = false;
    bool offeredDirectStream = false;
    bool forceServerStream = false;
    bool forceTranscode = false;
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

inline std::string playbackSummary(const PlaybackTarget& target, const JellyfinItem& item) {
    std::string summary = playbackMethodName(target.playMethod);
    if (!item.videoCodec.empty()) summary += " / " + item.videoCodec;
    if (item.videoWidth > 0 && item.videoHeight > 0) {
        summary += " / " + std::to_string(item.videoWidth) + "X" + std::to_string(item.videoHeight);
    }
    return summary;
}

inline PlaybackTickPlan planPlaybackTick(
    bool playbackEnded,
    bool playbackPlaying,
    int positionMs,
    std::string_view itemType,
    const PlaybackSessionState& sessionState,
    const PlaybackTelemetryState& telemetryState,
    const PlaybackContinuationState& continuationState,
    PlaybackTelemetryState::TimePoint now
) {
    PlaybackTickPlan plan;
    plan.refreshTelemetry = !playbackEnded;
    plan.requestMediaSegments = !sessionState.mediaSegmentsRequested();
    plan.reportPlaybackStart = !telemetryState.playbackStartReported();
    plan.reportProgress = telemetryState.progressReportDue(now, playbackPlaying);
    plan.requestNextEpisode = !continuationState.nextEpisodeRequested()
        && itemType == "Episode"
        && positionMs >= 30000;
    return plan;
}

inline PlaybackReleasePlan planPlaybackRelease(
    bool requestedStopReport,
    bool completed,
    bool playbackStartReported,
    bool sessionValid,
    const JellyfinItem& item,
    const PlaybackTarget& target,
    int positionMs
) {
    PlaybackReleasePlan plan;
    plan.reportTicks = completed && item.runtimeTicks > 0
        ? item.runtimeTicks
        : playbackTicksFromPositionMs(positionMs);
    plan.cachedPositionTicks = completed ? 0 : plan.reportTicks;
    plan.markPlayed = completed;
    plan.reportStop = requestedStopReport
        && playbackStartReported
        && sessionValid
        && !item.id.empty()
        && !target.url.empty();
    return plan;
}

inline PlaybackFallbackPlan planPlaybackFallback(
    bool fallbackAttempted,
    PlaybackMethod currentMethod,
    bool sessionValid,
    std::string_view itemId,
    std::string_view currentUrl,
    std::string_view fallbackUrl,
    bool playbackStartReported,
    int positionMs,
    bool preferServerStream
) {
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

inline PlaybackTarget offeredPlaybackFallbackTarget(
    PlaybackTarget target,
    const PlaybackFallbackPlan& plan
) {
    if (!plan.retry || !plan.useOfferedTarget) return target;
    target.url = target.fallbackTranscodeUrl;
    target.fallbackTranscodeUrl.clear();
    target.transcoding = true;
    target.playMethod = plan.offeredDirectStream
        ? PlaybackMethod::DirectStream
        : PlaybackMethod::Transcode;
    target.startTicks = plan.resumeTicks;
    return target;
}

inline PlaybackTransitionPlan planPlaybackTransition(
    const PlaybackTarget& target,
    const JellyfinItem& item,
    bool streamRestart,
    bool restartPaused,
    int audioStreamIndex
) {
    PlaybackTransitionPlan plan;
    plan.startPositionMs = playbackPositionMsFromTicks(target.startTicks);
    plan.durationMs = playbackPositionMsFromTicks(item.runtimeTicks);
    plan.selectedAudioServerIndex = audioStreamIndex >= 0
        ? audioStreamIndex
        : target.audioStreamIndex;
    if (plan.selectedAudioServerIndex < 0 && !item.audios.empty()) {
        const auto preferred = std::find_if(item.audios.begin(), item.audios.end(), [](const JellyfinAudioStream& audio) {
            return audio.isDefault;
        });
        plan.selectedAudioServerIndex = preferred == item.audios.end()
            ? item.audios.front().index
            : preferred->index;
    }
    plan.selectedSubtitleServerIndex = target.subtitleStreamIndex;
    plan.pauseAfterRestart = streamRestart && restartPaused;
    plan.resetContinuation = !streamRestart;
    plan.resetMediaSegments = !streamRestart;
    return plan;
}

inline std::optional<JellyfinItem> selectAdjacentPlaybackEpisode(
    std::vector<JellyfinItem> episodes,
    const std::string& currentItemId,
    int currentSeason,
    int currentEpisode,
    int direction
) {
    if (direction == 0) return std::nullopt;

    std::sort(episodes.begin(), episodes.end(), [](const JellyfinItem& left, const JellyfinItem& right) {
        if (left.parentIndexNumber != right.parentIndexNumber) return left.parentIndexNumber < right.parentIndexNumber;
        if (left.indexNumber != right.indexNumber) return left.indexNumber < right.indexNumber;
        return left.name < right.name;
    });
    auto current = std::find_if(episodes.begin(), episodes.end(), [&](const JellyfinItem& candidate) {
        return candidate.id == currentItemId;
    });
    if (current == episodes.end() && currentSeason >= 0 && currentEpisode >= 0) {
        current = std::find_if(episodes.begin(), episodes.end(), [&](const JellyfinItem& candidate) {
            return sameEpisodeSlot(
                candidate.parentIndexNumber,
                candidate.indexNumber,
                currentSeason,
                currentEpisode
            );
        });
    }
    if (current == episodes.end()) return std::nullopt;

    int candidateIndex = static_cast<int>(std::distance(episodes.begin(), current)) + direction;
    while (candidateIndex >= 0 && candidateIndex < static_cast<int>(episodes.size())) {
        const auto& candidate = episodes[static_cast<size_t>(candidateIndex)];
        const bool duplicateSlot = sameEpisodeSlot(
            candidate.parentIndexNumber,
            candidate.indexNumber,
            current->parentIndexNumber,
            current->indexNumber
        );
        const bool specialOutsideRegularRun = current->parentIndexNumber > 0 && candidate.parentIndexNumber <= 0;
        if (!duplicateSlot && !specialOutsideRegularRun) return candidate;
        candidateIndex += direction;
    }
    return std::nullopt;
}

inline PlaybackContinuationPlan planPlaybackContinuation(
    bool playbackEnded,
    int positionMs,
    int durationMs,
    const PlaybackQueueState& queueState,
    const PlaybackContinuationState& continuationState,
    bool autoplayNext,
    int stillWatchingAfter
) {
    PlaybackContinuationPlan plan;
    const bool playbackComplete = playbackEnded
        || (durationMs > 1000 && positionMs >= durationMs - 1000);
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
        plan.action = shouldAutoplayNextEpisode(
            autoplayNext,
            continuationState.autoplayChainCount(),
            stillWatchingAfter
        )
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
    [[nodiscard]] PlaybackTelemetryState& telemetry() { return telemetryState_; }
    [[nodiscard]] const PlaybackTelemetryState& telemetry() const { return telemetryState_; }
    [[nodiscard]] PlaybackContinuationState& continuation() { return continuationState_; }
    [[nodiscard]] const PlaybackContinuationState& continuation() const { return continuationState_; }

    void activate(const JellyfinItem& item, const PlaybackTarget& target, TimePoint now) {
        sessionState_.setActive(item, target);
        sessionState_.setLastPlaybackSummary(playbackSummary(target, item));
        sessionState_.resetFallbackAttempted();
        telemetryState_.beginPlayback(now);
    }

    [[nodiscard]] PlaybackReleasePlan releasePlan(
        bool requestedStopReport,
        bool completed,
        bool jellyfinSessionValid,
        int positionMs
    ) const {
        return planPlaybackRelease(
            requestedStopReport,
            completed,
            telemetryState_.playbackStartReported(),
            jellyfinSessionValid,
            sessionState_.activeItem(),
            sessionState_.activeTarget(),
            positionMs
        );
    }

    void finishRelease() {
        sessionState_.clearPreparing();
        telemetryState_.clearPlaybackStartReported();
        sessionState_.clearActive();
        telemetryState_.resetReadIntervals();
        continuationState_.clearNextEpisode();
        sessionState_.resetMediaSegments();
    }

    [[nodiscard]] PlaybackTickPlan tickPlan(
        bool playbackEnded,
        bool playbackPlaying,
        int positionMs,
        std::string_view itemType,
        TimePoint now
    ) const {
        return planPlaybackTick(
            playbackEnded,
            playbackPlaying,
            positionMs,
            itemType,
            sessionState_,
            telemetryState_,
            continuationState_,
            now
        );
    }

    [[nodiscard]] PlaybackContinuationPlan continuationPlan(
        bool playbackEnded,
        int positionMs,
        int durationMs,
        const PlaybackQueueState& queueState,
        bool autoplayNext,
        int stillWatchingAfter
    ) const {
        return planPlaybackContinuation(
            playbackEnded,
            positionMs,
            durationMs,
            queueState,
            continuationState_,
            autoplayNext,
            stillWatchingAfter
        );
    }

    [[nodiscard]] PlaybackFallbackPlan fallbackPlan(
        bool jellyfinSessionValid,
        int positionMs,
        bool preferServerStream
    ) const {
        return planPlaybackFallback(
            sessionState_.fallbackAttempted(),
            sessionState_.activeTarget().playMethod,
            jellyfinSessionValid,
            sessionState_.activeItem().id,
            sessionState_.activeTarget().url,
            sessionState_.activeTarget().fallbackTranscodeUrl,
            telemetryState_.playbackStartReported(),
            positionMs,
            preferServerStream
        );
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

private:
    PlaybackSessionState sessionState_;
    PlaybackTelemetryState telemetryState_;
    PlaybackContinuationState continuationState_;
};
