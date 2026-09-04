#pragma once

#include <algorithm>
#include <string>
#include <vector>

enum class ArtworkKind {
    Primary,
    Thumb,
    Backdrop,
    None,
};

struct ArtworkReference {
    std::string itemId;
    std::string tag;
    ArtworkKind kind = ArtworkKind::None;
};

constexpr int homeFirstVisibleRow(int currentFirst, int focusedRow, int totalRows, int visibleRows = 2) {
    if (totalRows <= 0 || visibleRows <= 0) return 0;
    const int maxFirst = std::max(0, totalRows - visibleRows);
    int first = std::clamp(currentFirst, 0, maxFirst);
    if (focusedRow < 0) return first;
    const int focused = std::clamp(focusedRow, 0, totalRows - 1);
    if (focused < first) first = focused;
    else if (focused >= first + visibleRows) first = focused - visibleRows + 1;
    return std::clamp(first, 0, maxFirst);
}

constexpr ArtworkKind homeImageKind(bool hasPrimary, bool hasThumb, bool hasBackdrop) {
    if (hasPrimary) return ArtworkKind::Primary;
    if (hasThumb) return ArtworkKind::Thumb;
    if (hasBackdrop) return ArtworkKind::Backdrop;
    return ArtworkKind::None;
}

inline bool preferHomeLandscapeArtwork(const std::string& itemType) {
    return itemType != "UserView" && itemType != "CollectionFolder" && itemType != "Folder";
}

inline ArtworkReference homeArtworkReference(
    const std::string& itemId,
    const std::string& primaryTag,
    const std::string& seriesId,
    const std::string& seriesPrimaryTag,
    bool preferSeries,
    const std::string& thumbTag,
    const std::string& backdropTag,
    const std::string& backdropItemId
) {
    const std::string backdropOwner = backdropItemId.empty() ? itemId : backdropItemId;
    const bool ownBackdrop = !backdropTag.empty() && backdropOwner == itemId;
    if (preferSeries && !thumbTag.empty()) return {itemId, thumbTag, ArtworkKind::Thumb};
    if (preferSeries && ownBackdrop) return {itemId, backdropTag, ArtworkKind::Backdrop};
    if (preferSeries && !primaryTag.empty()) return {itemId, primaryTag, ArtworkKind::Primary};
    if (preferSeries && !backdropTag.empty()) return {backdropOwner, backdropTag, ArtworkKind::Backdrop};
    if (preferSeries && !seriesId.empty() && !seriesPrimaryTag.empty()) {
        return {seriesId, seriesPrimaryTag, ArtworkKind::Primary};
    }
    const ArtworkKind kind = homeImageKind(!primaryTag.empty(), !thumbTag.empty(), !backdropTag.empty());
    const std::string& tag = kind == ArtworkKind::Primary ? primaryTag
        : (kind == ArtworkKind::Thumb ? thumbTag : backdropTag);
    return {kind == ArtworkKind::Backdrop ? backdropOwner : itemId, tag, kind};
}

class HomeScreenState {
public:
    void reset() {
        selections_.clear();
        row_ = 0;
        firstVisibleRow_ = 0;
        navIndex_ = 1;
        cancelCenterPress();
    }

    void focusToolbar(int navIndex) {
        row_ = -1;
        navIndex_ = std::clamp(navIndex, 0, 3);
    }

    void moveToolbar(int direction) {
        navIndex_ = std::clamp(navIndex_ + direction, 0, 3);
    }

    void moveRow(int direction, int totalRows) {
        if (row_ < 0 || totalRows <= 0) return;
        if (direction < 0) row_ = row_ == 0 ? -1 : row_ - 1;
        else if (direction > 0 && row_ + 1 < totalRows) ++row_;
        updateViewport(totalRows);
    }

    void setRow(int row) { row_ = row; }
    void setFirstVisibleRow(int row) { firstVisibleRow_ = std::max(0, row); }
    void setNavIndex(int index) { navIndex_ = std::clamp(index, 0, 3); }

    void updateViewport(int totalRows, int visibleRows = 2) {
        firstVisibleRow_ = homeFirstVisibleRow(firstVisibleRow_, row_, totalRows, visibleRows);
    }

    void setSelections(std::vector<int> selections) { selections_ = std::move(selections); }
    void clearSelections() { selections_.clear(); }
    void appendSelection(int selection) { selections_.push_back(selection); }

    void clampSelections(const std::vector<int>& itemCounts) {
        const size_t count = std::min(selections_.size(), itemCounts.size());
        for (size_t row = 0; row < count; ++row) {
            selections_[row] = itemCounts[row] <= 0 ? 0 : std::clamp(selections_[row], 0, itemCounts[row] - 1);
        }
    }

    [[nodiscard]] int selection(int row, int itemCount) const {
        if (row < 0 || row >= static_cast<int>(selections_.size()) || itemCount <= 0) return 0;
        return std::clamp(selections_[static_cast<size_t>(row)], 0, itemCount - 1);
    }

    void setSelection(int row, int selection, int itemCount) {
        if (row < 0 || row >= static_cast<int>(selections_.size())) return;
        selections_[static_cast<size_t>(row)] = itemCount <= 0 ? 0 : std::clamp(selection, 0, itemCount - 1);
    }

    void moveSelection(int row, int direction, int itemCount) {
        setSelection(row, selection(row, itemCount) + direction, itemCount);
    }

    [[nodiscard]] int row() const { return row_; }
    [[nodiscard]] int firstVisibleRow() const { return firstVisibleRow_; }
    [[nodiscard]] int navIndex() const { return navIndex_; }
    [[nodiscard]] const std::vector<int>& selections() const { return selections_; }
    [[nodiscard]] size_t selectionCount() const { return selections_.size(); }

    void beginCenterPress() {
        centerPending_ = true;
        centerLongPressed_ = false;
    }

    [[nodiscard]] bool centerPending() const { return centerPending_; }
    [[nodiscard]] bool centerLongPressed() const { return centerLongPressed_; }

    void markCenterLongPressed() { centerLongPressed_ = true; }

    bool consumeCenterRelease(bool onHomeScreen) {
        const bool activate = centerPending_ && !centerLongPressed_ && onHomeScreen;
        cancelCenterPress();
        return activate;
    }

private:
    void cancelCenterPress() {
        centerPending_ = false;
        centerLongPressed_ = false;
    }

    std::vector<int> selections_;
    int row_ = 0;
    int firstVisibleRow_ = 0;
    int navIndex_ = 1;
    bool centerPending_ = false;
    bool centerLongPressed_ = false;
};
