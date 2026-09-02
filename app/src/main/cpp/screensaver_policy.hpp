#pragma once

#include <algorithm>
#include <cstdint>

constexpr int normalizedScreensaverMinutes(int minutes) {
    if (minutes <= 0) return 0;
    if (minutes <= 5) return 5;
    if (minutes <= 10) return 10;
    if (minutes <= 20) return 20;
    return 30;
}

constexpr int64_t screensaverDelayMs(int minutes) {
    return static_cast<int64_t>(normalizedScreensaverMinutes(minutes)) * 60'000;
}

constexpr bool shouldActivateScreensaver(
    int minutes,
    int64_t idleMs,
    bool playerScreen,
    bool busy
) {
    const int64_t delay = screensaverDelayMs(minutes);
    return delay > 0 && !playerScreen && !busy && idleMs >= delay;
}

constexpr int screensaverPositionSlot(int64_t elapsedSeconds) {
    return static_cast<int>((std::max<int64_t>(0, elapsedSeconds) / 30) % 8);
}
