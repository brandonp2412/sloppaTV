#include "playback_transition.hpp"

#include <cassert>

int main() {
    PlaybackTransitionState state;
    assert(!state.hasPending());
    assert(!state.loading());
    assert(!state.fallbackResolving());
    assert(!state.pauseAfterRestart());

    PlaybackTarget target;
    target.url = "https://example.test/video";
    JellyfinItem item;
    item.id = "item-1";
    state.stage(target, item, true, true, 4);
    assert(state.hasPending());

    auto pending = state.take();
    assert(pending.has_value());
    assert(!state.hasPending());
    assert(pending->target.url == target.url);
    assert(pending->item.id == item.id);
    assert(pending->streamRestart);
    assert(pending->restartPaused);
    assert(pending->audioStreamIndex == 4);

    state.setLoading(true);
    state.setFallbackResolving(true);
    state.setPauseAfterRestart(true);
    assert(state.loading());
    assert(state.fallbackResolving());
    assert(state.pauseAfterRestart());
    state.clearPauseAfterRestart();
    assert(!state.pauseAfterRestart());

    state.stage({}, item);
    state.reset();
    assert(!state.hasPending());
    assert(!state.loading());
    assert(!state.fallbackResolving());
    assert(!state.pauseAfterRestart());

    return 0;
}
