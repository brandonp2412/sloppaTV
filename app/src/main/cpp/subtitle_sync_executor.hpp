#pragma once

#include "jellyfin_types.hpp"
#include "media_player_policy.hpp"
#include "subtitle_audio_probe.hpp"

#include <utility>
#include <limits>
#include <vector>

inline bool subtitleProbeSessionCompatible(const PlaybackTarget& playback, const PlaybackTarget& probe) {
    return !playback.playSessionId.empty() && !probe.playSessionId.empty() &&
           playback.playSessionId != probe.playSessionId && !playback.sourceFingerprint.empty() &&
           playback.sourceFingerprint == probe.sourceFingerprint && probe.playMethod == PlaybackMethod::DirectStream &&
           probe.audioStreamIndex == playback.audioStreamIndex && !probe.url.empty() && probe.url != playback.url;
}

// Opening an active HLS URL can restart its server transcode. Negotiate a
// separate device/session and verify the backing release before decoding it.
template <typename Client, typename Synchronize>
SubtitleAudioSyncResult executeSubtitleAudioSync(Client& client, JellyfinSession session, JellyfinItem item,
                                                 const PlaybackTarget& playback, SubtitleAudioSyncRequest request,
                                                 int maxBitrate, int maxAudioChannels, Synchronize synchronize) {
    const auto failure = [](const char* reason) {
        SubtitleAudioSyncResult result;
        result.failure = reason;
        return result;
    };
    if (request.cancelled && request.cancelled()) return failure("audio probe cancelled");
    if (request.mediaUrl.find(".m3u8") == std::string::npos) return synchronize(request);
    if (playback.sourceFingerprint.empty() || playback.playSessionId.empty())
        return failure("HLS source identity unavailable for independent subtitle probe");
    session.deviceId += "-subtitle-probe";
    item.mediaSourceId = playback.mediaSourceId;
    const int64_t initialDelta = static_cast<int64_t>(request.sampleStartMs) * 10000;
    if (request.sampleStartMs < 0 || playback.startTicks < 0 ||
        playback.startTicks > std::numeric_limits<int64_t>::max() - initialDelta)
        return failure("invalid subtitle probe sample position");
    item.positionTicks = playback.startTicks + initialDelta;
    // Leave a full verification window before the credits, even when playback
    // restarted close to EOF. The decoded origin still anchors this earlier probe.
    if (request.sourceDurationMs >= 300000) {
        const int64_t lastWindowTicks = static_cast<int64_t>(request.sourceDurationMs - 300000) * 10000;
        item.positionTicks = std::min(item.positionTicks, lastWindowTicks);
    }
    PlaybackOverrides overrides;
    overrides.forceServerStream = true;
    auto negotiated = client.resolvePlayback(session, item, maxBitrate, maxAudioChannels, overrides,
                                             playback.audioStreamIndex, kSubtitleOffIndex);
    if (!negotiated.ok) return failure("independent subtitle probe negotiation failed");
    const auto& target = negotiated.value;
    // Never clean up a session which the server accidentally shared with playback.
    if (target.playSessionId.empty() || target.playSessionId == playback.playSessionId)
        return failure("server did not isolate the subtitle probe session");

    struct Cleanup {
        Client& client;
        const JellyfinSession& session;
        std::vector<PlaybackTarget> targets;
        size_t stopped = 0;

        Cleanup(Client& api, const JellyfinSession& credentials) : client(api), session(credentials) {}

        void stopFinished() {
            for (; stopped < targets.size(); ++stopped)
                static_cast<void>(client.stopTranscoding(session, targets[stopped]));
        }

        ~Cleanup() { stopFinished(); }
    } cleanup{client, session};

    cleanup.targets.push_back(target);

    if (!subtitleProbeSessionCompatible(playback, target))
        return failure("independent subtitle probe resolved a different release or audio track");
    if (request.cancelled && request.cancelled()) return failure("audio probe cancelled");
    request.verificationMediaUrl = [&](int sampleStartMs) -> std::optional<std::string> {
        if (request.cancelled && request.cancelled()) return std::nullopt;
        auto windowItem = item;
        const int64_t delta = static_cast<int64_t>(sampleStartMs) * 10000;
        if ((delta < 0 && playback.startTicks < -delta) ||
            (delta > 0 && playback.startTicks > std::numeric_limits<int64_t>::max() - delta))
            return std::nullopt;
        windowItem.positionTicks = playback.startTicks + delta;
        auto window = client.resolvePlayback(session, windowItem, maxBitrate, maxAudioChannels, overrides,
                                             playback.audioStreamIndex, kSubtitleOffIndex);
        if (!window.ok || window.value.playSessionId.empty() || window.value.playSessionId == playback.playSessionId)
            return std::nullopt;
        for (const auto& previous : cleanup.targets)
            if (previous.playSessionId == window.value.playSessionId) return std::nullopt;
        cleanup.stopFinished();
        cleanup.targets.push_back(window.value);
        if (!subtitleProbeSessionCompatible(playback, window.value)) return std::nullopt;
        return window.value.url;
    };
    request.mediaUrl = target.url;
    request.sampleStartMs = 0; // The server started this independent HLS session at the requested window.
    request.audioOrdinal = -1; // The server selected the requested audio into this HLS stream.
    return synchronize(request);
}
