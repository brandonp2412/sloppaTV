#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

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
    ClientEmbedded,
    ServerTranscode,
};

enum class HdrOverrideMode {
    Auto = 0,
    ForceSdr = 1,
    AllowAllHdr = 2,
};

struct PlaybackOverrides {
    int maxAvcLevel = 0;
    int maxHevcLevel = 0;
    HdrOverrideMode hdrMode = HdrOverrideMode::Auto;
    bool forceTranscode = false;
    bool forceServerStream = false;
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

constexpr int heldSeekMultiplier(int repeatCount) {
    if (repeatCount < 3) return 1;
    if (repeatCount < 8) return 2;
    if (repeatCount < 14) return 4;
    if (repeatCount < 22) return 8;
    if (repeatCount < 32) return 16;
    return 32;
}

constexpr int64_t heldSeekDeltaMs(int seekSeconds, int repeatCount) {
    return static_cast<int64_t>(std::max(0, seekSeconds))
        * 1000
        * heldSeekMultiplier(std::max(0, repeatCount));
}

constexpr int playbackPrepareTimeoutMs(bool transcoding) {
    return transcoding ? 30'000 : 15'000;
}

constexpr bool playbackPrepareTimedOut(bool transcoding, int64_t elapsedMs) {
    return elapsedMs >= playbackPrepareTimeoutMs(transcoding);
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

inline std::string serverStreamVideoCodecList(const std::vector<std::string>& directCodecs) {
    std::string result;
    auto append = [&](std::string_view codec) {
        if (std::find(directCodecs.begin(), directCodecs.end(), codec) == directCodecs.end()) return;
        if (!result.empty()) result += ',';
        result += codec;
    };
    append("hevc");
    append("h264");
    return result.empty() ? "h264" : result;
}

constexpr bool postSeekPositionMatchesTarget(
    int observedPositionMs,
    int targetPositionMs,
    int toleranceMs = 1500
) {
    if (targetPositionMs < 0) return true;
    const int64_t difference = static_cast<int64_t>(observedPositionMs) - targetPositionMs;
    const int64_t absoluteDifference = difference < 0 ? -difference : difference;
    return absoluteDifference <= std::max(0, toleranceMs);
}

constexpr bool shouldAcceptPostSeekTelemetry(
    int observedPositionMs,
    int targetPositionMs,
    int64_t elapsedSinceSeekMs,
    int toleranceMs = 1500,
    int holdMs = 5000
) {
    return postSeekPositionMatchesTarget(observedPositionMs, targetPositionMs, toleranceMs)
        || elapsedSinceSeekMs >= std::max(0, holdMs);
}

constexpr bool postSeekPositionFailed(
    int observedPositionMs,
    int targetPositionMs,
    int64_t elapsedSinceSeekMs,
    int toleranceMs = 1500,
    int failureMs = 1500
) {
    return targetPositionMs >= 0
        && elapsedSinceSeekMs >= std::max(0, failureMs)
        && !postSeekPositionMatchesTarget(observedPositionMs, targetPositionMs, toleranceMs);
}

constexpr bool shouldFallbackAfterUnseekableSeek(
    bool mediaSeekable,
    int observedPositionMs,
    int targetPositionMs,
    bool seekFailureMatured
) {
    return targetPositionMs >= 0
        && !mediaSeekable
        && !postSeekPositionMatchesTarget(observedPositionMs, targetPositionMs)
        && seekFailureMatured;
}

constexpr int clampSeekPositionMs(int64_t positionMs) {
    return static_cast<int>(std::clamp<int64_t>(positionMs, 0, std::numeric_limits<int>::max()));
}

constexpr int relativeSeekPositionMs(int currentPositionMs, int64_t deltaMs, int durationMs) {
    const int64_t upperBound = durationMs > 0
        ? static_cast<int64_t>(durationMs)
        : static_cast<int64_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(currentPositionMs) + deltaMs, 0, upperBound));
}

constexpr int64_t playbackTicksFromPositionMs(int64_t positionMs) {
    return static_cast<int64_t>(clampSeekPositionMs(positionMs)) * 10000;
}

constexpr int playbackPositionMsFromTicks(int64_t ticks) {
    return clampSeekPositionMs(std::max<int64_t>(0, ticks) / 10000);
}

constexpr int initialPlayerSeekMs(int64_t desiredStartTicks) {
    // libmpv accepts the logical initial position as a loadfile start option for both
    // direct and server-streamed targets. No stream re-resolution is needed for ordinary resume.
    return playbackPositionMsFromTicks(desiredStartTicks);
}

constexpr bool subtitleCodecEquals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (size_t index = 0; index < left.size(); ++index) {
        unsigned char a = static_cast<unsigned char>(left[index]);
        unsigned char b = static_cast<unsigned char>(right[index]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

constexpr SubtitleStrategy subtitleStrategy(std::string_view codec) {
    if (subtitleCodecEquals(codec, "srt") || subtitleCodecEquals(codec, "subrip")
        || subtitleCodecEquals(codec, "vtt") || subtitleCodecEquals(codec, "webvtt")
        || subtitleCodecEquals(codec, "mov_text")) {
        return SubtitleStrategy::ClientText;
    }
    if (subtitleCodecEquals(codec, "ass") || subtitleCodecEquals(codec, "ssa")) {
        return SubtitleStrategy::ClientStyled;
    }
    if (subtitleCodecEquals(codec, "pgs") || subtitleCodecEquals(codec, "pgssub")
        || subtitleCodecEquals(codec, "hdmv_pgs_subtitle")
        || subtitleCodecEquals(codec, "dvdsub") || subtitleCodecEquals(codec, "dvd_subtitle")
        || subtitleCodecEquals(codec, "dvbsub") || subtitleCodecEquals(codec, "dvb_subtitle")
        || subtitleCodecEquals(codec, "xsub")) {
        return SubtitleStrategy::ClientEmbedded;
    }
    return SubtitleStrategy::ServerTranscode;
}

constexpr bool useNativeSubtitleRenderer(SubtitleStrategy strategy, bool subtitleSelected) {
    return subtitleSelected
        && (strategy == SubtitleStrategy::ClientText
            || strategy == SubtitleStrategy::ClientStyled);
}

constexpr bool canSwitchEmbeddedSubtitleInPlayer(SubtitleStrategy strategy, bool isExternal) {
    return strategy == SubtitleStrategy::ClientEmbedded && !isExternal;
}
