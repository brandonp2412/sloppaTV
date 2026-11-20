#include "player_tracks.hpp"
#include <cassert>

int main() {
    PlayerTrackState state;
    assert(!state.subtitleBusy());
    assert(state.beginSubtitleWork());
    assert(!state.beginSubtitleWork());
    state.endSubtitleWork();

    state.setSelectedAudioServerIndex(3);
    state.setSelectedSubtitleServerIndex(7);
    state.setAudioLanguagePreference(std::string("eng"));
    state.setSubtitleLanguagePreference(std::string("jpn"));
    assert(state.selectedAudioServerIndex() == 3);
    assert(state.selectedSubtitleServerIndex() == 7);

    state.applySubtitle(7, "English", {{100, 300, "first"}, {500, 900, "second"}});
    assert(state.subtitleEnabled());
    assert(state.activeSubtitleServerIndex() == 7);
    assert(state.subtitleLanguage() == "English");
    assert(state.activeSubtitleCue(50) == nullptr);
    assert(state.activeSubtitleCue(100)->text == "first");
    assert(state.activeSubtitleCue(299)->text == "first");
    assert(state.activeSubtitleCue(300) == nullptr);
    assert(state.activeSubtitleCue(600)->text == "second");
    assert(state.activeSubtitleCue(900) == nullptr);

    // Manual constant offset remains available, without automatic correction.
    state.setSubtitleOffsetMs(1200);
    assert(state.activeSubtitleCue(1300)->text == "first");
    assert(state.activeSubtitleCue(1700)->text == "second");
    assert(state.activeSubtitleCue(600) == nullptr);
    state.setSubtitleOffsetMs(-100);
    assert(state.activeSubtitleCue(0)->text == "first");
    state.setSubtitleOffsetMs(500000);
    assert(state.subtitleOffsetMs() == 120000);
    state.setSubtitleOffsetMs(-500000);
    assert(state.subtitleOffsetMs() == -120000);
    state.setSubtitleOffsetMs(0);
    state.setSubtitleEnabled(false);
    assert(state.activeSubtitleCue(600) == nullptr);
    state.setSubtitleEnabled(true);
    assert(state.activeSubtitleCue(600)->text == "second");
    // The cue at the SAME timestamp changes as soon as the offset changes,
    // without waiting for another cue boundary or seeking the playback clock.
    state.applySubtitle(7, "English", {{100, 1000, "present"}, {1000, 2000, "upcoming"}});
    assert(state.activeSubtitleCue(900)->text == "present");
    state.setSubtitleOffsetMs(-250);
    assert(state.activeSubtitleCue(900)->text == "upcoming");
    state.setSubtitleOffsetMs(250);
    assert(state.activeSubtitleCue(900)->text == "present");
    state.setSubtitleOffsetMs(0);

    state.applySubtitle(7, "English", {{1000, 5000, "long dialogue"}, {2000, 3000, "brief sign"}});
    assert(state.activeSubtitleCue(2500)->text == "brief sign");
    assert(state.activeSubtitleCue(3500)->text == "long dialogue");
    state.setSubtitleOffsetMs(3000);
    assert(state.activeSubtitleCue(5500)->text == "brief sign");
    assert(state.activeSubtitleCue(6500)->text == "long dialogue");
    state.setSelectedAudioServerIndex(99);
    assert(state.subtitleOffsetMs() == 0);
    state.setSubtitleOffsetMs(2500);
    state.failSelectedSubtitle();
    assert(state.subtitleOffsetMs() == 0);
    assert(!state.subtitleBusy());
    assert(state.selectedSubtitleServerIndex() == -1);
    assert(!state.subtitleEnabled());
    assert(state.subtitleCues().empty());

    state.setSelectedAudioServerIndex(4);
    state.setSelectedSubtitleServerIndex(8);
    state.resetSession();
    assert(state.selectedAudioServerIndex() == -1);
    assert(state.selectedSubtitleServerIndex() == -1);
    assert(!state.audioLanguagePreference().has_value());
    assert(!state.subtitleLanguagePreference().has_value());
    return 0;
}
