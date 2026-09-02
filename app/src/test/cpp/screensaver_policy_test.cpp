#include "screensaver_policy.hpp"

#include <cassert>

int main() {
    assert(normalizedScreensaverMinutes(0) == 0);
    assert(normalizedScreensaverMinutes(4) == 5);
    assert(normalizedScreensaverMinutes(8) == 10);
    assert(normalizedScreensaverMinutes(18) == 20);
    assert(normalizedScreensaverMinutes(30) == 30);

    assert(!shouldActivateScreensaver(0, 99999999, false, false));
    assert(!shouldActivateScreensaver(5, 299999, false, false));
    assert(shouldActivateScreensaver(5, 300000, false, false));
    assert(!shouldActivateScreensaver(5, 300000, true, false));
    assert(!shouldActivateScreensaver(5, 300000, false, true));

    assert(screensaverPositionSlot(0) == 0);
    assert(screensaverPositionSlot(29) == 0);
    assert(screensaverPositionSlot(30) == 1);
    assert(screensaverPositionSlot(240) == 0);
    return 0;
}
