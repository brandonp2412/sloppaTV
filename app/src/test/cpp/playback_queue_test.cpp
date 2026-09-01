#include "playback_queue.hpp"

#include <cassert>

int main() {
    assert(sameEpisodeSlot(1, 1, 1, 1));
    assert(!sameEpisodeSlot(1, 1, 1, 2));
    assert(!sameEpisodeSlot(0, 1, 0, 1));
    assert(preferAvailableDuplicate(false, true));
    assert(!preferAvailableDuplicate(true, false));
    assert(!preferAvailableDuplicate(true, true));
    assert(!preferAvailableDuplicate(false, false));

    assert(queueDefaultSelection(-1, 0) == 0);
    assert(queueDefaultSelection(0, 4) == 1);
    assert(queueDefaultSelection(3, 4) == 3);

    assert(!queueCanPlayNow(0, 0, 4));
    assert(queueCanPlayNow(2, 0, 4));

    assert(!queueCanPlayNext(1, 0, 4));
    assert(queueCanPlayNext(3, 0, 4));

    assert(!queueCanMoveUp(1, 0, 4));
    assert(queueCanMoveUp(2, 0, 4));
    assert(!queueCanMoveDown(0, 0, 4));
    assert(queueCanMoveDown(1, 0, 4));
    assert(!queueCanMoveDown(3, 0, 4));

    assert(!queueCanRemove(0, 0, 4));
    assert(queueCanRemove(1, 0, 4));
    assert(!queueCanRemove(4, 0, 4));
    return 0;
}
