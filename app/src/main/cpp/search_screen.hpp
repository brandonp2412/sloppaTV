#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
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

    void setQuery(std::string query) {
        query_ = std::move(query);
        selection_ = 0;
    }

    void append(char value) {
        query_.push_back(value);
        selection_ = 0;
    }

    [[nodiscard]] bool backspace() {
        if (query_.empty()) return false;
        query_.pop_back();
        selection_ = 0;
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
            return false;
        }
        loading_ = true;
        return true;
    }

    [[nodiscard]] bool finishSearch(const std::string& query, std::vector<JellyfinItem> results) {
        loading_ = false;
        if (query_ != query) return false;
        results_ = std::move(results);
        selection_ = 0;
        return true;
    }

    [[nodiscard]] bool failSearch(const std::string& query) {
        loading_ = false;
        return query_ == query;
    }

    void clearResults() {
        results_.clear();
        selection_ = 0;
    }

    void moveSelection(int dx, int dy, int columns) {
        if (results_.empty() || columns <= 0) return;
        const int rows = static_cast<int>((results_.size() + static_cast<size_t>(columns) - 1) / static_cast<size_t>(columns));
        int row = selection_ / columns;
        int col = selection_ % columns;
        row = std::clamp(row + dy, 0, rows - 1);
        col = std::clamp(col + dx, 0, columns - 1);
        const int next = row * columns + col;
        if (next >= 0 && next < static_cast<int>(results_.size())) selection_ = next;
    }

private:
    std::string query_;
    std::vector<JellyfinItem> results_;
    int selection_ = 0;
    bool keyboard_ = true;
    bool loading_ = false;
    bool debouncePending_ = false;
    Clock::time_point debounceDeadline_{};
};
