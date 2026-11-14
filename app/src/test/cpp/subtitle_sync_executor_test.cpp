#include "subtitle_sync_executor.hpp"
#include "hls_playback_policy.hpp"
#include "playback_coordinator.hpp"

#include <cassert>

struct Client {
    ApiValueResult<PlaybackTarget> result;
    int resolutions = 0;
    int stops = 0;
    std::string device;
    bool freshSessions = false;
    std::vector<int64_t> positions;

    std::string subtitleAudioProfileUrl(const JellyfinSession& session, const JellyfinItem&,
                                        const std::string& sourceId) {
        assert(session.deviceId == "player");
        assert(sourceId == "alias");
        return "https://fixture/static.mp4?MediaSourceId=alias";
    }

    ApiValueResult<PlaybackTarget> resolvePlayback(const JellyfinSession& session, const JellyfinItem& item, int, int,
                                                   PlaybackOverrides overrides, int audio, int subtitle) {
        ++resolutions;
        device = session.deviceId;
        assert(device == "player-subtitle-probe");
        assert(item.mediaSourceId == "alias");
        positions.push_back(item.positionTicks);
        assert(item.positionTicks >= 0);
        assert(overrides.forceServerStream);
        assert(audio == 4 && subtitle == kSubtitleOffIndex);
        auto response = result;
        if (freshSessions) {
            response.value.playSessionId = "probe-session-" + std::to_string(resolutions);
            response.value.url = "https://fixture/probe-" + std::to_string(resolutions) + ".m3u8";
        }
        return response;
    }

    ApiResult stopTranscoding(const JellyfinSession& session, const PlaybackTarget& target) {
        ++stops;
        assert(session.deviceId == "player-subtitle-probe");
        assert(target.playSessionId.starts_with("probe-session"));
        return {};
    }
};

int main() {
    // A library estimate must not autoplay over the selected release's ending.
    {
        JellyfinItem episode;
        episode.id = "episode";
        episode.runtimeTicks = 2580LL * 10000000;
        PlaybackTarget release;
        release.sourceRuntimeTicks = 26665390000LL;
        release.playMethod = PlaybackMethod::Transcode;
        const auto plan = planPlaybackTransition(release, episode, true, true, -1);
        assert(plan.durationMs == 2666539 && plan.pauseAfterRestart);
        PlaybackCoordinator coordinator;
        coordinator.activate(episode, release, PlaybackCoordinator::Clock::now());
        const auto read = coordinator.consumeTelemetryRead(PlaybackCoordinator::Clock::now(), true, 2580000);
        assert(read.knownDurationMs == 2666539);
        assert(coordinator.continuationPlan(false, 2579500, read.knownDurationMs, {}, true, 0).action ==
               PlaybackContinuationAction::None);
        release.sourceRuntimeTicks = 0;
        assert(planPlaybackTransition(release, episode, false, false, -1).durationMs == 2580000);
    }

    JellyfinSession session;
    session.deviceId = "player";
    PlaybackTarget playback;
    playback.url = "https://fixture/player.m3u8";
    playback.playSessionId = "player-session";
    playback.mediaSourceId = "alias";
    playback.sourceFingerprint = "release-a";
    playback.audioStreamIndex = 4;
    playback.audioTracks = {{1, 2, "aac", "jpn", "Japanese", true}, {4, 6, "aac", "eng", "English", false}};
    playback.startTicks = 3850000000LL;
    playback.playMethod = PlaybackMethod::DirectStream;
    Client client;
    client.result.ok = true;
    client.result.value = playback;
    client.result.value.playSessionId = "probe-session";
    client.result.value.url = "https://fixture/probe.m3u8";
    SubtitleAudioSyncRequest request;
    request.mediaUrl = playback.url;
    bool decoded = false;
    const auto synchronize = [&](const SubtitleAudioSyncRequest& probe) {
        decoded = true;
        assert(probe.mediaUrl == client.result.value.url);
        assert(probe.audioOrdinal == -1);
        assert(probe.profileMediaUrl == "https://fixture/static.mp4?MediaSourceId=alias");
        assert(probe.profileAudioOrdinal == 1); // second physical audio, first in the HLS
        SubtitleAudioSyncResult result;
        result.decision.confident = true;
        return result;
    };
    assert(
        executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, synchronize).decision.confident);
    assert(decoded && client.resolutions == 1 && client.stops == 1);
    // A reused alias with different backing metadata must never be decoded.
    decoded = false;
    client.result.value.sourceFingerprint = "release-b";
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, synchronize)
                .decision.confident);
    assert(!decoded && client.stops == 2);
    client.result.value.sourceFingerprint = playback.sourceFingerprint;
    client.result.value.audioStreamIndex = 1;
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, synchronize)
                .decision.confident);
    assert(!decoded && client.stops == 3);
    client.result.value.audioStreamIndex = 4;
    // A server which echoes the live session ID must not trigger its cleanup.
    client.result.value.playSessionId = playback.playSessionId;
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, synchronize)
                .decision.confident);
    assert(!decoded && client.stops == 3);
    const int resolutions = client.resolutions;
    request.cancelled = [] { return true; };
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, synchronize)
                .decision.confident);
    assert(client.resolutions == resolutions);
    request.cancelled = {};
    playback.sourceFingerprint.clear();
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, synchronize)
                .decision.confident);
    assert(client.resolutions == resolutions);
    // Ordinary files do not need a server-side probe session.
    request.mediaUrl = "https://fixture/file.mkv";
    const auto direct = [](const SubtitleAudioSyncRequest& probe) {
        assert(probe.mediaUrl == "https://fixture/file.mkv");
        SubtitleAudioSyncResult result;
        result.decision.confident = true;
        return result;
    };
    assert(executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, direct).decision.confident);
    assert(client.resolutions == resolutions);
    // Distant HLS windows negotiate their own source start and clean up all sessions.
    playback.sourceFingerprint = "release-a";
    client.result.value = playback;
    client.freshSessions = true;
    request.mediaUrl = playback.url;
    const int previousStops = client.stops;
    request.playbackTimelineOriginMs = 1153069;
    const auto verifyWindows = [&](const SubtitleAudioSyncRequest& probe) {
        assert(probe.verificationMediaUrl);
        assert(probe.subtitleTimelineBaseMs == 385000);
        assert(probe.playerTimelineBaseMs == 385000);
        assert(probe.playbackTimelineOriginMs == 1153069);
        const auto middle = probe.verificationMediaUrl(300000);
        const auto late = probe.verificationMediaUrl(700000);
        const auto earlier = probe.verificationMediaUrl(-385000);
        assert(earlier && *earlier != *middle && *earlier != *late);
        assert(!probe.verificationMediaUrl(-385001));
        assert(middle && late && *middle != *late && *middle != probe.mediaUrl);
        SubtitleAudioSyncResult result;
        result.decision.confident = true;
        return result;
    };
    assert(executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, verifyWindows)
               .decision.confident);
    assert(client.stops == previousStops + 4);
    assert(client.positions[client.positions.size() - 3] == 6850000000LL);
    assert(client.positions[client.positions.size() - 2] == 10850000000LL);
    assert(client.positions.back() == 0);
    // Reject and clean a wrong release returned only for a distant window.
    const int stopsBeforeMismatch = client.stops;
    const auto wrongWindow = [&](const SubtitleAudioSyncRequest& probe) {
        client.result.value.sourceFingerprint = "different-release";
        assert(!probe.verificationMediaUrl(700000));
        return SubtitleAudioSyncResult{};
    };
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, wrongWindow)
                .decision.confident);
    assert(client.stops == stopsBeforeMismatch + 2);
    client.result.value.sourceFingerprint = playback.sourceFingerprint;
    bool cancelled = false;
    request.cancelled = [&] { return cancelled; };
    const auto cancelWindow = [&](const SubtitleAudioSyncRequest& probe) {
        const int before = client.resolutions;
        cancelled = true;
        assert(!probe.verificationMediaUrl(700000));
        assert(client.resolutions == before);
        return SubtitleAudioSyncResult{};
    };
    const int stopsBeforeCancel = client.stops;
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, cancelWindow)
                .decision.confident);
    assert(client.stops == stopsBeforeCancel + 1);
    // Initial nonzero samples negotiate a fresh server base, avoiding a long HLS seek.
    cancelled = false;
    request.sampleStartMs = 30000;
    const auto shifted = [&](const SubtitleAudioSyncRequest& probe) {
        assert(probe.sampleStartMs == 0);
        assert(client.positions.back() == 4150000000LL);
        return SubtitleAudioSyncResult{};
    };
    executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, shifted);
    // Late restarts probe a complete earlier window without changing playback's base.
    playback.startTicks = 11737350000LL;
    request.sampleStartMs = 0;
    request.sourceDurationMs = 1336384;
    request.subtitleTimelineBaseMs = 1173735;
    const auto beforeCredits = [&](const SubtitleAudioSyncRequest& probe) {
        assert(probe.sampleStartMs == 0 && probe.subtitleTimelineBaseMs == 1036384);
        assert(probe.playerTimelineBaseMs == 1173735);
        assert(probe.playbackTimelineOriginMs == 1153069);
        assert(client.positions.back() == 10363840000LL);
        return SubtitleAudioSyncResult{};
    };
    executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, beforeCredits);
    request.sampleStartMs = -1;
    const int beforeInvalid = client.resolutions;
    assert(!executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, shifted).decision.confident);
    assert(client.resolutions == beforeInvalid);
    // A long subtitle analysis can outlive a paused HLS playlist. Seeking must
    // replace that playlist and an early EOF must never authorize autoplay.
    assert(serverStreamSeekNeedsRestart(false, "https://fixture/stream.m3u8", 1150000, 0));
    assert(serverStreamSeekNeedsRestart(false, "https://fixture/stream.m3u8", 1000, 1150000));
    assert(!serverStreamSeekNeedsRestart(true, "https://fixture/file.mp4", 1150000, 0));
    assert(!serverStreamSeekNeedsRestart(false, "https://fixture/file.ts", 1150000, 0));
    assert(prematureHlsEndPosition(45000, -1, 2666539) == 45000);
    assert(prematureHlsEndPosition(45000, 1150000, 2666539) == 1150000);
    assert(!prematureHlsEndPosition(2660000, -1, 2666539));
    HlsRecoveryBudget recovery;
    const auto instant = HlsRecoveryBudget::Clock::now();
    assert(recovery.consume("episode", "release", instant));
    assert(!recovery.consume("episode", "release", instant + std::chrono::seconds(1)));
    assert(!recovery.consume("episode", "release", instant + std::chrono::seconds(31)));
    recovery.observePlaying(true, instant + std::chrono::seconds(31));
    recovery.observePlaying(false, instant + std::chrono::seconds(50));
    assert(!recovery.consume("episode", "release", instant + std::chrono::seconds(61)));
    recovery.observePlaying(true, instant + std::chrono::seconds(62));
    recovery.observePlaying(true, instant + std::chrono::seconds(93));
    assert(recovery.consume("episode", "release", instant + std::chrono::seconds(93)));
    recovery.reset();
    assert(recovery.consume("episode", "release", instant + std::chrono::seconds(94)));
}
