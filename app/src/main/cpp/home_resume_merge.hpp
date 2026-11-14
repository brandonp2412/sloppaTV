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
    // The Resume index may contain abandoned S4E1/S4E9 positions even after
    // the viewer has watched S4E18. The fresh DatePlayed fallback also contains
    // watched episodes, so use it to exclude only provably superseded partials.
    // A newly restarted rewatch of an old episode remains visible if its
    // LastPlayedDate is later than the subsequent episode's activity.
    struct ProgressMarker {
        std::string seriesId;
        std::string lastPlayedDate;
        int season;
        int episode;
    };
    std::vector<ProgressMarker> laterPlayback;
    laterPlayback.reserve(recent.size());
    for (const auto& item : recent) {
        if (item.type == "Episode" && !item.seriesId.empty() && item.parentIndexNumber >= 0 &&
            item.indexNumber >= 0 && !item.lastPlayedDate.empty() && (item.played || item.positionTicks > 0)) {
            laterPlayback.push_back({item.seriesId, item.lastPlayedDate, item.parentIndexNumber, item.indexNumber});
        }
    }
    const auto supersededByLaterPlayback = [&](const JellyfinItem& item) {
        if (item.type != "Episode" || item.seriesId.empty() || item.parentIndexNumber < 0 ||
            item.indexNumber < 0 || item.lastPlayedDate.empty())
            return false;
        return std::any_of(laterPlayback.begin(), laterPlayback.end(), [&](const ProgressMarker& later) {
            if (later.seriesId != item.seriesId || later.lastPlayedDate <= item.lastPlayedDate) return false;
            return later.season > item.parentIndexNumber ||
                   (later.season == item.parentIndexNumber && later.episode > item.indexNumber);
        });
    };
    std::unordered_set<std::string> seen;
    seen.reserve(resume.size() + recent.size());
    std::vector<JellyfinItem> merged;
    merged.reserve(std::min(limit, resume.size() + recent.size()));
    auto add = [&](JellyfinItem& item, bool serverReportedResume) {
        if (merged.size() >= limit || item.id.empty() || (item.type != "Episode" && item.type != "Movie") ||
            (!serverReportedResume &&
             (item.positionTicks <= 0 || (item.runtimeTicks > 0 && item.positionTicks >= item.runtimeTicks))) ||
            supersededByLaterPlayback(item) || !seen.insert(item.id).second)
            return;
        merged.push_back(std::move(item));
    };
    // Trust entries from Resume even if that endpoint omitted UserData, then
    // append genuine unfinished positions from the recently played feed.
    for (auto& item : resume) add(item, true);
    for (auto& item : recent) add(item, false);
    resume = std::move(merged);
}
