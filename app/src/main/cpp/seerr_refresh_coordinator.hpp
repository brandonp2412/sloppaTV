#pragma once

#include "seerr_domain.hpp"

#include <utility>

template <typename AsyncExecutor> class SeerrRefreshCoordinator {
public:
    SeerrRefreshCoordinator(SeerrDomainState& domain, AsyncExecutor& async) : domain_(domain), async_(async) {}

    SeerrDomainState::RefreshStartAction refreshStorage(
        SeerrEndpoint endpoint, bool force, SeerrStorageState::TimePoint now = SeerrStorageState::Clock::now()) {
        const auto action = domain_.prepareStorageRefresh(endpoint, force, now);
        if (action == SeerrDomainState::RefreshStartAction::Submit) {
            async_.refreshStorage(std::move(endpoint));
        }
        return action;
    }

    SeerrDomainState::RefreshStartAction refreshPending(SeerrEndpoint endpoint) {
        const auto action = domain_.preparePendingRefresh(endpoint);
        if (action == SeerrDomainState::RefreshStartAction::Submit) {
            async_.refreshPending(std::move(endpoint));
        }
        return action;
    }

private:
    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
