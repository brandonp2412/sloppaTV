#pragma once

#include "subtitle_auto_sync.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <limits>
#include <string>
#include <vector>

struct SubtitleAudioSyncRequest {
    std::string mediaUrl;
    std::string privateFilesDir;
    std::vector<SubtitleCue> cues;
    int sampleStartMs = 0;
    // Start-of-episode music may obscure speech; analyse a longer continuous
    // sample without seeking or opening a second stream.
    bool extendedSample = false;
    // DirectStream starts at its own zero; subtitle cues remain episode-relative.
    int subtitleTimelineBaseMs = 0;
};

struct SubtitleAudioSyncResult {
    SubtitleSyncDecision decision;
    int audioMs = 0;
    std::string failure;
};

// Convert sample count to milliseconds using wide arithmetic. On 32-bit Android,
// a five-minute 16 kHz recording overflows size_t when multiplied by 1000.
inline int subtitlePcmDurationMs(std::size_t sampleCount, int sampleRate) {
    if (sampleRate <= 0) return 0;
    const auto duration = static_cast<std::uint64_t>(sampleCount) * 1000ULL / static_cast<std::uint64_t>(sampleRate);
    return static_cast<int>(std::min<std::uint64_t>(duration, std::numeric_limits<int>::max()));
}

// The audio filter sees decoded frame PTS before mpv trims a precise seek.
// HLS can land beyond the requested start; anchoring to the request in that
// case turns a seek error into a bogus subtitle offset.
inline std::optional<int> subtitleProbeFirstFrameMs(const std::string& message) {
    if (message.find("ashowinfo_") == std::string::npos || message.find(" n:0 ") == std::string::npos)
        return std::nullopt;
    const auto position = message.find("pts_time:");
    if (position == std::string::npos) return std::nullopt;
    const char* begin = message.c_str() + position + 9;
    char* end = nullptr;
    const double seconds = std::strtod(begin, &end);
    if (end == begin || (*end != '\0' && !std::isspace(static_cast<unsigned char>(*end))) || !std::isfinite(seconds) ||
        seconds > std::numeric_limits<int>::max() / 1000.0 || seconds < -1.0)
        return std::nullopt;
    return static_cast<int>(std::lround(std::max(0.0, seconds) * 1000.0));
}

inline int subtitleProbeActualStartMs(int requestedMs, int firstFrameMs) {
    // Frames preceding a precise seek are decoded but trimmed from PCM output.
    return std::max({0, requestedMs, firstFrameMs});
}

// Independent client clock fallback: FFmpeg log delivery can be suppressed by
// another libmpv instance in the same process. Use several stable observations
// of audio progress minus emitted PCM, never the requested seek position alone.
class SubtitleProbeClock {
public:
    void observe(double beforeSeconds, double afterSeconds, std::uint64_t pcmBytes) {
        if (!std::isfinite(beforeSeconds) || !std::isfinite(afterSeconds) || afterSeconds < beforeSeconds ||
            afterSeconds - beforeSeconds > 0.1 || pcmBytes < 32000 || pcmBytes == lastBytes_)
            return;
        const double start = ((beforeSeconds + afterSeconds) * 500.0) - static_cast<double>(pcmBytes) / 32.0;
        if (start < -500.0 || start > std::numeric_limits<int>::max()) return;
        starts_.push_back(static_cast<int>(std::lround(std::max(0.0, start))));
        lastBytes_ = pcmBytes;
    }

    [[nodiscard]] bool inconsistent() const {
        if (starts_.size() < 2) return false;
        const auto bounds = std::minmax_element(starts_.begin(), starts_.end());
        return *bounds.second - *bounds.first > 1000;
    }

    [[nodiscard]] std::optional<int> startMs() const {
        if (starts_.size() < 3) return std::nullopt;
        auto sorted = starts_;
        std::sort(sorted.begin(), sorted.end());
        if (sorted.back() - sorted.front() > 1000) return std::nullopt;
        return sorted[sorted.size() / 2];
    }

private:
    std::vector<int> starts_;
    std::uint64_t lastBytes_ = 0;
};

// Decode a bounded audio-only sample using an independent libmpv instance.
// This is intended for a background worker, never the playback/render thread.
SubtitleAudioSyncResult synchronizeSubtitleAudio(const SubtitleAudioSyncRequest& request);
