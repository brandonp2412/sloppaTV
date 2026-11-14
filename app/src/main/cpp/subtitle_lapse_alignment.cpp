#include "subtitle_lapse.hpp"
#include "third_party/lapse/align.h"
#include "third_party/lapse/correlate.h"
#include "third_party/lapse/cuetext.h"
#include "third_party/lapse/srt_parser.h"
#include <chrono>
#include <cmath>
#include <mutex>

SubtitleSceneProposal proposeSubtitleLapseAlignment(const std::vector<SubtitleCue>& cues,
                                                    const std::vector<std::pair<int, int>>& speech,
                                                    const std::vector<float>& weights,
                                                    const std::function<bool()>& cancelled) {
    static std::mutex alignmentMutex;
    std::scoped_lock lock(alignmentMutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
    const auto stop = [&] { return (cancelled && cancelled()) || std::chrono::steady_clock::now() >= deadline; };
    if (cues.size() < 10 || cues.size() > 20000 || speech.size() < 10 || speech.size() > 100000 ||
        (!weights.empty() && weights.size() != speech.size()) || stop())
        return {};
    for (size_t i = 0; i < speech.size(); ++i) {
        if (speech[i].first < 0 || speech[i].second <= speech[i].first || speech[i].second > 128 * 60000 ||
            (i && speech[i].first < speech[i - 1].second) ||
            (!weights.empty() && (!std::isfinite(weights[i]) || weights[i] < 0 || weights[i] > 1)))
            return {};
    }

    struct Cleanup {
        ~Cleanup() { align_drop(); }
    } cleanup;

    std::vector<std::pair<int, int>> timestamps;
    for (const auto& cue : cues)
        if (!is_junk_cue(cue.text)) timestamps.emplace_back(cue.startMs, cue.endMs);
    auto [spans, mapping] = process_spans(timestamps, true, false);
    if (spans.size() < 10 || speech.size() < 10 || stop()) return {};
    align_setup(speech, weights);
    chapter_hints({});
    const auto peak = best_offset(spans, speech, weights);
    if (stop()) return {};
    const auto offsets =
        split_alignment(spans, speech, weights,
                        static_cast<float>(std::max(3.0, std::log(static_cast<double>(spans.size())))), peak.offset);
    if (offsets.size() != spans.size() || stop()) return {};
    SubtitleSceneProposal result;
    result.sigma = peak.sigma;
    for (size_t begin = 0; begin < spans.size();) {
        size_t end = begin + 1;
        while (end < spans.size() && offsets[end] == offsets[begin]) ++end;
        // Four is only a placeholder here. The caller must replace it with the
        // count of independently recognized dialogue before applying a region.
        result.regions.push_back({spans[begin].first, spans[end - 1].second - 1, offsets[begin], 1.0, 4});
        begin = end;
    }
    if (!subtitleTimelineValid(result.regions) || result.regions.size() > 128) return {};
    return result;
}
