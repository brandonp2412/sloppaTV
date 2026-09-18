#pragma once

#include "seerr_domain.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class SeerrSearchDispatchAction {
    None,
    DeferredForConnection,
    Submit,
};

struct SeerrSearchDispatchPlan {
    SeerrSearchDispatchAction action = SeerrSearchDispatchAction::None;
    bool resultsChanged = false;
    SeerrEndpoint endpoint;
    std::string query;

    [[nodiscard]] bool ready() const { return action == SeerrSearchDispatchAction::Submit; }
};

struct SeerrSearchCompletionPlan {
    bool resultsChanged = false;
    bool pendingChanged = false;
    bool reconnect = false;
};

struct SeerrSearchSchedulePlan {
    bool resultsChanged = false;
    bool invalidateRequest = false;
};

template <typename AsyncExecutor> class SeerrSearchCoordinator {
public:
    SeerrSearchCoordinator(SeerrDomainState& domain, AsyncExecutor& async) : domain_(domain), async_(async) {}

    [[nodiscard]] SeerrSearchSchedulePlan schedule(std::string_view query, SeerrSearchState::Clock::time_point now,
                                                   bool configured) {
        const bool changed = domain_.scheduleSearch(query, now, configured);
        if (changed) async_.cancelSearch();
        return {
            .resultsChanged = changed,
            .invalidateRequest = changed,
        };
    }

    void cancel() {
        domain_.cancelSearch();
        async_.cancelSearch();
    }

    void reset() { domain_.resetSearch(); }

    [[nodiscard]] bool debounceDue(SeerrSearchState::Clock::time_point now) const {
        return domain_.searchDebounceDue(now);
    }

    [[nodiscard]] bool prepareReconnectRetry(std::string_view query, SeerrSearchState::Clock::time_point now) {
        return domain_.scheduleSearch(query, now, false);
    }

    [[nodiscard]] SeerrSearchDispatchPlan prepareImmediate(SeerrEndpoint endpoint, std::string query) {
        if (deferForConnection()) {
            return {
                .action = SeerrSearchDispatchAction::DeferredForConnection,
                .resultsChanged = false,
                .endpoint = {},
                .query = {},
            };
        }

        const auto start = domain_.beginImmediateSearch(query, endpoint.configured());
        if (!start.started) {
            return {
                .action = SeerrSearchDispatchAction::None,
                .resultsChanged = start.resultsChanged,
                .endpoint = {},
                .query = {},
            };
        }
        return {
            .action = SeerrSearchDispatchAction::Submit,
            .resultsChanged = start.resultsChanged,
            .endpoint = std::move(endpoint),
            .query = std::move(query),
        };
    }

    [[nodiscard]] SeerrSearchDispatchPlan prepareDue(SeerrEndpoint endpoint, std::string query,
                                                      SeerrSearchState::Clock::time_point now) {
        if (deferForConnection()) {
            return {
                .action = SeerrSearchDispatchAction::DeferredForConnection,
                .resultsChanged = false,
                .endpoint = {},
                .query = {},
            };
        }
        if (!domain_.beginDueSearch(now)) return {};
        return {
            .action = SeerrSearchDispatchAction::Submit,
            .resultsChanged = false,
            .endpoint = std::move(endpoint),
            .query = std::move(query),
        };
    }

    void submit(SeerrSearchDispatchPlan plan, uint64_t generation) {
        if (!plan.ready()) return;
        async_.search(std::move(plan.endpoint), std::move(plan.query), generation);
    }

    void abandonCompletion() { domain_.stopSearchLoading(); }

    [[nodiscard]] SeerrSearchCompletionPlan completeFailure(std::string_view query, std::string error,
                                                            bool hasSessionCookie) {
        const bool resultsChanged = domain_.failSearch(query, error);
        return {
            .resultsChanged = resultsChanged,
            .pendingChanged = false,
            .reconnect = domain_.completeSearchFailure(error, hasSessionCookie),
        };
    }

    [[nodiscard]] SeerrSearchCompletionPlan completeSuccess(std::string_view query,
                                                            std::vector<SeerrMediaItem> results,
                                                            SeerrRequestState::TimePoint now) {
        const bool pendingChanged = domain_.completeSearchSuccess(results, now);
        return {
            .resultsChanged = domain_.finishSearch(query, std::move(results)),
            .pendingChanged = pendingChanged,
            .reconnect = false,
        };
    }

private:
    [[nodiscard]] bool deferForConnection() {
        if (!domain_.deferSearchIfConnecting()) return false;
        domain_.stopSearchLoading();
        return true;
    }

    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
