#pragma once

#include "jellyfin_types.hpp"

#include <chrono>
#include <optional>
#include <utility>

class PlaybackContinuationState {
public:
    void reset() {
        nextItem_.reset();
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAt_ = {};
        autoplayChainCount_ = 0;
        stillWatchingPrompt_ = false;
    }

    [[nodiscard]] const std::optional<JellyfinItem>& nextItem() const { return nextItem_; }
    void setNextItem(JellyfinItem item) { nextItem_ = std::move(item); }
    void clearNextItem() { nextItem_.reset(); }

    [[nodiscard]] bool nextEpisodeRequested() const { return nextEpisodeRequested_; }
    bool beginNextEpisodeRequest(std::chrono::steady_clock::time_point now) {
        if (nextEpisodeRequested_ || (nextEpisodeRetryAt_ != std::chrono::steady_clock::time_point{} && now < nextEpisodeRetryAt_)) {
            return false;
        }
        nextEpisodeRequested_ = true;
        nextEpisodeRetryAt_ = {};
        return true;
    }
    void failNextEpisodeRequest(
        std::chrono::steady_clock::time_point now,
        std::chrono::seconds retryDelay = std::chrono::seconds(10)
    ) {
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAt_ = now + retryDelay;
    }
    void markNextEpisodeRequested() {
        nextEpisodeRequested_ = true;
        nextEpisodeRetryAt_ = {};
    }
    void clearNextEpisodeRequest() {
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAt_ = {};
    }
    void clearNextEpisode() {
        clearNextEpisodeRequest();
        nextItem_.reset();
    }

    [[nodiscard]] int autoplayChainCount() const { return autoplayChainCount_; }
    void resetAutoplayChain() { autoplayChainCount_ = 0; }
    void incrementAutoplayChain() { ++autoplayChainCount_; }

    [[nodiscard]] bool stillWatchingPrompt() const { return stillWatchingPrompt_; }
    void setStillWatchingPrompt(bool visible) { stillWatchingPrompt_ = visible; }

private:
    std::optional<JellyfinItem> nextItem_;
    bool nextEpisodeRequested_ = false;
    std::chrono::steady_clock::time_point nextEpisodeRetryAt_{};
    int autoplayChainCount_ = 0;
    bool stillWatchingPrompt_ = false;
};
