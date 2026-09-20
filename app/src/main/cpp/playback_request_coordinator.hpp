#pragma once

#include "app_screen.hpp"
#include "details_flow.hpp"
#include "playback_continuation_executor.hpp"
#include "playback_release_flow.hpp"
#include "playback_request_flow.hpp"
#include "playback_resolution_executor.hpp"
#include "series_playback_executor.hpp"

#include <chrono>
#include <utility>

struct PlaybackRequestHostEffects {
    bool popToDetails = false;
    bool resetIdle = false;
};

template <typename ReleaseFlow, typename ResolutionAsync, typename ContinuationAsync, typename SeriesPlaybackAsync>
class PlaybackRequestCoordinator {
public:
    PlaybackRequestCoordinator(PlaybackRequestFlow& requests, ReleaseFlow& releaseFlow,
                               PlaybackRuntimeController& runtime, ResolutionAsync& resolution,
                               ContinuationAsync& continuation, SeriesPlaybackAsync& seriesPlayback,
                               PlaybackQueueState& queue, PlaybackCoordinator& playback,
                               PlayerScreenState& playerScreen, DetailsFlow& details, JellyfinSession& session,
                               RequestEpoch& playbackEpoch, bool& loading, std::string& error)
        : requests_(requests), releaseFlow_(releaseFlow), runtime_(runtime), resolution_(resolution),
          continuation_(continuation), seriesPlayback_(seriesPlayback), queue_(queue), playback_(playback),
          playerScreen_(playerScreen), details_(details), session_(session), playbackEpoch_(playbackEpoch),
          loading_(loading), error_(error) {}

    void shuffleRemaining() { requests_.shuffleRemaining(); }

    void openQueue() { requests_.openQueue(); }

    void playQueued(int index, bool restartCurrent = false, bool replacingCompleted = false) {
        requests_.playQueued(
            index, restartCurrent, replacingCompleted,
            [this](bool reportStop, bool completed) { release(reportStop, completed); }, runtime_, resolution_);
    }

    void playItem(JellyfinItem item) {
        requests_.playPlayerItem(
            std::move(item), [this](bool reportStop, bool completed) { release(reportStop, completed); }, runtime_,
            resolution_);
    }

    void playAdjacent(int direction) {
        requests_.playAdjacent(
            direction, [this](bool reportStop, bool completed) { release(reportStop, completed); }, runtime_,
            resolution_, continuation_);
    }

    void handleQueue(ScreenNavigationKey key) {
        requests_.handleQueue(
            key, [this](bool reportStop, bool completed) { release(reportStop, completed); }, runtime_, resolution_);
    }

    void beginSeriesPlayAll() { requests_.beginSeriesPlayAll(seriesPlayback_); }

    void beginPlayback() { requests_.beginPlayback(runtime_, resolution_); }

    void release(bool reportStop, bool completed = false) { releaseFlow_.release(reportStop, completed, runtime_); }

    void queueAutoplay(JellyfinItem nextItem) {
        const int queuedNextIndex = queue_.autoplayAdvanceIndex(nextItem);
        release(true, true);
        playback_.beginAutoplayResolution();
        loading_ = true;
        details_.item() = nextItem;
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(10));
        PlaybackResolutionOptions options = runtime_.resolutionOptions();
        resolution_.resolveAutoplay(session_, std::move(nextItem), std::move(options), playbackEpoch_.begin(),
                                    queuedNextIndex);
    }

    [[nodiscard]] PlaybackRequestHostEffects showStillWatching(JellyfinItem nextItem) {
        release(true, true);
        details_.item() = std::move(nextItem);
        details_.state().beginDetails();
        playback_.showStillWatchingPrompt();
        error_.clear();
        return {.popToDetails = true, .resetIdle = true};
    }

private:
    PlaybackRequestFlow& requests_;
    ReleaseFlow& releaseFlow_;
    PlaybackRuntimeController& runtime_;
    ResolutionAsync& resolution_;
    ContinuationAsync& continuation_;
    SeriesPlaybackAsync& seriesPlayback_;
    PlaybackQueueState& queue_;
    PlaybackCoordinator& playback_;
    PlayerScreenState& playerScreen_;
    DetailsFlow& details_;
    JellyfinSession& session_;
    RequestEpoch& playbackEpoch_;
    bool& loading_;
    std::string& error_;
};
