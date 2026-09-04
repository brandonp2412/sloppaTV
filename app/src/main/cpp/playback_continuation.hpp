#pragma once

#include "jellyfin_types.hpp"

#include <optional>
#include <utility>

class PlaybackContinuationState {
public:
    void reset() {
        nextItem_.reset();
        nextEpisodeRequested_ = false;
        autoplayChainCount_ = 0;
        stillWatchingPrompt_ = false;
    }

    [[nodiscard]] const std::optional<JellyfinItem>& nextItem() const { return nextItem_; }
    void setNextItem(JellyfinItem item) { nextItem_ = std::move(item); }
    void clearNextItem() { nextItem_.reset(); }

    [[nodiscard]] bool nextEpisodeRequested() const { return nextEpisodeRequested_; }
    bool beginNextEpisodeRequest() {
        if (nextEpisodeRequested_) return false;
        nextEpisodeRequested_ = true;
        return true;
    }
    void markNextEpisodeRequested() { nextEpisodeRequested_ = true; }
    void clearNextEpisodeRequest() { nextEpisodeRequested_ = false; }
    void clearNextEpisode() {
        nextEpisodeRequested_ = false;
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
    int autoplayChainCount_ = 0;
    bool stillWatchingPrompt_ = false;
};
