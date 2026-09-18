#pragma once

#include "playback_queue.hpp"
#include "screen_navigation_key.hpp"

enum class QueueNavigationActionType {
    None,
    PlayIndex,
    QueueChanged,
    Shuffle,
};

struct QueueNavigationAction {
    QueueNavigationActionType type = QueueNavigationActionType::None;
    int index = -1;
};

class QueueNavigationController {
public:
    [[nodiscard]] static QueueNavigationAction handle(PlaybackQueueState& state, ScreenNavigationKey key);
};
