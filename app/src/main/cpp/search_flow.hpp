#pragma once

#include "jellyfin_search_executor.hpp"
#include "request_epoch.hpp"
#include "search_completion_controller.hpp"
#include "search_navigation_controller.hpp"
#include "seerr_search_coordinator.hpp"

#include <chrono>
#include <string_view>
#include <utility>

struct SearchScheduleEffects {
    bool clearError = false;
};

struct SearchDispatchEffects {
    bool clearError = false;
    bool refreshSeerrStorage = false;

    void merge(const SearchDispatchEffects& other) {
        clearError = clearError || other.clearError;
        refreshSeerrStorage = refreshSeerrStorage || other.refreshSeerrStorage;
    }
};

template <typename SeerrAsync, typename JellyfinSearchAsync> class SearchFlow {
public:
    SearchFlow(SeerrDomainState& seerrDomain, SeerrAsync& seerrAsync, JellyfinSearchAsync& jellyfinSearch,
               RequestEpoch& jellyfinEpoch, RequestEpoch& seerrEpoch)
        : coordinator_(seerrDomain, seerrAsync), state_(seerrDomain.searchResults()), jellyfinSearch_(jellyfinSearch),
          jellyfinEpoch_(jellyfinEpoch), seerrEpoch_(seerrEpoch) {}

    [[nodiscard]] SearchScreenState& state() { return state_; }

    [[nodiscard]] const SearchScreenState& state() const { return state_; }

    [[nodiscard]] SearchNavigationAction handleNavigation(ScreenNavigationKey key, int columns) {
        SearchNavigationAction action = SearchNavigationController::handle(state_, key, columns);
        if (action.type == SearchNavigationActionType::Exit) cancel();
        return action;
    }

    [[nodiscard]] SearchScheduleEffects scheduleLive(std::chrono::steady_clock::time_point now, bool seerrConfigured) {
        jellyfinEpoch_.invalidate();
        const auto seerrPlan = coordinator_.schedule(state_.query(), now, seerrConfigured);
        if (seerrPlan.resultsChanged) state_.refreshSeerrResults();
        if (seerrPlan.invalidateRequest) seerrEpoch_.invalidate();

        SearchScheduleEffects effects;
        effects.clearError = !state_.scheduleDebounce(now);
        return effects;
    }

    void cancelSeerrSearch() { coordinator_.cancel(); }

    void cancel() {
        state_.cancelPending();
        coordinator_.cancel();
        jellyfinEpoch_.invalidate();
        seerrEpoch_.invalidate();
    }

    [[nodiscard]] const JellyfinItem* selectedResult() const {
        if (state_.keyboard() || state_.selection() < 0 ||
            state_.selection() >= static_cast<int>(state_.results().size())) {
            return nullptr;
        }
        return &state_.results()[static_cast<std::size_t>(state_.selection())];
    }

    void reset() {
        coordinator_.reset();
        state_.reset();
    }

    [[nodiscard]] SearchDispatchEffects runDue(bool activeScreen, const JellyfinSession& session,
                                               SeerrEndpoint endpoint, std::chrono::steady_clock::time_point now) {
        if (!activeScreen) {
            cancel();
            return {};
        }

        SearchDispatchEffects effects;
        if (state_.debounceDue(now)) effects.merge(search(session, endpoint, false, now));
        if (coordinator_.debounceDue(now)) effects.merge(searchSeerr(std::move(endpoint), false, now));
        return effects;
    }

    [[nodiscard]] SearchDispatchEffects
    search(const JellyfinSession& session, SeerrEndpoint endpoint, bool includeSeerrImmediately = true,
           std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) {
        if (state_.query().empty()) {
            coordinator_.reset();
            state_.refreshSeerrResults();
        }
        if (!session.valid() || !state_.beginSearch()) return {};

        const std::string query = state_.query();
        jellyfinSearch_.search(session, query, jellyfinEpoch_.begin());

        SearchDispatchEffects effects;
        effects.clearError = true;
        if (includeSeerrImmediately) effects.merge(searchSeerr(std::move(endpoint), true, now));
        return effects;
    }

    [[nodiscard]] SearchDispatchEffects searchSeerr(SeerrEndpoint endpoint, bool immediate,
                                                    std::chrono::steady_clock::time_point now) {
        auto plan = immediate ? coordinator_.prepareImmediate(std::move(endpoint), state_.query())
                              : coordinator_.prepareDue(std::move(endpoint), state_.query(), now);
        if (plan.resultsChanged) state_.refreshSeerrResults();
        if (!plan.ready()) return {};

        coordinator_.submit(std::move(plan), seerrEpoch_.begin());
        return SearchDispatchEffects{.refreshSeerrStorage = true};
    }

    [[nodiscard]] SearchCompletionEffects complete(JellyfinSearchCompletion& completion, bool activeScreen) {
        return SearchCompletionController::apply(completion, jellyfinEpoch_.active(completion.generation), activeScreen,
                                                 state_);
    }

    [[nodiscard]] SearchCompletionEffects complete(SeerrSearchCompletion& completion, bool activeScreen,
                                                   bool hasSessionCookie, std::chrono::steady_clock::time_point now) {
        return SearchCompletionController::apply(completion, seerrEpoch_.active(completion.generation), activeScreen,
                                                 hasSessionCookie, state_, coordinator_, now);
    }

    [[nodiscard]] bool prepareReconnectRetry(std::chrono::steady_clock::time_point now) {
        const bool changed = coordinator_.prepareReconnectRetry(state_.query(), now);
        if (changed) state_.refreshSeerrResults();
        return changed;
    }

private:
    SeerrSearchCoordinator<SeerrAsync> coordinator_;
    SearchScreenState state_;
    JellyfinSearchAsync& jellyfinSearch_;
    RequestEpoch& jellyfinEpoch_;
    RequestEpoch& seerrEpoch_;
};
