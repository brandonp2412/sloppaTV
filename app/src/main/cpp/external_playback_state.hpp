#pragma once

#include "external_player_types.hpp"
#include "jellyfin_types.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

struct ExternalPlaybackLaunch {
    JellyfinItem item;
    ExternalPlayerApp player;
    std::string url;
    std::string subtitleUrl;
    std::string skipSegmentsJson;
};

struct ExternalPlaybackFinishPlan {
    bool failed = false;
    bool completed = false;
    std::optional<int64_t> positionTicks;
    std::optional<JellyfinItem> updatedItem;
};

inline ExternalPlaybackFinishPlan planExternalPlaybackFinish(const ExternalPlaybackLaunch& launch,
                                                             const ExternalPlayerResult& result) {
    ExternalPlaybackFinishPlan plan;
    plan.failed = !result.success;
    if (plan.failed) return plan;
    plan.completed = result.completionKnown && result.completed;
    if (result.positionMs >= 0) {
        constexpr int64_t ticksPerMillisecond = 10000;
        constexpr int64_t maxMilliseconds = std::numeric_limits<int64_t>::max() / ticksPerMillisecond;
        if (result.positionMs > maxMilliseconds)
            plan.positionTicks = std::numeric_limits<int64_t>::max();
        else
            plan.positionTicks = result.positionMs * ticksPerMillisecond;
    } else if (plan.completed && launch.item.runtimeTicks > 0) {
        plan.positionTicks = launch.item.runtimeTicks;
    }

    if (!plan.positionTicks && !plan.completed) return plan;
    JellyfinItem updated = launch.item;
    if (plan.positionTicks) {
        const int64_t bounded = launch.item.runtimeTicks > 0
                                    ? std::clamp<int64_t>(*plan.positionTicks, 0, launch.item.runtimeTicks)
                                    : std::max<int64_t>(0, *plan.positionTicks);
        plan.positionTicks = bounded;
        updated.positionTicks = bounded;
    }
    if (plan.completed) {
        updated.played = true;
        updated.positionTicks = 0;
    }
    plan.updatedItem = std::move(updated);
    return plan;
}

class ExternalPlaybackState {
public:
    void reset() {
        pending_.reset();
        active_.reset();
    }

    void stage(ExternalPlaybackLaunch launch) { pending_ = std::move(launch); }

    [[nodiscard]] bool hasPending() const { return pending_.has_value(); }

    std::optional<ExternalPlaybackLaunch> takePending() {
        if (!pending_) return std::nullopt;
        auto launch = std::move(pending_);
        pending_.reset();
        return launch;
    }

    void beginActive(ExternalPlaybackLaunch launch) { active_ = std::move(launch); }

    [[nodiscard]] bool hasActive() const { return active_.has_value(); }

    std::optional<ExternalPlaybackLaunch> takeActive() {
        if (!active_) return std::nullopt;
        auto launch = std::move(active_);
        active_.reset();
        return launch;
    }

private:
    std::optional<ExternalPlaybackLaunch> pending_;
    std::optional<ExternalPlaybackLaunch> active_;
};
