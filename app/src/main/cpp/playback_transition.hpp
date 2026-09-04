#pragma once

#include "jellyfin_types.hpp"

#include <optional>
#include <utility>

struct PendingPlaybackTransition {
    PlaybackTarget target;
    JellyfinItem item;
    bool streamRestart = false;
    bool restartPaused = false;
    int audioStreamIndex = -1;
};

class PlaybackTransitionState {
public:
    void reset() {
        pending_.reset();
        loading_ = false;
        fallbackResolving_ = false;
        pauseAfterRestart_ = false;
    }

    void stage(
        PlaybackTarget target,
        JellyfinItem item,
        bool streamRestart = false,
        bool restartPaused = false,
        int audioStreamIndex = -1
    ) {
        pending_ = PendingPlaybackTransition{
            .target = std::move(target),
            .item = std::move(item),
            .streamRestart = streamRestart,
            .restartPaused = restartPaused,
            .audioStreamIndex = audioStreamIndex,
        };
    }

    [[nodiscard]] bool hasPending() const { return pending_.has_value(); }

    std::optional<PendingPlaybackTransition> take() {
        if (!pending_) return std::nullopt;
        auto result = std::move(pending_);
        pending_.reset();
        return result;
    }

    [[nodiscard]] bool loading() const { return loading_; }
    void setLoading(bool loading) { loading_ = loading; }

    [[nodiscard]] bool fallbackResolving() const { return fallbackResolving_; }
    void setFallbackResolving(bool resolving) { fallbackResolving_ = resolving; }

    [[nodiscard]] bool pauseAfterRestart() const { return pauseAfterRestart_; }
    void setPauseAfterRestart(bool pause) { pauseAfterRestart_ = pause; }
    void clearPauseAfterRestart() { pauseAfterRestart_ = false; }

private:
    std::optional<PendingPlaybackTransition> pending_;
    bool loading_ = false;
    bool fallbackResolving_ = false;
    bool pauseAfterRestart_ = false;
};
