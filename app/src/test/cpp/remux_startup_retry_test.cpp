#include "remux_startup_retry.hpp"

#include <cassert>
#include <string>

int main() {
    int calls = 0;
    int notifications = 0;
    bool active = true;
    auto cancelled = awaitRemuxStartup(
        [&] {
            ++calls;
            ApiValueResult<int> result;
            result.pendingRemuxPlayback = PendingRemuxPlayback{.sourceId = "pending", .prefetch = {}, .retryAfterSeconds = 2};
            return result;
        },
        [&] { return active; },
        [&](const PendingRemuxPlayback& pending) {
            assert(pending.sourceId == "pending");
            ++notifications;
            active = false;
        });
    assert(!cancelled.ok && calls == 1 && notifications == 1);

    calls = 0;
    auto ready = awaitRemuxStartup(
        [&] {
            ++calls;
            ApiValueResult<int> result;
            result.ok = true;
            result.value = 42;
            return result;
        },
        [] { return true; },
        [&](const PendingRemuxPlayback&) { ++notifications; });
    assert(ready.ok && ready.value == 42 && calls == 1 && notifications == 1);
}
