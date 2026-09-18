#pragma once

#include "jellyfin_types.hpp"
#include "seerr_domain.hpp"

#include <string>
#include <utility>

struct SeerrConnectPlan {
    SeerrDomainState::ConnectAction action = SeerrDomainState::ConnectAction::MissingServer;
    std::string server;
    JellyfinSession jellyfin;

    [[nodiscard]] bool ready() const { return action == SeerrDomainState::ConnectAction::Submit; }
};

template <typename AsyncExecutor> class SeerrConnectionCoordinator {
public:
    SeerrConnectionCoordinator(SeerrDomainState& domain, AsyncExecutor& async) : domain_(domain), async_(async) {}

    [[nodiscard]] SeerrConnectPlan prepare(std::string server, JellyfinSession jellyfin) {
        const auto action = domain_.prepareConnect(server, jellyfin.valid());
        if (action != SeerrDomainState::ConnectAction::Submit) {
            return {
                .action = action,
                .server = {},
                .jellyfin = {},
            };
        }
        return {
            .action = action,
            .server = std::move(server),
            .jellyfin = std::move(jellyfin),
        };
    }

    void submit(SeerrConnectPlan plan, bool announce) {
        if (!plan.ready()) return;
        async_.connect(std::move(plan.server), std::move(plan.jellyfin), announce);
    }

private:
    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
