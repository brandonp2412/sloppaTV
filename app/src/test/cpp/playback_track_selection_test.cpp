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
        {.index = 5, .codec = "srt", .language = "eng", .title = "English Signs & Songs", .forced = false, .isDefault = true, .isExternal = false},
        {.index = 6, .codec = "srt", .language = "eng", .title = "English", .forced = false, .isDefault = true, .isExternal = false},
        {.index = 7, .codec = "srt", .language = "eng", .title = "English Forced", .forced = true, .isDefault = false, .isExternal = false},
        {.index = 8, .codec = "srt", .language = "spa", .title = "Spanish", .forced = false, .isDefault = false, .isExternal = false},
    };

    assert(playbackAudioIndexForItem(item, std::optional<std::string>{"eng"}) == 3);
    assert(playbackAudioIndexForItem(item, std::optional<std::string>{"jpn"}) == 1);
    assert(playbackAudioIndexForItem(item, std::nullopt) == -1);
    assert(playbackAudioLanguage(item, 3) == "eng");
    assert(playbackAudioLanguage(item, -1) == "jpn");

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
    autoPolicy.autoSubtitles = false;
    assert(playbackAutoSubtitleIndexForItem(item, 1, autoPolicy) == kSubtitleOffIndex);

    PlaybackTrackSelectionPolicy carriedPolicy{
        .autoSubtitles = false,
        .autoSubtitleLanguage = {},
        .autoSubtitleSourceLanguage = {},
        .allowedSubtitleLanguages = {"spa"},
    };
    assert(playbackSubtitleIndexForItem(item, 1, std::optional<std::string>{"spa"}, carriedPolicy) == 8);
    assert(playbackSubtitleIndexForItem(item, 1, std::optional<std::string>{"eng"}, carriedPolicy) == kSubtitleOffIndex);
    assert(playbackPreferredSubtitlePosition(item, carriedPolicy.allowedSubtitleLanguages) == 3);

    carriedPolicy.allowedSubtitleLanguages = {"eng"};
    assert(playbackPreferredSubtitlePosition(item, carriedPolicy.allowedSubtitleLanguages) == 1);

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

    return 0;
}
