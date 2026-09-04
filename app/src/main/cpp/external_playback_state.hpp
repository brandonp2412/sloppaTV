#pragma once

#include "external_player_types.hpp"
#include "jellyfin_types.hpp"

#include <optional>
#include <string>
#include <utility>

struct ExternalPlaybackLaunch {
    JellyfinItem item;
    ExternalPlayerApp player;
    std::string url;
    std::string subtitleUrl;
};

class ExternalPlaybackState {
public:
    void reset() {
        pending_.reset();
        active_.reset();
    }

    void stage(ExternalPlaybackLaunch launch) {
        pending_ = std::move(launch);
    }

    [[nodiscard]] bool hasPending() const { return pending_.has_value(); }

    std::optional<ExternalPlaybackLaunch> takePending() {
        if (!pending_) return std::nullopt;
        auto launch = std::move(pending_);
        pending_.reset();
        return launch;
    }

    void beginActive(ExternalPlaybackLaunch launch) {
        active_ = std::move(launch);
    }

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
