#pragma once

#include "subtitle_dialogue_sync.hpp"
#include "subtitle_timeline.hpp"

// Choose independent, dialogue-rich samples without prompting the recognizer
// with subtitle text. Quiet scenes should not consume repeated inference.
inline std::vector<int> subtitleSceneVerificationStarts(const SubtitleTimelineRegion& region,
                                                        const std::vector<SubtitleCue>& cues) {
    const double first = region.spoken(region.firstSubtitleMs);
    const double last = region.spoken(region.lastSubtitleMs);
    if (!region.valid() || first < 0 || last > std::numeric_limits<int>::max() || last - first < 20000) return {};
    const int begin = static_cast<int>(std::max(0.0, first - 1000));
    const int end = static_cast<int>(std::max(static_cast<double>(begin), last - 60000));
    std::vector<std::pair<int, int>> scored;
    for (int64_t position = begin; position <= end; position += 15000) {
        int count = 0;
        for (const auto& cue : cues) {
            const double spoken = region.spoken(cue.startMs);
            if (cue.startMs >= region.firstSubtitleMs && cue.startMs <= region.lastSubtitleMs &&
                spoken >= static_cast<double>(position) && spoken < static_cast<double>(position + 60000) &&
                subtitleDialogueWords(cue.text).size() >= 4)
                ++count;
        }
        if (count >= 4) scored.emplace_back(-count, static_cast<int>(position));
    }
    std::sort(scored.begin(), scored.end());
    std::vector<int> starts;
    for (const auto& [score, start] : scored) starts.push_back(start);
    return starts;
}

// Translate verified source evidence into a new live clock only after fresh,
// independently recognized phrases validate it in that session.
inline std::vector<SubtitleTimelineRegion> revalidateSubtitleScenes(const std::vector<SubtitleTimelineRegion>& source,
                                                                    const SubtitleDialogueWindow& fresh,
                                                                    int clockShiftMs) {
    if (!subtitleTimelineValid(source) || fresh.startMs < 0 || fresh.durationMs < 20000) return {};
    for (const auto& a : fresh.anchors)
        for (const auto& b : fresh.anchors)
            if (a.subtitleMs == b.subtitleMs && std::abs(a.spokenMs - b.spokenMs) > 500) return {};
    auto shifted = source;
    for (auto& region : shifted) {
        const int64_t offset = static_cast<int64_t>(region.offsetMs) + clockShiftMs;
        if (std::abs(offset) > 120000) return {};
        region.offsetMs = static_cast<int>(offset);
    }
    if (!subtitleTimelineValid(shifted)) return {};
    size_t count = 0, inliers = 0;
    int first = std::numeric_limits<int>::max(), last = 0;
    std::set<int> cues;
    for (const auto& anchor : fresh.anchors) {
        if (anchor.words < 4 || !std::isfinite(anchor.similarity) || anchor.similarity < 0.8 || anchor.similarity > 1 ||
            !std::isfinite(anchor.confidence) || anchor.confidence < 0.65 || anchor.confidence > 1 ||
            anchor.subtitleMs < 0 || anchor.spokenMs < fresh.startMs ||
            static_cast<int64_t>(anchor.spokenMs) > static_cast<int64_t>(fresh.startMs) + fresh.durationMs ||
            !cues.insert(anchor.subtitleMs).second)
            continue;
        ++count;
        for (const auto& region : shifted) {
            if (anchor.subtitleMs < region.firstSubtitleMs || anchor.subtitleMs > region.lastSubtitleMs ||
                std::abs(anchor.spokenMs - region.spoken(anchor.subtitleMs)) > 500)
                continue;
            ++inliers;
            first = std::min(first, anchor.spokenMs);
            last = std::max(last, anchor.spokenMs);
            break;
        }
    }
    if (inliers < 4 || inliers * 5 < count * 4 || last - first < 20000) return {};
    return shifted;
}
