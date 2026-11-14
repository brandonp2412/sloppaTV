#include "subtitle_lapse.hpp"
#include "third_party/lapse/log.h"
#include <cassert>
#include <cmath>
#include <random>

int main() {
    quiet = true;
    std::mt19937 random(17913);
    std::vector<SubtitleCue> cues;
    std::vector<std::pair<int, int>> speech;
    int position = 10000;
    for (int i = 0; i < 160; ++i) {
        // A deleted scene consumes a real gap; speech itself cannot go backwards.
        if (i > 0 && i % 40 == 0) position += 20000;
        const int duration = 800 + static_cast<int>(random() % 3200);
        const int offset = i < 40 ? 3000 : i < 80 ? 7000 : i < 120 ? -2000 : 11000;
        cues.push_back({position, position + duration, "independently timed dialogue"});
        speech.emplace_back(position + offset, position + duration + offset);
        position += duration + 1000 + static_cast<int>(random() % 6000);
    }
    const auto proposal = proposeSubtitleLapseAlignment(cues, speech, {}, {});
    assert(proposal.regions.size() == 4);
    for (int index : {20, 60, 100, 140}) {
        const auto& cue = cues[static_cast<size_t>(index)];
        const int spoken = speech[static_cast<size_t>(index)].first;
        const auto mapped = subtitleTimelinePosition(proposal.regions, spoken);
        assert(mapped && std::abs(*mapped - cue.startMs) < 100);
    }
    assert(proposeSubtitleLapseAlignment(cues, {}, {}, {}).regions.empty());
    assert(proposeSubtitleLapseAlignment(cues, speech, {}, [] { return true; }).regions.empty());
    auto malformed = speech;
    malformed[4].first = -1;
    assert(proposeSubtitleLapseAlignment(cues, malformed, {}, {}).regions.empty());
    std::vector<float> wrongWeights(1, 1.0F);
    assert(proposeSubtitleLapseAlignment(cues, speech, wrongWeights, {}).regions.empty());
}
