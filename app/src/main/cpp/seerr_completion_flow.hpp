#pragma once

#include "app_settings.hpp"
#include "content_mutation_flow.hpp"
#include "details_flow.hpp"
#include "search_screen.hpp"
#include "seerr_async_executor.hpp"
#include "seerr_connection_coordinator.hpp"
#include "seerr_refresh_coordinator.hpp"
#include "seerr_request_coordinator.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

enum class SeerrCompletionLogLevel { None, Info, Warning };

struct SeerrCompletionLog {
    SeerrCompletionLogLevel level = SeerrCompletionLogLevel::None;
    std::string message;
};

struct SeerrCompletionHostEffects {
    bool clearNotice = false;
    bool clearError = false;
    bool syncHome = false;
    bool closeItem = false;
    bool reconnect = false;
    bool refreshStorage = false;
    bool refreshPending = false;
    bool saveSession = false;
    bool retrySearch = false;
    std::optional<std::string> error;
    std::optional<std::string> notice;
    int noticeSeconds = 4;
    std::optional<SeerrMediaItem> openDrivePicker;
    std::optional<SeerrMediaItem> deferredRequest;
    std::optional<SeerrCompletionLog> log;
};

template <typename ConnectionCoordinator, typename RequestCoordinator, typename RefreshCoordinator>
class SeerrCompletionFlow {
public:
    SeerrCompletionFlow(SeerrDomainState& domain, ContentMutationFlow& mutations, SearchScreenState& search,
                        DetailsFlow& details, AppSettings& settings, JellyfinSession& session,
                        ConnectionCoordinator& connection, RequestCoordinator& request, RefreshCoordinator& refresh)
        : domain_(domain), mutations_(mutations), search_(search), details_(details), settings_(settings),
          session_(session), connection_(connection), request_(request), refresh_(refresh) {}

    [[nodiscard]] SeerrCompletionHostEffects complete(const SeerrDeleteCompletion& completion,
                                                      const SeerrEndpoint& currentEndpoint, bool activeItemMenu) {
        mutations_.finish();
        if (!activeItemMenu || details_.item().id != completion.request.itemId) return {};
        const auto outcome = request_.completeDelete(completion.endpoint, currentEndpoint, completion.request.itemId,
                                                     completion.request.requestId, completion.result.ok);
        if (outcome == SeerrDomainState::MutationOutcome::StaleEndpoint) return {};
        if (outcome == SeerrDomainState::MutationOutcome::Failed) {
            details_.state().setDeleteConfirmation(false);
            SeerrCompletionHostEffects effects;
            effects.error = "SEERR DELETE: " + completion.result.error;
            return effects;
        }
        search_.refreshSeerrResults();
        details_.item() = {};
        details_.state().setDeleteConfirmation(false);
        SeerrCompletionHostEffects effects;
        effects.syncHome = true;
        effects.closeItem = true;
        effects.clearError = true;
        effects.notice = "SEERR REQUEST DELETED";
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(const SeerrRequestCompletion& completion,
                                                      const SeerrEndpoint& currentEndpoint,
                                                      SeerrRequestState::TimePoint now) {
        mutations_.finish();
        const auto result = request_.complete(completion.endpoint, currentEndpoint, completion.requestedItem,
                                              completion.result.value, completion.result.ok, now);
        if (result.outcome == SeerrDomainState::MutationOutcome::StaleEndpoint) return {};
        if (result.outcome == SeerrDomainState::MutationOutcome::Failed) {
            SeerrCompletionHostEffects effects;
            effects.error = "SEERR REQUEST: " + completion.result.error;
            return effects;
        }
        search_.refreshSeerrResults();
        SeerrCompletionHostEffects effects;
        effects.syncHome = true;
        effects.clearError = true;
        effects.refreshStorage = true;
        effects.notice = "REQUEST SENT TO SEERR";
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(SeerrStorageRefreshCompletion& completion,
                                                      const SeerrEndpoint& currentEndpoint, bool driveSelectionEnabled,
                                                      SeerrStorageState::TimePoint now) {
        auto& apiResult = completion.result;
        const auto plan =
            refresh_.completeStorage(completion.endpoint, currentEndpoint, apiResult.ok, std::move(apiResult.value),
                                     apiResult.error, driveSelectionEnabled, now);
        if (plan.domain.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint) return {};
        SeerrCompletionHostEffects effects;
        if (plan.domain.outcome == SeerrDomainState::RefreshOutcome::Failed) {
            effects.log = SeerrCompletionLog{SeerrCompletionLogLevel::Warning,
                                             "Seerr storage refresh failed: " + apiResult.error};
            if (plan.domain.reconnect)
                effects.reconnect = true;
            else if (plan.domain.clearedPendingRequest) {
                effects.notice = "SEERR STORAGE: " + apiResult.error;
                effects.noticeSeconds = 5;
            }
            return effects;
        }
        effects.log =
            SeerrCompletionLog{SeerrCompletionLogLevel::Info,
                               "Seerr storage refresh found " + std::to_string(plan.targetCount) + " targets"};
        effects.openDrivePicker = plan.pendingRequest;
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(SeerrPendingRefreshCompletion& completion,
                                                      const SeerrEndpoint& currentEndpoint, bool activeItemMenu,
                                                      SeerrRequestState::TimePoint now) {
        auto& apiResult = completion.result;
        const auto plan = refresh_.completePending(completion.endpoint, currentEndpoint, apiResult.ok,
                                                   std::move(apiResult.value), now);
        if (plan.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint) return {};
        SeerrCompletionHostEffects effects;
        if (plan.outcome == SeerrDomainState::RefreshOutcome::Failed) {
            effects.log =
                SeerrCompletionLog{SeerrCompletionLogLevel::Warning, "Seerr pending refresh error: " + apiResult.error};
            return effects;
        }
        effects.log =
            SeerrCompletionLog{SeerrCompletionLogLevel::Info,
                               "Seerr pending refresh found " + std::to_string(plan.pendingCount) + " requests"};
        if (activeItemMenu && isSeerrItem(details_.item())) {
            if (const SeerrMediaItem* current = domain_.findPendingRequest(details_.item().id))
                details_.item() = jellyfinItemFromSeerrMedia(*current);
        }
        effects.syncHome = true;
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(SeerrConnectCompletion& completion) {
        auto& result = completion.result;
        auto plan =
            connection_.complete(result.ok, result.failedStage == SeerrQuickConnectStage::AuthenticateSeerr,
                                 completion.server, completion.jellyfinUserId, settings_.seerrServer, session_.userId);
        SeerrCompletionHostEffects effects;
        const auto action = plan.action;
        if (action == SeerrDomainState::ConnectCompletionAction::PreAuthenticationFailed) {
            if (completion.announce) {
                effects.clearNotice = true;
                effects.error =
                    (result.failedStage == SeerrQuickConnectStage::AuthorizeJellyfin ? "JELLYFIN QUICK CONNECT: "
                                                                                     : "SEERR QUICK CONNECT: ") +
                    result.error;
            } else {
                effects.log = SeerrCompletionLog{SeerrCompletionLogLevel::Warning,
                                                 (result.failedStage == SeerrQuickConnectStage::AuthorizeJellyfin
                                                      ? "Silent Jellyfin Quick Connect authorization failed: "
                                                      : "Silent Seerr reconnect failed: ") +
                                                     result.error};
            }
            return effects;
        }
        if (completion.announce) effects.clearNotice = true;
        if (action == SeerrDomainState::ConnectCompletionAction::Stale) return effects;
        if (action == SeerrDomainState::ConnectCompletionAction::AuthenticationFailed) {
            if (completion.announce)
                effects.error = "SEERR QUICK CONNECT: " + result.error;
            else
                effects.log = SeerrCompletionLog{SeerrCompletionLogLevel::Warning,
                                                 "Silent Seerr authentication failed: " + result.error};
            return effects;
        }
        settings_.seerrSessionCookie = std::move(result.sessionCookie);
        effects.saveSession = true;
        effects.refreshPending = true;
        effects.refreshStorage = true;
        effects.deferredRequest = std::move(plan.deferred.request);
        effects.retrySearch = plan.deferred.retrySearch;
        effects.log = SeerrCompletionLog{SeerrCompletionLogLevel::Info, "Seerr session refreshed"};
        if (completion.announce) {
            effects.clearError = true;
            effects.notice = "SEERR CONNECTED WITH JELLYFIN";
        }
        return effects;
    }

private:
    SeerrDomainState& domain_;
    ContentMutationFlow& mutations_;
    SearchScreenState& search_;
    DetailsFlow& details_;
    AppSettings& settings_;
    JellyfinSession& session_;
    ConnectionCoordinator& connection_;
    RequestCoordinator& request_;
    RefreshCoordinator& refresh_;
};
