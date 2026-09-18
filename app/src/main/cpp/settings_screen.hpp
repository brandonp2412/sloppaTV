#pragma once

#include "app_settings.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class SettingsScreenInput {
    None,
    Back,
    Search,
    Up,
    Down,
    Left,
    Right,
    Activate,
};

enum class SettingsScreenCommandType {
    None,
    Exit,
    EditSearch,
    Adjust,
    ActivateSetting,
    ToggleSubtitleLanguage,
};

struct SettingsScreenCommand {
    SettingsScreenCommandType type = SettingsScreenCommandType::None;
    SettingId setting = SettingId::UiTextSize;
    int direction = 0;
};

struct SettingsScreenRow {
    SettingId setting = SettingId::UiTextSize;
    std::string_view label;
    std::string value;
    bool focused = false;
    bool action = false;
    bool boolean = false;
};

struct SubtitleLanguageScreenRow {
    int index = 0;
    std::string_view label;
    bool focused = false;
    bool selected = false;
};

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
        const size_t nextPosition = static_cast<size_t>(position) + 1;
        if (nextPosition >= current.size()) return;
        selection_ = current[nextPosition];
        ensureVisible(static_cast<int>(nextPosition), static_cast<int>(current.size()));
    }

    SettingsScreenCommand handleInput(SettingsScreenInput input) {
        if (subtitleLanguagePicker_) {
            if (input == SettingsScreenInput::Back)
                closeSubtitleLanguagePicker();
            else if (input == SettingsScreenInput::Up)
                moveSubtitleLanguage(-1);
            else if (input == SettingsScreenInput::Down)
                moveSubtitleLanguage(1);
            else if (input == SettingsScreenInput::Activate)
                return {.type = SettingsScreenCommandType::ToggleSubtitleLanguage};
            return {};
        }

        if (input == SettingsScreenInput::Back) return {.type = SettingsScreenCommandType::Exit};
        if (input == SettingsScreenInput::Search) {
            focusSearch();
            return {.type = SettingsScreenCommandType::EditSearch};
        }

        if (searchFocused_) {
            if (input == SettingsScreenInput::Down) {
                moveDown();
            } else if (input == SettingsScreenInput::Activate) {
                return {.type = SettingsScreenCommandType::EditSearch};
            }
            return {};
        }

        if (input == SettingsScreenInput::Up) {
            moveUp();
            return {};
        }
        if (input == SettingsScreenInput::Down) {
            moveDown();
            return {};
        }
        if (input == SettingsScreenInput::Left || input == SettingsScreenInput::Right) {
            if (!isAdjustableSetting(selection_)) return {};
            return {
                .type = SettingsScreenCommandType::Adjust,
                .setting = selection_,
                .direction = input == SettingsScreenInput::Right ? 1 : -1,
            };
        }
        if (input == SettingsScreenInput::Activate) {
            return {
                .type = SettingsScreenCommandType::ActivateSetting,
                .setting = selection_,
            };
        }
        return {};
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
        subtitleLanguageSelection_ =
            std::clamp(subtitleLanguageSelection_ + (direction >= 0 ? 1 : -1), 0, itemCount - 1);
        constexpr int visibleRows = 8;
        if (subtitleLanguageSelection_ < subtitleLanguageFirstVisible_)
            subtitleLanguageFirstVisible_ = subtitleLanguageSelection_;
        if (subtitleLanguageSelection_ >= subtitleLanguageFirstVisible_ + visibleRows) {
            subtitleLanguageFirstVisible_ = subtitleLanguageSelection_ - visibleRows + 1;
        }
        subtitleLanguageFirstVisible_ =
            std::clamp(subtitleLanguageFirstVisible_, 0, std::max(0, itemCount - visibleRows));
    }

private:
    int selectedPosition(const std::vector<SettingId>& current) const {
        const auto selected = std::find(current.begin(), current.end(), selection_);
        return selected == current.end() ? 0 : static_cast<int>(std::distance(current.begin(), selected));
    }

    void refreshMatches() { matches_ = matchingSettings(searchQuery_, advanced_); }

    void selectFirstMatch() { selection_ = matches_.empty() ? SettingId::UiTextSize : matches_.front(); }

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

inline std::optional<SubtitleLanguageScreenRow> subtitleLanguageScreenRow(const SettingsScreenState& screen,
                                                                          const AppSettings& settings, int slot,
                                                                          int visibleRows = 8) {
    constexpr int languageCount = static_cast<int>(kSubtitleLanguageOptions.size()) + 1;
    if (visibleRows <= 0 || slot < 0 || slot >= visibleRows) return std::nullopt;

    const int first =
        std::clamp(screen.subtitleLanguageFirstVisible(), 0, std::max(0, languageCount - visibleRows));
    const int languageIndex = first + slot;
    if (languageIndex >= languageCount) return std::nullopt;

    const bool selected =
        languageIndex == 0
            ? settings.subtitleLanguages.empty()
            : std::find(settings.subtitleLanguages.begin(), settings.subtitleLanguages.end(),
                        kSubtitleLanguageOptions[static_cast<size_t>(languageIndex - 1)].code) !=
                  settings.subtitleLanguages.end();
    return SubtitleLanguageScreenRow{
        .index = languageIndex,
        .label = languageIndex == 0 ? std::string_view("All languages")
                                    : std::string_view(kSubtitleLanguageOptions[static_cast<size_t>(languageIndex - 1)].label),
        .focused = languageIndex == screen.subtitleLanguageSelection(),
        .selected = selected,
    };
}

inline std::optional<SettingsScreenRow> settingsScreenRow(const SettingsScreenState& screen,
                                                          const AppSettings& settings, int maxAudioOutputChannels,
                                                          std::string_view externalPlayer, std::string_view username,
                                                          bool seerrApiKeyTyping, int slot, int visibleRows = 6) {
    const auto& matches = screen.matches();
    if (matches.empty() || visibleRows <= 0 || slot < 0 || slot >= visibleRows) return std::nullopt;

    const int first = std::clamp(screen.firstVisible(), 0, std::max(0, static_cast<int>(matches.size()) - visibleRows));
    const int matchPosition = first + slot;
    if (matchPosition >= static_cast<int>(matches.size())) return std::nullopt;

    const SettingId setting = matches[static_cast<size_t>(matchPosition)];
    std::string value =
        settingValue(settings, setting, maxAudioOutputChannels, externalPlayer, username, screen.advanced());
    if (setting == SettingId::SeerrApiKey && seerrApiKeyTyping) {
        const size_t visibleMask = std::min<size_t>(settings.seerrApiKey.size(), 24);
        value.assign(visibleMask, '*');
        if (settings.seerrApiKey.size() > visibleMask) value += "…";
        if (value.empty()) value = "Typing…";
    }
    return SettingsScreenRow{
        .setting = setting,
        .label = settingLabel(setting, screen.advanced()),
        .value = std::move(value),
        .focused = !screen.subtitleLanguagePicker() && !screen.searchFocused() && setting == screen.selection(),
        .action = isActionSetting(setting),
        .boolean = isBooleanSetting(setting),
    };
}
