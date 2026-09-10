#include "playback_continuation.hpp"

#include <cassert>
#include <chrono>

int main() {
    PlaybackContinuationState state;
    assert(!state.nextItem());
    assert(!state.nextEpisodeRequested());
    assert(state.autoplayChainCount() == 0);
    assert(!state.stillWatchingPrompt());

    const auto now = std::chrono::steady_clock::time_point(std::chrono::seconds(100));
    assert(state.beginNextEpisodeRequest(now));
    assert(!state.beginNextEpisodeRequest(now));
    JellyfinItem next;
    next.id = "episode-2";
    state.setNextItem(next);
    assert(state.nextItem());
    assert(state.nextItem()->id == "episode-2");

    state.incrementAutoplayChain();
    state.incrementAutoplayChain();
    state.setStillWatchingPrompt(true);
    assert(state.autoplayChainCount() == 2);
    assert(state.stillWatchingPrompt());

    state.clearNextEpisode();
    assert(!state.nextEpisodeRequested());
    assert(!state.nextItem());

    assert(state.beginNextEpisodeRequest(now));
    state.failNextEpisodeRequest(now, std::chrono::seconds(10));
    assert(!state.nextEpisodeRequested());
    assert(!state.beginNextEpisodeRequest(now + std::chrono::seconds(9)));
    assert(state.beginNextEpisodeRequest(now + std::chrono::seconds(10)));
    state.clearNextEpisode();
    state.resetAutoplayChain();
    state.setStillWatchingPrompt(false);
    assert(state.autoplayChainCount() == 0);
    assert(!state.stillWatchingPrompt());

    state.markNextEpisodeRequested();
    state.setNextItem(next);
    state.incrementAutoplayChain();
    state.setStillWatchingPrompt(true);
    state.reset();
    assert(!state.nextEpisodeRequested());
    assert(!state.nextItem());
    assert(state.autoplayChainCount() == 0);
    assert(!state.stillWatchingPrompt());
    return 0;
}
