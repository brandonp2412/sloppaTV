#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

// Coalesces remote seek presses while a Jellyfin HLS seek needs a new stream.
// Keeping the target through loading prevents subsequent presses from silently
// seeking a stopped mpv instance and losing the requested position.
class DeferredPlaybackSeek {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void request(std::string_view itemId, int positionMs, TimePoint now) {
        if (itemId.empty()) return;
        itemId_ = itemId;
        targetMs_ = std::max(0, positionMs);
        lastInput_ = now;
    }

    [[nodiscard]] std::optional<int> positionFor(std::string_view itemId) const {
        if (itemId.empty() || itemId != itemId_) return std::nullopt;
        return targetMs_;
    }

    [[nodiscard]] std::optional<int> takeReady(std::string_view itemId, bool streamReady, TimePoint now) {
        if (!targetMs_) return std::nullopt;
        if (itemId.empty() || itemId != itemId_) {
            clear();
            return std::nullopt;
        }
        if (!streamReady || now - lastInput_ < std::chrono::milliseconds(350)) return std::nullopt;
        const int target = *targetMs_;
        clear();
        return target;
    }

    void clear() {
        itemId_.clear();
        targetMs_.reset();
        lastInput_ = {};
    }

private:
    std::string itemId_;
    std::optional<int> targetMs_;
    TimePoint lastInput_{};
};
