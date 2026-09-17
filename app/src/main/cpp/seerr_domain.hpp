#pragma once

#include "seerr_auth.hpp"
#include "seerr_connection_state.hpp"
#include "seerr_request_state.hpp"
#include "seerr_storage_state.hpp"

#include <optional>

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

private:
    SeerrConnectionState connection_;
    SeerrRequestState requests_;
    SeerrStorageState storage_;
};
