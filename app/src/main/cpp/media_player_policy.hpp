#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
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

struct PlaybackBufferDurations {
    int minBufferMs = -1;
    int maxBufferMs = -1;
    int bufferForPlaybackMs = -1;
    int bufferForPlaybackAfterRebufferMs = -1;

    [[nodiscard]] constexpr bool custom() const {
        return minBufferMs >= 0 && maxBufferMs >= 0
            && bufferForPlaybackMs >= 0 && bufferForPlaybackAfterRebufferMs >= 0;
    }
};

constexpr PlaybackBufferDurations playbackBufferDurations(int preset) {
    if (preset == 1) return {50'000, 120'000, 2'500, 5'000};
    if (preset == 2) return {80'000, 240'000, 5'000, 10'000};
    return {};
}

constexpr bool codecLevelAllowed(int mediaLevel, int overrideLevel) {
    return overrideLevel <= 0 || mediaLevel <= 0 || mediaLevel <= overrideLevel;
}

constexpr bool hdrCapabilityAllowed(bool detected, HdrOverrideMode mode) {
    if (mode == HdrOverrideMode::ForceSdr) return false;
    if (mode == HdrOverrideMode::AllowAllHdr) return true;
    return detected;
}

constexpr bool shouldAutoplayNextEpisode(bool autoplayEnabled, int completedAutoplays, int stillWatchingAfter) {
    if (!autoplayEnabled) return false;
    const int threshold = std::max(0, stillWatchingAfter);
    return completedAutoplays < threshold;
}

inline std::string transcodingReasonsFromUrl(std::string_view url) {
    constexpr std::string_view marker = "TranscodeReasons=";
    const size_t begin = url.find(marker);
    if (begin == std::string_view::npos) return {};
    const size_t valueBegin = begin + marker.size();
    const size_t end = url.find('&', valueBegin);
    std::string value(url.substr(valueBegin, end == std::string_view::npos ? url.size() - valueBegin : end - valueBegin));
    for (size_t index = 0; index + 2 < value.size();) {
        if (value[index] == '%' && value[index + 1] == '2' && (value[index + 2] == 'C' || value[index + 2] == 'c')) {
            value.replace(index, 3, ",");
        } else {
            ++index;
        }
    }
    return value;
}

inline bool directStreamTranscodeReason(std::string_view reason) {
    static constexpr std::array<std::string_view, 10> allowed{
        "AudioCodecNotSupported",
        "AudioBitrateNotSupported",
        "AudioChannelsNotSupported",
        "AudioProfileNotSupported",
        "AudioSampleRateNotSupported",
        "SecondaryAudioNotSupported",
        "AudioBitDepthNotSupported",
        "AudioIsExternal",
        "ContainerNotSupported",
        "VideoCodecTagNotSupported",
    };
    return std::find(allowed.begin(), allowed.end(), reason) != allowed.end();
}

inline bool transcodingUrlRepresentsDirectStream(std::string_view url) {
    const std::string reasons = transcodingReasonsFromUrl(url);
    if (reasons.empty()) return false;
    size_t begin = 0;
    while (begin < reasons.size()) {
        const size_t end = reasons.find(',', begin);
        const std::string_view reason(reasons.data() + begin, (end == std::string::npos ? reasons.size() : end) - begin);
        if (reason.empty() || !directStreamTranscodeReason(reason)) return false;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return true;
}

constexpr bool shouldAcceptPostSeekTelemetry(
    int observedPositionMs,
    int targetPositionMs,
    int64_t elapsedSinceSeekMs,
    int toleranceMs = 1500,
    int holdMs = 750
) {
    if (targetPositionMs < 0) return true;
    const int64_t difference = static_cast<int64_t>(observedPositionMs) - targetPositionMs;
    const int64_t absoluteDifference = difference < 0 ? -difference : difference;
    return absoluteDifference <= std::max(0, toleranceMs) || elapsedSinceSeekMs >= std::max(0, holdMs);
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

constexpr bool preferExternalSubtitleDelivery(SubtitleStrategy strategy, bool hasDeliveryUrl) {
    return strategy == SubtitleStrategy::ClientStyled && hasDeliveryUrl;
}
