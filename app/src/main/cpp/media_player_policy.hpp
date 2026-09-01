#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>

enum class StartupStep {
    PrepareMedia,
    ConfigureAvSync,
    ApplyInitialSeek,
    StartPlayback,
    ReadTrackMetadata,
};

enum class SeekApi {
    Legacy,
    ExactFrame,
};

enum class SeekStrategy {
    InPlace,
    RestartServerStream,
};

enum class SubtitleStrategy {
    ClientText,
    ServerTranscode,
};

constexpr int clampSeekPositionMs(int64_t positionMs) {
    return static_cast<int>(std::clamp<int64_t>(positionMs, 0, std::numeric_limits<int>::max()));
}

constexpr int64_t playbackTicksFromPositionMs(int64_t positionMs) {
    return static_cast<int64_t>(clampSeekPositionMs(positionMs)) * 10000;
}

constexpr int playbackPositionMsFromTicks(int64_t ticks) {
    return clampSeekPositionMs(std::max<int64_t>(0, ticks) / 10000);
}

constexpr SeekApi seekApiForPlayback() {
    return SeekApi::Legacy;
}

constexpr bool forceAudioClockSync() {
    return false;
}

constexpr bool preferServerStreamForStartup(std::string_view container, int64_t positionTicks) {
    return positionTicks > 0 && (container == "mkv" || container == "matroska");
}

constexpr int initialPlayerSeekMs(bool /* transcoding */, int64_t desiredStartTicks) {
    // Android's NuPlayer does not reliably instantiate the HLS decoders for Jellyfin
    // resume/transcode playlists until the prepared MediaPlayer receives the logical
    // initial seek. Keep this for both direct and transcoded targets; device E2E is the
    // authority here because omitting it produces a parsed HLS stream with no decoder.
    return playbackPositionMsFromTicks(desiredStartTicks);
}

constexpr SeekStrategy seekStrategy(bool currentlyTranscoding) {
    // HLS/transcoded playback is already decoder-safe and can seek in place. Re-resolving
    // PlaybackInfo for every HLS seek is both slower and can make Jellyfin return HTTP 500
    // while the previous transcode session is still active. Direct-play files use a fresh
    // server stream instead because in-place seeks have frozen video on Android TV.
    return currentlyTranscoding ? SeekStrategy::InPlace : SeekStrategy::RestartServerStream;
}

constexpr SubtitleStrategy subtitleStrategy(std::string_view codec) {
    return codec == "srt" || codec == "subrip" || codec == "vtt" || codec == "webvtt"
        ? SubtitleStrategy::ClientText
        : SubtitleStrategy::ServerTranscode;
}
