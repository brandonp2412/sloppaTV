#include "queue_overlay_screen.hpp"

#include <cassert>

int main() {
    assert(queueOverlayRemainingCount(0, 5) == 5);
    assert(queueOverlayRemainingCount(2, 5) == 3);
    assert(queueOverlayRemainingCount(9, 5) == 1);

    assert(queueOverlayFirstVisible(1, 0, 8) == 0);
    assert(queueOverlayFirstVisible(4, 0, 8) == 2);
    assert(queueOverlayFirstVisible(7, 0, 8) == 3);
    assert(queueOverlayFirstVisible(6, 3, 8) == 3);

    const auto current = queueOverlayMarker(2, 2);
    assert(current.label == "Current");
    assert(current.width == 122.0f);
    assert(current.current);

    const auto next = queueOverlayMarker(3, 2);
    assert(next.label == "Next");
    assert(next.width == 88.0f);
    assert(!next.current);

    const auto later = queueOverlayMarker(5, 2);
    assert(later.label == "4");
    assert(later.width == 58.0f);

    const auto labels = queueOverlayActionLabels(QueueRepeatMode::All);
    assert(labels[0] == "Play now");
    assert(labels[4] == "Remove");
    assert(labels[6] == "Repeat ALL");

    assert(queueOverlayActionEnabled(0, 2, 0, 4));
    assert(!queueOverlayActionEnabled(0, 0, 0, 4));
    assert(queueOverlayActionEnabled(1, 3, 0, 4));
    assert(!queueOverlayActionEnabled(1, 1, 0, 4));
    assert(queueOverlayActionEnabled(2, 3, 0, 4));
    assert(queueOverlayActionEnabled(3, 2, 0, 4));
    assert(queueOverlayActionEnabled(4, 1, 0, 4));
    assert(queueOverlayActionEnabled(5, 0, 0, 4));
    assert(queueOverlayActionEnabled(6, 0, 0, 1));
    return 0;
}
