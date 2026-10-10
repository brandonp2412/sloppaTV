#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// Resume endpoints sometimes omit brief playback sessions even when the item
// reports a positive saved position. Merge recently played, unfinished items
// even when the primary Resume feed contains some other resumable episode.
inline void mergeHomeResumeCandidates(std::vector<JellyfinItem>& resume, std::vector<JellyfinItem> recent,
                                      size_t limit = 30) {
    std::unordered_set<std::string> seen;
    seen.reserve(resume.size() + recent.size());
    std::vector<JellyfinItem> merged;
    merged.reserve(std::min(limit, resume.size() + recent.size()));
    auto add = [&](JellyfinItem& item, bool serverReportedResume) {
        if (merged.size() >= limit || item.id.empty() || (item.type != "Episode" && item.type != "Movie") ||
            (!serverReportedResume && (item.positionTicks <= 0 ||
                                       (item.runtimeTicks > 0 && item.positionTicks >= item.runtimeTicks))) ||
            !seen.insert(item.id).second) return;
        merged.push_back(std::move(item));
    };
    // Trust entries from Resume even if that endpoint omitted UserData, then
    // append genuine unfinished positions from the recently played feed.
    for (auto& item : resume) add(item, true);
    for (auto& item : recent) add(item, false);
    resume = std::move(merged);
}
