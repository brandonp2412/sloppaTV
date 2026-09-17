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
        const size_t nextPosition = static_cast<size_t>(position) + 1;
        if (nextPosition >= current.size()) return;
        selection_ = current[nextPosition];
        ensureVisible(static_cast<int>(nextPosition), static_cast<int>(current.size()));
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
