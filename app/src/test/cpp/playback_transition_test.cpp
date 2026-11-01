#include "playback_transition.hpp"

#include <cassert>

int main() {
    PlaybackTransitionState state;
    assert(!state.hasPending());
    assert(!state.loading());
    assert(!state.fallbackResolving());
    assert(!state.pauseAfterRestart());

    PendingPlaybackTransition episode;
    episode.item.type = "Episode";
    // An explicitly chosen episode must retain its source-loading controls.
    assert(!isEpisodeAdvance(true, episode));
    episode.automaticEpisodeAdvance = true;
    assert(isEpisodeAdvance(true, episode));
    assert(!isEpisodeAdvance(false, episode));
    episode.streamRestart = true;
    assert(!isEpisodeAdvance(true, episode));
    episode.streamRestart = false;
    episode.item.type = "Movie";
    assert(!isEpisodeAdvance(true, episode));

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
    assert(!pending->automaticEpisodeAdvance);

    state.stage(target, item, false, false, -1, true);
    pending = state.take();
    assert(pending && pending->automaticEpisodeAdvance);

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
