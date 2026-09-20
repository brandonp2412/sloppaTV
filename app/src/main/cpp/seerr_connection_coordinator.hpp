#pragma once

#include "jellyfin_types.hpp"
#include "seerr_domain.hpp"

#include <string>
#include <string_view>
#include <utility>

struct SeerrConnectPlan {
    SeerrDomainState::ConnectAction action = SeerrDomainState::ConnectAction::MissingServer;
    std::string server;
    JellyfinSession jellyfin;

    [[nodiscard]] bool ready() const { return action == SeerrDomainState::ConnectAction::Submit; }
};

struct SeerrConnectCompletionPlan {
    SeerrDomainState::ConnectCompletionAction action = SeerrDomainState::ConnectCompletionAction::Stale;
    SeerrConnectionState::DeferredWork deferred;
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

    bool submit(SeerrConnectPlan plan, bool announce) {
        if (!plan.ready()) return false;
        if (async_.connect(std::move(plan.server), std::move(plan.jellyfin), announce)) return true;
        domain_.connection().failConnect();
        return false;
    }

    [[nodiscard]] SeerrConnectCompletionPlan complete(bool ok, bool authenticationStageFailure,
                                                      std::string_view requestedServer,
                                                      std::string_view requestedJellyfinUserId,
                                                      std::string_view currentServer,
                                                      std::string_view currentJellyfinUserId) {
        const bool currentRequest =
            requestedServer == currentServer && requestedJellyfinUserId == currentJellyfinUserId;
        const auto action = domain_.completeConnect(ok, authenticationStageFailure, currentRequest);
        if (action != SeerrDomainState::ConnectCompletionAction::Connected) {
            return {
                .action = action,
                .deferred = {},
            };
        }
        return {
            .action = action,
            .deferred = domain_.takeDeferredConnectionWork(),
        };
    }

private:
    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
