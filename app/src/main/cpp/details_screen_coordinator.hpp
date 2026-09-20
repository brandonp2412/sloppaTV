#pragma once

#include "app_screen.hpp"
#include "browse_screen.hpp"
#include "content_mutation_flow.hpp"
#include "details_async_executor.hpp"
#include "details_flow.hpp"
#include "home_screen.hpp"
#include "home_visibility.hpp"
#include "item_mutation_executor.hpp"
#include "navigation_stack.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "request_epoch.hpp"
#include "search_screen.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "settings_flow.hpp"
#include "similar_prefetch_controller.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class DetailsScreenHostAction {
    None,
    BeginPlayback,
    BeginSeriesPlayAll,
    DeleteSeerrRequest,
    PlayExternal,
    ViewQueue,
};

struct DetailsScreenNotice {
    std::string message;
    std::chrono::seconds duration{6};
};

struct DetailsScreenEffects {
    DetailsScreenHostAction action = DetailsScreenHostAction::None;
    bool reloadBrowse = false;
    bool persistSession = false;
    std::optional<DetailsScreenNotice> notice;
    std::optional<JellyfinItem> prefetchItem;
};

template <typename DetailsAsync, typename ItemMutationAsync> class DetailsScreenCoordinator {
public:
    DetailsScreenCoordinator(DetailsFlow& details, NavigationStack<Screen>& navigation, Screen& screen,
                             JellyfinSession& session, bool& loading, std::string& error, RequestEpoch& contentEpoch,
                             RequestEpoch& sessionEpoch, DetailsAsync& detailsAsync,
                             ItemMutationAsync& itemMutationAsync, ContentMutationFlow& contentMutation,
                             JellyfinHomeData& home, HomeScreenState& homeState, BrowseScreenState& browseState,
                             SearchScreenState& searchState, PlaybackQueueState& queueState,
                             HomeVisibility& homeVisibility, SettingsFlow& settingsFlow,
                             PlaybackCoordinator& playbackCoordinator, SimilarPrefetchController& similarPrefetch)
        : details_(details), navigation_(navigation), screen_(screen), session_(session), loading_(loading),
          error_(error), contentEpoch_(contentEpoch), sessionEpoch_(sessionEpoch), detailsAsync_(detailsAsync),
          itemMutationAsync_(itemMutationAsync), contentMutation_(contentMutation), home_(home), homeState_(homeState),
          browseState_(browseState), searchState_(searchState), queueState_(queueState),
          homeVisibility_(homeVisibility), settingsFlow_(settingsFlow), playbackCoordinator_(playbackCoordinator),
          similarPrefetch_(similarPrefetch) {}

    [[nodiscard]] std::vector<std::string> detailActions() const {
        return details_.detailActions(playbackCoordinator_.continuation().stillWatchingPrompt());
    }

    [[nodiscard]] std::vector<std::string> itemMenuActions() const {
        return details_.itemMenuActions(isSeerrItem(details_.item()),
                                        settingsFlow_.selectedExternalPlayer().has_value(), !queueState_.empty(),
                                        homeVisibility_.isHidden(details_.item()));
    }

    [[nodiscard]] DetailsScreenEffects handleDetails(DetailsNavigationKey key) {
        DetailsScreenEffects effects;
        const DetailsFlowCommand command = details_.handleDetails(key);
        if (details_.state().similarFocused()) {
            if (const auto* selected = details_.state().selectedSimilar()) effects.prefetchItem = *selected;
        }
        applyCommand(command, effects);
        return effects;
    }

    [[nodiscard]] DetailsScreenEffects handleCast(DetailsNavigationKey key, int columns) {
        DetailsScreenEffects effects;
        applyCommand(details_.handleCast(key, columns), effects);
        return effects;
    }

    [[nodiscard]] DetailsScreenEffects handlePersonItems(DetailsNavigationKey key, int columns) {
        DetailsScreenEffects effects;
        applyCommand(details_.handlePersonItems(key, columns), effects);
        return effects;
    }

    [[nodiscard]] DetailsScreenEffects handleItemMenu(DetailsNavigationKey key) {
        DetailsScreenEffects effects;
        applyCommand(details_.handleItemMenu(key, isSeerrItem(details_.item()),
                                             settingsFlow_.selectedExternalPlayer().has_value(), !queueState_.empty()),
                     effects);
        return effects;
    }

    [[nodiscard]] DetailsScreenEffects handleSeasons(DetailsNavigationKey key, int columns) {
        DetailsScreenEffects effects;
        applyCommand(details_.handleSeasons(key, columns), effects);
        return effects;
    }

    [[nodiscard]] DetailsScreenEffects handleEpisodes(DetailsNavigationKey key, int columns) {
        DetailsScreenEffects effects;
        applyCommand(details_.handleEpisodes(key, columns), effects);
        return effects;
    }

    void openDetails(const JellyfinItem& item, bool replaceCurrent = false) {
        if (replaceCurrent)
            replaceScreen(Screen::Details);
        else
            pushScreen(Screen::Details);
        playbackCoordinator_.dismissStillWatchingPrompt();
        details_.beginDetails(item);

        const std::string id = details_.item().id;
        const auto prefetchedSimilar = similarPrefetch_.cached(session_, id);
        if (prefetchedSimilar) details_.state().setSimilar(*prefetchedSimilar);
        similarPrefetch_.clearPending();

        error_.clear();
        const RequestEpoch::Token requestToken = contentEpoch_.beginToken();
        if (!detailsAsync_.load(session_, id, requestToken, !prefetchedSimilar.has_value())) {
            contentEpoch_.invalidate();
            error_ = "DETAILS LOAD COULD NOT BE STARTED";
        }
    }

    void openItemMenuForItem(const JellyfinItem& item) {
        if (item.id.empty() || !supportsItemContextMenu(item)) return;
        details_.item() = item;
        pushScreen(Screen::ItemMenu);
        details_.state().beginItemMenu();
        error_.clear();
        if (isSeerrItem(item)) return;

        if (!detailsAsync_.loadItemMenuDetail(session_, item.id)) error_ = "ITEM DETAILS COULD NOT BE STARTED";
    }

    [[nodiscard]] bool restoreHomeVisibilityForPlayback(const JellyfinItem& item) {
        return homeVisibility_.restoreForPlayback(item);
    }

private:
    static bool supportsItemContextMenu(const JellyfinItem& item) {
        return item.type == "Movie" || item.type == "Series" || item.type == "Episode" || item.type == "BoxSet";
    }

    void pushScreen(Screen screen) {
        navigation_.push(screen);
        screen_ = navigation_.current();
    }

    void replaceScreen(Screen screen) {
        navigation_.replace(screen);
        screen_ = navigation_.current();
    }

    void popScreen(DetailsScreenEffects& effects, Screen fallback) {
        screen_ = navigation_.popOr(fallback);
        effects.reloadBrowse = effects.reloadBrowse || screen_ == Screen::Browse;
    }

    void cancelContentLoadForNavigation() {
        if (!loading_) return;
        contentEpoch_.invalidate();
        loading_ = false;
    }

    void applyCommand(DetailsFlowCommand command, DetailsScreenEffects& effects) {
        if (command.cancelContentLoad) cancelContentLoadForNavigation();
        if (command.resetContinuationPrompt) playbackCoordinator_.resetContinuationPrompt();
        if (command.closeItemMenu) {
            popScreen(effects, Screen::Details);
            if ((command.action == DetailsFlowAction::BeginSeriesPlayAll ||
                 command.action == DetailsFlowAction::PlayExternal) &&
                screen_ != Screen::Details) {
                pushScreen(Screen::Details);
            }
        }

        switch (command.action) {
        case DetailsFlowAction::None:
            return;
        case DetailsFlowAction::ExitDetails:
            popScreen(effects, Screen::Home);
            return;
        case DetailsFlowAction::OpenItemMenu:
            openItemMenu();
            return;
        case DetailsFlowAction::OpenDetails:
            if (command.item) openDetails(*command.item, command.replaceCurrentDetails);
            return;
        case DetailsFlowAction::OpenEpisodes:
            if (command.item) openEpisodes(*command.item);
            return;
        case DetailsFlowAction::BeginPlayback:
            effects.action = DetailsScreenHostAction::BeginPlayback;
            return;
        case DetailsFlowAction::OpenSeasons:
            openSeasons();
            return;
        case DetailsFlowAction::BeginSeriesPlayAll:
            effects.action = DetailsScreenHostAction::BeginSeriesPlayAll;
            return;
        case DetailsFlowAction::ToggleFavorite:
            toggleFavoriteAsync();
            return;
        case DetailsFlowAction::TogglePlayed:
            togglePlayedAsync();
            return;
        case DetailsFlowAction::OpenCast:
            openCast();
            return;
        case DetailsFlowAction::BackToDetails:
            popScreen(effects, Screen::Details);
            return;
        case DetailsFlowAction::OpenPersonItems:
            if (command.person) openPersonItems(*command.person);
            return;
        case DetailsFlowAction::BackToCast:
            popScreen(effects, Screen::Cast);
            return;
        case DetailsFlowAction::OpenItemContext:
            if (command.item) openItemMenuForItem(*command.item);
            return;
        case DetailsFlowAction::ConfirmDeleteMedia:
            deleteCurrentItemAsync();
            return;
        case DetailsFlowAction::ConfirmDeleteRequest:
            effects.action = DetailsScreenHostAction::DeleteSeerrRequest;
            return;
        case DetailsFlowAction::PlayExternal:
            effects.action = DetailsScreenHostAction::PlayExternal;
            return;
        case DetailsFlowAction::ViewQueue:
            effects.action = DetailsScreenHostAction::ViewQueue;
            return;
        case DetailsFlowAction::ToggleHomeVisibility:
            toggleHiddenFromHome(effects);
            return;
        case DetailsFlowAction::RefreshMetadata:
            refreshCurrentItemMetadataAsync();
            return;
        case DetailsFlowAction::BackToSeasons:
            popScreen(effects, Screen::Seasons);
            return;
        }
    }

    void openCast() {
        if (!details_.beginCast()) return;
        pushScreen(Screen::Cast);
        error_.clear();
    }

    void openPersonItems(const JellyfinPerson& person) {
        if (!session_.valid() || !details_.beginPerson(person)) return;
        pushScreen(Screen::PersonItems);
        loading_ = true;
        error_.clear();
        const uint64_t generation = contentEpoch_.begin();
        if (detailsAsync_.loadPersonItems(session_, person.id, generation, 60)) return;
        contentEpoch_.invalidate();
        loading_ = false;
        error_ = "PERSON ITEMS COULD NOT BE STARTED";
    }

    void openItemMenu() {
        if (!details_.beginItemMenu()) return;
        pushScreen(Screen::ItemMenu);
        error_.clear();
    }

    void openSeasons() {
        if (loading_ || !details_.beginSeries()) return;
        pushScreen(Screen::Seasons);
        loading_ = true;
        error_.clear();
        const uint64_t generation = contentEpoch_.begin();
        if (detailsAsync_.loadSeasons(session_, details_.state().seriesDetail().id, generation)) return;
        contentEpoch_.invalidate();
        loading_ = false;
        error_ = "SEASONS LOAD COULD NOT BE STARTED";
    }

    void openEpisodes(const JellyfinItem& season) {
        if (loading_ || !details_.beginSeason(season)) return;
        pushScreen(Screen::Episodes);
        loading_ = true;
        error_.clear();
        const uint64_t generation = contentEpoch_.begin();
        if (detailsAsync_.loadEpisodes(session_, details_.state().seriesDetail().id, season.id, generation)) return;
        contentEpoch_.invalidate();
        loading_ = false;
        error_ = "EPISODES LOAD COULD NOT BE STARTED";
    }

    void toggleHiddenFromHome(DetailsScreenEffects& effects) {
        const auto hiding = homeVisibility_.toggle(details_.item(), home_, homeState_);
        if (!hiding) return;
        effects.persistSession = true;
        effects.notice = DetailsScreenNotice{
            .message = *hiding ? "HIDDEN FROM HOME" : "HOME VISIBILITY RESTORED",
            .duration = std::chrono::seconds(2),
        };
    }

    void toggleFavoriteAsync() {
        if (loading_ || contentMutation_.loading() || details_.item().id.empty()) return;
        const bool desired = !details_.item().favorite;
        contentMutation_.begin();
        error_.clear();
        if (itemMutationAsync_.setFavorite(session_, details_.item(), desired, sessionEpoch_.snapshot())) return;
        contentMutation_.finish();
        error_ = "FAVORITE UPDATE COULD NOT BE STARTED";
    }

    void togglePlayedAsync() {
        if (loading_ || contentMutation_.loading() || details_.item().id.empty()) return;
        const uint64_t sessionEpoch = sessionEpoch_.snapshot();
        const bool hiddenFromHome = homeVisibility_.isHidden(details_.item());
        PlayedMutationPreparation preparation =
            contentMutation_.preparePlayedToggle(home_, homeState_, browseState_, searchState_, details_.state(),
                                                 queueState_, details_.item(), hiddenFromHome, sessionEpoch);
        contentMutation_.begin();
        error_.clear();
        if (itemMutationAsync_.setPlayed(session_, preparation.item, preparation.desired, sessionEpoch,
                                         preparation.nextUpReplacementIndex))
            return;
        contentMutation_.rejectPlayedToggle(preparation.item, hiddenFromHome, home_, homeState_, browseState_,
                                            searchState_, details_.state(), queueState_, details_.item());
        error_ = "PLAYED UPDATE COULD NOT BE STARTED";
    }

    void refreshCurrentItemMetadataAsync() {
        if (loading_ || contentMutation_.loading() || details_.item().id.empty()) return;
        contentMutation_.begin();
        error_.clear();
        if (itemMutationAsync_.refreshMetadata(session_, details_.item().id, sessionEpoch_.snapshot())) return;
        contentMutation_.finish();
        error_ = "METADATA REFRESH COULD NOT BE STARTED";
    }

    void deleteCurrentItemAsync() {
        if (loading_ || contentMutation_.loading() || details_.item().id.empty() || !details_.item().canDelete) return;
        contentMutation_.begin();
        error_.clear();
        if (itemMutationAsync_.deleteItem(session_, details_.item().id, sessionEpoch_.snapshot())) return;
        contentMutation_.finish();
        details_.state().setDeleteConfirmation(false);
        error_ = "DELETE COULD NOT BE STARTED";
    }

    DetailsFlow& details_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    JellyfinSession& session_;
    bool& loading_;
    std::string& error_;
    RequestEpoch& contentEpoch_;
    RequestEpoch& sessionEpoch_;
    DetailsAsync& detailsAsync_;
    ItemMutationAsync& itemMutationAsync_;
    ContentMutationFlow& contentMutation_;
    JellyfinHomeData& home_;
    HomeScreenState& homeState_;
    BrowseScreenState& browseState_;
    SearchScreenState& searchState_;
    PlaybackQueueState& queueState_;
    HomeVisibility& homeVisibility_;
    SettingsFlow& settingsFlow_;
    PlaybackCoordinator& playbackCoordinator_;
    SimilarPrefetchController& similarPrefetch_;
};
