#pragma once

#include "seerr_domain.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SeerrStorageRefreshCompletionPlan {
    SeerrDomainState::StorageRefreshCompletion domain;
    std::optional<SeerrMediaItem> pendingRequest;
    std::size_t targetCount = 0;
};

struct SeerrPendingRefreshCompletionPlan {
    SeerrDomainState::RefreshOutcome outcome = SeerrDomainState::RefreshOutcome::Failed;
    std::size_t pendingCount = 0;
};

template <typename AsyncExecutor> class SeerrRefreshCoordinator {
public:
    SeerrRefreshCoordinator(SeerrDomainState& domain, AsyncExecutor& async) : domain_(domain), async_(async) {}

    SeerrDomainState::RefreshStartAction
    refreshStorage(SeerrEndpoint endpoint, bool force,
                   SeerrStorageState::TimePoint now = SeerrStorageState::Clock::now()) {
        const auto action = domain_.prepareStorageRefresh(endpoint, force, now);
        if (action != SeerrDomainState::RefreshStartAction::Submit) return action;
        if (async_.refreshStorage(std::move(endpoint))) return action;
        domain_.storage().invalidateRefresh();
        return SeerrDomainState::RefreshStartAction::None;
    }

    SeerrDomainState::RefreshStartAction
    refreshPending(SeerrEndpoint endpoint, SeerrRequestState::TimePoint now = SeerrRequestState::Clock::now()) {
        const auto action = domain_.preparePendingRefresh(endpoint);
        if (action != SeerrDomainState::RefreshStartAction::Submit) return action;
        if (async_.refreshPending(std::move(endpoint))) return action;
        domain_.requests().invalidatePendingRefresh(now);
        return SeerrDomainState::RefreshStartAction::None;
    }

    [[nodiscard]] SeerrStorageRefreshCompletionPlan completeStorage(const SeerrEndpoint& requestedEndpoint,
                                                                    const SeerrEndpoint& currentEndpoint, bool ok,
                                                                    std::vector<SeerrStorageTarget> targets,
                                                                    std::string error, bool driveSelectionEnabled,
                                                                    SeerrStorageState::TimePoint now) {
        auto completion =
            domain_.completeStorageRefresh(requestedEndpoint, currentEndpoint, ok, std::move(targets), error, now);
        if (completion.outcome != SeerrDomainState::RefreshOutcome::Applied) {
            return {
                .domain = completion,
                .pendingRequest = std::nullopt,
                .targetCount = 0,
            };
        }
        return {
            .domain = completion,
            .pendingRequest = domain_.takePendingStorageRequest(driveSelectionEnabled),
            .targetCount = domain_.storageTargets().size(),
        };
    }

    [[nodiscard]] SeerrPendingRefreshCompletionPlan completePending(const SeerrEndpoint& requestedEndpoint,
                                                                    const SeerrEndpoint& currentEndpoint, bool ok,
                                                                    std::vector<SeerrMediaItem> pending,
                                                                    SeerrRequestState::TimePoint now) {
        const auto outcome =
            domain_.completePendingRefresh(requestedEndpoint, currentEndpoint, ok, std::move(pending), now);
        return {
            .outcome = outcome,
            .pendingCount = outcome == SeerrDomainState::RefreshOutcome::Applied ? domain_.pendingRequests().size() : 0,
        };
    }

private:
    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
