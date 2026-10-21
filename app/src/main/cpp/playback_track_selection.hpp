#pragma once

#include "audio_policy.hpp"
#include "jellyfin_types.hpp"
#include "media_player_policy.hpp"
#include "series_track_preferences.hpp"
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
    std::string server{};
    std::string userId{};
    SeriesTrackPreferenceMap seriesTrackPreferences{};
};

inline const SeriesTrackPreferences* playbackSeriesTrackPreferences(const JellyfinItem& item,
                                                                    const PlaybackTrackSelectionPolicy& policy) {
    return findSeriesTrackPreferences(policy.seriesTrackPreferences, policy.server, policy.userId, item.seriesId);
}

struct PlaybackTrackSelection {
    int audioStreamIndex = -1;
    int subtitleStreamIndex = kSubtitleOffIndex;
};

struct PlaybackAudioCyclePlan {
    bool available = false;
    int audioStreamIndex = -1;
    int audioOrdinal = -1;
    int subtitleStreamIndex = -1;
    bool tryEmbeddedSwitch = false;
};

enum class PlaybackSubtitleCycleAction {
    NoSubtitles,
    NoAllowedTracks,
    DisableInPlayer,
    LoadNative,
    RestartPlayback,
};

struct PlaybackSubtitleCyclePlan {
    PlaybackSubtitleCycleAction action = PlaybackSubtitleCycleAction::NoSubtitles;
    int subtitleStreamIndex = kSubtitleOffIndex;
    SubtitleStrategy strategy = SubtitleStrategy::ServerTranscode;
    std::optional<JellyfinSubtitleStream> directPlayStream;
};

inline int playbackAudioIndexForItem(const JellyfinItem& item, const std::optional<std::string>& languagePreference) {
    std::vector<AudioPreferenceCandidate> candidates;
    candidates.reserve(item.audios.size());
    for (const auto& audio : item.audios) {
        candidates.push_back({audio.index, audio.language});
    }
    return audioIndexForQueuePreference(candidates, languagePreference);
}

inline bool playbackSubtitleAllowed(const JellyfinSubtitleStream& subtitle,
                                    const std::vector<std::string>& allowedLanguages) {
    return subtitleLanguageAllowed(subtitle.language, allowedLanguages);
}

inline std::vector<JellyfinSubtitleStream>
playbackSubtitleLoadCandidates(const JellyfinItem& item, const JellyfinSubtitleStream& requested,
                               const std::vector<std::string>& allowedLanguages) {
    std::vector<JellyfinSubtitleStream> candidates{requested};
    const auto requestedInItem =
        std::find_if(item.subtitles.begin(), item.subtitles.end(),
                     [&](const JellyfinSubtitleStream& candidate) { return candidate.index == requested.index; });
    if (requestedInItem == item.subtitles.end()) return candidates;

    const std::string requestedLanguage = normalizeSubtitleLanguage(requested.language);
    for (auto candidate = std::next(requestedInItem); candidate != item.subtitles.end(); ++candidate) {
        if (!playbackSubtitleAllowed(*candidate, allowedLanguages) ||
            !useNativeSubtitleRenderer(subtitleStrategy(candidate->codec), true)) {
            continue;
        }
        if (!requestedLanguage.empty() && normalizeSubtitleLanguage(candidate->language) != requestedLanguage) {
            continue;
        }
        candidates.push_back(*candidate);
    }
    return candidates;
}

inline int playerAudioOrdinal(const PlaybackTarget& target, const JellyfinItem& item) {
    if (target.playMethod != PlaybackMethod::DirectPlay || target.audioStreamIndex < 0) return -1;
    if (target.audioOrdinal >= 0) return target.audioOrdinal;
    for (size_t index = 0; index < item.audios.size(); ++index) {
        if (item.audios[index].index == target.audioStreamIndex) return static_cast<int>(index);
    }
    return -1;
}

inline int playerSubtitleStreamIndex(const PlaybackTarget& target, const JellyfinItem& item) {
    if (target.playMethod != PlaybackMethod::DirectPlay || target.subtitleStreamIndex < 0) return kSubtitleOffIndex;
    const auto selected =
        std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return subtitle.index == target.subtitleStreamIndex;
        });
    if (selected == item.subtitles.end()) return kSubtitleOffIndex;
    return subtitleStrategy(selected->codec) == SubtitleStrategy::ClientEmbedded ? target.subtitleStreamIndex
                                                                                 : kSubtitleOffIndex;
}

inline int playerSubtitleOrdinal(const PlaybackTarget& target, const JellyfinItem& item) {
    if (playerSubtitleStreamIndex(target, item) < 0) return -1;
    for (size_t index = 0; index < item.subtitles.size(); ++index) {
        if (item.subtitles[index].index == target.subtitleStreamIndex && !item.subtitles[index].isExternal) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

inline std::string directExternalSubtitleUrl(const PlaybackTarget& target, const JellyfinItem& item) {
    if (playerSubtitleStreamIndex(target, item) < 0 || target.subtitleUrl.empty()) return {};
    const auto selected =
        std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return subtitle.index == target.subtitleStreamIndex;
        });
    return selected != item.subtitles.end() && selected->isExternal ? target.subtitleUrl : std::string{};
}

inline std::string playbackAudioLanguage(const JellyfinItem& item, int audioStreamIndex) {
    auto selected = item.audios.end();
    if (audioStreamIndex >= 0) {
        selected = std::find_if(item.audios.begin(), item.audios.end(),
                                [&](const JellyfinAudioStream& audio) { return audio.index == audioStreamIndex; });
    }
    if (selected == item.audios.end()) {
        selected = std::find_if(item.audios.begin(), item.audios.end(),
                                [](const JellyfinAudioStream& audio) { return audio.isDefault; });
    }
    if (selected == item.audios.end() && !item.audios.empty()) selected = item.audios.begin();
    return selected == item.audios.end() ? std::string{} : normalizeSubtitleLanguage(selected->language);
}

inline int playbackAutoSubtitleIndexForItem(const JellyfinItem& item, int audioStreamIndex,
                                            const PlaybackTrackSelectionPolicy& policy) {
    if (!policy.autoSubtitles || item.subtitles.empty()) return kSubtitleOffIndex;
    const std::string targetLanguage = normalizeSubtitleLanguage(policy.autoSubtitleLanguage);
    if (targetLanguage.empty()) return kSubtitleOffIndex;

    const std::string audioLanguage = playbackAudioLanguage(item, audioStreamIndex);
    const std::string source = policy.autoSubtitleSourceLanguage.empty() ? "any" : policy.autoSubtitleSourceLanguage;
    bool sourceMatches = source == "any";
    if (source == "different")
        sourceMatches = !audioLanguage.empty() && audioLanguage != targetLanguage;
    else if (source != "any")
        sourceMatches = audioLanguage == normalizeSubtitleLanguage(source);
    if (!sourceMatches) return kSubtitleOffIndex;

    const auto matchesTarget = [&](const JellyfinSubtitleStream& subtitle) {
        return subtitle.index >= 0 && normalizeSubtitleLanguage(subtitle.language) == targetLanguage &&
               !isLikelySignsOnlySubtitle(subtitle.title);
    };
    auto selected =
        std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return matchesTarget(subtitle) && subtitle.isDefault;
        });
    if (selected == item.subtitles.end()) {
        selected =
            std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
                return matchesTarget(subtitle) && !subtitle.forced;
            });
    }
    if (selected == item.subtitles.end())
        selected = std::find_if(item.subtitles.begin(), item.subtitles.end(), matchesTarget);
    return selected == item.subtitles.end() ? kSubtitleOffIndex : selected->index;
}

// Remux can offer no subtitle metadata until libmpv opens the selected file.
// Only auto-select tracks that the current decoder can display directly.
inline int playbackAutoDiscoveredSubtitleIndexForItem(const JellyfinItem& item, int audioStreamIndex,
                                                      const PlaybackTrackSelectionPolicy& policy, bool softwareDecode) {
    const auto* manual = playbackSeriesTrackPreferences(item, policy);
    if (!policy.autoSubtitles && !(manual && manual->subtitleLanguage)) return kSubtitleOffIndex;
    JellyfinItem playable;
    playable.audios = item.audios;
    for (const auto& subtitle : item.subtitles) {
        if (!subtitle.discoveredByPlayer || subtitle.isExternal) continue;
        const SubtitleStrategy strategy = subtitleStrategy(subtitle.codec);
        if (useNativeSubtitleRenderer(strategy, true) ||
            (softwareDecode && canSwitchEmbeddedSubtitleInPlayer(strategy, false))) {
            playable.subtitles.push_back(subtitle);
        }
    }
    if (manual && manual->subtitleLanguage) {
        std::vector<SubtitlePreferenceCandidate> candidates;
        for (const auto& track : playable.subtitles) {
            if (playbackSubtitleAllowed(track, policy.allowedSubtitleLanguages) &&
                !isLikelySignsOnlySubtitle(track.title) && !track.forced)
                candidates.push_back({track.index, track.language});
        }
        return subtitleIndexForQueuePreference(candidates, manual->subtitleLanguage);
    }
    return playbackAutoSubtitleIndexForItem(playable, audioStreamIndex, policy);
}

inline int playbackSubtitleIndexForItem(const JellyfinItem& item, int audioStreamIndex,
                                        const std::optional<std::string>& languagePreference,
                                        const PlaybackTrackSelectionPolicy& policy);

inline PlaybackAudioCyclePlan planPlaybackAudioTrackSelection(
    const JellyfinItem& item, int requestedAudioIndex, int selectedSubtitleStreamIndex,
    PlaybackMethod playbackMethod, const PlaybackTrackSelectionPolicy& policy) {
    PlaybackAudioCyclePlan plan;
    const auto chosen = std::find_if(item.audios.begin(), item.audios.end(),
                                     [&](const JellyfinAudioStream& track) { return track.index == requestedAudioIndex; });
    if (chosen == item.audios.end()) return plan;
    plan.available = true;
    plan.audioStreamIndex = chosen->index;
    plan.audioOrdinal = static_cast<int>(std::distance(item.audios.begin(), chosen));
    const auto* saved = playbackSeriesTrackPreferences(item, policy);
    plan.subtitleStreamIndex =
        saved && saved->subtitleLanguage
            ? playbackSubtitleIndexForItem(item, plan.audioStreamIndex, saved->subtitleLanguage, policy)
        : policy.autoSubtitles ? playbackAutoSubtitleIndexForItem(item, plan.audioStreamIndex, policy)
                               : selectedSubtitleStreamIndex;
    plan.tryEmbeddedSwitch =
        plan.subtitleStreamIndex == selectedSubtitleStreamIndex && playbackMethod == PlaybackMethod::DirectPlay;
    return plan;
}

inline PlaybackAudioCyclePlan planPlaybackAudioTrackCycle(const JellyfinItem& item, int selectedAudioStreamIndex,
                                                          int selectedSubtitleStreamIndex,
                                                          PlaybackMethod playbackMethod,
                                                          const PlaybackTrackSelectionPolicy& policy) {
    PlaybackAudioCyclePlan plan;
    if (item.audios.size() < 2) return plan;

    const auto selected = std::find_if(item.audios.begin(), item.audios.end(), [&](const JellyfinAudioStream& audio) {
        return audio.index == selectedAudioStreamIndex;
    });
    const size_t next =
        selected == item.audios.end()
            ? 0
            : (static_cast<size_t>(std::distance(item.audios.begin(), selected)) + 1) % item.audios.size();

    return planPlaybackAudioTrackSelection(item, item.audios[next].index, selectedSubtitleStreamIndex,
                                           playbackMethod, policy);
}

inline int playbackSubtitleIndexForItem(const JellyfinItem& item, int audioStreamIndex,
                                        const std::optional<std::string>& languagePreference,
                                        const PlaybackTrackSelectionPolicy& policy) {
#ifdef SLOPPATV_BENCHMARK
    (void)item;
    (void)audioStreamIndex;
    (void)languagePreference;
    (void)policy;
    return kSubtitleOffIndex;
#else
    if (languagePreference.has_value()) {
        if (languagePreference->empty()) return kSubtitleOffIndex;
        // Temporary manual selections and saved series preferences take precedence over auto subtitles.
    } else if (policy.autoSubtitles) {
        return playbackAutoSubtitleIndexForItem(item, audioStreamIndex, policy);
    }
    std::vector<SubtitlePreferenceCandidate> candidates;
    candidates.reserve(item.subtitles.size());
    for (const auto& subtitle : item.subtitles) {
        if (playbackSubtitleAllowed(subtitle, policy.allowedSubtitleLanguages)) {
            candidates.push_back({subtitle.index, subtitle.language});
        }
    }
    if (languagePreference && !languagePreference->empty()) {
        const std::string preferred = normalizeSubtitleLanguage(*languagePreference);
        // A manual choice means full subtitles in that language where available.
        for (const bool requireDefault : {true, false}) {
            for (const auto& subtitle : item.subtitles) {
                if (!playbackSubtitleAllowed(subtitle, policy.allowedSubtitleLanguages) ||
                    normalizeSubtitleLanguage(subtitle.language) != preferred || subtitle.forced ||
                    isLikelySignsOnlySubtitle(subtitle.title) || (requireDefault && !subtitle.isDefault))
                    continue;
                return subtitle.index;
            }
        }
    }
    return subtitleIndexForQueuePreference(candidates, languagePreference);
#endif
}

inline PlaybackTrackSelection selectPlaybackTracks(const JellyfinItem& item,
                                                   const std::optional<std::string>& audioLanguagePreference,
                                                   const std::optional<std::string>& subtitleLanguagePreference,
                                                   const PlaybackTrackSelectionPolicy& policy) {
    PlaybackTrackSelection selection;
    const auto* saved = playbackSeriesTrackPreferences(item, policy);
    const bool seriesScoped = !item.seriesId.empty() && !policy.server.empty() && !policy.userId.empty();
    const std::optional<std::string> audio = saved && saved->audioLanguage ? saved->audioLanguage
                                             : seriesScoped                ? std::nullopt
                                                                           : audioLanguagePreference;
    const std::optional<std::string> subtitle = saved && saved->subtitleLanguage ? saved->subtitleLanguage
                                                : seriesScoped                   ? std::nullopt
                                                                                 : subtitleLanguagePreference;
    selection.audioStreamIndex = playbackAudioIndexForItem(item, audio);
    selection.subtitleStreamIndex = playbackSubtitleIndexForItem(item, selection.audioStreamIndex, subtitle, policy);
    return selection;
}

inline int playbackPreferredSubtitlePosition(const JellyfinItem& item,
                                             const std::vector<std::string>& allowedLanguages) {
    if (item.subtitles.empty()) return -1;
    auto preferred =
        std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return playbackSubtitleAllowed(subtitle, allowedLanguages) && subtitle.isDefault &&
                   !isLikelySignsOnlySubtitle(subtitle.title);
        });
    if (preferred == item.subtitles.end()) {
        preferred =
            std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
                return playbackSubtitleAllowed(subtitle, allowedLanguages) && !subtitle.forced &&
                       !isLikelySignsOnlySubtitle(subtitle.title);
            });
    }
    if (preferred == item.subtitles.end()) {
        preferred =
            std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
                return playbackSubtitleAllowed(subtitle, allowedLanguages);
            });
    }
    return preferred == item.subtitles.end() ? -1 : static_cast<int>(std::distance(item.subtitles.begin(), preferred));
}

constexpr bool usePlayerDiscoveredTextTrack(const JellyfinSubtitleStream& track, PlaybackMethod playbackMethod) {
    return playbackMethod == PlaybackMethod::DirectPlay && track.discoveredByPlayer && !track.isExternal &&
           useNativeSubtitleRenderer(subtitleStrategy(track.codec), true);
}

inline PlaybackSubtitleCyclePlan planPlaybackSubtitleTrackSelection(
    const JellyfinItem& item, int requestedSubtitleIndex, PlaybackMethod playbackMethod,
    const std::vector<std::string>& allowedLanguages) {
    PlaybackSubtitleCyclePlan plan;
    if (requestedSubtitleIndex >= 0) {
        const auto chosen = std::find_if(item.subtitles.begin(), item.subtitles.end(),
                                        [&](const JellyfinSubtitleStream& track) {
                                            return track.index == requestedSubtitleIndex;
                                        });
        if (chosen == item.subtitles.end()) return plan;
        if (!playbackSubtitleAllowed(*chosen, allowedLanguages)) {
            plan.action = PlaybackSubtitleCycleAction::NoAllowedTracks;
            return plan;
        }
    }
    plan.subtitleStreamIndex = requestedSubtitleIndex;
    if (playbackMethod == PlaybackMethod::DirectPlay && requestedSubtitleIndex < 0) {
        plan.action = PlaybackSubtitleCycleAction::DisableInPlayer;
        return plan;
    }
    if (requestedSubtitleIndex >= 0) {
        const auto chosen = std::find_if(item.subtitles.begin(), item.subtitles.end(),
                                        [&](const JellyfinSubtitleStream& track) {
                                            return track.index == requestedSubtitleIndex;
                                        });
        plan.strategy = subtitleStrategy(chosen->codec);
        if (playbackMethod == PlaybackMethod::DirectPlay) {
            plan.directPlayStream = *chosen;
            if (useNativeSubtitleRenderer(plan.strategy, true)) {
                plan.action = PlaybackSubtitleCycleAction::LoadNative;
                return plan;
            }
        }
    }
    plan.action = PlaybackSubtitleCycleAction::RestartPlayback;
    return plan;
}

inline PlaybackSubtitleCyclePlan planPlaybackSubtitleTrackCycle(const JellyfinItem& item,
                                                                int selectedSubtitleStreamIndex,
                                                                PlaybackMethod playbackMethod,
                                                                const std::vector<std::string>& allowedLanguages) {
    PlaybackSubtitleCyclePlan plan;
    if (item.subtitles.empty()) return plan;

    std::vector<const JellyfinSubtitleStream*> allowed;
    allowed.reserve(item.subtitles.size());
    for (const auto& subtitle : item.subtitles) {
        if (playbackSubtitleAllowed(subtitle, allowedLanguages)) allowed.push_back(&subtitle);
    }
    if (allowed.empty()) {
        plan.action = PlaybackSubtitleCycleAction::NoAllowedTracks;
        return plan;
    }

    if (selectedSubtitleStreamIndex < 0) {
        const int preferred = playbackPreferredSubtitlePosition(item, allowedLanguages);
        if (preferred >= 0) plan.subtitleStreamIndex = item.subtitles[static_cast<size_t>(preferred)].index;
    } else {
        const auto selected = std::find_if(allowed.begin(), allowed.end(), [&](const JellyfinSubtitleStream* subtitle) {
            return subtitle->index == selectedSubtitleStreamIndex;
        });
        if (selected != allowed.end() && std::next(selected) != allowed.end()) {
            plan.subtitleStreamIndex = (*std::next(selected))->index;
        }
    }

    return planPlaybackSubtitleTrackSelection(item, plan.subtitleStreamIndex, playbackMethod, allowedLanguages);
}
