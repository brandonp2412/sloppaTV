#include "subtitle_audio_probe.hpp"
#include "fixtures/subtitle_sync_evidence.hpp"

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

void testStrongVadEvidenceRequiresGlobalConfirmation() {
    // These peaks represent an actual difficult external-SRT sample. They can
    // trigger further verification but must never be applied by themselves.
    const SubtitleSyncPeak moderate{4600, 680, 440, 1400, 4500, 0.244411, 0.155308};
    const SubtitleSyncPeak aggressive{4600, 900, 650, 1800, 4500, 0.392739, 0.270550};
    assert(subtitleStrongVadGlobalCandidate(moderate, aggressive, 275370, 94));
    assert(!subtitleStrongVadGlobalCandidate(moderate, aggressive, 90000, 94));
    auto wrongOffset = aggressive;
    wrongOffset.offsetMs += 18000;
    assert(!subtitleStrongVadGlobalCandidate(moderate, wrongOffset, 275370, 94));
    auto weakMargin = moderate;
    weakMargin.alternativeCorrelation = 0.215;
    assert(!subtitleStrongVadGlobalCandidate(weakMargin, aggressive, 275370, 94));
    assert(!evaluateSubtitleSyncWindows({}, {}).confident);
}

void testProbeUsesDecodedHlsStart() {
    const auto firstFrame = subtitleProbeFirstFrameMs(
        "Parsed_ashowinfo_0: n:0 pts:2256000 pts_time:141.0 fmt:s16 rate:16000 nb_samples:1024");
    assert(firstFrame && *firstFrame == 141000);
    assert(subtitleProbeActualStartMs(30000, *firstFrame) == 141000);
    assert(subtitleSyncLogicalSampleStart(subtitleProbeActualStartMs(30000, *firstFrame), 547579) == 688579);
    assert(subtitleProbeActualStartMs(30000, 29952) == 30000);
    assert(!subtitleProbeFirstFrameMs("Parsed_ashowinfo_0: n:1 pts_time:466.0 fmt:s16"));
    assert(!subtitleProbeFirstFrameMs("Parsed_ashowinfo_0: n:0 pts_time:nan fmt:s16"));
    assert(!subtitleProbeFirstFrameMs("Parsed_ashowinfo_0: n:0 pts_time:1e100 fmt:s16"));
    assert(!subtitleProbeFirstFrameMs("Parsed_ashowinfo_0: n:0 pts_time:4junk fmt:s16"));
    assert(!subtitleProbeFirstFrameMs("unrelated decoder log pts_time:466.0"));
}

void testProbeClockFallback() {
    SubtitleProbeClock clock;
    assert(!clock.startMs());
    // A 111-second HLS seek jump, independently measured from output PCM.
    for (int duration : {1000, 2000, 3000}) {
        const double position = 141.0 + duration / 1000.0;
        clock.observe(position, position, static_cast<std::uint64_t>(duration) * 32);
    }
    assert(clock.startMs() && *clock.startMs() == 141000);
    // Timestamp discontinuities within the captured audio are unsafe.
    clock.observe(155.0, 155.0, 4000ULL * 32);
    assert(!clock.startMs());
    SubtitleProbeClock racing;
    for (int i = 0; i < 4; ++i) racing.observe(142.0, 144.0, 32000ULL * (i + 1));
    assert(!racing.startMs());
}

void testPcmSampleDurationNeverOverflowsArm32() {
    // Captured on the real TV: WAV length 9,123,502 PCM bytes at 16 kHz.
    // Previous size_t multiplication wrapped to 16,673 ms on 32-bit ARM.
    assert(subtitlePcmDurationMs(4561751, 16000) == 285109);
    assert(subtitlePcmDurationMs(8000000, 16000) == 500000);
    assert(subtitlePcmDurationMs(160, 16000) == 10);
    assert(subtitlePcmDurationMs(0, 16000) == 0);
    assert(subtitlePcmDurationMs(4561751, 0) == 0);
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
    assert(subtitleSyncCueIsMetadata({11000, 17074, "Advertise your product or brand here"}));
    assert(subtitleSyncCueIsMetadata({0, 2000, "contact OpenSubtitles.org today"}));
    assert(subtitleSyncCueIsMetadata({0, 2000, "Synced and corrected by a contributor"}));
    assert(!subtitleSyncCueIsMetadata({0, 2000, "spoken dialogue"}));
    assert(!subtitleSyncCueIsMetadata({0, 2000, "We should advertise your product tomorrow."}));
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

void testFixedOffsetVadWindowDisagreement() {
    std::vector<SubtitleCue> cues;
    for (int time = 1500, i = 0; time < 175000; ++i) {
        cues.push_back({time, time + 800 + (i * 173) % 1300, "dialogue"});
        time += 2500 + (i * 251) % 3700;
    }
    const auto audio = [&](int lateOffset) {
        std::vector<uint8_t> speech(18000, 0);
        for (size_t i = 0; i < speech.size(); ++i) {
            const int position = static_cast<int>(i) * 10 - (i < 9000 ? 3300 : lateOffset);
            speech[i] = std::any_of(cues.begin(), cues.end(), [&](const SubtitleCue& cue) {
                return cue.startMs <= position && position < cue.endMs;
            });
        }
        return speech;
    };
    const auto aggressive = audio(3300);
    assert(subtitleTimeWarpStable(audio(4500), aggressive, cues, 0, 1.0));
    assert(!subtitleTimeWarpStable(audio(5500), aggressive, cues, 0, 1.0));
}

void testPalFrameRateDriftCorrection() {
    // Captions from a 23.976 fps master must be accelerated for a 25 fps
    // playback source; a constant offset is wrong at the end of the episode.
    std::vector<SubtitleCue> cues;
    for (int t = 1500, i = 0; t < 43 * 60000; ++i) {
        cues.push_back({t, t + 800 + (i * 17) % 1300, "dialogue"});
        t += 2500 + (i * 251) % 3700;
    }
    constexpr int sampleStartMs = 15 * 60000;
    constexpr int mediaOffsetMs = 2400;
    const auto makeAudio = [&](double actualScale, int mode) {
        std::vector<uint8_t> audio(18000, 0);
        for (size_t i = 0; i < audio.size(); ++i) {
            const double cueTime = (sampleStartMs + static_cast<int>(i) * 10 - mediaOffsetMs) / actualScale;
            const auto it = std::upper_bound(cues.begin(), cues.end(), cueTime,
                                             [](double value, const SubtitleCue& cue) { return value < cue.startMs; });
            const bool spoken = it != cues.begin() && cueTime < (it - 1)->endMs;
            audio[i] = (spoken && (mode == 2 || i % 4 != 0)) || (!spoken && i % 167 == 0);
        }
        return audio;
    };
    for (double scale : {1.0, 24.0 / 25.0, 24000.0 / 25025.0, 25.0 / 24.0}) {
        const auto result = evaluateSubtitleTimeWarp(makeAudio(scale, 2), makeAudio(scale, 3), cues, sampleStartMs);
        assert(result.decision.confident);
        assert(std::abs(result.decision.timeScale - scale) <= 0.0011);
        assert(std::abs(result.decision.offsetMs - mediaOffsetMs) <= 500);
    }
    // Nonstandard drift must not be misdiagnosed as a trustworthy fixed offset.
    const auto unsupported = evaluateSubtitleTimeWarp(makeAudio(0.973, 2), makeAudio(0.973, 3), cues, sampleStartMs);
    assert(!unsupported.decision.confident);
    const auto unrelated = std::vector<uint8_t>(18000, 1);
    assert(!evaluateSubtitleTimeWarp(unrelated, unrelated, cues, sampleStartMs).decision.confident);
}

} // namespace

void testSeparatedWindowsRequireOneTimeline() {
    std::vector<SubtitleCue> cues;
    for (int time = 1500, i = 0; time < 43 * 60000; ++i) {
        cues.push_back({time, time + 800 + (i * 173) % 1300, "dialogue"});
        time += 2500 + (i * 251) % 3700;
    }
    cues.push_back({1000, 7000, "Advertise your product or brand here"});
    std::sort(cues.begin(), cues.end(), [](const auto& a, const auto& b) { return a.startMs < b.startMs; });
    const auto windowsFor = [&](double scale, bool differentEdit) {
        std::vector<SubtitleSyncWindow> windows;
        for (int start : {60000, 900000, 1800000}) {
            SubtitleSyncWindow window;
            window.startMs = start;
            const int offset = differentEdit && start == 900000 ? 5100 : 2100;
            for (int frame = 0; frame < 18000; ++frame) {
                const double time = (start + frame * 10 - offset) / scale;
                const auto cue = std::upper_bound(cues.begin(), cues.end(), time,
                                                  [](double value, const auto& line) { return value < line.startMs; });
                const bool active =
                    cue != cues.begin() && time < (cue - 1)->endMs && !subtitleSyncCueIsMetadata(*(cue - 1));
                window.moderate.push_back(active || frame % 167 == 0);
                window.aggressive.push_back((active && frame % 4 != 0) || (!active && frame % 251 == 0));
            }
            windows.push_back(std::move(window));
        }
        return windows;
    };
    for (double scale :
         {1.0, 1000.0 / 1001.0, 1001.0 / 1000.0, 24.0 / 25.0, 24000.0 / 25025.0, 25.0 / 24.0, 25025.0 / 24000.0}) {
        const auto decision = evaluateSubtitleSyncWindows(windowsFor(scale, false), cues);
        assert(decision.confident);
        assert(std::abs(decision.timeScale - scale) < 0.000001);
        assert(std::abs(decision.offsetMs - 2100) <= 150);
    }
    assert(!evaluateSubtitleSyncWindows(windowsFor(1.0, true), cues).confident);
    assert(!evaluateSubtitleSyncWindows(windowsFor(1000.0 / 1001.0, true), cues).confident);
    assert(!evaluateSubtitleSyncWindows(windowsFor(0.973, false), cues).confident);
    auto duplicate = windowsFor(1.0, false);
    duplicate[1] = duplicate[0];
    assert(!evaluateSubtitleSyncWindows(duplicate, cues).confident);
    auto missing = windowsFor(1.0, false);
    missing[1].aggressive.clear();
    assert(!evaluateSubtitleSyncWindows(missing, cues).confident);
    auto wrong = windowsFor(1.0, false);
    for (auto& window : wrong) std::fill(window.moderate.begin(), window.moderate.end(), 1);
    assert(!evaluateSubtitleSyncWindows(wrong, cues).confident);
}

void testPackedMasksPreserveCountsAtPartialWords() {
    std::vector<uint8_t> speech(137, 0);
    for (size_t i = 0; i < speech.size(); i += 3) speech[i] = 7;
    const std::vector<SubtitleCue> cues{{50, 670, "first"}, {600, 1250, "overlap"}};
    const auto peak = findSubtitleSyncPeak(speech, cues, 0, 0);
    assert(peak.speechFrames == 46);
    assert(peak.subtitleFrames == 120);
    assert(peak.matchedSpeechFrames == 40);
    const double expected = (40.0 * 137 - 46.0 * 120) / std::sqrt(46.0 * 91 * 120 * 17);
    assert(std::abs(peak.correlation - expected) < 1e-12);
}

void testProbeAndPlaybackClockOrigins() {
    // A rolling playlist has advanced 111 seconds since the player opened it.
    assert(subtitleProbeLogicalStartMs(30000, 385000, 112464, 1464) == 526000);
    // Transport timestamps can retain a large absolute media clock. Only the
    // difference between origins belongs in the normalized player timeline.
    assert(subtitleProbeLogicalStartMs(64, 385000, 381538, 381538) == 385064);
    assert(subtitleProbeLogicalStartMs(6400, 385000, 375138, 381538) == 385000);
    assert(!subtitleProbeLogicalStartMs(0, 0, 1464, 381538));
    assert(!subtitleProbeLogicalStartMs(-1, 385000, 0, 0));
    assert(!subtitleProbeLogicalStartMs(100, -1, 0, 0));
    assert(!subtitleProbeLogicalStartMs(std::numeric_limits<int>::max(), 1, 0, 0));
}

void testRetainedFriendsEvidence() {
    using namespace subtitle_sync_evidence;
    const auto decode = [](int frames, const std::vector<std::pair<int, int>>& intervals) {
        std::vector<uint8_t> mask(static_cast<size_t>(frames), 0);
        for (const auto& [begin, end] : intervals) {
            assert(begin >= 0 && end >= begin && end <= frames);
            std::fill(mask.begin() + begin, mask.begin() + end, 1);
        }
        return mask;
    };
    const auto valid = evaluateSubtitleTimeWarp(decode(friends0Frames, friends0Moderate),
                                                decode(friends0Frames, friends0Aggressive), friendsCues, 385000);
    assert(valid.decision.confident);
    assert(valid.decision.timeScale == 1.0);
    assert(valid.decision.offsetMs == 2250);
    assert(valid.matchingCues == 47);
    const auto ambiguous = evaluateSubtitleTimeWarp(decode(friends1Frames, friends1Moderate),
                                                    decode(friends1Frames, friends1Aggressive), friendsCues, 547579);
    assert(!ambiguous.decision.confident);
    assert(ambiguous.matchingCues == 59);
}

int main() {
    testRetainedFriendsEvidence();
    testProbeAndPlaybackClockOrigins();
    testPackedMasksPreserveCountsAtPartialWords();
    testSeparatedWindowsRequireOneTimeline();
    testPositiveAndNegativeOffsets();
    testLargeBroadcastAndRecapOffsets();
    testDirectStreamResumeTimeline();
    testUncertainMatchesAreNotApplied();
    testStrongVadEvidenceRequiresGlobalConfirmation();
    testProbeUsesDecodedHlsStart();
    testProbeClockFallback();
    testPcmSampleDurationNeverOverflowsArm32();
    testProviderCreditsNeverCountAsSpeech();
    testWatermarkCannotDominateAlignment();
    testLongSampleConfidenceAndMismatchedTracks();
    testFixedOffsetVadWindowDisagreement();
    testPalFrameRateDriftCorrection();
}
