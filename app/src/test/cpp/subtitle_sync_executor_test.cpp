#include "subtitle_sync_executor.hpp"

#include <cassert>

struct Client {
    ApiValueResult<PlaybackTarget> result;
    int resolutions = 0;
    int stops = 0;
    std::string device;
    bool freshSessions = false;
    std::vector<int64_t> positions;

    ApiValueResult<PlaybackTarget> resolvePlayback(const JellyfinSession& session, const JellyfinItem& item, int, int,
                                                   PlaybackOverrides overrides, int audio, int subtitle) {
        ++resolutions;
        device = session.deviceId;
        assert(device == "player-subtitle-probe");
        assert(item.mediaSourceId == "alias");
        positions.push_back(item.positionTicks);
        assert(item.positionTicks >= 3850000000LL);
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
    JellyfinSession session;
    session.deviceId = "player";
    PlaybackTarget playback;
    playback.url = "https://fixture/player.m3u8";
    playback.playSessionId = "player-session";
    playback.mediaSourceId = "alias";
    playback.sourceFingerprint = "release-a";
    playback.audioStreamIndex = 4;
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
    const auto verifyWindows = [&](const SubtitleAudioSyncRequest& probe) {
        assert(probe.verificationMediaUrl);
        const auto middle = probe.verificationMediaUrl(300000);
        const auto late = probe.verificationMediaUrl(700000);
        assert(middle && late && *middle != *late && *middle != probe.mediaUrl);
        SubtitleAudioSyncResult result;
        result.decision.confident = true;
        return result;
    };
    assert(executeSubtitleAudioSync(client, session, {}, playback, request, 120000000, 2, verifyWindows)
               .decision.confident);
    assert(client.stops == previousStops + 3);
    assert(client.positions[client.positions.size() - 2] == 6850000000LL);
    assert(client.positions.back() == 10850000000LL);
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
}
