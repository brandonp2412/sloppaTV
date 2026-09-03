#pragma once

#include <algorithm>

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
