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

    assert(queueShuffleBegin(-1, 4) == 0);
    assert(queueShuffleBegin(0, 4) == 1);
    assert(queueShuffleBegin(3, 4) == 4);
    assert(queueCanShuffle(0, 4));
    assert(!queueCanShuffle(2, 4));

    assert(nextQueueRepeatMode(QueueRepeatMode::Off) == QueueRepeatMode::One);
    assert(nextQueueRepeatMode(QueueRepeatMode::One) == QueueRepeatMode::All);
    assert(nextQueueRepeatMode(QueueRepeatMode::All) == QueueRepeatMode::Off);

    assert(queueNextIndex(0, 3, QueueRepeatMode::Off, false) == 1);
    assert(queueNextIndex(0, 3, QueueRepeatMode::One, false) == 0);
    assert(queueNextIndex(2, 3, QueueRepeatMode::Off, false) == -1);
    assert(queueNextIndex(2, 3, QueueRepeatMode::All, false) == 0);
    assert(queueNextIndex(1, 3, QueueRepeatMode::One, true) == 2);
    assert(queueNextIndex(2, 3, QueueRepeatMode::One, true) == -1);
    assert(queueNextIndex(2, 3, QueueRepeatMode::All, true) == 0);
    return 0;
}
