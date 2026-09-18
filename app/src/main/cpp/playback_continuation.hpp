#pragma once

#include "jellyfin_types.hpp"
#include "playback_queue.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

inline std::optional<JellyfinItem> selectAdjacentPlaybackEpisode(std::vector<JellyfinItem> episodes,
                                                                 const std::string& currentItemId, int currentSeason,
                                                                 int currentEpisode, int direction) {
    if (direction == 0) return std::nullopt;

    std::sort(episodes.begin(), episodes.end(), [](const JellyfinItem& left, const JellyfinItem& right) {
        if (left.parentIndexNumber != right.parentIndexNumber) return left.parentIndexNumber < right.parentIndexNumber;
        if (left.indexNumber != right.indexNumber) return left.indexNumber < right.indexNumber;
        return left.name < right.name;
    });
    auto current = std::find_if(episodes.begin(), episodes.end(),
                                [&](const JellyfinItem& candidate) { return candidate.id == currentItemId; });
    if (current == episodes.end() && currentSeason >= 0 && currentEpisode >= 0) {
        current = std::find_if(episodes.begin(), episodes.end(), [&](const JellyfinItem& candidate) {
            return sameEpisodeSlot(candidate.parentIndexNumber, candidate.indexNumber, currentSeason, currentEpisode);
        });
    }
    if (current == episodes.end()) return std::nullopt;

    int candidateIndex = static_cast<int>(std::distance(episodes.begin(), current)) + direction;
    while (candidateIndex >= 0 && candidateIndex < static_cast<int>(episodes.size())) {
        const auto& candidate = episodes[static_cast<size_t>(candidateIndex)];
        const bool duplicateSlot = sameEpisodeSlot(candidate.parentIndexNumber, candidate.indexNumber,
                                                   current->parentIndexNumber, current->indexNumber);
        const bool specialOutsideRegularRun = current->parentIndexNumber > 0 && candidate.parentIndexNumber <= 0;
        if (!duplicateSlot && !specialOutsideRegularRun) return candidate;
        candidateIndex += direction;
    }
    return std::nullopt;
}

class PlaybackContinuationState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto kNextEpisodeRetryDelay = std::chrono::seconds(10);

    void reset() {
        nextItem_.reset();
        nextEpisodeRequested_ = false;
        nextEpisodeRetryAfter_ = {};
        adjacentEpisodeLookup_ = false;
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
        nextEpisodeRetryAfter_ = {};
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

    [[nodiscard]] bool adjacentEpisodeLookupInProgress() const { return adjacentEpisodeLookup_; }

    bool beginAdjacentEpisodeLookup() {
        if (adjacentEpisodeLookup_) return false;
        adjacentEpisodeLookup_ = true;
        return true;
    }

    void finishAdjacentEpisodeLookup() { adjacentEpisodeLookup_ = false; }

    [[nodiscard]] int autoplayChainCount() const { return autoplayChainCount_; }

    void resetAutoplayChain() { autoplayChainCount_ = 0; }

    void incrementAutoplayChain() { ++autoplayChainCount_; }

    [[nodiscard]] bool stillWatchingPrompt() const { return stillWatchingPrompt_; }

    void setStillWatchingPrompt(bool visible) { stillWatchingPrompt_ = visible; }

private:
    std::optional<JellyfinItem> nextItem_;
    bool nextEpisodeRequested_ = false;
    TimePoint nextEpisodeRetryAfter_{};
    bool adjacentEpisodeLookup_ = false;
    int autoplayChainCount_ = 0;
    bool stillWatchingPrompt_ = false;
};
