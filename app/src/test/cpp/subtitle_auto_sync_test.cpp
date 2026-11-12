#include "subtitle_audio_probe.hpp"

#include <cassert>
#include <cmath>
#include <vector>

namespace {
std::vector<SubtitleCue> makeCues(int baseMs) {
    std::vector<SubtitleCue> cues;
    for (int second : {3, 9, 14, 22, 29, 35, 43, 49, 56, 63, 71, 79}) {
        cues.push_back({baseMs + second * 1000, baseMs + second * 1000 + 1900, "spoken line"});
    }
    return cues;
}

std::vector<uint8_t> makeSpeech(const std::vector<SubtitleCue>& cues, int baseMs, int offsetMs, int mode) {
    std::vector<uint8_t> speech(9000, 0);
    for (size_t i = 0; i < speech.size(); ++i) {
        const int position = baseMs + static_cast<int>(i) * 10 - offsetMs;
        const bool active = std::any_of(cues.begin(), cues.end(), [position](const SubtitleCue& cue) {
            return cue.startMs <= position && position < cue.endMs;
        });
        if (active && (mode == 2 || i % 4 != 0)) speech[i] = 1;
        if (!active && i % 103 == 0) speech[i] = 1;
    }
    return speech;
}

void testPositiveAndNegativeOffsets() {
    constexpr int baseMs = 120000;
    const auto cues = makeCues(baseMs);
    for (int offset : {1200, -3600, 0}) {
        const auto moderate = makeSpeech(cues, baseMs, offset, 2);
        const auto aggressive = makeSpeech(cues, baseMs, offset, 3);
        const auto score2 = findSubtitleSyncPeak(moderate, cues, baseMs);
        const auto score3 = findSubtitleSyncPeak(aggressive, cues, baseMs);
        const auto decision = decideSubtitleSync(score2, score3, 90000, static_cast<int>(cues.size()));
        assert(decision.confident);
        assert(std::abs(decision.offsetMs - offset) <= 100);
    }
}

void testLargeBroadcastAndRecapOffsets() {
    // Different episode edits can shift dialogue by more than 30 seconds.
    // Keep a wide speech sample so both early and late cues are observable.
    constexpr int sampleStartMs = 120000;
    const auto cues = makeCues(sampleStartMs + 125000);
    for (const int offset : {-110000, 110000}) {
        auto speech = std::vector<uint8_t>(47000, 0);
        for (size_t i = 0; i < speech.size(); ++i) {
            const int mediaMs = sampleStartMs + static_cast<int>(i) * 10;
            const int cueMs = mediaMs - offset;
            speech[i] = std::any_of(cues.begin(), cues.end(),
                                    [&](const SubtitleCue& cue) { return cue.startMs <= cueMs && cueMs < cue.endMs; })
                            ? 1
                            : 0;
        }
        const auto peak = findSubtitleSyncPeak(speech, cues, sampleStartMs);
        assert(std::abs(peak.offsetMs - offset) <= 100);
    }
}

void testDirectStreamResumeTimeline() {
    constexpr int timelineBaseMs = 18 * 60 * 1000;
    constexpr int playerSampleMs = 14000;
    const int logicalSampleMs = subtitleSyncLogicalSampleStart(playerSampleMs, timelineBaseMs);
    assert(logicalSampleMs == 1094000);
    const auto cues = makeCues(logicalSampleMs);
    const auto moderate = makeSpeech(cues, logicalSampleMs, 1200, 2);
    const auto aggressive = makeSpeech(cues, logicalSampleMs, 1200, 3);
    const auto decision = decideSubtitleSync(findSubtitleSyncPeak(moderate, cues, logicalSampleMs),
                                             findSubtitleSyncPeak(aggressive, cues, logicalSampleMs), 90000,
                                             static_cast<int>(cues.size()));
    assert(decision.confident);
    assert(std::abs(decision.offsetMs - 1200) <= 100);
}

void testUncertainMatchesAreNotApplied() {
    SubtitleSyncPeak moderate{1000, 5900, 5850, 7000, 6000};
    SubtitleSyncPeak aggressive{-2000, 3700, 3200, 4000, 6000};
    assert(!decideSubtitleSync(moderate, aggressive, 90000, 30).confident);
    aggressive.offsetMs = 1100;
    aggressive.alternativeSpeechFrames = 3620;
    assert(!decideSubtitleSync(moderate, aggressive, 90000, 30).confident);
    moderate.alternativeSpeechFrames = 5700;
    assert(!decideSubtitleSync(moderate, aggressive, 45000, 30).confident);
    assert(!decideSubtitleSync(moderate, aggressive, 90000, 3).confident);
}

void testLongSampleConfidenceAndMismatchedTracks() {
    // Real-world dense-dialogue score shape: both modes agree on a small offset.
    const SubtitleSyncPeak moderate{200, 0, 0, 23777, 20894, 0.111, 0.092};
    const SubtitleSyncPeak aggressive{200, 0, 0, 20288, 19000, 0.130, 0.113};
    const auto trusted = decideSubtitleSync(moderate, aggressive, 297633, 89);
    assert(trusted.confident && trusted.offsetMs == 200);
    assert(!decideSubtitleSync(moderate, aggressive, 95000, 15).confident);
    // From a 23-minute episode: 4.5 minutes is ambiguous, but after 7.9
    // minutes both detectors have isolated, agreeing peaks despite weak phi.
    const SubtitleSyncPeak opening2{26800, 0, 0, 22500, 20000, 0.084362, 0.081430};
    const SubtitleSyncPeak opening3{200, 0, 0, 19900, 19500, 0.056664, 0.019683};
    assert(!decideSubtitleSync(opening2, opening3, 272063, 68).confident);
    const SubtitleSyncPeak long2{100, 0, 0, 22500, 20000, 0.077398, 0.057020};
    const SubtitleSyncPeak long3{100, 0, 0, 19900, 19500, 0.110385, 0.022242};
    const auto longMatch = decideSubtitleSync(long2, long3, 472554, 130);
    assert(longMatch.confident && longMatch.offsetMs == 100);
    SubtitleSyncPeak ambiguousLong = long2;
    ambiguousLong.alternativeCorrelation = 0.061;
    assert(!decideSubtitleSync(ambiguousLong, long3, 472554, 130).confident);
    // In a mismatched external track, the two VAD modes disagree by many seconds.
    const SubtitleSyncPeak wrongModerate{-12800, 0, 0, 24840, 18785, 0.115, 0.102};
    const SubtitleSyncPeak wrongAggressive{-25400, 0, 0, 22045, 18000, 0.097, 0.091};
    assert(!decideSubtitleSync(wrongModerate, wrongAggressive, 300000, 76).confident);
}

void testProviderCreditsNeverCountAsSpeech() {
    const std::vector<SubtitleCue> credits{
        {6000, 12000, "Watch Online Movies and Series for FREE\nwww.osdb.link/lm"},
        {20000, 24000, "Original Air Date on January 18, 2012"},
        {25000, 29000, "== sync, corrected by elderman =="},
    };
    assert(subtitleSyncCueIsMetadata(credits[0]));
    assert(subtitleSyncCueIsMetadata(credits[1]));
    assert(subtitleSyncCueIsMetadata(credits[2]));
    assert(!subtitleSyncCueIsMetadata({0, 2000, "spoken dialogue"}));
    const auto score = findSubtitleSyncPeak(makeSpeech(makeCues(0), 0, 1200, 2), credits, 0);
    assert(score.correlation == -1.0);
}

void testWatermarkCannotDominateAlignment() {
    const auto cues = makeCues(0);
    auto withAd = cues;
    withAd.push_back({5000, 85000, "subtitle provider watermark"});
    const auto speech = makeSpeech(cues, 0, 1200, 2);
    const auto result = findSubtitleSyncPeak(speech, withAd, 0);
    assert(std::abs(result.offsetMs - 1200) <= 100);
}
} // namespace

int main() {
    testPositiveAndNegativeOffsets();
    testLargeBroadcastAndRecapOffsets();
    testDirectStreamResumeTimeline();
    testUncertainMatchesAreNotApplied();
    testProviderCreditsNeverCountAsSpeech();
    testWatermarkCannotDominateAlignment();
    testLongSampleConfidenceAndMismatchedTracks();
}
