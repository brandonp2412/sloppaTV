#pragma once

#include "subtitle_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
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
    // Media-clock milliseconds per subtitle-clock millisecond (PAL speedup ~= 0.96).
    double timeScale = 1.0;
};

inline int subtitleSyncLogicalSampleStart(int playerSampleMs, int timelineBaseMs) {
    return std::max(0, playerSampleMs) + std::max(0, timelineBaseMs);
}

// Subtitle credits, ads and delivery URLs aren't spoken dialogue. Exclude them
// from alignment while leaving their rendering untouched.
inline bool subtitleSyncCueIsMetadata(const SubtitleCue& cue) {
    std::string text = cue.text;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text.find("www.") != std::string::npos || text.find("http://") != std::string::npos ||
           text.find("https://") != std::string::npos || text.find("original air date") != std::string::npos ||
           text.find("sync, corrected by") != std::string::npos || text.find("subtitles by") != std::string::npos;
}

inline SubtitleSyncPeak findSubtitleSyncPeak(const std::vector<uint8_t>& speech, const std::vector<SubtitleCue>& cues,
                                             int sampleStartMs, int maxOffsetMs = 120000, double timeScale = 1.0) {
    SubtitleSyncPeak result;
    if (speech.empty() || cues.empty() || maxOffsetMs < 0 || !std::isfinite(timeScale) || timeScale < 0.9 ||
        timeScale > 1.1)
        return result;

    constexpr int kFrameMs = 10;
    constexpr int kStepMs = 100;
    constexpr int kAlternativeSeparationMs = 1500;
    const int padding = (maxOffsetMs + kFrameMs - 1) / kFrameMs + 1;
    const int width = static_cast<int>(speech.size()) + 2 * padding;
    std::vector<uint8_t> cueActive(static_cast<size_t>(width), 0);
    const int64_t maskStartMs = static_cast<int64_t>(sampleStartMs) - static_cast<int64_t>(padding) * kFrameMs;
    for (const SubtitleCue& cue : cues) {
        if (cue.endMs <= cue.startMs || cue.endMs - cue.startMs > 12000 || subtitleSyncCueIsMetadata(cue)) continue;
        const int64_t scaledStartMs = std::llround(static_cast<double>(cue.startMs) * timeScale);
        const int64_t scaledEndMs = std::llround(static_cast<double>(cue.endMs) * timeScale);
        const int begin = static_cast<int>(std::clamp<int64_t>((scaledStartMs - maskStartMs + 5) / kFrameMs, 0, width));
        const int end = static_cast<int>(std::clamp<int64_t>((scaledEndMs - maskStartMs + 5) / kFrameMs, 0, width));
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

// Speech can be uniformly accelerated when a 23.976/24 fps release is converted
// to 25 fps (or the reverse). A fixed subtitle offset cannot fix that drift.
// Consider only the known frame-rate ratios, and require convincing speech
// correlation before altering subtitle speed. Never infer scale from file length.
struct SubtitleTimeWarpEvaluation {
    SubtitleSyncDecision decision;
    SubtitleSyncPeak moderate;
    SubtitleSyncPeak aggressive;
    int matchingCues = 0;
};

// A wrong frame-rate (or a different edit) can occasionally produce a sharp
// *local* offset peak despite continuing to drift throughout the episode.
// Requiring independently matching offsets in two halves prevents that false fix.
inline bool subtitleTimeWarpStable(const std::vector<uint8_t>& moderateSpeech,
                                   const std::vector<uint8_t>& aggressiveSpeech, const std::vector<SubtitleCue>& cues,
                                   int sampleStartMs, double timeScale) {
    const size_t n = moderateSpeech.size();
    if (n < 18000) return timeScale == 1.0; // No reliable speed correction from less than three minutes.
    const auto half = static_cast<std::vector<uint8_t>::difference_type>(n / 2);
    const auto earlyModerate =
        findSubtitleSyncPeak(std::vector<uint8_t>(moderateSpeech.begin(), moderateSpeech.begin() + half), cues,
                             sampleStartMs, 120000, timeScale);
    const auto lateModerate =
        findSubtitleSyncPeak(std::vector<uint8_t>(moderateSpeech.begin() + half, moderateSpeech.end()), cues,
                             sampleStartMs + static_cast<int>(half) * 10, 120000, timeScale);
    const auto earlyAggressive =
        findSubtitleSyncPeak(std::vector<uint8_t>(aggressiveSpeech.begin(), aggressiveSpeech.begin() + half), cues,
                             sampleStartMs, 120000, timeScale);
    const auto lateAggressive =
        findSubtitleSyncPeak(std::vector<uint8_t>(aggressiveSpeech.begin() + half, aggressiveSpeech.end()), cues,
                             sampleStartMs + static_cast<int>(half) * 10, 120000, timeScale);
    const bool enoughEvidence = earlyModerate.correlation >= 0.10 && lateModerate.correlation >= 0.10 &&
                                earlyAggressive.correlation >= 0.10 && lateAggressive.correlation >= 0.10 &&
                                earlyModerate.speechFrames >= 250 && lateModerate.speechFrames >= 250 &&
                                earlyModerate.subtitleFrames >= 350 && lateModerate.subtitleFrames >= 350;
    if (!enoughEvidence) return timeScale == 1.0;
    return std::abs(earlyModerate.offsetMs - lateModerate.offsetMs) <= 1800 &&
           std::abs(earlyAggressive.offsetMs - lateAggressive.offsetMs) <= 1800 &&
           std::abs(earlyModerate.offsetMs - earlyAggressive.offsetMs) <= 1000 &&
           std::abs(lateModerate.offsetMs - lateAggressive.offsetMs) <= 1000;
}

inline SubtitleTimeWarpEvaluation evaluateSubtitleTimeWarp(const std::vector<uint8_t>& moderateSpeech,
                                                           const std::vector<uint8_t>& aggressiveSpeech,
                                                           const std::vector<SubtitleCue>& cues, int sampleStartMs) {
    SubtitleTimeWarpEvaluation baseline;
    if (moderateSpeech.empty() || aggressiveSpeech.size() != moderateSpeech.size() || cues.empty()) return baseline;
    const int durationMs = static_cast<int>(moderateSpeech.size()) * 10;
    SubtitleTimeWarpEvaluation best;
    double bestQuality = -1.0;
    // PAL conversion ratios: 24->25 and 24000/1001->25, plus their inverses.
    for (double timeScale : {1.0, 24.0 / 25.0, 24000.0 / 25025.0, 25.0 / 24.0, 25025.0 / 24000.0}) {
        SubtitleTimeWarpEvaluation candidate;
        candidate.matchingCues = static_cast<int>(std::count_if(cues.begin(), cues.end(), [&](const SubtitleCue& cue) {
            if (cue.endMs <= cue.startMs || cue.endMs - cue.startMs > 12000 || subtitleSyncCueIsMetadata(cue))
                return false;
            const double scaledStartMs = static_cast<double>(cue.startMs) * timeScale;
            const double scaledEndMs = static_cast<double>(cue.endMs) * timeScale;
            return scaledStartMs < sampleStartMs + durationMs && scaledEndMs > sampleStartMs;
        }));
        candidate.moderate = findSubtitleSyncPeak(moderateSpeech, cues, sampleStartMs, 120000, timeScale);
        candidate.aggressive = findSubtitleSyncPeak(aggressiveSpeech, cues, sampleStartMs, 120000, timeScale);
        candidate.decision =
            decideSubtitleSync(candidate.moderate, candidate.aggressive, durationMs, candidate.matchingCues);
        if (candidate.decision.confident &&
            !subtitleTimeWarpStable(moderateSpeech, aggressiveSpeech, cues, sampleStartMs, timeScale))
            candidate.decision.confident = false;
        if (timeScale == 1.0) {
            baseline = candidate;
            // Never sacrifice an already credible match to try speculative frame-rate changes.
            if (baseline.decision.confident) return baseline;
        }
        if (!candidate.decision.confident) continue;
        const double minCorrelation = std::min(candidate.moderate.correlation, candidate.aggressive.correlation);
        // A mistaken frame-rate correction can create a severe late-episode error.
        // Require stronger evidence for a time warp than for an ordinary offset.
        if (timeScale != 1.0 && (minCorrelation < 0.13 || candidate.decision.agreement < 0.03)) continue;
        const double quality = minCorrelation + 0.5 * candidate.decision.agreement;
        if (quality > bestQuality) {
            bestQuality = quality;
            candidate.decision.timeScale = timeScale;
            best = candidate;
        }
    }
    if (!best.decision.confident) return baseline;
    return best;
}
