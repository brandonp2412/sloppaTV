#include "subtitle_dialogue_sync.hpp"
#include "fixtures/subtitle_dialogue_evidence.hpp"
#include <cassert>
#include <cmath>

std::vector<SubtitleDialogueWindow> synthetic(double scale, int offset) {
    std::vector<SubtitleDialogueWindow> windows;
    for (int base : {10000, 700000, 1400000}) {
        SubtitleDialogueWindow window{static_cast<int>(std::llround(base * scale)) + offset - 500, 90000, {}};
        for (int i = 0; i < 8; ++i) {
            const int cue = base + i * 5000;
            window.anchors.push_back(
                {cue, static_cast<int>(std::llround(cue * scale)) + offset + (i % 3 - 1) * 40, 6, 1.0, 0.95});
        }
        windows.push_back(window);
    }
    return windows;
}

int main() {
    assert((subtitleDialogueWords("MAN: I’m here\\N[phone rings] Don't <i>move</i>.") ==
            std::vector<std::string>{"man", "im", "here", "dont", "move"}));
    const std::vector<std::string> phrases{"please bring the blue suitcase", "someone left their jacket outside",
                                           "we should meet after breakfast", "they have already gone upstairs"};
    std::vector<SubtitleCue> cues;
    std::vector<SubtitleSpeechWord> words;
    for (int i = 0; i < 4; ++i) {
        const int cue = 100000 + i * 20000;
        cues.push_back({cue, cue + 2500, phrases[static_cast<size_t>(i)]});
        const auto tokens = subtitleDialogueWords(phrases[static_cast<size_t>(i)]);
        for (size_t j = 0; j < tokens.size(); ++j)
            words.push_back({i * 20000 + 4200 + static_cast<int>(j) * 250, i * 20000 + 4300 + static_cast<int>(j) * 250,
                             tokens[j], 0.95});
    }
    auto anchors = matchSubtitleDialogue(words, cues, 100000, 90000);
    assert(anchors.size() == 4);
    const auto local = evaluateSubtitleDialogue({{100000, 90000, anchors}}, false);
    assert(local.confident && local.offsetMs == 4200 && local.timeScale == 1.0);
    assert(!evaluateSubtitleDialogue({{100000, 90000, anchors}}).confident);
    // Recognition can contain an isolated word error, without changing the onset.
    words[2].text = "green";
    assert(matchSubtitleDialogue(words, cues, 100000, 90000).size() == 4);
    auto wrong = words;
    for (auto& word : wrong) word.text = "unrelated";
    assert(matchSubtitleDialogue(wrong, cues, 100000, 90000).empty());
    for (auto& word : wrong) {
        word.text = "please";
        word.confidence = 0.2;
    }
    assert(matchSubtitleDialogue(wrong, cues, 100000, 90000).empty());
    auto repeated = words;
    for (auto word : words) {
        word.startMs += 10000;
        word.endMs += 10000;
        repeated.push_back(word);
    }
    assert(matchSubtitleDialogue(repeated, cues, 100000, 90000).empty());
    auto advertisement = cues;
    for (auto& cue : advertisement) cue.text = "www.fixture.invalid " + cue.text;
    assert(matchSubtitleDialogue(words, advertisement, 100000, 90000).empty());
    auto music = cues;
    for (auto& cue : music) cue.text = "♪ " + cue.text;
    assert(matchSubtitleDialogue(words, music, 100000, 90000).empty());
    for (double scale :
         {1.0, 1000.0 / 1001.0, 1001.0 / 1000.0, 24.0 / 25.0, 24000.0 / 25025.0, 25.0 / 24.0, 25025.0 / 24000.0}) {
        const auto decision = evaluateSubtitleDialogue(synthetic(scale, 3300));
        assert(decision.confident && std::abs(decision.offsetMs - 3300) <= 50);
        assert(std::abs(decision.timeScale - scale) < 0.000001);
    }
    assert(evaluateSubtitleDialogue(synthetic(1.0, -5000)).confident);
    auto jump = synthetic(1.0, 3300);
    for (auto& anchor : jump.back().anchors) anchor.spokenMs -= 19000;
    assert(!evaluateSubtitleDialogue(jump).confident);
    auto duplicate = synthetic(1.0, 3300);
    duplicate[1] = duplicate[0];
    assert(!evaluateSubtitleDialogue(duplicate).confident);
    auto insufficient = synthetic(1.0, 3300);
    insufficient[1].anchors.resize(3);
    assert(!evaluateSubtitleDialogue(insufficient).confident);
    auto uncertain = synthetic(1.0, 3300);
    for (auto& anchor : uncertain[1].anchors) anchor.confidence = 0.2;
    assert(!evaluateSubtitleDialogue(uncertain).confident);
    auto outside = synthetic(1.0, 3300);
    outside[1].startMs += 100000;
    assert(!evaluateSubtitleDialogue(outside).confident);
    // Actual independent English speech recognition resolves the VAD rejection.
    const auto actual = evaluateSubtitleDialogue(subtitle_dialogue_evidence::friendsEmbedded);
    assert(actual.confident && std::abs(actual.offsetMs - 5212) <= 50 && actual.timeScale == 1.0);
    assert(!evaluateSubtitleDialogue(subtitle_dialogue_evidence::friendsProvider).confident);
    SubtitleDialogueWindow recognized;
    recognized.recognizedWords = 40;
    assert(subtitleHasDialogueEvidence({recognized}));
    recognized.recognizedWords = 39;
    assert(!subtitleHasDialogueEvidence({recognized}));
    assert(subtitleHasDialogueEvidence(subtitle_dialogue_evidence::friendsProvider));
    // An unusual uniform release speed can now be recovered from independent
    // phrase timing, without accepting an abrupt inserted/deleted scene.
    for (double rate : {0.973, 0.985, 1.027}) {
        const auto corrected = evaluateSubtitleDialogue(synthetic(rate, 3300));
        assert(corrected.confident);
        assert(std::abs(corrected.timeScale - rate) < 0.00015);
        assert(std::abs(corrected.offsetMs - 3300) <= 200);
    }
    auto novelRateEdit = synthetic(0.985, 3300);
    for (auto& anchor : novelRateEdit.back().anchors) anchor.spokenMs -= 19000;
    assert(!evaluateSubtitleDialogue(novelRateEdit).confident);
    // Re-time real recognized dialogue to known frame-rate mismatches.
    auto extended = subtitle_dialogue_evidence::friendsEmbedded;
    extended[2] = {805731, 90000, subtitle_dialogue_evidence::friendsExtendedMiddle};
    for (double scale :
         {1.0, 1000.0 / 1001.0, 1001.0 / 1000.0, 24.0 / 25.0, 24000.0 / 25025.0, 25.0 / 24.0, 25025.0 / 24000.0}) {
        auto changedRate = extended;
        for (auto& window : changedRate)
            for (auto& anchor : window.anchors)
                anchor.subtitleMs = static_cast<int>(std::llround((anchor.subtitleMs - 3300.0) / scale));
        const auto fitted = evaluateSubtitleDialogue(changedRate);
        assert(fitted.confident && std::abs(fitted.timeScale - scale) < 0.000001);
        assert(std::abs(fitted.offsetMs - 8512) <= 150);
    }
}
