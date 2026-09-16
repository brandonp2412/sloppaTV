#pragma once

#include "app_settings.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class SettingsScreenState {
public:
    void reset() {
        advanced_ = false;
        searchQuery_.clear();
        searchFocused_ = false;
        firstVisible_ = 0;
        subtitleLanguagePicker_ = false;
        subtitleLanguageSelection_ = 0;
        subtitleLanguageFirstVisible_ = 0;
        refreshMatches();
        selectFirstMatch();
    }

    void setSearchText(std::string text) {
        searchQuery_ = std::move(text);
        searchFocused_ = true;
        firstVisible_ = 0;
        refreshMatches();
        selectFirstMatch();
    }

    void focusSearch() { searchFocused_ = true; }

    void moveUp() {
        if (searchFocused_) return;
        const auto& current = matches();
        const int position = selectedPosition(current);
        if (position <= 0) {
            searchFocused_ = true;
            return;
        }
        selection_ = current[static_cast<size_t>(position - 1)];
        ensureVisible(position - 1, static_cast<int>(current.size()));
    }

    void moveDown() {
        const auto& current = matches();
        if (current.empty()) return;
        if (searchFocused_) {
            searchFocused_ = false;
            firstVisible_ = 0;
            selection_ = current.front();
            return;
        }
        const int position = selectedPosition(current);
        if (position + 1 >= static_cast<int>(current.size())) return;
        selection_ = current[static_cast<size_t>(position + 1)];
        ensureVisible(position + 1, static_cast<int>(current.size()));
    }

    void toggleAdvanced() {
        advanced_ = !advanced_;
        searchQuery_.clear();
        searchFocused_ = false;
        firstVisible_ = 0;
        refreshMatches();
        selectFirstMatch();
    }

    [[nodiscard]] const std::vector<SettingId>& matches() const { return matches_; }

    [[nodiscard]] SettingId selection() const { return selection_; }
    [[nodiscard]] int firstVisible() const { return firstVisible_; }
    [[nodiscard]] const std::string& searchQuery() const { return searchQuery_; }
    [[nodiscard]] bool searchFocused() const { return searchFocused_; }
    [[nodiscard]] bool advanced() const { return advanced_; }
    [[nodiscard]] bool subtitleLanguagePicker() const { return subtitleLanguagePicker_; }
    [[nodiscard]] int subtitleLanguageSelection() const { return subtitleLanguageSelection_; }
    [[nodiscard]] int subtitleLanguageFirstVisible() const { return subtitleLanguageFirstVisible_; }

    void openSubtitleLanguagePicker() {
        subtitleLanguagePicker_ = true;
        subtitleLanguageSelection_ = 0;
        subtitleLanguageFirstVisible_ = 0;
    }

    void closeSubtitleLanguagePicker() { subtitleLanguagePicker_ = false; }

    void moveSubtitleLanguage(int direction) {
        constexpr int itemCount = static_cast<int>(kSubtitleLanguageOptions.size()) + 1;
        subtitleLanguageSelection_ = std::clamp(subtitleLanguageSelection_ + (direction >= 0 ? 1 : -1), 0, itemCount - 1);
        constexpr int visibleRows = 8;
        if (subtitleLanguageSelection_ < subtitleLanguageFirstVisible_) subtitleLanguageFirstVisible_ = subtitleLanguageSelection_;
        if (subtitleLanguageSelection_ >= subtitleLanguageFirstVisible_ + visibleRows) {
            subtitleLanguageFirstVisible_ = subtitleLanguageSelection_ - visibleRows + 1;
        }
        subtitleLanguageFirstVisible_ = std::clamp(subtitleLanguageFirstVisible_, 0, std::max(0, itemCount - visibleRows));
    }

private:
    int selectedPosition(const std::vector<SettingId>& current) const {
        const auto selected = std::find(current.begin(), current.end(), selection_);
        return selected == current.end() ? 0 : static_cast<int>(std::distance(current.begin(), selected));
    }

    void refreshMatches() {
        matches_ = matchingSettings(searchQuery_, advanced_);
    }

    void selectFirstMatch() {
        selection_ = matches_.empty() ? SettingId::UiTextSize : matches_.front();
    }

    void ensureVisible(int selectedPosition, int itemCount) {
        constexpr int visibleRows = 6;
        if (selectedPosition < firstVisible_) firstVisible_ = selectedPosition;
        if (selectedPosition >= firstVisible_ + visibleRows) firstVisible_ = selectedPosition - visibleRows + 1;
        firstVisible_ = std::clamp(firstVisible_, 0, std::max(0, itemCount - visibleRows));
    }

    SettingId selection_ = SettingId::MaxStreamingBitrate;
    int firstVisible_ = 0;
    std::string searchQuery_;
    std::vector<SettingId> matches_;
    bool searchFocused_ = false;
    bool advanced_ = false;
    bool subtitleLanguagePicker_ = false;
    int subtitleLanguageSelection_ = 0;
    int subtitleLanguageFirstVisible_ = 0;
};

inline std::string settingValue(
    const AppSettings& settings,
    SettingId setting,
    int maxAudioOutputChannels,
    std::string_view externalPlayer,
    std::string_view username,
    bool advanced
) {
    switch (setting) {
        case SettingId::MaxStreamingBitrate: return std::to_string(settings.maxBitrateMbps) + " MBIT/S";
        case SettingId::PlaybackBuffer: return playbackBufferName(settings.playbackBufferPreset);
        case SettingId::SkipBack: return std::to_string(settings.seekBackSeconds) + " SECONDS";
        case SettingId::SkipAhead: return std::to_string(settings.seekForwardSeconds) + " SECONDS";
        case SettingId::DefaultVideoZoom: return videoZoomName(static_cast<VideoZoomMode>(settings.zoomMode));
        case SettingId::AutoplayNextEpisode: return settings.autoplayNext ? "ON" : "OFF";
        case SettingId::StillWatchingAfter: return std::to_string(settings.stillWatchingAfter) + " AUTOPLAYS";
        case SettingId::MatchVideoRefreshRate: return settings.refreshRateSwitching ? "ON" : "OFF";
        case SettingId::WatchedIndicators: return settings.showWatchedIndicators ? "ON" : "OFF";
        case SettingId::Clock: return settings.showClock ? "ON" : "OFF";
        case SettingId::Backdrops: return backdropModeName(settings.backdropMode);
        case SettingId::SubtitleSize: return subtitleSizeName(settings.subtitleSize);
        case SettingId::SubtitleBackground: return settings.subtitleBackground ? "ON" : "OFF";
        case SettingId::SubtitlePosition: return subtitlePositionName(settings.subtitlePosition);
        case SettingId::AudioOutput:
            return settings.maxAudioChannels <= 2
                ? "DOWNMIX TO STEREO"
                : "DIRECT / " + std::to_string(std::max(2, maxAudioOutputChannels)) + "CH ROUTE";
        case SettingId::AvcMaxLevel: return avcLevelName(settings.avcLevelOverride);
        case SettingId::HevcMaxLevel: return hevcLevelName(settings.hevcLevelOverride);
        case SettingId::HdrPlayback: return hdrOverrideName(settings.hdrOverride);
        case SettingId::UiTextSize: return uiTextSizeName(settings.uiTextSize);
        case SettingId::OverscanSafeArea:
            return settings.safeAreaPercent == 0 ? "OFF" : std::to_string(settings.safeAreaPercent) + "% PER EDGE";
        case SettingId::Screensaver: return screensaverName(settings.screensaverMinutes);
        case SettingId::ExternalPlayer: return std::string(externalPlayer);
        case SettingId::Diagnostics: return "DEVICE / SERVER / PLAYBACK";
        case SettingId::SwitchUser: return username.empty() ? "CURRENT USER" : std::string(username);
        case SettingId::SubtitleLanguages: return subtitleLanguageSummary(settings);
        case SettingId::TimeFormat: return clockFormatName(settings.clock24Hour);
        case SettingId::AutoSubtitles: return settings.autoSubtitles ? "ON" : "OFF";
        case SettingId::AutoSubtitleLanguage: return subtitleLanguageLabel(settings.autoSubtitleLanguage);
        case SettingId::AutoSubtitleSourceAudio: return autoSubtitleSourceName(settings);
        case SettingId::SeerrServer: return settings.seerrServer.empty() ? "NOT SET" : settings.seerrServer;
        case SettingId::SeerrConnection:
            return !settings.seerrSessionCookie.empty()
                ? "CONNECTED"
                : (settings.seerrApiKey.empty() ? "CONNECT WITH JELLYFIN" : "LEGACY API KEY");
        case SettingId::SeerrDriveSelection: return settings.seerrSelectDrive ? "ON" : "OFF";
        case SettingId::SeerrApiKey: return settings.seerrApiKey.empty() ? "NOT SET" : "SET";
        case SettingId::AdvancedToggle: return advanced ? "SHOW COMMON" : "SHOW TECHNICAL";
    }
    return {};
}
