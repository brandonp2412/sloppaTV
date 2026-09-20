#include "playback_track_selection.hpp"

#include <cassert>
#include <optional>
#include <string>

int main() {
    JellyfinItem item;
    item.audios = {
        {.index = 1, .channels = 2, .codec = "aac", .language = "jpn", .title = "Japanese", .isDefault = true},
        {.index = 3, .channels = 2, .codec = "aac", .language = "eng", .title = "English", .isDefault = false},
    };
    item.subtitles = {
        {.index = 5,
         .codec = "srt",
         .language = "eng",
         .title = "English Signs & Songs",
         .forced = false,
         .isDefault = true,
         .isExternal = false},
        {.index = 6,
         .codec = "srt",
         .language = "eng",
         .title = "English",
         .forced = false,
         .isDefault = true,
         .isExternal = false},
        {.index = 7,
         .codec = "srt",
         .language = "eng",
         .title = "English Forced",
         .forced = true,
         .isDefault = false,
         .isExternal = false},
        {.index = 8,
         .codec = "srt",
         .language = "spa",
         .title = "Spanish",
         .forced = false,
         .isDefault = false,
         .isExternal = false},
    };

    assert(playbackAudioIndexForItem(item, std::optional<std::string>{"eng"}) == 3);
    assert(playbackAudioIndexForItem(item, std::optional<std::string>{"jpn"}) == 1);
    assert(playbackAudioIndexForItem(item, std::nullopt) == -1);
    assert(playbackAudioLanguage(item, 3) == "eng");
    assert(playbackAudioLanguage(item, -1) == "jpn");

    PlaybackTarget directPlayerTarget;
    directPlayerTarget.playMethod = PlaybackMethod::DirectPlay;
    directPlayerTarget.audioStreamIndex = 3;
    assert(playerAudioOrdinal(directPlayerTarget, item) == 1);
    directPlayerTarget.audioStreamIndex = 99;
    assert(playerAudioOrdinal(directPlayerTarget, item) == -1);
    directPlayerTarget.playMethod = PlaybackMethod::DirectStream;
    directPlayerTarget.audioStreamIndex = 3;
    assert(playerAudioOrdinal(directPlayerTarget, item) == -1);

    JellyfinItem playerSubtitleItem;
    playerSubtitleItem.subtitles = {
        {.index = 11,
         .codec = "pgs",
         .language = "eng",
         .title = "PGS",
         .forced = false,
         .isDefault = true,
         .isExternal = false},
        {.index = 12,
         .codec = "srt",
         .language = "eng",
         .title = "Text",
         .forced = false,
         .isDefault = false,
         .isExternal = false},
        {.index = 13,
         .codec = "pgs",
         .language = "eng",
         .title = "External PGS",
         .forced = false,
         .isDefault = false,
         .isExternal = true},
    };
    PlaybackTarget playerSubtitleTarget;
    playerSubtitleTarget.playMethod = PlaybackMethod::DirectPlay;
    playerSubtitleTarget.subtitleStreamIndex = 11;
    playerSubtitleTarget.subtitleUrl = "https://media.example/subtitle";
    assert(playerSubtitleStreamIndex(playerSubtitleTarget, playerSubtitleItem) == 11);
    assert(playerSubtitleOrdinal(playerSubtitleTarget, playerSubtitleItem) == 0);
    assert(directExternalSubtitleUrl(playerSubtitleTarget, playerSubtitleItem).empty());
    playerSubtitleTarget.subtitleStreamIndex = 12;
    assert(playerSubtitleStreamIndex(playerSubtitleTarget, playerSubtitleItem) == kSubtitleOffIndex);
    assert(playerSubtitleOrdinal(playerSubtitleTarget, playerSubtitleItem) == -1);
    playerSubtitleTarget.subtitleStreamIndex = 13;
    assert(playerSubtitleStreamIndex(playerSubtitleTarget, playerSubtitleItem) == 13);
    assert(playerSubtitleOrdinal(playerSubtitleTarget, playerSubtitleItem) == -1);
    assert(directExternalSubtitleUrl(playerSubtitleTarget, playerSubtitleItem) == playerSubtitleTarget.subtitleUrl);
    playerSubtitleTarget.playMethod = PlaybackMethod::DirectStream;
    assert(playerSubtitleStreamIndex(playerSubtitleTarget, playerSubtitleItem) == kSubtitleOffIndex);
    assert(directExternalSubtitleUrl(playerSubtitleTarget, playerSubtitleItem).empty());

    PlaybackTrackSelectionPolicy autoPolicy{
        .autoSubtitles = true,
        .autoSubtitleLanguage = "eng",
        .autoSubtitleSourceLanguage = "different",
        .allowedSubtitleLanguages = {"eng"},
    };
    assert(playbackAutoSubtitleIndexForItem(item, 1, autoPolicy) == 6);
    assert(playbackSubtitleIndexForItem(item, 1, std::nullopt, autoPolicy) == 6);
    assert(playbackAutoSubtitleIndexForItem(item, 3, autoPolicy) == kSubtitleOffIndex);

    autoPolicy.autoSubtitleSourceLanguage = "any";
    assert(playbackAutoSubtitleIndexForItem(item, 3, autoPolicy) == 6);

    const auto selectedTracks = selectPlaybackTracks(item, std::optional<std::string>{"jpn"}, std::nullopt, autoPolicy);
    assert(selectedTracks.audioStreamIndex == 1);
    assert(selectedTracks.subtitleStreamIndex == 6);

    autoPolicy.autoSubtitles = false;
    assert(playbackAutoSubtitleIndexForItem(item, 1, autoPolicy) == kSubtitleOffIndex);

    auto audioCycle = planPlaybackAudioTrackCycle(item, 1, 6, PlaybackMethod::DirectPlay, autoPolicy);
    assert(audioCycle.available);
    assert(audioCycle.audioStreamIndex == 3);
    assert(audioCycle.audioOrdinal == 1);
    assert(audioCycle.subtitleStreamIndex == 6);
    assert(audioCycle.tryEmbeddedSwitch);

    audioCycle = planPlaybackAudioTrackCycle(item, 3, 6, PlaybackMethod::DirectStream, autoPolicy);
    assert(audioCycle.available);
    assert(audioCycle.audioStreamIndex == 1);
    assert(audioCycle.audioOrdinal == 0);
    assert(audioCycle.subtitleStreamIndex == 6);
    assert(!audioCycle.tryEmbeddedSwitch);

    PlaybackTrackSelectionPolicy audioChangePolicy{
        .autoSubtitles = true,
        .autoSubtitleLanguage = "eng",
        .autoSubtitleSourceLanguage = "different",
        .allowedSubtitleLanguages = {"eng"},
    };
    audioCycle = planPlaybackAudioTrackCycle(item, 1, 6, PlaybackMethod::DirectPlay, audioChangePolicy);
    assert(audioCycle.available);
    assert(audioCycle.audioStreamIndex == 3);
    assert(audioCycle.subtitleStreamIndex == kSubtitleOffIndex);
    assert(!audioCycle.tryEmbeddedSwitch);

    audioCycle = planPlaybackAudioTrackCycle(item, 99, 6, PlaybackMethod::DirectPlay, autoPolicy);
    assert(audioCycle.available);
    assert(audioCycle.audioStreamIndex == 1);
    assert(audioCycle.audioOrdinal == 0);

    PlaybackTrackSelectionPolicy carriedPolicy{
        .autoSubtitles = false,
        .autoSubtitleLanguage = {},
        .autoSubtitleSourceLanguage = {},
        .allowedSubtitleLanguages = {"spa"},
    };
    assert(playbackSubtitleIndexForItem(item, 1, std::optional<std::string>{"spa"}, carriedPolicy) == 8);
    assert(playbackSubtitleIndexForItem(item, 1, std::optional<std::string>{"eng"}, carriedPolicy) ==
           kSubtitleOffIndex);
    assert(playbackPreferredSubtitlePosition(item, carriedPolicy.allowedSubtitleLanguages) == 3);

    carriedPolicy.allowedSubtitleLanguages = {"eng"};
    assert(playbackPreferredSubtitlePosition(item, carriedPolicy.allowedSubtitleLanguages) == 1);

    auto subtitleCycle = planPlaybackSubtitleTrackCycle(item, kSubtitleOffIndex, PlaybackMethod::DirectPlay,
                                                        carriedPolicy.allowedSubtitleLanguages);
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::LoadNative);
    assert(subtitleCycle.subtitleStreamIndex == 6);
    assert(subtitleCycle.strategy == SubtitleStrategy::ClientText);
    assert(subtitleCycle.directPlayStream);
    assert(subtitleCycle.directPlayStream->index == 6);

    subtitleCycle =
        planPlaybackSubtitleTrackCycle(item, 6, PlaybackMethod::DirectPlay, carriedPolicy.allowedSubtitleLanguages);
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::LoadNative);
    assert(subtitleCycle.subtitleStreamIndex == 7);

    subtitleCycle =
        planPlaybackSubtitleTrackCycle(item, 7, PlaybackMethod::DirectPlay, carriedPolicy.allowedSubtitleLanguages);
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::DisableInPlayer);
    assert(subtitleCycle.subtitleStreamIndex == kSubtitleOffIndex);

    subtitleCycle =
        planPlaybackSubtitleTrackCycle(item, 7, PlaybackMethod::DirectStream, carriedPolicy.allowedSubtitleLanguages);
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::RestartPlayback);
    assert(subtitleCycle.subtitleStreamIndex == kSubtitleOffIndex);
    assert(!subtitleCycle.directPlayStream);

    subtitleCycle = planPlaybackSubtitleTrackCycle(item, kSubtitleOffIndex, PlaybackMethod::DirectPlay, {"deu"});
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::NoAllowedTracks);

    JellyfinItem noSubtitles;
    subtitleCycle = planPlaybackSubtitleTrackCycle(noSubtitles, kSubtitleOffIndex, PlaybackMethod::DirectPlay, {});
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::NoSubtitles);

    JellyfinItem loadFallbacks;
    loadFallbacks.subtitles = {
        {.index = 20,
         .codec = "srt",
         .language = "eng",
         .title = "English",
         .forced = false,
         .isDefault = true,
         .isExternal = true},
        {.index = 21,
         .codec = "pgs",
         .language = "eng",
         .title = "English PGS",
         .forced = false,
         .isDefault = false,
         .isExternal = false},
        {.index = 22,
         .codec = "srt",
         .language = "eng",
         .title = "English alternate",
         .forced = false,
         .isDefault = false,
         .isExternal = true},
        {.index = 23,
         .codec = "srt",
         .language = "spa",
         .title = "Spanish",
         .forced = false,
         .isDefault = false,
         .isExternal = true},
    };
    const auto loadCandidates = playbackSubtitleLoadCandidates(loadFallbacks, loadFallbacks.subtitles.front(), {"eng"});
    assert(loadCandidates.size() == 2);
    assert(loadCandidates[0].index == 20);
    assert(loadCandidates[1].index == 22);
    const JellyfinSubtitleStream detachedRequested{
        .index = 99,
        .codec = "srt",
        .language = "eng",
        .title = "Detached",
        .forced = false,
        .isDefault = false,
        .isExternal = true,
    };
    const auto detachedCandidates = playbackSubtitleLoadCandidates(loadFallbacks, detachedRequested, {"eng"});
    assert(detachedCandidates.size() == 1);
    assert(detachedCandidates.front().index == 99);

    JellyfinItem bitmapSubtitles;
    bitmapSubtitles.subtitles = {{
        .index = 11,
        .codec = "pgs",
        .language = "eng",
        .title = "English PGS",
        .forced = false,
        .isDefault = true,
        .isExternal = false,
    }};
    subtitleCycle =
        planPlaybackSubtitleTrackCycle(bitmapSubtitles, kSubtitleOffIndex, PlaybackMethod::DirectPlay, {"eng"});
    assert(subtitleCycle.action == PlaybackSubtitleCycleAction::RestartPlayback);
    assert(subtitleCycle.subtitleStreamIndex == 11);
    assert(subtitleCycle.strategy == SubtitleStrategy::ClientEmbedded);
    assert(subtitleCycle.directPlayStream);
    assert(subtitleCycle.directPlayStream->index == 11);

    JellyfinItem fallbackAudio;
    fallbackAudio.audios = {{
        .index = 9,
        .channels = 2,
        .codec = "aac",
        .language = "fra",
        .title = "French",
        .isDefault = false,
    }};
    assert(playbackAudioLanguage(fallbackAudio, 42) == "fra");
    assert(!planPlaybackAudioTrackCycle(fallbackAudio, 9, kSubtitleOffIndex, PlaybackMethod::DirectPlay, autoPolicy)
                .available);

    return 0;
}
