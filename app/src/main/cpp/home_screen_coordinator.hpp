#pragma once

#include "app_screen.hpp"
#include "home_async_executor.hpp"
#include "home_completion_flow.hpp"
#include "home_navigation_controller.hpp"
#include "home_screen.hpp"
#include "request_epoch.hpp"
#include "screen_navigation_key.hpp"
#include "seerr_home_projection.hpp"
#include "seerr_media.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class HomeScreenHostAction { None, FinishActivity, OpenProfiles, OpenSearch, OpenSettings };

struct HomeScreenEffects {
    HomeScreenHostAction action = HomeScreenHostAction::None;
    std::optional<JellyfinItem> openContextItem;
    std::optional<JellyfinItem> openLibraryItem;
    std::optional<JellyfinItem> openDetailsItem;
    std::optional<JellyfinItem> prefetchSimilarItem;
    std::optional<std::chrono::steady_clock::time_point> rowSlideStarted;
};

struct HomeCoreScreenEffects {
    bool active = false;
    bool sessionExpired = false;
    std::optional<int> retryDelaySeconds;
    std::optional<long long> readyMs;
    std::optional<JellyfinItem> openDetailsItem;
    std::optional<std::string> searchQuery;
    bool refreshSeerrPending = false;
    std::vector<JellyfinItem> secondaryViews;
    int coreRestoredRow = -1;
};

struct HomeSecondaryScreenEffects {
    bool active = false;
    std::optional<long long> readyMs;
};

template <typename HomeAsync, typename UiPresentation> class HomeScreenCoordinator {
public:
    HomeScreenCoordinator(HomeScreenState& state, JellyfinHomeData& home, JellyfinSession& session,
                          const std::vector<SeerrMediaItem>& pendingSeerr, bool& homeLoading,
                          std::chrono::steady_clock::time_point& homeRetryAt, RequestEpoch& homeEpoch,
                          HomeCompletionFlow& completionFlow, Screen& screen, std::string& error,
                          std::string& pendingDeepLinkItemId, std::string& pendingSearchQuery, HomeAsync& homeAsync,
                          UiPresentation& uiPresentation, std::recursive_mutex& stateMutex)
        : state_(state), home_(home), session_(session), pendingSeerr_(pendingSeerr), homeLoading_(homeLoading),
          homeRetryAt_(homeRetryAt), homeEpoch_(homeEpoch), completionFlow_(completionFlow), screen_(screen),
          error_(error), pendingDeepLinkItemId_(pendingDeepLinkItemId), pendingSearchQuery_(pendingSearchQuery),
          homeAsync_(homeAsync), uiPresentation_(uiPresentation), stateMutex_(stateMutex) {}

    [[nodiscard]] HomeScreenEffects handle(ScreenNavigationKey key) {
        HomeScreenEffects effects;
        const HomeNavigationAction navigation =
            HomeNavigationController::handle(state_, key, home_.rows, pendingSeerr_);
        switch (navigation.type) {
        case HomeNavigationActionType::None:
            return effects;
        case HomeNavigationActionType::FinishActivity:
            effects.action = HomeScreenHostAction::FinishActivity;
            return effects;
        case HomeNavigationActionType::OpenProfiles:
            effects.action = HomeScreenHostAction::OpenProfiles;
            return effects;
        case HomeNavigationActionType::OpenSearch:
            effects.action = HomeScreenHostAction::OpenSearch;
            return effects;
        case HomeNavigationActionType::OpenSettings:
            effects.action = HomeScreenHostAction::OpenSettings;
            return effects;
        case HomeNavigationActionType::OpenContext:
            effects.openContextItem = navigation.item;
            return effects;
        case HomeNavigationActionType::OpenLibrary:
            effects.openLibraryItem = navigation.item;
            return effects;
        case HomeNavigationActionType::OpenDetails:
            effects.openDetailsItem = navigation.item;
            return effects;
        case HomeNavigationActionType::FinalizeNavigation:
            effects.rowSlideStarted =
                beginRowSlide(navigation.previousFirstVisibleRow, navigation.currentFirstVisibleRow);
            if (navigation.prefetchRow >= 0) {
                uiPresentation_.prefetchHomeWindow(session_, home_, navigation.prefetchRow,
                                                   navigation.prefetchSelection);
                if (navigation.prefetchRow < static_cast<int>(home_.rows.size())) {
                    const auto& items = home_.rows[static_cast<size_t>(navigation.prefetchRow)].items;
                    if (navigation.prefetchSelection >= 0 &&
                        navigation.prefetchSelection < static_cast<int>(items.size())) {
                        effects.prefetchSimilarItem = items[static_cast<size_t>(navigation.prefetchSelection)];
                    }
                }
            }
            return effects;
        }
        return effects;
    }

    void load() {
        const JellyfinSession session = session_;
        if (!session.valid()) return;
        HomeSelectionSnapshot snapshot;
        {
            std::scoped_lock lock(stateMutex_);
            snapshot = state_.snapshot(home_.rows);
            homeLoading_ = true;
            homeRetryAt_ = {};
        }
        homeAsync_.loadCore(session, homeEpoch_.begin(), std::move(snapshot), std::chrono::steady_clock::now());
    }

    [[nodiscard]] HomeCoreScreenEffects complete(HomeCoreCompletion& completion) {
        HomeCoreScreenEffects result;
        HomeCoreFlowEffects effects =
            completionFlow_.complete(completion, homeEpoch_.active(completion.generation), screen_ == Screen::Home,
                                     std::chrono::steady_clock::now());
        result.active = effects.active;
        result.sessionExpired = effects.sessionExpired;
        result.retryDelaySeconds = effects.retryDelaySeconds;
        if (!effects.active || effects.sessionExpired) return result;

        if (effects.visibleError) error_ = std::move(*effects.visibleError);
        if (!effects.loaded) return result;

        syncSeerrHome();
        if (effects.prefetch)
            uiPresentation_.prefetchHomeWindow(session_, home_, effects.prefetch->row, effects.prefetch->selection);
        result.readyMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                               completion.startedAt)
                             .count();

        if (!pendingDeepLinkItemId_.empty()) {
            JellyfinItem linked;
            linked.id = std::move(pendingDeepLinkItemId_);
            pendingDeepLinkItemId_.clear();
            result.openDetailsItem = std::move(linked);
            return result;
        }
        if (!pendingSearchQuery_.empty()) {
            result.searchQuery = std::move(pendingSearchQuery_);
            pendingSearchQuery_.clear();
            return result;
        }

        result.refreshSeerrPending = true;
        result.secondaryViews = std::move(effects.secondaryViews);
        result.coreRestoredRow = effects.coreRestoredRow;
        return result;
    }

    void loadSecondary(HomeCoreCompletion& completion, HomeCoreScreenEffects effects) {
        homeAsync_.loadSecondary(session_, completion.generation, std::move(effects.secondaryViews),
                                 std::move(completion.snapshot), effects.coreRestoredRow, completion.startedAt);
    }

    [[nodiscard]] HomeSecondaryScreenEffects complete(HomeSecondaryCompletion& completion) {
        HomeSecondaryScreenEffects result;
        HomeSecondaryFlowEffects effects =
            completionFlow_.complete(completion, homeEpoch_.active(completion.generation), screen_ == Screen::Home);
        result.active = effects.active;
        if (!effects.active) return result;
        if (effects.visibleError) error_ = std::move(*effects.visibleError);
        if (!effects.loaded) return result;
        if (effects.prefetch)
            uiPresentation_.prefetchHomeWindow(session_, home_, effects.prefetch->row, effects.prefetch->selection);
        result.readyMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                               completion.startedAt)
                             .count();
        return result;
    }

    void syncSeerrHome() {
        const HomeSelectionSnapshot snapshot = state_.snapshot(home_.rows);
        projectSeerrHomeRow(home_.rows, pendingSeerr_);
        HomeRestorePlan restore = HomeScreenState::restorePlan(snapshot, home_.rows);
        state_.setSelections(std::move(restore.selections));
        state_.setRow(restore.focusedRow);
        state_.updateViewport(static_cast<int>(home_.rows.size()));
    }

    [[nodiscard]] int slideFromFirst() const { return slideFromFirst_; }

    [[nodiscard]] int slideToFirst() const { return slideToFirst_; }

    [[nodiscard]] std::chrono::steady_clock::time_point slideStarted() const { return slideStarted_; }

private:
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> beginRowSlide(int fromFirst, int toFirst) {
        if (fromFirst == toFirst) return std::nullopt;
        slideFromFirst_ = fromFirst;
        slideToFirst_ = toFirst;
        slideStarted_ = std::chrono::steady_clock::now();
        return slideStarted_;
    }

    HomeScreenState& state_;
    JellyfinHomeData& home_;
    JellyfinSession& session_;
    const std::vector<SeerrMediaItem>& pendingSeerr_;
    bool& homeLoading_;
    std::chrono::steady_clock::time_point& homeRetryAt_;
    RequestEpoch& homeEpoch_;
    HomeCompletionFlow& completionFlow_;
    Screen& screen_;
    std::string& error_;
    std::string& pendingDeepLinkItemId_;
    std::string& pendingSearchQuery_;
    HomeAsync& homeAsync_;
    UiPresentation& uiPresentation_;
    std::recursive_mutex& stateMutex_;
    int slideFromFirst_ = 0;
    int slideToFirst_ = 0;
    std::chrono::steady_clock::time_point slideStarted_{};
};
