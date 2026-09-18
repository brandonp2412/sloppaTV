#pragma once

#include "seerr_auth.hpp"
#include "seerr_connection_state.hpp"
#include "seerr_request_state.hpp"
#include "seerr_storage_state.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class SeerrDomainState {
public:
    enum class RequestAction {
        Invalid,
        AlreadyRequested,
        NotConfigured,
        DeferredForConnection,
        ChooseStorage,
        Submit,
    };

    struct RequestPlan {
        RequestAction action = RequestAction::Invalid;
        bool refreshStorage = false;
        std::optional<SeerrStorageTarget> target;
    };

    enum class RefreshOutcome {
        StaleEndpoint,
        Failed,
        Applied,
    };

    enum class MutationOutcome {
        StaleEndpoint,
        Failed,
        Applied,
    };

    struct RequestMutationCompletion {
        MutationOutcome outcome = MutationOutcome::Failed;
        std::string status;
    };

    struct StorageRefreshCompletion {
        RefreshOutcome outcome = RefreshOutcome::Failed;
        bool reconnect = false;
        bool clearedPendingRequest = false;
    };

    [[nodiscard]] SeerrConnectionState& connection() { return connection_; }

    [[nodiscard]] const SeerrConnectionState& connection() const { return connection_; }

    [[nodiscard]] SeerrRequestState& requests() { return requests_; }

    [[nodiscard]] const SeerrRequestState& requests() const { return requests_; }

    [[nodiscard]] SeerrStorageState& storage() { return storage_; }

    [[nodiscard]] const SeerrStorageState& storage() const { return storage_; }

    [[nodiscard]] RequestPlan prepareRequest(const SeerrMediaItem& item, const SeerrEndpoint& endpoint,
                                             bool selectDrive, bool skipDrivePrompt,
                                             const SeerrStorageTarget* selectedTarget = nullptr) {
        if (!item.valid()) return {};
        if (item.requested) {
            return {
                .action = RequestAction::AlreadyRequested,
                .refreshStorage = false,
                .target = std::nullopt,
            };
        }
        if (!endpoint.configured()) {
            return {
                .action = RequestAction::NotConfigured,
                .refreshStorage = false,
                .target = std::nullopt,
            };
        }
        if (connection_.connecting()) {
            connection_.deferRequest(item);
            return {
                .action = RequestAction::DeferredForConnection,
                .refreshStorage = false,
                .target = std::nullopt,
            };
        }
        if (selectDrive && !skipDrivePrompt && selectedTarget == nullptr) {
            return {
                .action = RequestAction::ChooseStorage,
                .refreshStorage = storage_.empty(),
                .target = std::nullopt,
            };
        }
        return {
            .action = RequestAction::Submit,
            .target = selectedTarget == nullptr ? std::nullopt : std::optional<SeerrStorageTarget>(*selectedTarget),
        };
    }

    bool deferSearchIfConnecting() {
        if (!connection_.connecting()) return false;
        connection_.deferSearchRetry();
        return true;
    }

    [[nodiscard]] MutationOutcome completeDeleteRequest(const SeerrEndpoint& requestedEndpoint,
                                                        const SeerrEndpoint& currentEndpoint, std::string_view itemId,
                                                        int requestId, bool ok) {
        if (!requestedEndpoint.matches(currentEndpoint.server, currentEndpoint.auth)) {
            return MutationOutcome::StaleEndpoint;
        }
        if (!ok) return MutationOutcome::Failed;
        requests_.erasePending(itemId, requestId);
        return MutationOutcome::Applied;
    }

    [[nodiscard]] RequestMutationCompletion completeRequest(const SeerrEndpoint& requestedEndpoint,
                                                            const SeerrEndpoint& currentEndpoint,
                                                            SeerrMediaItem requestedItem, int requestId, bool ok,
                                                            SeerrRequestState::TimePoint now) {
        if (!requestedEndpoint.matches(currentEndpoint.server, currentEndpoint.auth)) {
            return {
                .outcome = MutationOutcome::StaleEndpoint,
                .status = {},
            };
        }
        if (!ok) {
            return {
                .outcome = MutationOutcome::Failed,
                .status = {},
            };
        }
        std::string status = requestedItem.television() ? "Queued" : "Queued for download";
        requests_.markRequestSucceeded(std::move(requestedItem), requestId, status, now);
        return {
            .outcome = MutationOutcome::Applied,
            .status = std::move(status),
        };
    }

    [[nodiscard]] RefreshOutcome completePendingRefresh(const SeerrEndpoint& requestedEndpoint,
                                                        const SeerrEndpoint& currentEndpoint, bool ok,
                                                        std::vector<SeerrMediaItem> pending,
                                                        SeerrRequestState::TimePoint now) {
        if (!requestedEndpoint.matches(currentEndpoint.server, currentEndpoint.auth)) {
            requests_.invalidatePendingRefresh(now);
            return RefreshOutcome::StaleEndpoint;
        }
        if (!ok) {
            requests_.failPendingRefresh(now);
            return RefreshOutcome::Failed;
        }
        requests_.finishPendingRefresh(std::move(pending), now);
        return RefreshOutcome::Applied;
    }

    [[nodiscard]] StorageRefreshCompletion completeStorageRefresh(const SeerrEndpoint& requestedEndpoint,
                                                                  const SeerrEndpoint& currentEndpoint, bool ok,
                                                                  std::vector<SeerrStorageTarget> targets,
                                                                  std::string_view error,
                                                                  SeerrStorageState::TimePoint now) {
        if (!requestedEndpoint.matches(currentEndpoint.server, currentEndpoint.auth)) {
            storage_.invalidateRefresh();
            return {.outcome = RefreshOutcome::StaleEndpoint};
        }
        if (!ok) {
            storage_.failRefresh(std::string(error));
            const bool reconnect = isSeerrAuthError(error) && !currentEndpoint.auth.sessionCookie.empty();
            const bool clearPendingRequest = !reconnect && storage_.pendingRequest().has_value();
            if (clearPendingRequest) storage_.clearPendingRequest();
            return {
                .outcome = RefreshOutcome::Failed,
                .reconnect = reconnect,
                .clearedPendingRequest = clearPendingRequest,
            };
        }
        storage_.finishRefresh(std::move(targets), now);
        return {.outcome = RefreshOutcome::Applied};
    }

private:
    SeerrConnectionState connection_;
    SeerrRequestState requests_;
    SeerrStorageState storage_;
};
