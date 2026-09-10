#include "player_track_labels.hpp"

#include <cassert>
#include <vector>

int main() {
    JellyfinItem item;
    item.id = "item";
    JellyfinAudioStream englishAudio;
    englishAudio.index = 1;
    englishAudio.language = "eng";
    JellyfinAudioStream japaneseAudio;
    japaneseAudio.index = 2;
    japaneseAudio.language = "jpn";
    item.audios = {englishAudio, japaneseAudio};
    JellyfinSubtitleStream english;
    english.index = 3;
    english.language = "eng";
    JellyfinSubtitleStream french;
    french.index = 4;
    french.language = "fra";
    item.subtitles = {english, french};

    PlayerTrackState state;
    state.setSelectedAudioServerIndex(2);
    state.setSelectedSubtitleServerIndex(3);
    assert(audioTrackLabel(item, 2) == "JPN 2/2");
    assert(subtitleTrackLabel(item, state) == "ENG");

    PlayerControlLabelCache cache;
    cache.update(item, state);
    assert(cache.audio == "Audio  JPN 2/2");
    assert(cache.subtitle == "Subtitles  ENG");

    state.setSelectedSubtitleServerIndex(-1);
    cache.update(item, state);
    assert(cache.subtitle == "Subtitles  OFF");

    std::vector<SubtitleCue> cues{{0, 1000, "hello"}};
    state.applySubtitle(4, "fra", std::move(cues));
    cache.update(item, state);
    assert(cache.subtitle == "Subtitles  FRA 2/2");
    return 0;
}
