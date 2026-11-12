#pragma once

#include "subtitle_auto_sync.hpp"

#include <cstdint>
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

// Decode a bounded audio-only sample using an independent libmpv instance.
// This is intended for a background worker, never the playback/render thread.
SubtitleAudioSyncResult synchronizeSubtitleAudio(const SubtitleAudioSyncRequest& request);
