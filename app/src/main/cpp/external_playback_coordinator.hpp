#pragma once

#include "browse_screen.hpp"
#include "details_flow.hpp"
#include "external_playback_executor.hpp"
#include "external_playback_state.hpp"
#include "external_player.hpp"
#include "home_screen.hpp"
#include "home_visibility.hpp"
#include "item_mutation_controller.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "search_screen.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <utility>

struct ExternalPlaybackTickWork {
    std::optional<ExternalPlayerResult> result;
    std::optional<ExternalPlaybackLaunch> completed;
    std::optional<ExternalPlaybackLaunch> pending;
};

template <typename ExternalPlaybackAsync> class ExternalPlaybackCoordinator {
public:
    ExternalPlaybackCoordinator(NativeExternalPlayer& player, ExternalPlaybackState& state,
                                ExternalPlaybackAsync& async, PlaybackCoordinator& playback, JellyfinSession& session,
                                JellyfinHomeData& home, HomeScreenState& homeState, BrowseScreenState& browseState,
                                SearchScreenState& searchState, DetailsFlow& details, PlaybackQueueState& queue,
                                HomeVisibility& homeVisibility, std::string& error, std::recursive_mutex& stateMutex)
        : player_(player), state_(state), async_(async), playback_(playback), session_(session), home_(home),
          homeState_(homeState), browseState_(browseState), searchState_(searchState), details_(details), queue_(queue),
          homeVisibility_(homeVisibility), error_(error), stateMutex_(stateMutex) {}

    [[nodiscard]] ExternalPlaybackTickWork collect() {
        ExternalPlaybackTickWork work;
        work.result = player_.takeResult();
        std::scoped_lock lock(stateMutex_);
        if (work.result && state_.hasActive()) work.completed = state_.takeActive();
        if (state_.hasPending()) work.pending = state_.takePending();
        return work;
    }

    void finish(const ExternalPlaybackTickWork& work) {
        if (!work.result || !work.completed) return;
        const auto& completed = *work.completed;
        const ExternalPlaybackFinishPlan plan = planExternalPlaybackFinish(completed, *work.result);
        if (plan.failed) {
            std::scoped_lock lock(stateMutex_);
            error_ = "EXTERNAL PLAYER REPORTED PLAYBACK FAILURE";
            return;
        }

        if (plan.updatedItem) {
            std::scoped_lock lock(stateMutex_);
            ItemMutationController::updateCachedUserData(home_, homeState_, browseState_, searchState_,
                                                         details_.state(), queue_, *plan.updatedItem,
                                                         homeVisibility_.isHidden(*plan.updatedItem));
            if (details_.item().id == completed.item.id) {
                details_.item().played = plan.updatedItem->played;
                details_.item().positionTicks = plan.updatedItem->positionTicks;
            }
        }
        async_.reportStopped(session_, ExternalPlaybackReportRequest{
                                           .itemId = completed.item.id,
                                           .mediaSourceId = completed.item.mediaSourceId,
                                           .positionTicks = plan.positionTicks,
                                       });
        std::scoped_lock lock(stateMutex_);
        error_.clear();
    }

    [[nodiscard]] bool launch(ExternalPlaybackTickWork& work) {
        if (!work.pending) return false;
        auto pending = std::move(*work.pending);
        std::string launchError;
        const std::string title =
            pending.item.seriesName.empty() ? pending.item.name : pending.item.seriesName + " - " + pending.item.name;
        const int positionMs = playbackPositionMsFromTicks(pending.item.positionTicks);
        if (!player_.launch(pending.player, pending.url, title, positionMs, pending.subtitleUrl,
                            pending.skipSegmentsJson, launchError)) {
            std::scoped_lock lock(stateMutex_);
            error_ = launchError.empty() ? "EXTERNAL PLAYER COULD NOT BE LAUNCHED" : launchError;
        } else {
            std::scoped_lock lock(stateMutex_);
            error_.clear();
            playback_.recordExternalPlayback(pending.player.label);
            state_.beginActive(std::move(pending));
        }
        return true;
    }

private:
    NativeExternalPlayer& player_;
    ExternalPlaybackState& state_;
    ExternalPlaybackAsync& async_;
    PlaybackCoordinator& playback_;
    JellyfinSession& session_;
    JellyfinHomeData& home_;
    HomeScreenState& homeState_;
    BrowseScreenState& browseState_;
    SearchScreenState& searchState_;
    DetailsFlow& details_;
    PlaybackQueueState& queue_;
    HomeVisibility& homeVisibility_;
    std::string& error_;
    std::recursive_mutex& stateMutex_;
};
