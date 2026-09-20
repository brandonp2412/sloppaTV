#pragma once

#include "seerr_domain.hpp"

#include <optional>
#include <utility>

struct SeerrRequestDispatchPlan {
    SeerrDomainState::RequestAction action = SeerrDomainState::RequestAction::Invalid;
    bool refreshStorage = false;
    SeerrEndpoint endpoint;
    SeerrMediaItem item;
    std::optional<SeerrStorageTarget> target;

    [[nodiscard]] bool ready() const { return action == SeerrDomainState::RequestAction::Submit; }
};

template <typename AsyncExecutor> class SeerrRequestCoordinator {
public:
    SeerrRequestCoordinator(SeerrDomainState& domain, AsyncExecutor& async) : domain_(domain), async_(async) {}

    [[nodiscard]] SeerrRequestDispatchPlan prepare(const SeerrMediaItem& item, SeerrEndpoint endpoint, bool selectDrive,
                                                   bool skipDrivePrompt,
                                                   const SeerrStorageTarget* selectedTarget = nullptr) {
        auto domainPlan = domain_.prepareRequest(item, endpoint, selectDrive, skipDrivePrompt, selectedTarget);
        if (domainPlan.action != SeerrDomainState::RequestAction::Submit) {
            return {
                .action = domainPlan.action,
                .refreshStorage = domainPlan.refreshStorage,
                .endpoint = {},
                .item = {},
                .target = std::nullopt,
            };
        }
        return {
            .action = domainPlan.action,
            .refreshStorage = false,
            .endpoint = std::move(endpoint),
            .item = item,
            .target = std::move(domainPlan.target),
        };
    }

    bool submit(SeerrRequestDispatchPlan plan) {
        if (!plan.ready()) return false;
        return async_.requestMedia(std::move(plan.endpoint), std::move(plan.item), std::move(plan.target));
    }

    [[nodiscard]] SeerrDomainState::RequestMutationCompletion complete(const SeerrEndpoint& requestedEndpoint,
                                                                       const SeerrEndpoint& currentEndpoint,
                                                                       SeerrMediaItem requestedItem, int requestId,
                                                                       bool ok, SeerrRequestState::TimePoint now) {
        const std::string itemId = requestedItem.id;
        auto completion =
            domain_.completeRequest(requestedEndpoint, currentEndpoint, std::move(requestedItem), requestId, ok, now);
        if (completion.outcome == SeerrDomainState::MutationOutcome::Applied) {
            domain_.markSearchRequested(itemId, completion.status, requestId);
        }
        return completion;
    }

    [[nodiscard]] SeerrDomainState::MutationOutcome completeDelete(const SeerrEndpoint& requestedEndpoint,
                                                                   const SeerrEndpoint& currentEndpoint,
                                                                   const std::string& itemId, int requestId, bool ok) {
        const auto outcome = domain_.completeDeleteRequest(requestedEndpoint, currentEndpoint, itemId, requestId, ok);
        if (outcome == SeerrDomainState::MutationOutcome::Applied) domain_.markSearchUnrequested(itemId);
        return outcome;
    }

private:
    SeerrDomainState& domain_;
    AsyncExecutor& async_;
};
