#pragma once

#include "audio_policy.hpp"
#include "jellyfin_types.hpp"
#include "subtitle_policy.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

struct PlaybackTrackSelectionPolicy {
    bool autoSubtitles = false;
    std::string autoSubtitleLanguage;
    std::string autoSubtitleSourceLanguage;
    std::vector<std::string> allowedSubtitleLanguages;
};

inline int playbackAudioIndexForItem(
    const JellyfinItem& item,
    const std::optional<std::string>& languagePreference
) {
    std::vector<AudioPreferenceCandidate> candidates;
    candidates.reserve(item.audios.size());
    for (const auto& audio : item.audios) {
        candidates.push_back({audio.index, audio.language});
    }
    return audioIndexForQueuePreference(candidates, languagePreference);
}

inline bool playbackSubtitleAllowed(
    const JellyfinSubtitleStream& subtitle,
    const std::vector<std::string>& allowedLanguages
) {
    return subtitleLanguageAllowed(subtitle.language, allowedLanguages);
}

inline std::string playbackAudioLanguage(const JellyfinItem& item, int audioStreamIndex) {
    auto selected = item.audios.end();
    if (audioStreamIndex >= 0) {
        selected = std::find_if(item.audios.begin(), item.audios.end(), [&](const JellyfinAudioStream& audio) {
            return audio.index == audioStreamIndex;
        });
    }
    if (selected == item.audios.end()) {
        selected = std::find_if(item.audios.begin(), item.audios.end(), [](const JellyfinAudioStream& audio) {
            return audio.isDefault;
        });
    }
    if (selected == item.audios.end() && !item.audios.empty()) selected = item.audios.begin();
    return selected == item.audios.end() ? std::string{} : normalizeSubtitleLanguage(selected->language);
}

inline int playbackAutoSubtitleIndexForItem(
    const JellyfinItem& item,
    int audioStreamIndex,
    const PlaybackTrackSelectionPolicy& policy
) {
    if (!policy.autoSubtitles || item.subtitles.empty()) return kSubtitleOffIndex;
    const std::string targetLanguage = normalizeSubtitleLanguage(policy.autoSubtitleLanguage);
    if (targetLanguage.empty()) return kSubtitleOffIndex;

    const std::string audioLanguage = playbackAudioLanguage(item, audioStreamIndex);
    const std::string source = policy.autoSubtitleSourceLanguage.empty()
        ? "any"
        : policy.autoSubtitleSourceLanguage;
    bool sourceMatches = source == "any";
    if (source == "different") sourceMatches = !audioLanguage.empty() && audioLanguage != targetLanguage;
    else if (source != "any") sourceMatches = audioLanguage == normalizeSubtitleLanguage(source);
    if (!sourceMatches) return kSubtitleOffIndex;

    const auto matchesTarget = [&](const JellyfinSubtitleStream& subtitle) {
        return subtitle.index >= 0
            && normalizeSubtitleLanguage(subtitle.language) == targetLanguage
            && !isLikelySignsOnlySubtitle(subtitle.title);
    };
    auto selected = std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
        return matchesTarget(subtitle) && subtitle.isDefault;
    });
    if (selected == item.subtitles.end()) {
        selected = std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return matchesTarget(subtitle) && !subtitle.forced;
        });
    }
    if (selected == item.subtitles.end()) selected = std::find_if(item.subtitles.begin(), item.subtitles.end(), matchesTarget);
    return selected == item.subtitles.end() ? kSubtitleOffIndex : selected->index;
}

inline int playbackSubtitleIndexForItem(
    const JellyfinItem& item,
    int audioStreamIndex,
    const std::optional<std::string>& languagePreference,
    const PlaybackTrackSelectionPolicy& policy
) {
#ifdef SLOPPATV_BENCHMARK
    (void)item;
    (void)audioStreamIndex;
    (void)languagePreference;
    (void)policy;
    return kSubtitleOffIndex;
#else
    if (policy.autoSubtitles) return playbackAutoSubtitleIndexForItem(item, audioStreamIndex, policy);
    std::vector<SubtitlePreferenceCandidate> candidates;
    candidates.reserve(item.subtitles.size());
    for (const auto& subtitle : item.subtitles) {
        if (playbackSubtitleAllowed(subtitle, policy.allowedSubtitleLanguages)) {
            candidates.push_back({subtitle.index, subtitle.language});
        }
    }
    return subtitleIndexForQueuePreference(candidates, languagePreference);
#endif
}

inline int playbackPreferredSubtitlePosition(
    const JellyfinItem& item,
    const std::vector<std::string>& allowedLanguages
) {
    if (item.subtitles.empty()) return -1;
    auto preferred = std::find_if(
        item.subtitles.begin(),
        item.subtitles.end(),
        [&](const JellyfinSubtitleStream& subtitle) {
            return playbackSubtitleAllowed(subtitle, allowedLanguages)
                && subtitle.isDefault
                && !isLikelySignsOnlySubtitle(subtitle.title);
        }
    );
    if (preferred == item.subtitles.end()) {
        preferred = std::find_if(
            item.subtitles.begin(),
            item.subtitles.end(),
            [&](const JellyfinSubtitleStream& subtitle) {
                return playbackSubtitleAllowed(subtitle, allowedLanguages)
                    && !subtitle.forced
                    && !isLikelySignsOnlySubtitle(subtitle.title);
            }
        );
    }
    if (preferred == item.subtitles.end()) {
        preferred = std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return playbackSubtitleAllowed(subtitle, allowedLanguages);
        });
    }
    return preferred == item.subtitles.end()
        ? -1
        : static_cast<int>(std::distance(item.subtitles.begin(), preferred));
}
