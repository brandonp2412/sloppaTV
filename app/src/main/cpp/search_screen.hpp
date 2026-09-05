#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

class SearchScreenState {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kDebounceDelay = std::chrono::milliseconds(180);

    void reset() {
        query_.clear();
        results_.clear();
        selection_ = 0;
        firstVisible_ = {0, 0};
        keyboard_ = true;
        loading_ = false;
        debouncePending_ = false;
        debounceDeadline_ = {};
    }

    [[nodiscard]] const std::string& query() const { return query_; }
    [[nodiscard]] const std::vector<JellyfinItem>& results() const { return results_; }
    [[nodiscard]] std::vector<JellyfinItem>& results() { return results_; }
    [[nodiscard]] int selection() const { return selection_; }
    [[nodiscard]] bool keyboard() const { return keyboard_; }
    [[nodiscard]] bool loading() const { return loading_; }
    [[nodiscard]] bool debouncePending() const { return debouncePending_; }
    [[nodiscard]] Clock::time_point debounceDeadline() const { return debounceDeadline_; }
    [[nodiscard]] int topLevelCount() const {
        const auto firstEpisode = std::find_if(results_.begin(), results_.end(), [](const JellyfinItem& item) {
            return item.type == "Episode";
        });
        return static_cast<int>(std::distance(results_.begin(), firstEpisode));
    }
    [[nodiscard]] int rowStart(int row) const { return row <= 0 ? 0 : topLevelCount(); }
    [[nodiscard]] int rowItemCount(int row) const {
        const int top = topLevelCount();
        return row <= 0 ? top : static_cast<int>(results_.size()) - top;
    }
    [[nodiscard]] int selectedRow() const {
        if (results_.empty()) return 0;
        return results_[static_cast<size_t>(selection_)].type == "Episode" ? 1 : 0;
    }
    [[nodiscard]] bool selectionOnFirstResultRow() const {
        return selectedRow() == 0 || topLevelCount() == 0;
    }
    [[nodiscard]] int firstVisibleInRow(int row, int columns) const {
        if (row < 0 || row > 1 || columns <= 0) return 0;
        return std::clamp(firstVisible_[static_cast<size_t>(row)], 0, std::max(0, rowItemCount(row) - columns));
    }

    void setQuery(std::string query) {
        query_ = std::move(query);
        selection_ = 0;
        firstVisible_ = {0, 0};
    }

    void append(char value) {
        query_.push_back(value);
        selection_ = 0;
        firstVisible_ = {0, 0};
    }

    [[nodiscard]] bool backspace() {
        if (query_.empty()) return false;
        query_.pop_back();
        selection_ = 0;
        firstVisible_ = {0, 0};
        return true;
    }

    void setKeyboard(bool keyboard) { keyboard_ = keyboard; }
    void setLoading(bool loading) { loading_ = loading; }
    void setSelection(int selection) {
        selection_ = results_.empty()
            ? 0
            : std::clamp(selection, 0, static_cast<int>(results_.size()) - 1);
    }

    [[nodiscard]] bool scheduleDebounce(Clock::time_point now) {
        selection_ = 0;
        if (query_.empty()) {
            debouncePending_ = false;
            loading_ = false;
            results_.clear();
            firstVisible_ = {0, 0};
            return false;
        }
        debouncePending_ = true;
        debounceDeadline_ = now + kDebounceDelay;
        return true;
    }

    void cancelPending() {
        debouncePending_ = false;
        loading_ = false;
    }

    [[nodiscard]] bool debounceDue(Clock::time_point now) const {
        return debouncePending_ && now >= debounceDeadline_;
    }

    [[nodiscard]] bool beginSearch() {
        debouncePending_ = false;
        selection_ = 0;
        if (query_.empty()) {
            loading_ = false;
            results_.clear();
            firstVisible_ = {0, 0};
            return false;
        }
        loading_ = true;
        return true;
    }

    [[nodiscard]] bool finishSearch(const std::string& query, std::vector<JellyfinItem> results) {
        if (query_ != query) return false;
        loading_ = false;
        std::stable_partition(results.begin(), results.end(), [](const JellyfinItem& item) {
            return item.type != "Episode";
        });
        results_ = std::move(results);
        selection_ = 0;
        firstVisible_ = {0, 0};
        return true;
    }

    [[nodiscard]] bool failSearch(const std::string& query) {
        if (query_ != query) return false;
        loading_ = false;
        return true;
    }

    void clearResults() {
        results_.clear();
        selection_ = 0;
        firstVisible_ = {0, 0};
    }

    void moveSelection(int dx, int dy, int columns) {
        if (results_.empty() || columns <= 0) return;
        int row = selectedRow();
        const int start = rowStart(row);
        const int count = rowItemCount(row);
        int local = selection_ - start;

        if (dx != 0 && count > 0) {
            local = std::clamp(local + dx, 0, count - 1);
            selection_ = start + local;
        }

        if (dy != 0) {
            const int targetRow = std::clamp(row + (dy > 0 ? 1 : -1), 0, 1);
            const int targetCount = rowItemCount(targetRow);
            if (targetRow != row && targetCount > 0) {
                row = targetRow;
                selection_ = rowStart(row) + std::min(local, targetCount - 1);
            }
        }

        row = selectedRow();
        const int selectedLocal = selection_ - rowStart(row);
        const int maxFirst = std::max(0, rowItemCount(row) - columns);
        int first = std::clamp(firstVisible_[static_cast<size_t>(row)], 0, maxFirst);
        if (selectedLocal < first) first = selectedLocal;
        else if (selectedLocal >= first + columns) first = selectedLocal - columns + 1;
        firstVisible_[static_cast<size_t>(row)] = std::clamp(first, 0, maxFirst);
    }

private:
    std::string query_;
    std::vector<JellyfinItem> results_;
    int selection_ = 0;
    std::array<int, 2> firstVisible_{0, 0};
    bool keyboard_ = true;
    bool loading_ = false;
    bool debouncePending_ = false;
    Clock::time_point debounceDeadline_{};
};
