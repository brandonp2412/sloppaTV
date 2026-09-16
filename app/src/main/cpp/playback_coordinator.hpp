#pragma once

#include "media_player_policy.hpp"
#include "playback_continuation.hpp"
#include "playback_queue.hpp"
#include "playback_session.hpp"
#include "playback_telemetry.hpp"

#include <chrono>
#include <string_view>

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
