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

enum class SubtitleStrategy {
    ClientText,
    ClientStyled,
    ServerTranscode,
};

enum class HdrOverrideMode {
    Auto = 0,
    ForceSdr = 1,
    AllowAllHdr = 2,
};

constexpr bool codecLevelAllowed(int mediaLevel, int overrideLevel) {
    return overrideLevel <= 0 || mediaLevel <= 0 || mediaLevel <= overrideLevel;
}

constexpr bool hdrCapabilityAllowed(bool detected, HdrOverrideMode mode) {
    if (mode == HdrOverrideMode::ForceSdr) return false;
    if (mode == HdrOverrideMode::AllowAllHdr) return true;
    return detected;
}

constexpr int clampSeekPositionMs(int64_t positionMs) {
    return static_cast<int>(std::clamp<int64_t>(positionMs, 0, std::numeric_limits<int>::max()));
}

constexpr int64_t playbackTicksFromPositionMs(int64_t positionMs) {
    return static_cast<int64_t>(clampSeekPositionMs(positionMs)) * 10000;
}

constexpr int playbackPositionMsFromTicks(int64_t ticks) {
    return clampSeekPositionMs(std::max<int64_t>(0, ticks) / 10000);
}

constexpr int initialPlayerSeekMs(int64_t desiredStartTicks) {
    // Media3 accepts a logical initial position before prepare, for both direct and
    // server-streamed targets. No stream re-resolution is needed for ordinary resume.
    return playbackPositionMsFromTicks(desiredStartTicks);
}

constexpr SubtitleStrategy subtitleStrategy(std::string_view codec) {
    if (codec == "srt" || codec == "subrip" || codec == "vtt" || codec == "webvtt") {
        return SubtitleStrategy::ClientText;
    }
    if (codec == "ass" || codec == "ssa") return SubtitleStrategy::ClientStyled;
    return SubtitleStrategy::ServerTranscode;
}
