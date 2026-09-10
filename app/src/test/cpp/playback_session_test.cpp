#include "playback_session.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <vector>

int main() {
    using namespace std::chrono_literals;
    PlaybackSessionState state;
    assert(!state.mediaSegmentsRequested());
    assert(!state.fallbackAttempted());
    assert(state.zoomMode() == VideoZoomMode::Fit);

    state.begin(VideoZoomMode::Fill);
    assert(!state.mediaSegmentsRequested());
    assert(!state.fallbackAttempted());
    assert(state.zoomMode() == VideoZoomMode::Fill);

    const auto requestStart = PlaybackSessionState::Clock::now();
    assert(state.beginMediaSegmentsRequest(requestStart));
    assert(!state.beginMediaSegmentsRequest(requestStart));
    state.mediaSegmentsRequestFailed(requestStart);
    assert(!state.mediaSegmentsRequested());
    assert(!state.beginMediaSegmentsRequest(requestStart + 4999ms));
    assert(state.beginMediaSegmentsRequest(requestStart + 5000ms));
    state.setMediaSegments({
        JellyfinMediaSegment{"Intro", 10'000'000, 60'000'000},
        JellyfinMediaSegment{"Tiny", 70'000'000, 80'000'000},
    });
    assert(!state.activeSkippableSegment(5'000'000));
    const auto intro = state.activeSkippableSegment(20'000'000);
    assert(intro);
    assert(intro->type == "Intro");
    assert(!state.activeSkippableSegment(75'000'000));
    assert(state.mediaSegmentsRequested());

    state.markFallbackAttempted();
    assert(state.fallbackAttempted());
    state.resetMediaSegments();
    assert(!state.mediaSegmentsRequested());
    assert(state.mediaSegments().empty());

    state.reset();
    assert(!state.fallbackAttempted());
    assert(state.zoomMode() == VideoZoomMode::Fit);
    return 0;
}
