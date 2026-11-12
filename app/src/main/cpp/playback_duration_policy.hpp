#pragma once

#include <algorithm>
#include <cstdint>

// Item metadata is sometimes an approximate TV-episode runtime. For a full
// DirectPlay file, prefer libmpv's probed media duration when the difference is
// material. DirectStream/Transcode durations may describe only a segment.
constexpr int preferredPlaybackDurationMs(int metadataMs, int playerMs, bool directPlay) {
    if (metadataMs <= 0) return std::max(0, playerMs);
    if (!directPlay || playerMs <= 0) return metadataMs;
    const std::int64_t delta = static_cast<std::int64_t>(playerMs) - metadataMs;
    return delta >= 2000 || delta <= -2000 ? playerMs : metadataMs;
}
