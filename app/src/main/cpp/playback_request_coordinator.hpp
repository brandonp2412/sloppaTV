#pragma once

#include "app_screen.hpp"
#include "details_flow.hpp"
#include "playback_continuation_executor.hpp"
#include "playback_release_flow.hpp"
#include "playback_resolution_executor.hpp"
#include "playback_runtime_controller.hpp"
#include "queue_navigation_controller.hpp"
#include "request_epoch.hpp"
#include "series_playback_executor.hpp"

#include <chrono>
#include <random>
#include <utility>

struct PlaybackRequestHostEffects {
    bool popToDetails = false;
    bool resetIdle = false;
};

template <typename ReleaseFlow, typename ResolutionAsync, typename ContinuationAsync, typename SeriesPlaybackAsync>
class PlaybackRequestCoordinator {
public:
    PlaybackRequestCoordinator(PlaybackQueueState& queue, PlaybackCoordinator& playback,
                               PlayerScreenState& playerScreen, DetailsFlow& details, JellyfinSession& session,
                               AppSettings& settings, RequestEpoch& playbackEpoch, Screen& screen, bool& loading,
                               std::string& error, ReleaseFlow& releaseFlow, PlaybackRuntimeController& runtime,
                               ResolutionAsync& resolution, ContinuationAsync& continuation,
                               SeriesPlaybackAsync& seriesPlayback)
        : releaseFlow_(releaseFlow), runtime_(runtime), resolution_(resolution), continuation_(continuation),
          seriesPlayback_(seriesPlayback), queue_(queue), playback_(playback), playerScreen_(playerScreen),
          details_(details), session_(session), settings_(settings), playbackEpoch_(playbackEpoch), screen_(screen),
          loading_(loading), error_(error) {}

    void shuffleRemaining() {
        static thread_local std::mt19937 generator(std::random_device{}());
        if (queue_.shuffleRemaining(generator)) playback_.syncQueueContinuation(queue_);
    }

    void openQueue() {
        if (!queue_.openOverlay()) {
            error_.clear();
            return;
        }
        error_.clear();
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(10));
    }

    void playQueued(int index, bool restartCurrent = false, bool replacingCompleted = false) {
        if (loading_ || index < 0 || index >= queue_.size() || !session_.valid()) return;
        if (index == queue_.currentIndex() && screen_ == Screen::Player && !restartCurrent) {
            queue_.closeOverlay();
            return;
        }
        const Screen originScreen = screen_;
        const bool replacingPlayer = screen_ == Screen::Player && playback_.activeItemAvailable();
        if (replacingPlayer) release(true, replacingCompleted);
        const int previousQueueIndex = queue_.currentIndex();
        queue_.closeOverlay();
        loading_ = true;
        playback_.beginPlaybackResolution(replacingPlayer);
        error_.clear();
        JellyfinItem queued = *queue_.itemAt(index);
        if (restartCurrent) queued.positionTicks = 0;
        if (!resolution_.resolveQueued(session_, std::move(queued), runtime_.resolutionOptions(),
                                       playbackEpoch_.begin(), originScreen, index, previousQueueIndex,
                                       replacingPlayer))
            rejectPlaybackResolution("PLAYBACK COULD NOT BE STARTED");
    }

    void playItem(JellyfinItem item) {
        if (loading_ || screen_ != Screen::Player || !session_.valid() || item.id.empty()) return;
        queue_.reset();
        release(true, false);
        loading_ = true;
        playback_.beginPlaybackResolution(true);
        error_.clear();
        if (!resolution_.resolvePlayerItem(session_, std::move(item), runtime_.resolutionOptions(),
                                           playbackEpoch_.begin()))
            rejectPlaybackResolution("PLAYBACK COULD NOT BE STARTED");
    }

    void playAdjacent(int direction) {
        if (direction == 0 || loading_ || screen_ != Screen::Player) return;
        const int current = queue_.currentIndex();
        if (current >= 0) {
            const int target = direction > 0 ? queue_.nextIndex(true) : current - 1;
            if (target >= 0 && target < queue_.size()) {
                playQueued(target);
                return;
            }
        }
        if (!session_.valid()) return;
        PlaybackAdjacentEpisodePlan plan = playback_.beginAdjacentEpisodePlan(direction);
        if (plan.nextItem) {
            playItem(std::move(*plan.nextItem));
            return;
        }
        if (!plan.lookup) return;
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(5));
        if (!continuation_.requestAdjacentEpisode(session_, plan.lookup->seriesId, plan.lookup->currentItemId,
                                                  plan.lookup->currentSeason, plan.lookup->currentEpisode, direction)) {
            static_cast<void>(playback_.finishAdjacentEpisodeLookup(plan.lookup->currentItemId));
            error_ = "ADJACENT EPISODE LOOKUP COULD NOT BE STARTED";
        }
    }

    void handleQueue(ScreenNavigationKey key) {
        const QueueNavigationAction action = QueueNavigationController::handle(queue_, key);
        switch (action.type) {
        case QueueNavigationActionType::None:
            return;
        case QueueNavigationActionType::PlayIndex:
            playQueued(action.index);
            return;
        case QueueNavigationActionType::QueueChanged:
            playback_.syncQueueContinuation(queue_);
            return;
        case QueueNavigationActionType::Shuffle:
            shuffleRemaining();
            return;
        }
    }

    void beginSeriesPlayAll() {
        if (loading_ || details_.item().type != "Series" || details_.item().id.empty() || !session_.valid()) return;
        loading_ = true;
        error_.clear();
        playback_.beginUserPlayback(false);
        SeriesPlayAllOptions options{.maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
                                     .maxAudioChannels = settings_.maxAudioChannels,
                                     .overrides = playbackOverridesFor(settings_)};
        if (!seriesPlayback_.playAll(session_, details_.item(), options, playbackEpoch_.begin()))
            rejectPlaybackResolution("PLAY ALL COULD NOT BE STARTED");
    }

    void beginPlayback() {
        if (loading_ || details_.item().id.empty()) return;
        const PlaybackUserSelectionPlan selection = playback_.beginUserPlaybackSelection(queue_, details_.item().id);
        if (selection.resetQueue) queue_.reset();
        loading_ = true;
        error_.clear();
        if (!resolution_.resolveSelection(session_, details_.item(), runtime_.resolutionOptions(),
                                          playbackEpoch_.begin(), selection.queuedPlaybackIndex))
            rejectPlaybackResolution("PLAYBACK COULD NOT BE STARTED");
    }

    void release(bool reportStop, bool completed = false) { releaseFlow_.release(reportStop, completed, runtime_); }

    [[nodiscard]] PlaybackRequestHostEffects queueAutoplay(JellyfinItem nextItem) {
        const int queuedNextIndex = queue_.autoplayAdvanceIndex(nextItem);
        release(true, true);
        playback_.beginAutoplayResolution();
        loading_ = true;
        details_.item() = nextItem;
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(10));
        PlaybackResolutionOptions options = runtime_.resolutionOptions();
        if (resolution_.resolveAutoplay(session_, std::move(nextItem), std::move(options), playbackEpoch_.begin(),
                                        queuedNextIndex))
            return {};
        rejectPlaybackResolution("NEXT EPISODE: PLAYBACK COULD NOT BE STARTED");
        return {.popToDetails = true};
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
    void rejectPlaybackResolution(std::string error) {
        playbackEpoch_.invalidate();
        loading_ = false;
        playback_.finishPlaybackResolution();
        error_ = std::move(error);
    }

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
    AppSettings& settings_;
    RequestEpoch& playbackEpoch_;
    Screen& screen_;
    bool& loading_;
    std::string& error_;
};
