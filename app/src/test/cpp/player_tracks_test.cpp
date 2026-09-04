#include "player_tracks.hpp"

#include <cassert>

int main() {
    PlayerTrackState state;

    assert(!state.subtitleBusy());
    assert(state.beginSubtitleWork());
    assert(state.subtitleBusy());
    assert(!state.beginSubtitleWork());
    state.endSubtitleWork();
    assert(!state.subtitleBusy());

    state.setSelectedAudioServerIndex(3);
    state.setSelectedSubtitleServerIndex(7);
    state.setAudioLanguagePreference(std::string("eng"));
    state.setSubtitleLanguagePreference(std::string("jpn"));
    assert(state.selectedAudioServerIndex() == 3);
    assert(state.selectedSubtitleServerIndex() == 7);
    assert(state.audioLanguagePreference() == std::optional<std::string>("eng"));
    assert(state.subtitleLanguagePreference() == std::optional<std::string>("jpn"));

    std::vector<SubtitleCue> cues{
        {.startMs = 100, .endMs = 300, .text = "first"},
        {.startMs = 500, .endMs = 900, .text = "second"},
    };
    state.applySubtitle(7, "English", std::move(cues));
    assert(state.subtitleEnabled());
    assert(state.activeSubtitleServerIndex() == 7);
    assert(state.subtitleLanguage() == "English");
    assert(state.subtitleCues().size() == 2);
    assert(state.activeSubtitleCue(50) == nullptr);
    assert(state.activeSubtitleCue(100)->text == "first");
    assert(state.activeSubtitleCue(299)->text == "first");
    assert(state.activeSubtitleCue(300) == nullptr);
    assert(state.activeSubtitleCue(600)->text == "second");
    assert(state.activeSubtitleCue(900) == nullptr);

    state.setSubtitleEnabled(false);
    assert(state.activeSubtitleCue(600) == nullptr);
    state.setSubtitleEnabled(true);
    state.failSelectedSubtitle();
    assert(!state.subtitleBusy());
    assert(state.selectedSubtitleServerIndex() == -1);
    assert(state.activeSubtitleServerIndex() == -1);
    assert(!state.subtitleEnabled());
    assert(state.subtitleCues().empty());

    state.setSelectedAudioServerIndex(4);
    state.setSelectedSubtitleServerIndex(8);
    state.setAudioLanguagePreference(std::string("eng"));
    state.setSubtitleLanguagePreference(std::string("spa"));
    state.resetSession();
    assert(state.selectedAudioServerIndex() == -1);
    assert(state.selectedSubtitleServerIndex() == -1);
    assert(!state.audioLanguagePreference().has_value());
    assert(!state.subtitleLanguagePreference().has_value());
    assert(state.subtitleCues().empty());

    return 0;
}
