#pragma once

#include "subtitle_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
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
        invalidateSubtitleSync();
        subtitleBusy_ = false;
        activeSubtitleCues_.clear();
        subtitleCueMaxEndMs_.clear();
        subtitleOffsetMs_ = 0;
        subtitleTimeScale_ = 1.0;
        subtitleCueHint_ = 0;
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

    void setSelectedAudioServerIndex(int index) {
        if (selectedAudioServerIndex_ != index) invalidateSubtitleSync();
        selectedAudioServerIndex_ = index;
    }

    [[nodiscard]] int selectedSubtitleServerIndex() const { return selectedSubtitleServerIndex_; }

    void setSelectedSubtitleServerIndex(int index) {
        if (selectedSubtitleServerIndex_ != index) invalidateSubtitleSync();
        selectedSubtitleServerIndex_ = index;
    }

    [[nodiscard]] uint64_t subtitleSyncRevision() const { return subtitleSyncRevision_; }

    void setSubtitleContextChanged(std::function<void()> changed) { subtitleContextChanged_ = std::move(changed); }

    [[nodiscard]] const std::optional<std::string>& audioLanguagePreference() const { return audioLanguagePreference_; }

    void setAudioLanguagePreference(std::optional<std::string> preference) {
        audioLanguagePreference_ = std::move(preference);
    }

    [[nodiscard]] const std::optional<std::string>& subtitleLanguagePreference() const {
        return subtitleLanguagePreference_;
    }

    void setSubtitleLanguagePreference(std::optional<std::string> preference) {
        subtitleLanguagePreference_ = std::move(preference);
    }

    void clearLanguagePreferences() {
        audioLanguagePreference_.reset();
        subtitleLanguagePreference_.reset();
    }

    void applySubtitle(int serverIndex, std::string language, std::vector<SubtitleCue> cues) {
        invalidateSubtitleSync();
        activeSubtitleCues_ = std::move(cues);
        subtitleOffsetMs_ = 0;
        subtitleTimeScale_ = 1.0;
        subtitleCueMaxEndMs_.clear();
        subtitleCueMaxEndMs_.reserve(activeSubtitleCues_.size());
        int maxEndMs = 0;
        for (const auto& cue : activeSubtitleCues_) {
            maxEndMs = std::max(maxEndMs, cue.endMs);
            subtitleCueMaxEndMs_.push_back(maxEndMs);
        }
        subtitleCueHint_ = 0;
        activeSubtitleLanguage_ = language.empty() ? "SUB" : std::move(language);
        activeSubtitleServerIndex_ = serverIndex;
        activeSubtitleEnabled_ = true;
    }

    void failSelectedSubtitle() {
        invalidateSubtitleSync();
        subtitleBusy_ = false;
        selectedSubtitleServerIndex_ = -1;
        activeSubtitleServerIndex_ = -1;
        activeSubtitleEnabled_ = false;
        activeSubtitleCues_.clear();
        subtitleCueMaxEndMs_.clear();
        subtitleOffsetMs_ = 0;
        subtitleTimeScale_ = 1.0;
        subtitleCueHint_ = 0;
        activeSubtitleLanguage_.clear();
    }

    [[nodiscard]] bool subtitleEnabled() const { return activeSubtitleEnabled_; }

    void setSubtitleEnabled(bool enabled) {
        if (activeSubtitleEnabled_ != enabled) invalidateSubtitleSync();
        activeSubtitleEnabled_ = enabled;
    }

    [[nodiscard]] const std::vector<SubtitleCue>& subtitleCues() const { return activeSubtitleCues_; }

    void setSubtitleOffsetMs(int offsetMs) { setSubtitleTimeWarp(offsetMs, 1.0); }

    void setSubtitleTimeWarp(int offsetMs, double timeScale) {
        subtitleOffsetMs_ = std::clamp(offsetMs, -120000, 120000);
        subtitleTimeScale_ = std::isfinite(timeScale) ? std::clamp(timeScale, 0.9, 1.1) : 1.0;
    }

    [[nodiscard]] int subtitleOffsetMs() const { return subtitleOffsetMs_; }

    [[nodiscard]] double subtitleTimeScale() const { return subtitleTimeScale_; }

    [[nodiscard]] const std::string& subtitleLanguage() const { return activeSubtitleLanguage_; }

    [[nodiscard]] int activeSubtitleServerIndex() const { return activeSubtitleServerIndex_; }

    [[nodiscard]] const SubtitleCue* activeSubtitleCue(int positionMs) const {
        if (!activeSubtitleEnabled_ || activeSubtitleCues_.empty()) return nullptr;
        positionMs =
            static_cast<int>(std::lround((static_cast<double>(positionMs) - subtitleOffsetMs_) / subtitleTimeScale_));

        if (subtitleCueHint_ < activeSubtitleCues_.size() &&
            activeSubtitleCues_[subtitleCueHint_].startMs <= positionMs) {
            while (subtitleCueHint_ + 1 < activeSubtitleCues_.size() &&
                   activeSubtitleCues_[subtitleCueHint_ + 1].startMs <= positionMs) {
                ++subtitleCueHint_;
            }
        } else {
            const auto it = std::upper_bound(activeSubtitleCues_.begin(), activeSubtitleCues_.end(), positionMs,
                                             [](int value, const SubtitleCue& cue) { return value < cue.startMs; });
            if (it == activeSubtitleCues_.begin()) {
                subtitleCueHint_ = 0;
                return nullptr;
            }
            subtitleCueHint_ = static_cast<size_t>(std::distance(activeSubtitleCues_.begin(), it) - 1);
        }

        size_t candidate = subtitleCueHint_;
        while (true) {
            const auto& cue = activeSubtitleCues_[candidate];
            if (positionMs >= cue.startMs && positionMs < cue.endMs) return &cue;
            if (candidate == 0 || subtitleCueMaxEndMs_.size() != activeSubtitleCues_.size() ||
                subtitleCueMaxEndMs_[candidate - 1] <= positionMs) {
                return nullptr;
            }
            --candidate;
        }
    }

private:
    void invalidateSubtitleSync() {
        ++subtitleSyncRevision_;
        if (subtitleContextChanged_) subtitleContextChanged_();
    }

    std::function<void()> subtitleContextChanged_;
    uint64_t subtitleSyncRevision_ = 0;
    bool subtitleBusy_ = false;
    int subtitleOffsetMs_ = 0;
    double subtitleTimeScale_ = 1.0;
    std::vector<SubtitleCue> activeSubtitleCues_;
    std::vector<int> subtitleCueMaxEndMs_;
    mutable size_t subtitleCueHint_ = 0;
    std::string activeSubtitleLanguage_;
    int activeSubtitleServerIndex_ = -1;
    int selectedAudioServerIndex_ = -1;
    int selectedSubtitleServerIndex_ = -1;
    std::optional<std::string> audioLanguagePreference_;
    std::optional<std::string> subtitleLanguagePreference_;
    bool activeSubtitleEnabled_ = false;
};
