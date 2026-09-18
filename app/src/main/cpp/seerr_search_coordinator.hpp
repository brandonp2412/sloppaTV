#pragma once

#include "seerr_domain.hpp"

#include <cstdint>
#include <string>
#include <utility>

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

template <typename AsyncExecutor> class SeerrSearchCoordinator {
public:
    SeerrSearchCoordinator(SeerrDomainState& domain, AsyncExecutor& async) : domain_(domain), async_(async) {}

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

private:
    [[nodiscard]] bool deferForConnection() {
        if (!domain_.deferSearchIfConnecting()) return false;
        domain_.stopSearchLoading();
        return true;
    }

    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
