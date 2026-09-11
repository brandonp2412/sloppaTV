#include "media_session_policy.hpp"

#include <cassert>
#include <optional>

int main() {
    assert(!mediaSessionNeedsScreenOn(MediaSessionState::Stopped));
    assert(!mediaSessionNeedsScreenOn(MediaSessionState::Paused));
    assert(mediaSessionNeedsScreenOn(MediaSessionState::Buffering));
    assert(mediaSessionNeedsScreenOn(MediaSessionState::Playing));

    std::optional<bool> applied;
    assert(shouldUpdateKeepScreenOn(applied, true));
    applied = keepScreenOnAfterAttempt(applied, true, false);
    assert(!applied.has_value());
    assert(shouldUpdateKeepScreenOn(applied, true));

    applied = keepScreenOnAfterAttempt(applied, true, true);
    assert(applied == std::optional<bool>{true});
    assert(!shouldUpdateKeepScreenOn(applied, true));
    assert(shouldUpdateKeepScreenOn(applied, false));

    applied = keepScreenOnAfterAttempt(applied, false, false);
    assert(applied == std::optional<bool>{true});
    assert(shouldUpdateKeepScreenOn(applied, false));

    applied = keepScreenOnAfterAttempt(applied, false, true);
    assert(applied == std::optional<bool>{false});
    assert(!shouldUpdateKeepScreenOn(applied, false));
    return 0;
}
