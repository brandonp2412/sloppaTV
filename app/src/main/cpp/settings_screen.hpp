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
        selectFirstMatch();
    }

    void setSearchText(std::string text) {
        searchQuery_ = std::move(text);
        searchFocused_ = true;
        firstVisible_ = 0;
        selectFirstMatch();
    }

    void focusSearch() { searchFocused_ = true; }

    void moveUp() {
        if (searchFocused_) return;
        const auto current = matches();
        const int position = selectedPosition(current);
        if (position <= 0) {
            searchFocused_ = true;
            return;
        }
        selection_ = current[static_cast<size_t>(position - 1)];
        ensureVisible(position - 1, static_cast<int>(current.size()));
    }

    void moveDown() {
        const auto current = matches();
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
        selectFirstMatch();
    }

    [[nodiscard]] std::vector<int> matches() const {
        return matchingSettings(searchQuery_, advanced_);
    }

    [[nodiscard]] int selection() const { return selection_; }
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
    int selectedPosition(const std::vector<int>& current) const {
        const auto selected = std::find(current.begin(), current.end(), selection_);
        return selected == current.end() ? 0 : static_cast<int>(std::distance(current.begin(), selected));
    }

    void selectFirstMatch() {
        const auto current = matches();
        selection_ = current.empty() ? 18 : current.front();
    }

    void ensureVisible(int selectedPosition, int itemCount) {
        constexpr int visibleRows = 6;
        if (selectedPosition < firstVisible_) firstVisible_ = selectedPosition;
        if (selectedPosition >= firstVisible_ + visibleRows) firstVisible_ = selectedPosition - visibleRows + 1;
        firstVisible_ = std::clamp(firstVisible_, 0, std::max(0, itemCount - visibleRows));
    }

    int selection_ = 0;
    int firstVisible_ = 0;
    std::string searchQuery_;
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

inline SettingChangeEffect adjustSetting(AppSettings& settings, int selection, int direction) {
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
        case 0: {
            static constexpr std::array<int, 6> choices{20, 40, 80, 120, 160, 200};
            stepSettingChoice(settings.maxBitrateMbps, choices, direction, 3);
            return SettingChangeEffect::Save;
        }
        case 1:
            settings.playbackBufferPreset = std::clamp(settings.playbackBufferPreset + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 2:
        case 3: {
            static constexpr std::array<int, 6> choices{5, 10, 15, 20, 30, 60};
            int& value = selection == 2 ? settings.seekBackSeconds : settings.seekForwardSeconds;
            stepSettingChoice(value, choices, direction, 1);
            return SettingChangeEffect::Save;
        }
        case 4:
            settings.zoomMode = std::clamp(settings.zoomMode + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 5:
            settings.autoplayNext = !settings.autoplayNext;
            return SettingChangeEffect::Save;
        case 6: {
            static constexpr std::array<int, 5> choices{2, 3, 4, 5, 6};
            stepSettingChoice(settings.stillWatchingAfter, choices, direction, 1);
            return SettingChangeEffect::Save;
        }
        case 7:
            settings.refreshRateSwitching = !settings.refreshRateSwitching;
            return settings.refreshRateSwitching
                ? SettingChangeEffect::Save
                : SettingChangeEffect::Save | SettingChangeEffect::RestoreDisplayMode;
        case 8:
            settings.showWatchedIndicators = !settings.showWatchedIndicators;
            return SettingChangeEffect::Save;
        case 9:
            settings.showClock = !settings.showClock;
            return SettingChangeEffect::Save;
        case 10:
            settings.backdropMode = std::clamp(settings.backdropMode + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 11:
            settings.subtitleSize = std::clamp(settings.subtitleSize + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 12:
            settings.subtitleBackground = !settings.subtitleBackground;
            return SettingChangeEffect::Save;
        case 13:
            settings.subtitlePosition = std::clamp(settings.subtitlePosition + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 14:
            settings.maxAudioChannels = settings.maxAudioChannels <= 2 ? 8 : 2;
            return SettingChangeEffect::Save;
        case 15: {
            static constexpr std::array<int, 10> choices{0, 40, 41, 42, 50, 51, 52, 60, 61, 62};
            stepSettingChoice(settings.avcLevelOverride, choices, direction, 0);
            return SettingChangeEffect::Save;
        }
        case 16: {
            static constexpr std::array<int, 9> choices{0, 120, 123, 150, 153, 156, 180, 183, 186};
            stepSettingChoice(settings.hevcLevelOverride, choices, direction, 0);
            return SettingChangeEffect::Save;
        }
        case 17:
            settings.hdrOverride = std::clamp(settings.hdrOverride + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 18:
            settings.uiTextSize = std::clamp(settings.uiTextSize + direction, 0, 2);
            return SettingChangeEffect::Save;
        case 19: {
            static constexpr std::array<int, 4> choices{0, 2, 4, 6};
            stepSettingChoice(settings.safeAreaPercent, choices, direction, 0);
            return SettingChangeEffect::Save;
        }
        case 20: {
            static constexpr std::array<int, 5> choices{0, 5, 10, 20, 30};
            stepSettingChoice(settings.screensaverMinutes, choices, direction, 0);
            return SettingChangeEffect::Save | SettingChangeEffect::ResetScreensaver;
        }
        case 21:
            return SettingChangeEffect::Save | SettingChangeEffect::CycleExternalPlayer;
        case kTimeFormatSetting:
            settings.clock24Hour = !settings.clock24Hour;
            return SettingChangeEffect::Save;
        case kAutoSubtitlesSetting:
            settings.autoSubtitles = !settings.autoSubtitles;
            return SettingChangeEffect::Save;
        case kAutoSubtitleLanguageSetting:
            stepLanguage(settings.autoSubtitleLanguage);
            return SettingChangeEffect::Save;
        case kAutoSubtitleSourceSetting: {
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

inline std::array<std::string, 30> settingsValues(
    const AppSettings& settings,
    int maxAudioOutputChannels,
    std::string externalPlayer,
    std::string username,
    bool advanced
) {
    return {
        std::to_string(settings.maxBitrateMbps) + " MBIT/S",
        playbackBufferName(settings.playbackBufferPreset),
        std::to_string(settings.seekBackSeconds) + " SECONDS",
        std::to_string(settings.seekForwardSeconds) + " SECONDS",
        videoZoomName(static_cast<VideoZoomMode>(settings.zoomMode)),
        settings.autoplayNext ? "ON" : "OFF",
        std::to_string(settings.stillWatchingAfter) + " AUTOPLAYS",
        settings.refreshRateSwitching ? "ON" : "OFF",
        settings.showWatchedIndicators ? "ON" : "OFF",
        settings.showClock ? "ON" : "OFF",
        backdropModeName(settings.backdropMode),
        subtitleSizeName(settings.subtitleSize),
        settings.subtitleBackground ? "ON" : "OFF",
        subtitlePositionName(settings.subtitlePosition),
        settings.maxAudioChannels <= 2
            ? "DOWNMIX TO STEREO"
            : "DIRECT / " + std::to_string(std::max(2, maxAudioOutputChannels)) + "CH ROUTE",
        avcLevelName(settings.avcLevelOverride),
        hevcLevelName(settings.hevcLevelOverride),
        hdrOverrideName(settings.hdrOverride),
        uiTextSizeName(settings.uiTextSize),
        settings.safeAreaPercent == 0 ? "OFF" : std::to_string(settings.safeAreaPercent) + "% PER EDGE",
        screensaverName(settings.screensaverMinutes),
        std::move(externalPlayer),
        "DEVICE / SERVER / PLAYBACK",
        username.empty() ? "CURRENT USER" : std::move(username),
        subtitleLanguageSummary(settings),
        clockFormatName(settings.clock24Hour),
        settings.autoSubtitles ? "ON" : "OFF",
        subtitleLanguageLabel(settings.autoSubtitleLanguage),
        autoSubtitleSourceName(settings),
        advanced ? "SHOW COMMON" : "SHOW TECHNICAL",
    };
}
