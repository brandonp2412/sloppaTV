#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

// A region is supported only between independently matched dialogue onsets.
// Missing scenes and unobserved gaps deliberately have no inferred mapping.
struct SubtitleTimelineRegion {
    int firstSubtitleMs = 0;
    int lastSubtitleMs = 0;
    int offsetMs = 0;
    double scale = 1.0;
    int anchors = 0;

    [[nodiscard]] bool valid() const {
        return firstSubtitleMs >= 0 && lastSubtitleMs > firstSubtitleMs && anchors >= 4 && std::isfinite(scale) &&
               scale >= 0.9 && scale <= 1.1 && std::abs(static_cast<int64_t>(offsetMs)) <= 120000;
    }

    [[nodiscard]] double spoken(int subtitleMs) const { return subtitleMs * scale + offsetMs; }
};

inline bool subtitleTimelineValid(const std::vector<SubtitleTimelineRegion>& regions) {
    if (regions.empty()) return false;
    for (size_t i = 0; i < regions.size(); ++i) {
        if (!regions[i].valid() || regions[i].spoken(regions[i].firstSubtitleMs) < 0 ||
            regions[i].spoken(regions[i].lastSubtitleMs) > std::numeric_limits<int>::max())
            return false;
        if (i &&
            (regions[i - 1].lastSubtitleMs >= regions[i].firstSubtitleMs ||
             regions[i - 1].spoken(regions[i - 1].lastSubtitleMs) >= regions[i].spoken(regions[i].firstSubtitleMs)))
            return false;
    }
    return true;
}

inline std::optional<int> subtitleTimelinePosition(const std::vector<SubtitleTimelineRegion>& regions, int spokenMs) {
    for (const auto& region : regions) {
        if (spokenMs < region.spoken(region.firstSubtitleMs) || spokenMs > region.spoken(region.lastSubtitleMs))
            continue;
        const double position = (spokenMs - static_cast<double>(region.offsetMs)) / region.scale;
        if (position >= 0 && position <= std::numeric_limits<int>::max())
            return static_cast<int>(std::llround(position));
    }
    return std::nullopt;
}
