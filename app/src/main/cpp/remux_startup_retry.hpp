#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>

struct RemuxPlaybackPendingCompletion {
    std::uint64_t generation = 0;
    std::string itemId;
    PendingRemuxPlayback pending;
};

// A Remux 503 with P2P_SOURCE_BUFFERING means the selected torrent is still
// making progress. Keep the original playback request alive until it becomes
// playable or the user cancels it (BACK / another title / app shutdown).
template <typename Resolve, typename Active, typename OnPending>
auto awaitRemuxStartup(Resolve&& resolve, Active&& active, OnPending&& onPending) -> decltype(resolve()) {
    using Result = decltype(resolve());
    while (active()) {
        Result result = resolve();
        const auto& pending = [&]() -> const std::optional<PendingRemuxPlayback>& {
            if constexpr (requires { result.target.pendingRemuxPlayback; })
                return result.target.pendingRemuxPlayback;
            else
                return result.pendingRemuxPlayback;
        }();
        if (!pending || !active()) return result;
        onPending(*pending);
        const int quarters = std::clamp(pending->retryAfterSeconds, 2, 20) * 4;
        for (int tick = 0; tick < quarters; ++tick) {
            if (!active()) return {};
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    return {};
}
