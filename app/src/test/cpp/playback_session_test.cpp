#include "playback_session.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    PlaybackSessionState state;
    assert(!state.mediaSegmentsRequested());
    assert(!state.fallbackAttempted());
    assert(state.zoomMode() == VideoZoomMode::Fit);

    state.begin(VideoZoomMode::Fill);
    assert(!state.mediaSegmentsRequested());
    assert(!state.fallbackAttempted());
    assert(state.zoomMode() == VideoZoomMode::Fill);

    assert(state.beginMediaSegmentsRequest());
    assert(!state.beginMediaSegmentsRequest());
    state.setMediaSegments({
        JellyfinMediaSegment{"Intro", 10'000'000, 60'000'000},
        JellyfinMediaSegment{"Tiny", 70'000'000, 80'000'000},
    });
    assert(!state.activeSkippableSegment(5'000'000));
    const auto intro = state.activeSkippableSegment(20'000'000);
    assert(intro);
    assert(intro->type == "Intro");
    assert(!state.activeSkippableSegment(75'000'000));

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
