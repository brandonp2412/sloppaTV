#pragma once

#include "app_settings.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class SettingChangeEffect : uint8_t {
    None = 0,
    Save = 1 << 0,
    RestoreDisplayMode = 1 << 1,
    ResetScreensaver = 1 << 2,
    CycleExternalPlayer = 1 << 3,
};

constexpr SettingChangeEffect operator|(SettingChangeEffect left, SettingChangeEffect right) {
    return static_cast<SettingChangeEffect>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

constexpr bool hasSettingEffect(SettingChangeEffect effects, SettingChangeEffect effect) {
    return (static_cast<uint8_t>(effects) & static_cast<uint8_t>(effect)) != 0;
}

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

template <size_t N>
inline void stepSettingChoice(int& value, const std::array<int, N>& choices, int direction, int fallbackIndex) {
    const auto current = std::find(choices.begin(), choices.end(), value);
    int index = current == choices.end() ? fallbackIndex : static_cast<int>(std::distance(choices.begin(), current));
    index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
    value = choices[static_cast<size_t>(index)];
}

inline SettingChangeEffect adjustSetting(AppSettings& settings, SettingId selection, int direction) {
    direction = direction >= 0 ? 1 : -1;
    const auto stepLanguage = [&](std::string& language) {
        const std::string normalized = normalizeSubtitleLanguage(language);
        auto current = std::find_if(kSubtitleLanguageOptions.begin(), kSubtitleLanguageOptions.end(), [&](const auto& option) {
            return normalized == option.code;
        });
        int index = current == kSubtitleLanguageOptions.end()
            ? 0
            : static_cast<int>(std::distance(kSubtitleLanguageOptions.begin(), current));
        index = std::clamp(index + direction, 0, static_cast<int>(kSubtitleLanguageOptions.size()) - 1);
        language = kSubtitleLanguageOptions[static_cast<size_t>(index)].code;
    };
    switch (selection) {
        case SettingId::MaxStreamingBitrate: {
            static constexpr std::array<int, 6> choices{20, 40, 80, 120, 160, 200};
            stepSettingChoice(settings.maxBitrateMbps, choices, direction, 3);
            return SettingChangeEffect::Save;
        }
        case SettingId::PlaybackBuffer:
            settings.playbackBufferPreset = std::clamp(settings.playbackBufferPreset + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::SkipBack:
        case SettingId::SkipAhead: {
            static constexpr std::array<int, 6> choices{5, 10, 15, 20, 30, 60};
            int& value = selection == SettingId::SkipBack ? settings.seekBackSeconds : settings.seekForwardSeconds;
            stepSettingChoice(value, choices, direction, 1);
            return SettingChangeEffect::Save;
        }
        case SettingId::DefaultVideoZoom:
            settings.zoomMode = std::clamp(settings.zoomMode + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::AutoplayNextEpisode:
            settings.autoplayNext = !settings.autoplayNext;
            return SettingChangeEffect::Save;
        case SettingId::StillWatchingAfter: {
            static constexpr std::array<int, 5> choices{2, 3, 4, 5, 6};
            stepSettingChoice(settings.stillWatchingAfter, choices, direction, 1);
            return SettingChangeEffect::Save;
        }
        case SettingId::MatchVideoRefreshRate:
            settings.refreshRateSwitching = !settings.refreshRateSwitching;
            return settings.refreshRateSwitching
                ? SettingChangeEffect::Save
                : SettingChangeEffect::Save | SettingChangeEffect::RestoreDisplayMode;
        case SettingId::WatchedIndicators:
            settings.showWatchedIndicators = !settings.showWatchedIndicators;
            return SettingChangeEffect::Save;
        case SettingId::Clock:
            settings.showClock = !settings.showClock;
            return SettingChangeEffect::Save;
        case SettingId::Backdrops:
            settings.backdropMode = std::clamp(settings.backdropMode + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::SubtitleSize:
            settings.subtitleSize = std::clamp(settings.subtitleSize + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::SubtitleBackground:
            settings.subtitleBackground = !settings.subtitleBackground;
            return SettingChangeEffect::Save;
        case SettingId::SubtitlePosition:
            settings.subtitlePosition = std::clamp(settings.subtitlePosition + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::AudioOutput:
            settings.maxAudioChannels = settings.maxAudioChannels <= 2 ? 8 : 2;
            return SettingChangeEffect::Save;
        case SettingId::AvcMaxLevel: {
            static constexpr std::array<int, 10> choices{0, 40, 41, 42, 50, 51, 52, 60, 61, 62};
            stepSettingChoice(settings.avcLevelOverride, choices, direction, 0);
            return SettingChangeEffect::Save;
        }
        case SettingId::HevcMaxLevel: {
            static constexpr std::array<int, 9> choices{0, 120, 123, 150, 153, 156, 180, 183, 186};
            stepSettingChoice(settings.hevcLevelOverride, choices, direction, 0);
            return SettingChangeEffect::Save;
        }
        case SettingId::HdrPlayback:
            settings.hdrOverride = std::clamp(settings.hdrOverride + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::UiTextSize:
            settings.uiTextSize = std::clamp(settings.uiTextSize + direction, 0, 2);
            return SettingChangeEffect::Save;
        case SettingId::OverscanSafeArea: {
            static constexpr std::array<int, 4> choices{0, 2, 4, 6};
            stepSettingChoice(settings.safeAreaPercent, choices, direction, 0);
            return SettingChangeEffect::Save;
        }
        case SettingId::Screensaver: {
            static constexpr std::array<int, 5> choices{0, 5, 10, 20, 30};
            stepSettingChoice(settings.screensaverMinutes, choices, direction, 0);
            return SettingChangeEffect::Save | SettingChangeEffect::ResetScreensaver;
        }
        case SettingId::ExternalPlayer:
            return SettingChangeEffect::Save | SettingChangeEffect::CycleExternalPlayer;
        case SettingId::TimeFormat:
            settings.clock24Hour = !settings.clock24Hour;
            return SettingChangeEffect::Save;
        case SettingId::SeerrDriveSelection:
            settings.seerrSelectDrive = !settings.seerrSelectDrive;
            return SettingChangeEffect::Save;
        case SettingId::AutoSubtitles:
            settings.autoSubtitles = !settings.autoSubtitles;
            return SettingChangeEffect::Save;
        case SettingId::AutoSubtitleLanguage:
            stepLanguage(settings.autoSubtitleLanguage);
            return SettingChangeEffect::Save;
        case SettingId::AutoSubtitleSourceAudio: {
            std::vector<std::string> choices{"any", "different"};
            for (const auto& option : kSubtitleLanguageOptions) choices.emplace_back(option.code);
            const std::string normalized = settings.autoSubtitleSourceLanguage == "any" || settings.autoSubtitleSourceLanguage == "different"
                ? settings.autoSubtitleSourceLanguage
                : normalizeSubtitleLanguage(settings.autoSubtitleSourceLanguage);
            auto current = std::find(choices.begin(), choices.end(), normalized);
            int index = current == choices.end() ? 0 : static_cast<int>(std::distance(choices.begin(), current));
            index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
            settings.autoSubtitleSourceLanguage = choices[static_cast<size_t>(index)];
            return SettingChangeEffect::Save;
        }
        default:
            return SettingChangeEffect::None;
    }
}

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
