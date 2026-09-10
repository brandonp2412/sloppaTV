#pragma once

#include "jellyfin_types.hpp"

#include <chrono>
#include <optional>
#include <utility>

class PlaybackContinuationState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto kNextEpisodeRetryDelay = std::chrono::seconds(10);

    void reset() {
        nextItem_.reset();
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAfter_ = {};
        autoplayChainCount_ = 0;
        stillWatchingPrompt_ = false;
    }

    [[nodiscard]] const std::optional<JellyfinItem>& nextItem() const { return nextItem_; }
    void setNextItem(JellyfinItem item) {
        nextItem_ = std::move(item);
        nextEpisodeRetryAfter_ = {};
    }
    void clearNextItem() { nextItem_.reset(); }

    [[nodiscard]] bool nextEpisodeRequested() const { return nextEpisodeRequested_; }
    bool beginNextEpisodeRequest(TimePoint now = Clock::now()) {
        if (nextEpisodeRequested_ || (nextEpisodeRetryAfter_ != TimePoint{} && now < nextEpisodeRetryAfter_)) {
            return false;
        }
        nextEpisodeRequested_ = true;
        return true;
    }
    void markNextEpisodeRequested() {
        nextEpisodeRequested_ = true;
        nextEpisodeRetryAfter_ = {};
    }
    void clearNextEpisodeRequest() {
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAfter_ = {};
    }
    void nextEpisodeRequestFailed(TimePoint now = Clock::now()) {
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAfter_ = now + kNextEpisodeRetryDelay;
    }
    void clearNextEpisode() {
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAfter_ = {};
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
    TimePoint nextEpisodeRetryAfter_{};
    int autoplayChainCount_ = 0;
    bool stillWatchingPrompt_ = false;
};
