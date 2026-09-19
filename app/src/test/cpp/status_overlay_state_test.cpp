#include "status_overlay_renderer.hpp"

#include <cassert>
#include <chrono>

using namespace std::chrono_literals;

int main() {
    using Clock = std::chrono::steady_clock;
    StatusOverlayState state;
    const auto start = Clock::time_point{} + 10s;

    state.showNotice("SAVED", 3s, false, start);
    auto visible = state.renderState({}, false, false, start + 1s);
    assert(visible.noticeVisible);
    assert(visible.notice == "SAVED");
    assert(state.wakeDeadline() == start + 3s);

    auto expired = state.renderState({}, false, false, start + 4s);
    assert(!expired.noticeVisible);

    auto error = state.renderState("FAILED", true, true, start + 5s);
    assert(error.loading);
    assert(error.playerScreen);
    assert(error.errorVisible);
    assert(error.error == "FAILED");
    assert(state.wakeDeadline() == start + 11s);

    auto hiddenError = state.renderState("FAILED", false, false, start + 12s);
    assert(!hiddenError.errorVisible);

    auto cleared = state.renderState({}, false, false, start + 13s);
    assert(!cleared.errorVisible);
    assert(state.wakeDeadline() == Clock::time_point{});

    state.showNotice("PINNED", 1s, true, start + 14s);
    auto persistent = state.renderState({}, false, false, start + 30s);
    assert(persistent.noticeVisible);
    assert(state.wakeDeadline() == Clock::time_point{});
    state.clearNotice();
    assert(!state.renderState({}, false, false, start + 31s).noticeVisible);

    return 0;
}
