#pragma once
#include "subtitle_timeline.hpp"
#include "subtitle_policy.hpp"
#include <functional>
#include <string>
#include <vector>

struct SubtitleSceneProposal {
    std::vector<SubtitleTimelineRegion> regions;
    double sigma = 0;
};

// LAPSE proposes boundaries from the full speech profile. A proposal still
// needs independent recognized dialogue before it can affect playback.
SubtitleSceneProposal proposeSubtitleLapse(const std::vector<float>& audio, const std::vector<SubtitleCue>& cues,
                                           int logicalStartMs, const std::string& model,
                                           const std::function<bool()>& cancelled);

SubtitleSceneProposal proposeSubtitleLapseAlignment(const std::vector<SubtitleCue>& cues,
                                                    const std::vector<std::pair<int, int>>& speech,
                                                    const std::vector<float>& weights,
                                                    const std::function<bool()>& cancelled);
