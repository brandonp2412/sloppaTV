#pragma once

#include "subtitle_auto_sync.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct SubtitleSpeechWord {
    int startMs = 0;
    int endMs = 0;
    std::string text;
    double confidence = 1.0;
};

struct SubtitleDialogueAnchor {
    int subtitleMs = 0;
    int spokenMs = 0;
    int words = 0;
    double similarity = 0.0;
    double confidence = 0.0;
};

struct SubtitleDialogueWindow {
    int startMs = 0;
    int durationMs = 0;
    std::vector<SubtitleDialogueAnchor> anchors;
    int recognizedWords = 0;
};

inline std::vector<std::string> subtitleDialogueWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    char closing = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (closing) {
            if (c == static_cast<unsigned char>(closing)) closing = 0;
            continue;
        }
        if (c == '{' || c == '[' || c == '(' || c == '<') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
            closing = c == '{' ? '}' : c == '[' ? ']' : c == '(' ? ')' : '>';
            continue;
        }
        if (c == '\\' && i + 1 < text.size() && (text[i + 1] == 'N' || text[i + 1] == 'n')) {
            ++i;
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
            continue;
        }
        // Straight and typographic apostrophes normalize contractions identically.
        if (c == '\'') continue;
        if (c == 0xe2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 &&
            static_cast<unsigned char>(text[i + 2]) == 0x99) {
            i += 2;
            continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            word += static_cast<char>(c);
        else if (c >= 'A' && c <= 'Z')
            word += static_cast<char>(c - 'A' + 'a');
        else if (!word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) words.push_back(std::move(word));
    return words;
}

inline double subtitleDialogueSimilarity(const std::vector<std::string>& cue,
                                         const std::vector<SubtitleSpeechWord>& speech, size_t begin, size_t count) {
    if (begin + count > speech.size() || count == 0 || cue.empty()) return 0.0;
    std::vector<size_t> previous(count + 1), current(count + 1);
    for (size_t j = 0; j <= count; ++j) previous[j] = j;
    for (size_t i = 0; i < cue.size(); ++i) {
        current[0] = i + 1;
        for (size_t j = 0; j < count; ++j)
            current[j + 1] = std::min(
                {previous[j + 1] + 1, current[j] + 1, previous[j] + (cue[i] == speech[begin + j].text ? 0U : 1U)});
        previous.swap(current);
    }
    return 1.0 - static_cast<double>(previous[count]) / static_cast<double>(std::max(cue.size(), count));
}

inline std::vector<SubtitleDialogueAnchor> matchSubtitleDialogue(const std::vector<SubtitleSpeechWord>& words,
                                                                 const std::vector<SubtitleCue>& cues,
                                                                 int sampleStartMs, int durationMs) {
    std::vector<SubtitleSpeechWord> speech;
    for (const auto& word : words) {
        if (word.startMs < 0 || word.endMs < word.startMs || word.endMs > durationMs ||
            !std::isfinite(word.confidence) || word.confidence < 0.0 || word.confidence > 1.0)
            continue;
        for (auto token : subtitleDialogueWords(word.text))
            speech.push_back({word.startMs, word.endMs, std::move(token), word.confidence});
    }
    std::sort(speech.begin(), speech.end(), [](const auto& a, const auto& b) { return a.startMs < b.startMs; });
    std::vector<SubtitleDialogueAnchor> anchors;
    for (const auto& cue : cues) {
        if (subtitleSyncCueIsMetadata(cue) || cue.text.find("♪") != std::string::npos ||
            cue.text.find("♫") != std::string::npos || cue.endMs <= cue.startMs || cue.endMs - cue.startMs > 12000)
            continue;
        std::string text = cue.text;
        const auto colon = text.find(':');
        if (colon != std::string::npos && colon < 24 &&
            std::all_of(
                text.begin(), text.begin() + static_cast<std::string::difference_type>(colon),
                [](unsigned char c) { return (c >= 'A' && c <= 'Z') || c == ' ' || c == '-'; }))
            text.erase(0, colon + 1);
        const auto tokens = subtitleDialogueWords(text);
        if (tokens.size() < 4 || tokens.size() > 32 || std::set<std::string>(tokens.begin(), tokens.end()).size() < 3)
            continue;

        struct Match {
            size_t start;
            double similarity;
            double confidence;
        };

        std::vector<Match> matches;
        for (size_t i = 0; i < speech.size(); ++i) {
            const int64_t absolute = static_cast<int64_t>(sampleStartMs) + speech[i].startMs;
            if (absolute < 0 || absolute > std::numeric_limits<int>::max() ||
                std::abs(absolute - cue.startMs) > 120000 || speech[i].text != tokens.front())
                continue;
            Match best{i, 0.0, 0.0};
            for (size_t count = tokens.size() - 1; count <= tokens.size() + 1; ++count) {
                if (count < 4 || i + count > speech.size()) continue;
                const double similarity = subtitleDialogueSimilarity(tokens, speech, i, count);
                double confidence = 0.0;
                for (size_t j = i; j < i + count; ++j) confidence += speech[j].confidence;
                confidence /= static_cast<double>(count);
                if (similarity >= 0.8 && confidence >= 0.65 && similarity > best.similarity)
                    best = {i, similarity, confidence};
            }
            if (best.similarity >= 0.8) matches.push_back(best);
        }
        if (matches.empty()) continue;
        std::sort(matches.begin(), matches.end(),
                  [](const auto& a, const auto& b) { return a.similarity > b.similarity; });
        const auto& best = matches.front();
        const int onset = speech[best.start].startMs;
        if (std::any_of(matches.begin() + 1, matches.end(), [&](const auto& other) {
                return other.similarity >= best.similarity - 0.05 &&
                       std::abs(speech[other.start].startMs - onset) > 1000;
            }))
            continue;
        anchors.push_back(
            {cue.startMs, sampleStartMs + onset, static_cast<int>(tokens.size()), best.similarity, best.confidence});
    }
    // One recognized phrase cannot independently support several subtitle cues.
    std::vector<SubtitleDialogueAnchor> unique;
    for (const auto& anchor : anchors) {
        if (std::none_of(anchors.begin(), anchors.end(), [&](const auto& other) {
                return std::abs(other.spokenMs - anchor.spokenMs) < 200 &&
                       std::abs(other.subtitleMs - anchor.subtitleMs) > 1000;
            }))
            unique.push_back(anchor);
    }
    return unique;
}

// Reliable recognition which does not match captions is negative evidence,
// rather than permission to accept a coincidental speech-activity peak.
inline bool subtitleHasDialogueEvidence(const std::vector<SubtitleDialogueWindow>& windows) {
    size_t anchors = 0;
    for (const auto& window : windows) {
        if (window.recognizedWords >= 40) return true;
        anchors += window.anchors.size();
    }
    return anchors >= 8;
}

inline double subtitleDialogueMedian(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

inline SubtitleSyncDecision evaluateSubtitleDialogue(const std::vector<SubtitleDialogueWindow>& observations,
                                                     bool requireGlobal = true) {
    SubtitleSyncDecision rejected;
    auto windows = observations;
    for (auto& window : windows) {
        std::set<int> seen;
        std::erase_if(window.anchors, [&](const auto& anchor) {
            return window.startMs < 0 || window.durationMs < 20000 || anchor.subtitleMs < 0 ||
                   anchor.spokenMs < window.startMs ||
                   static_cast<int64_t>(anchor.spokenMs) > static_cast<int64_t>(window.startMs) + window.durationMs ||
                   !std::isfinite(anchor.similarity) || !std::isfinite(anchor.confidence) || anchor.similarity < 0.8 ||
                   anchor.similarity > 1.0 || anchor.confidence < 0.65 || anchor.confidence > 1.0 || anchor.words < 4 ||
                   !seen.insert(anchor.subtitleMs).second;
        });
    }
    if (windows.empty() || (requireGlobal && windows.size() < 3)) return rejected;
    if (requireGlobal) {
        std::vector<int> starts;
        for (const auto& window : windows) starts.push_back(window.startMs);
        std::sort(starts.begin(), starts.end());
        if (static_cast<int64_t>(starts.back()) - starts.front() < 600000) return rejected;
        for (size_t i = 1; i < starts.size(); ++i)
            if (starts[i] - starts[i - 1] < 180000) return rejected;
    }
    // Provider releases can have non-standard but *uniform* speed differences.
    // Fit a robust rate from independent, distant spoken-to-caption matches rather
    // than guessing a frame rate from the media duration. The existing per-window
    // inlier checks below still reject missing edits and clock discontinuities.
    const std::vector<double> standardRates{
        1.0, 1000.0 / 1001.0, 1001.0 / 1000.0, 24.0 / 25.0,
        24000.0 / 25025.0, 25.0 / 24.0, 25025.0 / 24000.0};
    std::vector<double> candidateRates = standardRates;
    if (requireGlobal && windows.size() >= 3) {
        std::vector<double> measuredRates;
        for (size_t i = 0; i < windows.size(); ++i) {
            for (size_t j = i + 1; j < windows.size(); ++j) {
                if (std::abs(windows[j].startMs - windows[i].startMs) < 300000) continue;
                for (const auto& early : windows[i].anchors) {
                    for (const auto& late : windows[j].anchors) {
                        const double subtitleSpan = static_cast<double>(late.subtitleMs) - early.subtitleMs;
                        const double spokenSpan = static_cast<double>(late.spokenMs) - early.spokenMs;
                        if (std::abs(subtitleSpan) < 300000 || subtitleSpan * spokenSpan <= 0) continue;
                        const double rate = spokenSpan / subtitleSpan;
                        if (rate >= 0.9 && rate <= 1.1) measuredRates.push_back(rate);
                    }
                }
            }
        }
        if (measuredRates.size() >= 24) {
            const double rate = subtitleDialogueMedian(std::move(measuredRates));
            const bool distinct = std::all_of(standardRates.begin(), standardRates.end(),
                                              [rate](double standard) { return std::abs(rate - standard) >= 0.0015; });
            if (distinct) candidateRates.push_back(rate);
        }
    }
    SubtitleSyncDecision best;
    double bestError = std::numeric_limits<double>::max();
    double secondError = std::numeric_limits<double>::max();
    for (double scale : candidateRates) {
        if (!requireGlobal && scale != 1.0) continue;
        std::vector<double> residuals;
        std::set<int> uniqueCues;
        for (const auto& window : windows)
            for (const auto& anchor : window.anchors)
                if (anchor.words >= 4 && anchor.similarity >= 0.8 && anchor.confidence >= 0.65) {
                    residuals.push_back(anchor.spokenMs - scale * anchor.subtitleMs);
                    uniqueCues.insert(anchor.subtitleMs);
                }
        const size_t minimum = scale == 1.0 ? 4 : 6;
        if (residuals.size() < minimum * windows.size() || uniqueCues.size() < minimum * windows.size()) continue;
        const double offset = subtitleDialogueMedian(residuals);
        if (std::abs(offset) > 120000) continue;
        std::vector<double> errors;
        bool consistent = true;
        double agreement = 1.0;
        for (const auto& window : windows) {
            size_t count = 0, inliers = 0;
            int earliest = std::numeric_limits<int>::max(), latest = 0;
            std::vector<double> local;
            for (const auto& anchor : window.anchors) {
                if (anchor.words < 4 || anchor.similarity < 0.8 || anchor.confidence < 0.65) continue;
                ++count;
                const double residual = anchor.spokenMs - scale * anchor.subtitleMs;
                if (std::abs(residual - offset) <= 500.0) {
                    ++inliers;
                    earliest = std::min(earliest, anchor.spokenMs);
                    latest = std::max(latest, anchor.spokenMs);
                    local.push_back(residual);
                    errors.push_back(std::abs(residual - offset));
                }
            }
            if (count == 0 || inliers < minimum || static_cast<double>(inliers) / static_cast<double>(count) < 0.8 ||
                latest - earliest < 20000 || std::abs(subtitleDialogueMedian(local) - offset) > 250.0) {
                consistent = false;
                break;
            }
            agreement = std::min(agreement, static_cast<double>(inliers) / static_cast<double>(count));
        }
        if (!consistent) continue;
        const SubtitleSyncDecision decision{true, static_cast<int>(std::llround(offset)), agreement, scale};
        if (scale == 1.0) return decision;
        const double error = subtitleDialogueMedian(errors);
        if (error < bestError) {
            secondError = bestError;
            bestError = error;
            best = decision;
        } else {
            secondError = std::min(secondError, error);
        }
    }
    if (best.confident && secondError - bestError < 100.0) return rejected;
    return best;
}
