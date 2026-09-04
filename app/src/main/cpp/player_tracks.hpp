#pragma once

#include "subtitle_policy.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class PlayerTrackState {
public:
    void resetSession() {
        resetPlayback();
        selectedAudioServerIndex_ = -1;
        selectedSubtitleServerIndex_ = -1;
        audioLanguagePreference_.reset();
        subtitleLanguagePreference_.reset();
    }

    void resetPlayback() {
        subtitleBusy_ = false;
        activeSubtitleCues_.clear();
        activeSubtitleLanguage_.clear();
        activeSubtitleServerIndex_ = -1;
        activeSubtitleEnabled_ = false;
    }

    [[nodiscard]] bool subtitleBusy() const { return subtitleBusy_; }
    bool beginSubtitleWork() {
        if (subtitleBusy_) return false;
        subtitleBusy_ = true;
        return true;
    }
    void endSubtitleWork() { subtitleBusy_ = false; }

    [[nodiscard]] int selectedAudioServerIndex() const { return selectedAudioServerIndex_; }
    void setSelectedAudioServerIndex(int index) { selectedAudioServerIndex_ = index; }

    [[nodiscard]] int selectedSubtitleServerIndex() const { return selectedSubtitleServerIndex_; }
    void setSelectedSubtitleServerIndex(int index) { selectedSubtitleServerIndex_ = index; }

    [[nodiscard]] const std::optional<std::string>& audioLanguagePreference() const { return audioLanguagePreference_; }
    void setAudioLanguagePreference(std::optional<std::string> preference) {
        audioLanguagePreference_ = std::move(preference);
    }

    [[nodiscard]] const std::optional<std::string>& subtitleLanguagePreference() const { return subtitleLanguagePreference_; }
    void setSubtitleLanguagePreference(std::optional<std::string> preference) {
        subtitleLanguagePreference_ = std::move(preference);
    }

    void clearLanguagePreferences() {
        audioLanguagePreference_.reset();
        subtitleLanguagePreference_.reset();
    }

    void applySubtitle(int serverIndex, std::string language, std::vector<SubtitleCue> cues) {
        activeSubtitleCues_ = std::move(cues);
        activeSubtitleLanguage_ = language.empty() ? "SUB" : std::move(language);
        activeSubtitleServerIndex_ = serverIndex;
        activeSubtitleEnabled_ = true;
    }

    void failSelectedSubtitle() {
        subtitleBusy_ = false;
        selectedSubtitleServerIndex_ = -1;
        activeSubtitleServerIndex_ = -1;
        activeSubtitleEnabled_ = false;
        activeSubtitleCues_.clear();
        activeSubtitleLanguage_.clear();
    }

    [[nodiscard]] bool subtitleEnabled() const { return activeSubtitleEnabled_; }
    void setSubtitleEnabled(bool enabled) { activeSubtitleEnabled_ = enabled; }
    [[nodiscard]] const std::vector<SubtitleCue>& subtitleCues() const { return activeSubtitleCues_; }
    [[nodiscard]] const std::string& subtitleLanguage() const { return activeSubtitleLanguage_; }
    [[nodiscard]] int activeSubtitleServerIndex() const { return activeSubtitleServerIndex_; }

    [[nodiscard]] const SubtitleCue* activeSubtitleCue(int positionMs) const {
        if (!activeSubtitleEnabled_ || activeSubtitleCues_.empty()) return nullptr;
        auto it = std::upper_bound(
            activeSubtitleCues_.begin(),
            activeSubtitleCues_.end(),
            positionMs,
            [](int value, const SubtitleCue& cue) { return value < cue.startMs; }
        );
        if (it == activeSubtitleCues_.begin()) return nullptr;
        --it;
        return positionMs >= it->startMs && positionMs < it->endMs ? &*it : nullptr;
    }

private:
    bool subtitleBusy_ = false;
    std::vector<SubtitleCue> activeSubtitleCues_;
    std::string activeSubtitleLanguage_;
    int activeSubtitleServerIndex_ = -1;
    int selectedAudioServerIndex_ = -1;
    int selectedSubtitleServerIndex_ = -1;
    std::optional<std::string> audioLanguagePreference_;
    std::optional<std::string> subtitleLanguagePreference_;
    bool activeSubtitleEnabled_ = false;
};
