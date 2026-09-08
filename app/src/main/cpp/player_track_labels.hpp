#pragma once

#include "jellyfin_types.hpp"
#include "player_tracks.hpp"
#include "unicode_text.hpp"

#include <algorithm>
#include <string>

inline unsigned char playerTrackAsciiUpper(unsigned char value) {
    return value >= 'a' && value <= 'z'
        ? static_cast<unsigned char>(value - ('a' - 'A'))
        : value;
}

inline std::string audioTrackLabel(const JellyfinItem& item, int selectedServerIndex) {
    if (item.audios.empty()) return "DEFAULT";
    const auto selected = std::find_if(item.audios.begin(), item.audios.end(), [&](const JellyfinAudioStream& audio) {
        return audio.index == selectedServerIndex;
    });
    const auto chosen = selected == item.audios.end() ? item.audios.begin() : selected;
    std::string label = chosen->language.empty() ? "AUDIO" : chosen->language;
    std::transform(label.begin(), label.end(), label.begin(), playerTrackAsciiUpper);
    if (item.audios.size() > 1) {
        label += " " + std::to_string(std::distance(item.audios.begin(), chosen) + 1)
            + "/" + std::to_string(item.audios.size());
    }
    return label;
}

inline std::string subtitleTrackLabel(const JellyfinItem& item, const PlayerTrackState& state) {
    if (state.subtitleBusy()) return "LOADING";
    if (state.selectedSubtitleServerIndex() >= 0) {
        const auto selected = std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& subtitle) {
            return subtitle.index == state.selectedSubtitleServerIndex();
        });
        if (selected != item.subtitles.end()) {
            std::string label = selected->language.empty() ? "ON" : selected->language;
            std::transform(label.begin(), label.end(), label.begin(), playerTrackAsciiUpper);
            return label;
        }
    }
    if (!state.subtitleCues().empty()) {
        if (!state.subtitleEnabled()) return "OFF";
        std::string label = state.subtitleLanguage().empty() ? "ON" : state.subtitleLanguage();
        std::transform(label.begin(), label.end(), label.begin(), playerTrackAsciiUpper);
        const auto subtitle = std::find_if(item.subtitles.begin(), item.subtitles.end(), [&](const JellyfinSubtitleStream& candidate) {
            return candidate.index == state.activeSubtitleServerIndex();
        });
        if (subtitle != item.subtitles.end() && item.subtitles.size() > 1) {
            label += " " + std::to_string(std::distance(item.subtitles.begin(), subtitle) + 1)
                + "/" + std::to_string(item.subtitles.size());
        }
        return label;
    }
    return "OFF";
}

class PlayerControlLabelCache {
public:
    void update(const JellyfinItem& item, const PlayerTrackState& state) {
        const bool hasCues = !state.subtitleCues().empty();
        if (item.id == itemId_
            && state.selectedAudioServerIndex() == audioIndex_
            && state.selectedSubtitleServerIndex() == subtitleIndex_
            && state.activeSubtitleServerIndex() == activeSubtitleIndex_
            && state.subtitleLanguage() == activeSubtitleLanguage_
            && state.subtitleBusy() == subtitleBusy_
            && state.subtitleEnabled() == subtitleEnabled_
            && hasCues == hasCues_) {
            return;
        }
        itemId_ = item.id;
        audioIndex_ = state.selectedAudioServerIndex();
        subtitleIndex_ = state.selectedSubtitleServerIndex();
        activeSubtitleIndex_ = state.activeSubtitleServerIndex();
        activeSubtitleLanguage_ = state.subtitleLanguage();
        subtitleBusy_ = state.subtitleBusy();
        subtitleEnabled_ = state.subtitleEnabled();
        hasCues_ = hasCues;
        audio = "AUDIO  " + audioTrackLabel(item, audioIndex_);
        subtitle = "SUBTITLES  " + subtitleTrackLabel(item, state);
    }

    void clear() {
        itemId_.clear();
        audio.clear();
        subtitle.clear();
        audioIndex_ = -999;
        subtitleIndex_ = -999;
        activeSubtitleIndex_ = -999;
        activeSubtitleLanguage_.clear();
        subtitleBusy_ = false;
        subtitleEnabled_ = false;
        hasCues_ = false;
    }

    std::string audio;
    std::string subtitle;

private:
    std::string itemId_;
    int audioIndex_ = -999;
    int subtitleIndex_ = -999;
    int activeSubtitleIndex_ = -999;
    std::string activeSubtitleLanguage_;
    bool subtitleBusy_ = false;
    bool subtitleEnabled_ = false;
    bool hasCues_ = false;
};
