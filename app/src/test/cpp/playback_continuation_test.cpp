#include "playback_continuation.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    PlaybackContinuationState state;
    assert(!state.nextItem());
    assert(!state.nextEpisodeRequested());
    assert(state.autoplayChainCount() == 0);
    assert(!state.stillWatchingPrompt());

    const auto requestStart = PlaybackContinuationState::Clock::now();
    assert(state.beginNextEpisodeRequest(requestStart));
    assert(!state.beginNextEpisodeRequest(requestStart));
    state.nextEpisodeRequestFailed(requestStart);
    assert(!state.nextEpisodeRequested());
    assert(!state.beginNextEpisodeRequest(requestStart + 9999ms));
    assert(state.beginNextEpisodeRequest(requestStart + 10000ms));
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
