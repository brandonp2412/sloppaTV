#pragma once

#include "jellyfin_types.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "unicode_text.hpp"

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
    static constexpr auto kSeerrDebounceDelay = std::chrono::milliseconds(550);
    static constexpr size_t kSeerrMinQueryBytes = 3;
    static constexpr int kLibraryRow = 0;
    static constexpr int kSeerrRow = 1;
    static constexpr int kEpisodeRow = 2;
    static constexpr int kRowCount = 3;

    void reset() {
        query_.clear();
        libraryTitles_.clear();
        seerrResults_.clear();
        episodes_.clear();
        results_.clear();
        selection_ = 0;
        firstVisible_ = {0, 0, 0};
        keyboard_ = true;
        loading_ = false;
        seerrLoading_ = false;
        seerrError_.clear();
        debouncePending_ = false;
        debounceDeadline_ = {};
        seerrDebouncePending_ = false;
        seerrDebounceDeadline_ = {};
        seerrQuery_.clear();
    }

    [[nodiscard]] const std::string& query() const { return query_; }

    [[nodiscard]] const std::vector<JellyfinItem>& results() const { return results_; }

    [[nodiscard]] std::vector<JellyfinItem>& results() { return results_; }

    [[nodiscard]] int selection() const { return selection_; }

    [[nodiscard]] bool keyboard() const { return keyboard_; }

    [[nodiscard]] bool loading() const { return loading_; }

    [[nodiscard]] bool seerrLoading() const { return seerrLoading_; }

    [[nodiscard]] const std::string& seerrError() const { return seerrError_; }

    [[nodiscard]] bool debouncePending() const { return debouncePending_; }

    [[nodiscard]] Clock::time_point debounceDeadline() const { return debounceDeadline_; }

    [[nodiscard]] bool seerrDebouncePending() const { return seerrDebouncePending_; }

    [[nodiscard]] Clock::time_point seerrDebounceDeadline() const { return seerrDebounceDeadline_; }

    [[nodiscard]] int rowStart(int row) const {
        if (row <= kLibraryRow) return 0;
        if (row == kSeerrRow) return static_cast<int>(libraryTitles_.size());
        return static_cast<int>(libraryTitles_.size() + visibleSeerrCount());
    }

    [[nodiscard]] int rowItemCount(int row) const {
        if (row == kLibraryRow) return static_cast<int>(libraryTitles_.size());
        if (row == kSeerrRow) return static_cast<int>(visibleSeerrCount());
        if (row == kEpisodeRow) return static_cast<int>(episodes_.size());
        return 0;
    }

    [[nodiscard]] int selectedRow() const {
        if (results_.empty()) return firstPopulatedRow();
        const int libraryEnd = static_cast<int>(libraryTitles_.size());
        const int seerrEnd = libraryEnd + static_cast<int>(visibleSeerrCount());
        if (selection_ < libraryEnd) return kLibraryRow;
        if (selection_ < seerrEnd) return kSeerrRow;
        return kEpisodeRow;
    }

    [[nodiscard]] int firstPopulatedRow() const {
        for (int row = 0; row < kRowCount; ++row) {
            if (rowItemCount(row) > 0) return row;
        }
        return kLibraryRow;
    }

    [[nodiscard]] bool selectionOnFirstResultRow() const { return selectedRow() == firstPopulatedRow(); }

    [[nodiscard]] int firstVisibleInRow(int row, int columns) const {
        if (row < 0 || row >= kRowCount || columns <= 0) return 0;
        return std::clamp(firstVisible_[static_cast<size_t>(row)], 0, std::max(0, rowItemCount(row) - columns));
    }

    void setQuery(std::string query) {
        query_ = std::move(query);
        selection_ = 0;
        firstVisible_ = {0, 0, 0};
    }

    void append(char value) {
        query_.push_back(value);
        selection_ = 0;
        firstVisible_ = {0, 0, 0};
    }

    [[nodiscard]] bool backspace() {
        if (!eraseLastUtf8CodePoint(query_)) return false;
        selection_ = 0;
        firstVisible_ = {0, 0, 0};
        return true;
    }

    void setKeyboard(bool keyboard) { keyboard_ = keyboard; }

    void setLoading(bool loading) { loading_ = loading; }

    void setSeerrLoading(bool loading) { seerrLoading_ = loading; }

    void setSelection(int selection) {
        selection_ = results_.empty() ? 0 : std::clamp(selection, 0, static_cast<int>(results_.size()) - 1);
    }

    [[nodiscard]] bool scheduleDebounce(Clock::time_point now) {
        selection_ = 0;
        if (query_.empty()) {
            debouncePending_ = false;
            loading_ = false;
            seerrLoading_ = false;
            libraryTitles_.clear();
            seerrResults_.clear();
            episodes_.clear();
            results_.clear();
            seerrError_.clear();
            firstVisible_ = {0, 0, 0};
            return false;
        }
        debouncePending_ = true;
        debounceDeadline_ = now + kDebounceDelay;
        return true;
    }

    void cancelPending() {
        debouncePending_ = false;
        seerrDebouncePending_ = false;
        loading_ = false;
        seerrLoading_ = false;
    }

    [[nodiscard]] bool debounceDue(Clock::time_point now) const { return debouncePending_ && now >= debounceDeadline_; }

    [[nodiscard]] bool scheduleSeerrDebounce(Clock::time_point now, bool configured) {
        const bool eligible = configured && query_.size() >= kSeerrMinQueryBytes;
        if (!eligible) {
            const bool changed = seerrDebouncePending_ || seerrLoading_ || !seerrQuery_.empty() ||
                                 !seerrResults_.empty() || !seerrError_.empty();
            seerrDebouncePending_ = false;
            seerrLoading_ = false;
            seerrDebounceDeadline_ = {};
            seerrQuery_.clear();
            seerrResults_.clear();
            seerrError_.clear();
            if (changed) rebuildResults();
            return changed;
        }
        if (query_ == seerrQuery_) return false;
        seerrQuery_ = query_;
        seerrDebouncePending_ = true;
        seerrDebounceDeadline_ = now + kSeerrDebounceDelay;
        seerrLoading_ = false;
        seerrResults_.clear();
        seerrError_.clear();
        rebuildResults();
        return true;
    }

    [[nodiscard]] bool seerrDebounceDue(Clock::time_point now) const {
        return seerrDebouncePending_ && now >= seerrDebounceDeadline_;
    }

    [[nodiscard]] bool beginDueSeerrSearch(Clock::time_point now) {
        if (!seerrDebounceDue(now)) return false;
        seerrDebouncePending_ = false;
        seerrLoading_ = true;
        return true;
    }

    [[nodiscard]] bool beginImmediateSeerrSearch(bool configured) {
        if (!configured || query_.size() < kSeerrMinQueryBytes) {
            (void)scheduleSeerrDebounce(Clock::now(), false);
            return false;
        }
        if (query_ == seerrQuery_) {
            if (!seerrDebouncePending_) return false;
            seerrDebouncePending_ = false;
            seerrLoading_ = true;
            return true;
        }
        seerrQuery_ = query_;
        seerrDebouncePending_ = false;
        seerrLoading_ = true;
        seerrResults_.clear();
        seerrError_.clear();
        rebuildResults();
        return true;
    }

    [[nodiscard]] bool beginSearch() {
        debouncePending_ = false;
        selection_ = 0;
        if (query_.empty()) {
            loading_ = false;
            seerrLoading_ = false;
            clearResults();
            return false;
        }
        loading_ = true;
        libraryTitles_.clear();
        episodes_.clear();
        rebuildResults();
        return true;
    }

    void beginSeerrSearch() {
        seerrDebouncePending_ = false;
        seerrLoading_ = true;
        seerrQuery_ = query_;
        seerrError_.clear();
        seerrResults_.clear();
        rebuildResults();
    }

    [[nodiscard]] bool finishLibrarySearch(const std::string& query, std::vector<JellyfinItem> results) {
        if (query_ != query) return false;
        loading_ = false;
        const auto firstEpisode = std::stable_partition(
            results.begin(), results.end(), [](const JellyfinItem& item) { return item.type != "Episode"; });
        libraryTitles_.assign(results.begin(), firstEpisode);
        episodes_.assign(firstEpisode, results.end());
        rebuildResults();
        return true;
    }

    [[nodiscard]] bool failLibrarySearch(const std::string& query) {
        if (query_ != query) return false;
        loading_ = false;
        return true;
    }

    [[nodiscard]] bool finishSeerrSearch(const std::string& query, std::vector<SeerrMediaItem> results) {
        if (query_ != query || seerrQuery_ != query) return false;
        seerrLoading_ = false;
        seerrError_.clear();
        seerrResults_ = std::move(results);
        rebuildResults();
        return true;
    }

    [[nodiscard]] bool failSeerrSearch(const std::string& query, std::string error) {
        if (query_ != query || seerrQuery_ != query) return false;
        seerrLoading_ = false;
        seerrError_ = std::move(error);
        seerrResults_.clear();
        rebuildResults();
        return true;
    }

    void markSeerrRequested(const std::string& itemId, std::string status, int requestId = 0) {
        for (auto& item : seerrResults_) {
            if (item.id != itemId) continue;
            item.requested = true;
            item.requestId = requestId;
            item.status = std::move(status);
            if (item.mediaStatus <= 1) item.mediaStatus = 2;
            break;
        }
        rebuildResults();
    }

    void markSeerrUnrequested(const std::string& itemId) {
        for (auto& item : seerrResults_) {
            if (item.id != itemId) continue;
            item.requested = false;
            item.requestId = 0;
            item.mediaStatus = 0;
            item.status.clear();
            break;
        }
        // Force a future search of the same text to re-check Seerr instead of
        // treating the pre-delete result as a valid cached remote query.
        seerrQuery_.clear();
        rebuildResults();
    }

    void clearResults() {
        libraryTitles_.clear();
        seerrResults_.clear();
        episodes_.clear();
        results_.clear();
        selection_ = 0;
        firstVisible_ = {0, 0, 0};
    }

    bool removeItem(const std::string& itemId) {
        if (itemId.empty()) return false;
        const size_t before = libraryTitles_.size() + episodes_.size();
        std::erase_if(libraryTitles_, [&](const JellyfinItem& item) { return item.id == itemId; });
        std::erase_if(episodes_, [&](const JellyfinItem& item) { return item.id == itemId; });
        if (libraryTitles_.size() + episodes_.size() == before) return false;
        rebuildResults();
        return true;
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
            int targetRow = row;
            while (true) {
                targetRow += dy > 0 ? 1 : -1;
                if (targetRow < 0 || targetRow >= kRowCount) break;
                const int targetCount = rowItemCount(targetRow);
                if (targetCount <= 0) continue;
                row = targetRow;
                selection_ = rowStart(row) + std::min(local, targetCount - 1);
                break;
            }
        }

        row = selectedRow();
        const int selectedLocal = selection_ - rowStart(row);
        const int maxFirst = std::max(0, rowItemCount(row) - columns);
        int first = std::clamp(firstVisible_[static_cast<size_t>(row)], 0, maxFirst);
        if (selectedLocal < first)
            first = selectedLocal;
        else if (selectedLocal >= first + columns)
            first = selectedLocal - columns + 1;
        firstVisible_[static_cast<size_t>(row)] = std::clamp(first, 0, maxFirst);
    }

private:
    [[nodiscard]] bool duplicatesLocalLibrary(const SeerrMediaItem& candidate) const {
        if (candidate.tmdbId <= 0) return false;
        const std::string tmdbId = std::to_string(candidate.tmdbId);
        return std::any_of(libraryTitles_.begin(), libraryTitles_.end(),
                           [&](const JellyfinItem& local) { return !local.tmdbId.empty() && local.tmdbId == tmdbId; });
    }

    [[nodiscard]] size_t visibleSeerrCount() const {
        return static_cast<size_t>(
            std::count_if(seerrResults_.begin(), seerrResults_.end(),
                          [&](const SeerrMediaItem& item) { return !duplicatesLocalLibrary(item); }));
    }

    void rebuildResults() {
        std::string selectedId;
        if (!results_.empty() && selection_ >= 0 && selection_ < static_cast<int>(results_.size())) {
            selectedId = results_[static_cast<size_t>(selection_)].id;
        }

        results_.clear();
        results_.reserve(libraryTitles_.size() + seerrResults_.size() + episodes_.size());
        results_.insert(results_.end(), libraryTitles_.begin(), libraryTitles_.end());
        for (const auto& item : seerrResults_) {
            if (!duplicatesLocalLibrary(item)) results_.push_back(jellyfinItemFromSeerrMedia(item));
        }
        results_.insert(results_.end(), episodes_.begin(), episodes_.end());

        if (!selectedId.empty()) {
            const auto selected = std::find_if(results_.begin(), results_.end(),
                                               [&](const JellyfinItem& item) { return item.id == selectedId; });
            if (selected != results_.end()) selection_ = static_cast<int>(std::distance(results_.begin(), selected));
        }
        if (results_.empty())
            selection_ = 0;
        else
            selection_ = std::clamp(selection_, 0, static_cast<int>(results_.size()) - 1);
        for (int row = 0; row < kRowCount; ++row) {
            firstVisible_[static_cast<size_t>(row)] =
                std::clamp(firstVisible_[static_cast<size_t>(row)], 0, std::max(0, rowItemCount(row) - 1));
        }
    }

    std::string query_;
    std::vector<JellyfinItem> libraryTitles_;
    std::vector<SeerrMediaItem> seerrResults_;
    std::vector<JellyfinItem> episodes_;
    std::vector<JellyfinItem> results_;
    int selection_ = 0;
    std::array<int, kRowCount> firstVisible_{0, 0, 0};
    bool keyboard_ = true;
    bool loading_ = false;
    bool seerrLoading_ = false;
    std::string seerrError_;
    bool debouncePending_ = false;
    Clock::time_point debounceDeadline_{};
    bool seerrDebouncePending_ = false;
    Clock::time_point seerrDebounceDeadline_{};
    std::string seerrQuery_;
};
