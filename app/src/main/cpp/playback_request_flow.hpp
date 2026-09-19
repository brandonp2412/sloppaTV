#pragma once

#include "app_screen.hpp"
#include "app_settings.hpp"
#include "details_flow.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_runtime_controller.hpp"
#include "queue_navigation_controller.hpp"
#include "request_epoch.hpp"
#include "series_playback_executor.hpp"

#include <chrono>
#include <random>
#include <utility>

class PlaybackRequestFlow {
public:
    PlaybackRequestFlow(PlaybackQueueState& queue, PlaybackCoordinator& coordinator, PlayerScreenState& playerScreen,
                        DetailsFlow& details, JellyfinSession& session, AppSettings& settings,
                        RequestEpoch& playbackEpoch, Screen& screen, bool& loading, std::string& error)
        : queue_(queue), coordinator_(coordinator), playerScreen_(playerScreen), details_(details), session_(session),
          settings_(settings), playbackEpoch_(playbackEpoch), screen_(screen), loading_(loading), error_(error) {}

    void shuffleRemaining() {
        static thread_local std::mt19937 generator(std::random_device{}());
        if (queue_.shuffleRemaining(generator)) coordinator_.syncQueueContinuation(queue_);
    }

    void openQueue() {
        if (!queue_.openOverlay()) {
            error_.clear();
            return;
        }
        error_.clear();
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(10));
    }

    template <typename Release, typename Resolution>
    void playQueued(int index, bool restartCurrent, bool replacingCompleted, Release&& release,
                    PlaybackRuntimeController& runtime, Resolution& resolution) {
        if (loading_ || index < 0 || index >= queue_.size() || !session_.valid()) return;
        if (index == queue_.currentIndex() && screen_ == Screen::Player && !restartCurrent) {
            queue_.closeOverlay();
            return;
        }

        const Screen originScreen = screen_;
        const bool replacingPlayer = screen_ == Screen::Player && coordinator_.activeItemAvailable();
        if (replacingPlayer) release(true, replacingCompleted);
        const int previousQueueIndex = queue_.currentIndex();
        queue_.closeOverlay();
        loading_ = true;
        coordinator_.beginPlaybackResolution(replacingPlayer);
        error_.clear();

        JellyfinItem queued = *queue_.itemAt(index);
        if (restartCurrent) queued.positionTicks = 0;
        PlaybackResolutionOptions options = runtime.resolutionOptions();
        const uint64_t generation = playbackEpoch_.begin();
        resolution.resolveQueued(session_, std::move(queued), std::move(options), generation, originScreen, index,
                                 previousQueueIndex, replacingPlayer);
    }

    template <typename Release, typename Resolution>
    void playPlayerItem(JellyfinItem selected, Release&& release, PlaybackRuntimeController& runtime,
                        Resolution& resolution) {
        if (loading_ || screen_ != Screen::Player || !session_.valid() || selected.id.empty()) return;
        PlaybackResolutionOptions options = runtime.resolutionOptions();
        queue_.reset();
        release(true, false);
        loading_ = true;
        coordinator_.beginPlaybackResolution(true);
        error_.clear();
        resolution.resolvePlayerItem(session_, std::move(selected), std::move(options), playbackEpoch_.begin());
    }

    template <typename Release, typename Resolution, typename Continuation>
    void playAdjacent(int direction, Release&& release, PlaybackRuntimeController& runtime, Resolution& resolution,
                      Continuation& continuation) {
        if (direction == 0 || loading_ || screen_ != Screen::Player) return;
        const int current = queue_.currentIndex();
        if (current >= 0) {
            const int target = direction > 0 ? queue_.nextIndex(true) : current - 1;
            if (target >= 0 && target < queue_.size()) {
                playQueued(target, false, false, std::forward<Release>(release), runtime, resolution);
                return;
            }
        }
        if (!session_.valid()) return;

        PlaybackAdjacentEpisodePlan plan = coordinator_.beginAdjacentEpisodePlan(direction);
        if (plan.nextItem) {
            playPlayerItem(std::move(*plan.nextItem), std::forward<Release>(release), runtime, resolution);
            return;
        }
        if (!plan.lookup) return;
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(5));
        continuation.requestAdjacentEpisode(session_, plan.lookup->seriesId, plan.lookup->currentItemId,
                                            plan.lookup->currentSeason, plan.lookup->currentEpisode, direction);
    }

    template <typename Release, typename Resolution>
    void handleQueue(ScreenNavigationKey key, Release&& release, PlaybackRuntimeController& runtime,
                     Resolution& resolution) {
        const QueueNavigationAction action = QueueNavigationController::handle(queue_, key);
        switch (action.type) {
        case QueueNavigationActionType::None:
            return;
        case QueueNavigationActionType::PlayIndex:
            playQueued(action.index, false, false, std::forward<Release>(release), runtime, resolution);
            return;
        case QueueNavigationActionType::QueueChanged:
            coordinator_.syncQueueContinuation(queue_);
            return;
        case QueueNavigationActionType::Shuffle:
            shuffleRemaining();
            return;
        }
    }

    template <typename SeriesPlayback> void beginSeriesPlayAll(SeriesPlayback& executor) {
        if (loading_ || details_.item().type != "Series" || details_.item().id.empty() || !session_.valid()) return;
        loading_ = true;
        error_.clear();
        coordinator_.beginUserPlayback(false);
        SeriesPlayAllOptions options{
            .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
            .maxAudioChannels = settings_.maxAudioChannels,
            .overrides = playbackOverridesFor(settings_),
        };
        executor.playAll(session_, details_.item(), std::move(options), playbackEpoch_.begin());
    }

    template <typename Resolution> void beginPlayback(PlaybackRuntimeController& runtime, Resolution& resolution) {
        if (loading_ || details_.item().id.empty()) return;
        const PlaybackUserSelectionPlan selection = coordinator_.beginUserPlaybackSelection(queue_, details_.item().id);
        if (selection.resetQueue) queue_.reset();
        loading_ = true;
        error_.clear();
        PlaybackResolutionOptions options = runtime.resolutionOptions();
        resolution.resolveSelection(session_, details_.item(), std::move(options), playbackEpoch_.begin(),
                                    selection.queuedPlaybackIndex);
    }

private:
    PlaybackQueueState& queue_;
    PlaybackCoordinator& coordinator_;
    PlayerScreenState& playerScreen_;
    DetailsFlow& details_;
    JellyfinSession& session_;
    AppSettings& settings_;
    RequestEpoch& playbackEpoch_;
    Screen& screen_;
    bool& loading_;
    std::string& error_;
};
