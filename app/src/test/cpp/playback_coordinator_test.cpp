#include "playback_coordinator.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;

    PlaybackSessionState session;
    PlaybackTelemetryState telemetry;
    PlaybackContinuationState continuation;
    PlaybackQueueState queue;
    const auto now = PlaybackTelemetryState::Clock::now();
    telemetry.beginPlayback(now - 11s);

    auto plan = planPlaybackTick(
        false,
        true,
        35000,
        "Episode",
        session,
        telemetry,
        continuation,
        now
    );
    assert(plan.refreshTelemetry);
    assert(plan.requestMediaSegments);
    assert(plan.reportPlaybackStart);
    assert(plan.reportProgress);
    assert(plan.requestNextEpisode);

    assert(telemetry.markPlaybackStartReported());
    telemetry.markProgressReport(now);
    assert(session.beginMediaSegmentsRequest(now));
    assert(continuation.beginNextEpisodeRequest(now));
    plan = planPlaybackTick(
        false,
        false,
        36000,
        "Episode",
        session,
        telemetry,
        continuation,
        now + 1s
    );
    assert(plan.refreshTelemetry);
    assert(!plan.requestMediaSegments);
    assert(!plan.reportPlaybackStart);
    assert(!plan.reportProgress);
    assert(!plan.requestNextEpisode);

    JellyfinItem releaseItem;
    releaseItem.id = "movie-1";
    releaseItem.runtimeTicks = 900'000'000;
    PlaybackTarget releaseTarget;
    releaseTarget.url = "https://media.example/movie-1";

    auto releasePlan = planPlaybackRelease(
        true,
        false,
        true,
        true,
        releaseItem,
        releaseTarget,
        12345
    );
    assert(releasePlan.reportTicks == 123'450'000);
    assert(releasePlan.cachedPositionTicks == 123'450'000);
    assert(!releasePlan.markPlayed);
    assert(releasePlan.reportStop);

    releasePlan = planPlaybackRelease(
        true,
        true,
        true,
        true,
        releaseItem,
        releaseTarget,
        12345
    );
    assert(releasePlan.reportTicks == releaseItem.runtimeTicks);
    assert(releasePlan.cachedPositionTicks == 0);
    assert(releasePlan.markPlayed);
    assert(releasePlan.reportStop);

    releasePlan = planPlaybackRelease(
        true,
        false,
        false,
        true,
        releaseItem,
        releaseTarget,
        12345
    );
    assert(!releasePlan.reportStop);

    releaseItem.runtimeTicks = 0;
    releasePlan = planPlaybackRelease(
        false,
        true,
        true,
        true,
        releaseItem,
        releaseTarget,
        54321
    );
    assert(releasePlan.reportTicks == 543'210'000);
    assert(releasePlan.cachedPositionTicks == 0);
    assert(releasePlan.markPlayed);
    assert(!releasePlan.reportStop);

    PlaybackTarget fallbackTarget;
    fallbackTarget.url = "https://media.example/direct";
    fallbackTarget.fallbackTranscodeUrl = "/master.m3u8?TranscodeReasons=ContainerNotSupported";
    fallbackTarget.playSessionId = "play-session";
    fallbackTarget.mediaSourceId = "media-source";
    fallbackTarget.audioStreamIndex = 3;
    fallbackTarget.subtitleStreamIndex = 7;

    auto fallbackPlan = planPlaybackFallback(
        false,
        PlaybackMethod::DirectPlay,
        true,
        "movie-1",
        fallbackTarget.url,
        fallbackTarget.fallbackTranscodeUrl,
        true,
        43210,
        true
    );
    assert(fallbackPlan.retry);
    assert(fallbackPlan.resumeTicks == 432'100'000);
    assert(fallbackPlan.reportPrevious);
    assert(fallbackPlan.useOfferedTarget);
    assert(fallbackPlan.offeredDirectStream);
    assert(!fallbackPlan.forceServerStream);
    assert(!fallbackPlan.forceTranscode);

    auto offeredTarget = offeredPlaybackFallbackTarget(fallbackTarget, fallbackPlan);
    assert(offeredTarget.url == fallbackTarget.fallbackTranscodeUrl);
    assert(offeredTarget.fallbackTranscodeUrl.empty());
    assert(offeredTarget.transcoding);
    assert(offeredTarget.playMethod == PlaybackMethod::DirectStream);
    assert(offeredTarget.startTicks == fallbackPlan.resumeTicks);
    assert(offeredTarget.playSessionId == fallbackTarget.playSessionId);
    assert(offeredTarget.mediaSourceId == fallbackTarget.mediaSourceId);
    assert(offeredTarget.audioStreamIndex == fallbackTarget.audioStreamIndex);
    assert(offeredTarget.subtitleStreamIndex == fallbackTarget.subtitleStreamIndex);

    fallbackPlan = planPlaybackFallback(
        false,
        PlaybackMethod::DirectPlay,
        true,
        "movie-1",
        fallbackTarget.url,
        {},
        false,
        12345,
        false
    );
    assert(fallbackPlan.retry);
    assert(!fallbackPlan.reportPrevious);
    assert(!fallbackPlan.useOfferedTarget);
    assert(!fallbackPlan.offeredDirectStream);
    assert(!fallbackPlan.forceServerStream);
    assert(fallbackPlan.forceTranscode);

    fallbackPlan = planPlaybackFallback(
        false,
        PlaybackMethod::DirectStream,
        true,
        "movie-1",
        fallbackTarget.url,
        {},
        true,
        12345,
        true
    );
    assert(fallbackPlan.retry);
    assert(fallbackPlan.forceServerStream);
    assert(!fallbackPlan.forceTranscode);

    assert(!planPlaybackFallback(
        true,
        PlaybackMethod::DirectPlay,
        true,
        "movie-1",
        fallbackTarget.url,
        fallbackTarget.fallbackTranscodeUrl,
        true,
        1000,
        false
    ).retry);
    assert(!planPlaybackFallback(
        false,
        PlaybackMethod::Transcode,
        true,
        "movie-1",
        fallbackTarget.url,
        {},
        true,
        1000,
        false
    ).retry);
    assert(!planPlaybackFallback(
        false,
        PlaybackMethod::DirectPlay,
        false,
        "movie-1",
        fallbackTarget.url,
        {},
        true,
        1000,
        false
    ).retry);
    assert(!planPlaybackFallback(
        false,
        PlaybackMethod::DirectPlay,
        true,
        {},
        fallbackTarget.url,
        {},
        true,
        1000,
        false
    ).retry);

    JellyfinItem episode1;
    episode1.id = "episode-1";
    JellyfinItem episode2;
    episode2.id = "episode-2";
    queue.replace({episode1, episode2}, 0);
    queue.setRepeatMode(QueueRepeatMode::One);
    auto continuationPlan = planPlaybackContinuation(
        true,
        120000,
        120000,
        queue,
        continuation,
        true,
        3
    );
    assert(continuationPlan.action == PlaybackContinuationAction::PlayQueueIndex);
    assert(continuationPlan.queueIndex == 0);
    assert(continuationPlan.repeatCurrentQueueItem);
    assert(!continuationPlan.resetAutoplayChain);

    queue.setRepeatMode(QueueRepeatMode::All);
    queue.setCurrentIndex(1);
    continuationPlan = planPlaybackContinuation(
        false,
        119500,
        120000,
        queue,
        continuation,
        true,
        3
    );
    assert(continuationPlan.action == PlaybackContinuationAction::PlayQueueIndex);
    assert(continuationPlan.queueIndex == 0);
    assert(!continuationPlan.repeatCurrentQueueItem);

    queue.setRepeatMode(QueueRepeatMode::Off);
    continuation.clearNextEpisodeRequest();
    continuation.setNextItem(episode2);
    continuationPlan = planPlaybackContinuation(
        false,
        119500,
        120000,
        queue,
        continuation,
        true,
        3
    );
    assert(continuationPlan.action == PlaybackContinuationAction::AutoplayNext);

    continuation.incrementAutoplayChain();
    continuation.incrementAutoplayChain();
    continuation.incrementAutoplayChain();
    continuationPlan = planPlaybackContinuation(
        false,
        119500,
        120000,
        queue,
        continuation,
        true,
        3
    );
    assert(continuationPlan.action == PlaybackContinuationAction::ShowStillWatching);

    continuation.clearNextItem();
    continuationPlan = planPlaybackContinuation(
        true,
        0,
        0,
        queue,
        continuation,
        true,
        3
    );
    assert(continuationPlan.action == PlaybackContinuationAction::Stop);
    assert(continuationPlan.resetAutoplayChain);

    queue.reset();
    continuationPlan = planPlaybackContinuation(
        false,
        0,
        1000,
        queue,
        continuation,
        true,
        3
    );
    assert(continuationPlan.action == PlaybackContinuationAction::None);
    assert(!continuationPlan.resetAutoplayChain);

    return 0;
}
