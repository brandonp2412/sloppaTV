#include "player_screen.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    PlayerScreenState state;
    const auto now = PlayerScreenState::Clock::time_point{10s};

    assert(PlayerScreenState::controlCount() == 3);
    assert(!state.controlsActive());
    assert(state.controlSelection() == 0);
    assert(!state.overlayVisible(now));

    state.showControls(now);
    assert(state.controlsActive());
    assert(state.overlayVisible(now + 9s));
    assert(!state.overlayVisible(now + 10s));
    state.moveControl(1);
    state.moveControl(1);
    state.moveControl(1);
    assert(state.controlSelection() == 2);
    state.moveControl(-1);
    assert(state.controlSelection() == 1);
    assert(state.shouldDismissOnBack(now + 20s));
    state.dismissOverlay(now + 20s);
    assert(!state.controlsActive());
    assert(!state.shouldDismissOnBack(now + 20s));

    state.beginPlayback(12'000, 60'000);
    assert(state.positionMs() == 12'000);
    assert(state.durationMs() == 60'000);
    state.beginSeek(30'000, now);
    assert(state.positionMs() == 30'000);
    assert(state.pendingSeekTargetMs() == 30'000);
    assert(state.overlayVisible(now + 2s));
    state.applyObservedPosition(12'000, now + 200ms);
    assert(state.positionMs() == 30'000);
    assert(state.pendingSeekTargetMs() == 30'000);
    state.applyObservedPosition(30'500, now + 400ms);
    assert(state.positionMs() == 30'500);
    assert(state.pendingSeekTargetMs() == 30'000);
    state.applyObservedPosition(30'800, now + 600ms);
    assert(state.positionMs() == 30'800);
    assert(state.pendingSeekTargetMs() == -1);
    assert(state.recentSeekTargetMs() == 30'000);
    assert(!state.recentSeekAppearsFailed(31'000, now + 700ms));
    assert(state.recentSeekAppearsFailed(0, now + 700ms));

    state.beginSeek(40'000, now);
    state.applyObservedPosition(15'000, now + 800ms);
    assert(state.positionMs() == 40'000);
    assert(state.pendingSeekTargetMs() == 40'000);
    assert(state.recentSeekAppearsFailed(15'000, now + 800ms));
    assert(!state.pendingSeekAppearsFailed(15'000, now + 1499ms));
    assert(state.pendingSeekAppearsFailed(15'000, now + 1500ms));
    state.applyObservedPosition(15'000, now + 5s);
    assert(state.positionMs() == 15'000);
    assert(state.pendingSeekTargetMs() == -1);

    assert(!state.windowRestorePending());
    assert(!state.resumeOnFocusRequested());
    state.beginWindowRestore(true);
    assert(state.windowRestorePending());
    assert(state.resumeOnFocusRequested());
    assert(!state.takeResumeOnFocus());
    state.completeWindowRestore();
    assert(!state.windowRestorePending());
    assert(!state.resumeOnFocusRequested());
    state.requestResumeOnFocus();
    assert(state.takeResumeOnFocus());
    assert(!state.resumeOnFocusRequested());

    state.resetSession();
    assert(!state.controlsActive());
    assert(state.controlSelection() == 0);
    assert(state.positionMs() == 0);
    assert(state.durationMs() == 0);
    assert(state.pendingSeekTargetMs() == -1);
    assert(state.recentSeekTargetMs() == -1);
    assert(!state.windowRestorePending());
    assert(!state.resumeOnFocusRequested());

    return 0;
}
