#pragma once

#include "app_screen.hpp"
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
#include "playback_runtime_controller.hpp"
#include "request_epoch.hpp"
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
                                HomeVisibility& homeVisibility, RequestEpoch& playbackEpoch, Screen& screen,
                                bool& loading, PlaybackRuntimeController& runtime, std::string& error,
                                std::recursive_mutex& stateMutex)
        : player_(player), state_(state), async_(async), playback_(playback), session_(session), home_(home),
          homeState_(homeState), browseState_(browseState), searchState_(searchState), details_(details), queue_(queue),
          homeVisibility_(homeVisibility), playbackEpoch_(playbackEpoch), screen_(screen), loading_(loading),
          runtime_(runtime), error_(error), stateMutex_(stateMutex) {}

    void prepare(std::optional<ExternalPlayerApp> selectedPlayer) {
        if (loading_ || !session_.valid() || details_.item().id.empty()) return;
        if (!selectedPlayer) {
            error_ = "EXTERNAL PLAYER IS NOT CONFIGURED";
            return;
        }
        loading_ = true;
        error_.clear();
        const PlaybackLanguagePreferences preferences = playback_.languagePreferences();
        const PlaybackTrackSelectionPolicy trackPolicy = runtime_.trackSelectionPolicy();
        if (!async_.prepare(session_, ExternalPlaybackRequest{
                                          .generation = playbackEpoch_.begin(),
                                          .selectedItemId = details_.item().id,
                                          .selectedItemType = details_.item().type,
                                          .seriesId = details_.item().seriesId,
                                          .container = details_.item().container,
                                          .mediaSourceId = details_.item().mediaSourceId,
                                          .audios = details_.item().audios,
                                          .subtitles = details_.item().subtitles,
                                          .player = std::move(*selectedPlayer),
                                          .subtitlePreference = preferences.subtitle,
                                          .trackPolicy = trackPolicy,
                                      })) {
            loading_ = false;
            error_ = "EXTERNAL PLAYER COULD NOT BE STARTED";
        }
    }

    [[nodiscard]] int complete(ExternalPlaybackCompletion& completion) {
        if (!playbackEpoch_.active(completion.generation)) return 0;
        loading_ = false;
        if (screen_ != Screen::Details || details_.item().id != completion.selectedItemId) return 0;
        if (!completion.error.empty()) {
            error_ = std::move(completion.error);
            return 0;
        }
        if (!completion.launch) return 0;

        int persistCount = 0;
        JellyfinItem selected;
        selected.id = completion.selectedItemId;
        selected.seriesId = completion.selectedSeriesId;
        if (homeVisibility_.restoreForPlayback(selected)) ++persistCount;
        if (homeVisibility_.restoreForPlayback(completion.launch->item)) ++persistCount;
        state_.stage(std::move(*completion.launch));
        return persistCount;
    }

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
    RequestEpoch& playbackEpoch_;
    Screen& screen_;
    bool& loading_;
    PlaybackRuntimeController& runtime_;
    std::string& error_;
    std::recursive_mutex& stateMutex_;
};
