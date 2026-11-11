#pragma once

#include "subtitle_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// An audio probe supplies one 10 ms speech/non-speech observation per frame.
// Positive offsets delay subtitles; negative offsets advance them.
struct SubtitleSyncPeak {
    int offsetMs = 0;
    int matchedSpeechFrames = 0;
    int alternativeSpeechFrames = 0;
    int speechFrames = 0;
    int subtitleFrames = 0;
    double correlation = -1.0;
    double alternativeCorrelation = -1.0;
};

struct SubtitleSyncDecision {
    bool confident = false;
    int offsetMs = 0;
    double agreement = 0.0;
};

inline int subtitleSyncLogicalSampleStart(int playerSampleMs, int timelineBaseMs) {
    return std::max(0, playerSampleMs) + std::max(0, timelineBaseMs);
}

inline SubtitleSyncPeak findSubtitleSyncPeak(const std::vector<uint8_t>& speech, const std::vector<SubtitleCue>& cues,
                                             int sampleStartMs, int maxOffsetMs = 30000) {
    SubtitleSyncPeak result;
    if (speech.empty() || cues.empty() || maxOffsetMs < 0) return result;

    constexpr int kFrameMs = 10;
    constexpr int kStepMs = 100;
    constexpr int kAlternativeSeparationMs = 1500;
    const int padding = (maxOffsetMs + kFrameMs - 1) / kFrameMs + 1;
    const int width = static_cast<int>(speech.size()) + 2 * padding;
    std::vector<uint8_t> cueActive(static_cast<size_t>(width), 0);
    const int64_t maskStartMs = static_cast<int64_t>(sampleStartMs) - static_cast<int64_t>(padding) * kFrameMs;
    for (const SubtitleCue& cue : cues) {
        if (cue.endMs <= cue.startMs || cue.endMs - cue.startMs > 12000) continue;
        const int begin = static_cast<int>(
            std::clamp<int64_t>((static_cast<int64_t>(cue.startMs) - maskStartMs + 5) / kFrameMs, 0, width));
        const int end = static_cast<int>(
            std::clamp<int64_t>((static_cast<int64_t>(cue.endMs) - maskStartMs + 5) / kFrameMs, 0, width));
        for (int i = begin; i < end; ++i) cueActive[static_cast<size_t>(i)] = 1;
    }

    const int n = static_cast<int>(speech.size());
    for (uint8_t value : speech)
        if (value) ++result.speechFrames;
    const int speechFrames = result.speechFrames;
    if (speechFrames <= 0 || speechFrames >= n) return result;

    struct Candidate {
        int offsetMs;
        int matched;
        int subtitleFrames;
        double correlation;
    };

    std::vector<Candidate> candidates;
    double bestCorrelation = -2.0;
    for (int offset = -maxOffsetMs; offset <= maxOffsetMs; offset += kStepMs) {
        const int shift = offset / kFrameMs;
        int matched = 0;
        int subtitleFrames = 0;
        for (int i = 0; i < n; ++i) {
            const bool cue = cueActive[static_cast<size_t>(i + padding - shift)] != 0;
            if (cue) ++subtitleFrames;
            if (cue && speech[static_cast<size_t>(i)]) ++matched;
        }
        if (subtitleFrames == 0 || subtitleFrames == n) continue;
        const double numerator = static_cast<double>(matched) * n - static_cast<double>(speechFrames) * subtitleFrames;
        const double denominator =
            std::sqrt(static_cast<double>(speechFrames) * (n - speechFrames) * subtitleFrames * (n - subtitleFrames));
        const double correlation = numerator / denominator;
        candidates.push_back({offset, matched, subtitleFrames, correlation});
        if (correlation > bestCorrelation) {
            bestCorrelation = correlation;
            result.offsetMs = offset;
            result.matchedSpeechFrames = matched;
            result.subtitleFrames = subtitleFrames;
            result.correlation = correlation;
        }
    }
    result.alternativeCorrelation = -1.0;
    for (const Candidate& candidate : candidates) {
        if (std::abs(candidate.offsetMs - result.offsetMs) >= kAlternativeSeparationMs &&
            candidate.correlation > result.alternativeCorrelation) {
            result.alternativeCorrelation = candidate.correlation;
            result.alternativeSpeechFrames = candidate.matched;
        }
    }
    return result;
}

inline SubtitleSyncDecision decideSubtitleSync(const SubtitleSyncPeak& moderate, const SubtitleSyncPeak& aggressive,
                                               int audioDurationMs, int cueCount) {
    SubtitleSyncDecision decision;
    if (audioDurationMs < 60000 || cueCount < 12) return decision;
    if (moderate.speechFrames < 500 || aggressive.speechFrames < 350 || moderate.subtitleFrames < 1000 ||
        aggressive.subtitleFrames < 1000)
        return decision;

    // Phi correlation removes the bias towards offsets with more subtitle coverage,
    // which is especially important when a voice detector marks most sitcom audio as speech.
    const double moderateMargin = moderate.correlation - moderate.alternativeCorrelation;
    const double aggressiveMargin = aggressive.correlation - aggressive.alternativeCorrelation;
    const bool longSample = audioDurationMs >= 180000 && cueCount >= 40;
    const double requiredMargin = longSample ? 0.015 : 0.025;
    const int offsetDisagreement = std::abs(moderate.offsetMs - aggressive.offsetMs);
    const bool normalMatch = moderate.correlation >= 0.10 && aggressive.correlation >= 0.10 &&
                             moderateMargin >= requiredMargin && aggressiveMargin >= requiredMargin &&
                             offsetDisagreement <= 700;
    // With several minutes of sparse dialogue, VAD intensity varies by voice and music.
    // Require tighter agreement and distinctly isolated peaks before accepting lower phi scores.
    const bool strongLongMatch = audioDurationMs >= 360000 && cueCount >= 80 && moderate.correlation >= 0.075 &&
                                 aggressive.correlation >= 0.105 && moderateMargin >= 0.020 &&
                                 aggressiveMargin >= 0.040 && offsetDisagreement <= 250;
    if (!normalMatch && !strongLongMatch) return decision;

    decision.confident = true;
    decision.offsetMs = (moderate.offsetMs + aggressive.offsetMs) / 2;
    decision.agreement = std::min(moderateMargin, aggressiveMargin);
    return decision;
}
