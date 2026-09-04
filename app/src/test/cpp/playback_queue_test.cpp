#include "playback_queue.hpp"

#include <cassert>
#include <random>

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

    assert(queueAutoplayAdvanceIndex(0, 3, true) == 1);
    assert(queueAutoplayAdvanceIndex(1, 3, true) == 2);
    assert(queueAutoplayAdvanceIndex(2, 3, true) == -1);
    assert(queueAutoplayAdvanceIndex(0, 3, false) == -1);
    assert(queueAutoplayAdvanceIndex(-1, 3, true) == -1);

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

    PlaybackQueueState state;
    std::vector<JellyfinItem> items(4);
    items[0].id = "a";
    items[1].id = "b";
    items[2].id = "c";
    items[3].id = "d";
    state.replace(std::move(items), 0);
    assert(state.size() == 4);
    assert(state.currentIndex() == 0);
    assert(state.selection() == 1);
    assert(state.itemMatches(2, "c"));
    assert(state.findItemIndex("d") == 3);
    assert(state.openOverlay());
    state.moveSelection(2);
    assert(state.selection() == 3);
    state.moveAction(4);
    assert(state.actionSelection() == 4);
    assert(state.moveItem(3, 1));
    assert(state.selection() == 1);
    assert(state.itemMatches(1, "d"));
    state.moveSelection(1);
    assert(state.selection() == 2);
    assert(state.removeSelected());
    assert(state.size() == 3);
    assert(state.selection() == 2);
    state.cycleRepeatMode();
    assert(state.repeatMode() == QueueRepeatMode::One);
    assert(state.nextIndex(false) == 0);
    state.cycleRepeatMode();
    assert(state.repeatMode() == QueueRepeatMode::All);
    assert(state.setCurrentIndex(2));
    assert(state.nextIndex(false) == 0);

    JellyfinItem expectedNext;
    expectedNext.id = state.items().front().id;
    assert(state.autoplayAdvanceIndex(expectedNext) == -1);
    assert(state.setCurrentIndex(0));
    expectedNext.id = state.items()[1].id;
    assert(state.autoplayAdvanceIndex(expectedNext) == 1);

    std::mt19937 generator(1234);
    assert(state.shuffleRemaining(generator));
    assert(state.currentIndex() == 0);
    assert(state.selection() == 1);
    state.closeOverlay();
    assert(!state.overlayActive());
    state.reset();
    assert(state.empty());
    assert(state.currentIndex() == -1);
    return 0;
}
