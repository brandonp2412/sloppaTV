#include "clock_text.hpp"

#include <cassert>

int main() {
    const std::time_t instant = 1788883200;
    LocalClockTextCache cache;
    const std::string first = cache.text(instant, false);
    assert(cache.text(instant + 20, false) == first);
    assert(cache.text(instant, true) == formatLocalClock(instant, true));
    assert(cache.text(instant + 60, false) == formatLocalClock(instant + 60, false));
    return 0;
}
