#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

enum class QueueRepeatMode {
    Off,
    One,
    All,
};

constexpr bool sameEpisodeSlot(int leftSeason, int leftEpisode, int rightSeason, int rightEpisode) {
    return leftSeason > 0 && leftEpisode > 0
        && leftSeason == rightSeason
        && leftEpisode == rightEpisode;
}

constexpr bool preferAvailableDuplicate(bool selectedAvailable, bool candidateAvailable) {
    return !selectedAvailable && candidateAvailable;
}

constexpr int queueDefaultSelection(int currentIndex, int size) {
    if (size <= 0) return 0;
    const int current = std::clamp(currentIndex, 0, size - 1);
    return current + 1 < size ? current + 1 : current;
}

constexpr bool queueCanPlayNow(int selectedIndex, int currentIndex, int size) {
    return selectedIndex >= 0 && selectedIndex < size && selectedIndex != currentIndex;
}

constexpr bool queueCanPlayNext(int selectedIndex, int currentIndex, int size) {
    return selectedIndex > currentIndex + 1 && selectedIndex < size;
}

constexpr bool queueCanMoveUp(int selectedIndex, int currentIndex, int size) {
    return selectedIndex > currentIndex + 1 && selectedIndex < size;
}

constexpr bool queueCanMoveDown(int selectedIndex, int currentIndex, int size) {
    return selectedIndex > currentIndex && selectedIndex >= 0 && selectedIndex + 1 < size;
}

constexpr bool queueCanRemove(int selectedIndex, int currentIndex, int size) {
    return selectedIndex > currentIndex && selectedIndex < size;
}

constexpr int queueShuffleBegin(int currentIndex, int size) {
    if (size <= 0) return 0;
    return std::clamp(currentIndex + 1, 0, size);
}

constexpr bool queueCanShuffle(int currentIndex, int size) {
    return size - queueShuffleBegin(currentIndex, size) > 1;
}

constexpr int queueAutoplayAdvanceIndex(int currentIndex, int size, bool nextItemMatches) {
    if (!nextItemMatches || currentIndex < 0 || currentIndex + 1 >= size) return -1;
    return currentIndex + 1;
}

constexpr QueueRepeatMode nextQueueRepeatMode(QueueRepeatMode mode) {
    return mode == QueueRepeatMode::Off ? QueueRepeatMode::One
        : (mode == QueueRepeatMode::One ? QueueRepeatMode::All : QueueRepeatMode::Off);
}

constexpr const char* queueRepeatModeName(QueueRepeatMode mode) {
    return mode == QueueRepeatMode::One ? "ONE"
        : (mode == QueueRepeatMode::All ? "ALL" : "OFF");
}

constexpr int queueNextIndex(int currentIndex, int size, QueueRepeatMode repeatMode, bool manualAdvance) {
    if (size <= 0 || currentIndex < 0 || currentIndex >= size) return -1;
    if (!manualAdvance && repeatMode == QueueRepeatMode::One) return currentIndex;
    if (currentIndex + 1 < size) return currentIndex + 1;
    if (repeatMode == QueueRepeatMode::All) return 0;
    return -1;
}

class PlaybackQueueState {
public:
    void reset() {
        items_.clear();
        currentIndex_ = -1;
        selection_ = 0;
        actionSelection_ = 0;
        repeatMode_ = QueueRepeatMode::Off;
        overlayActive_ = false;
    }

    [[nodiscard]] bool empty() const { return items_.empty(); }
    [[nodiscard]] int size() const { return static_cast<int>(items_.size()); }
    [[nodiscard]] const std::vector<JellyfinItem>& items() const { return items_; }
    [[nodiscard]] std::vector<JellyfinItem>& items() { return items_; }

    [[nodiscard]] int currentIndex() const { return currentIndex_; }
    bool setCurrentIndex(int index) {
        if (index < -1 || index >= size()) return false;
        currentIndex_ = index;
        clampSelection();
        return true;
    }

    [[nodiscard]] const JellyfinItem* itemAt(int index) const {
        if (index < 0 || index >= size()) return nullptr;
        return &items_[static_cast<size_t>(index)];
    }

    [[nodiscard]] JellyfinItem* itemAt(int index) {
        if (index < 0 || index >= size()) return nullptr;
        return &items_[static_cast<size_t>(index)];
    }

    bool setItemAt(int index, JellyfinItem item) {
        auto* target = itemAt(index);
        if (!target) return false;
        *target = std::move(item);
        return true;
    }

    [[nodiscard]] bool itemMatches(int index, const std::string& itemId) const {
        const auto* item = itemAt(index);
        return item && item->id == itemId;
    }

    [[nodiscard]] int findItemIndex(const std::string& itemId) const {
        const auto item = std::find_if(items_.begin(), items_.end(), [&](const JellyfinItem& candidate) {
            return candidate.id == itemId;
        });
        return item == items_.end() ? -1 : static_cast<int>(std::distance(items_.begin(), item));
    }

    void replace(std::vector<JellyfinItem> items, int currentIndex) {
        items_ = std::move(items);
        currentIndex_ = items_.empty() ? -1 : std::clamp(currentIndex, 0, size() - 1);
        repeatMode_ = QueueRepeatMode::Off;
        selection_ = queueDefaultSelection(currentIndex_, size());
        actionSelection_ = 0;
        overlayActive_ = false;
    }

    [[nodiscard]] QueueRepeatMode repeatMode() const { return repeatMode_; }
    void setRepeatMode(QueueRepeatMode mode) { repeatMode_ = mode; }
    void cycleRepeatMode() { repeatMode_ = nextQueueRepeatMode(repeatMode_); }

    [[nodiscard]] int nextIndex(bool manualAdvance) const {
        return queueNextIndex(currentIndex_, size(), repeatMode_, manualAdvance);
    }

    [[nodiscard]] bool overlayActive() const { return overlayActive_; }
    bool openOverlay() {
        if (items_.empty()) {
            overlayActive_ = false;
            return false;
        }
        overlayActive_ = true;
        selection_ = queueDefaultSelection(currentIndex_, size());
        actionSelection_ = 0;
        return true;
    }
    void closeOverlay() { overlayActive_ = false; }

    [[nodiscard]] int selection() const { return selection_; }
    void setSelection(int selection) {
        selection_ = selection;
        clampSelection();
    }
    void moveSelection(int direction) {
        if (items_.empty()) {
            selection_ = 0;
            return;
        }
        const int minimum = std::clamp(currentIndex_, 0, size() - 1);
        selection_ = std::clamp(selection_ + direction, minimum, size() - 1);
    }

    [[nodiscard]] int actionSelection() const { return actionSelection_; }
    void moveAction(int direction, int actionCount = 7) {
        actionSelection_ = actionCount <= 0
            ? 0
            : std::clamp(actionSelection_ + direction, 0, actionCount - 1);
    }

    bool moveItem(int from, int to) {
        const int count = size();
        if (from < 0 || from >= count || to < 0 || to >= count || from == to) return false;
        JellyfinItem item = std::move(items_[static_cast<size_t>(from)]);
        items_.erase(items_.begin() + from);
        items_.insert(items_.begin() + to, std::move(item));
        selection_ = to;
        return true;
    }

    bool removeSelected() {
        if (!queueCanRemove(selection_, currentIndex_, size())) return false;
        items_.erase(items_.begin() + selection_);
        selection_ = std::min(selection_, size() - 1);
        clampSelection();
        return true;
    }

    template <typename Generator>
    bool shuffleRemaining(Generator& generator) {
        const int begin = queueShuffleBegin(currentIndex_, size());
        if (!queueCanShuffle(currentIndex_, size())) return false;
        std::shuffle(items_.begin() + begin, items_.end(), generator);
        selection_ = queueDefaultSelection(currentIndex_, size());
        return true;
    }

    [[nodiscard]] int autoplayAdvanceIndex(const JellyfinItem& nextItem) const {
        const bool matches = currentIndex_ >= 0
            && currentIndex_ + 1 < size()
            && itemMatches(currentIndex_ + 1, nextItem.id);
        return queueAutoplayAdvanceIndex(currentIndex_, size(), matches);
    }

private:
    void clampSelection() {
        if (items_.empty()) {
            selection_ = 0;
            return;
        }
        const int minimum = std::clamp(currentIndex_, 0, size() - 1);
        selection_ = std::clamp(selection_, minimum, size() - 1);
    }

    std::vector<JellyfinItem> items_;
    int currentIndex_ = -1;
    int selection_ = 0;
    int actionSelection_ = 0;
    QueueRepeatMode repeatMode_ = QueueRepeatMode::Off;
    bool overlayActive_ = false;
};
