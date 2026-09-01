#pragma once

#include <algorithm>

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
