#include "playback_telemetry.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    PlaybackTelemetryState state;
    const auto now = PlaybackTelemetryState::TimePoint{100s};

    assert(!state.playbackStartReported());
    assert(state.shouldReadPlayback(now, false));
    assert(state.shouldProbeDuration(now, false));
    assert(state.progressReportDue(now));

    state.beginPlayback(now);
    assert(!state.playbackStartReported());
    assert(!state.progressReportDue(now + 9s));
    assert(state.progressReportDue(now + 10s));
    assert(state.markPlaybackStartReported());
    assert(!state.markPlaybackStartReported());

    state.markPlaybackRead(now);
    assert(!state.shouldReadPlayback(now + 249ms, false));
    assert(state.shouldReadPlayback(now + 250ms, false));
    assert(state.shouldReadPlayback(now + 1ms, true));

    state.markDurationProbe(now);
    assert(!state.shouldProbeDuration(now + 1999ms, false));
    assert(state.shouldProbeDuration(now + 2s, false));
    assert(state.shouldProbeDuration(now + 1ms, true));

    state.markProgressReport(now + 10s);
    assert(!state.progressReportDue(now + 19s));
    assert(state.progressReportDue(now + 20s));

    state.clearPlaybackStartReported();
    state.resetReadIntervals();
    assert(!state.playbackStartReported());
    assert(state.shouldReadPlayback(now, false));
    assert(state.shouldProbeDuration(now, false));

    state.reset();
    assert(!state.playbackStartReported());
    assert(state.progressReportDue(now));
    return 0;
}
