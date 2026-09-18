#pragma once

#include "playback_queue.hpp"

#include <algorithm>
#include <array>
#include <string>

struct QueueOverlayMarker {
    std::string label;
    float width = 58.0f;
    bool current = false;
};

inline int queueOverlayFirstVisible(int selection, int current, int size, int visibleRows = 5) {
    if (size <= 0 || visibleRows <= 0) return 0;
    current = std::clamp(current, 0, size - 1);
    selection = std::clamp(selection, current, size - 1);
    return std::clamp(selection - 2, current, std::max(current, size - visibleRows));
}

inline int queueOverlayRemainingCount(int current, int size) {
    if (size <= 0) return 0;
    return size - std::clamp(current, 0, size - 1);
}

inline QueueOverlayMarker queueOverlayMarker(int index, int current) {
    if (index == current) return {.label = "Current", .width = 122.0f, .current = true};
    if (index == current + 1) return {.label = "Next", .width = 88.0f, .current = false};
    return {.label = std::to_string(index - current + 1), .width = 58.0f, .current = false};
}

inline std::array<std::string, 7> queueOverlayActionLabels(QueueRepeatMode repeatMode) {
    return {
        "Play now",
        "Play next",
        "Move up",
        "Move down",
        "Remove",
        "Shuffle",
        std::string("Repeat ") + queueRepeatModeName(repeatMode),
    };
}

inline bool queueOverlayActionEnabled(int action, int selection, int current, int size) {
    switch (action) {
    case 0:
        return queueCanPlayNow(selection, current, size);
    case 1:
        return queueCanPlayNext(selection, current, size);
    case 2:
        return queueCanMoveUp(selection, current, size);
    case 3:
        return queueCanMoveDown(selection, current, size);
    case 4:
        return queueCanRemove(selection, current, size);
    case 5:
        return queueCanShuffle(current, size);
    default:
        return true;
    }
}
