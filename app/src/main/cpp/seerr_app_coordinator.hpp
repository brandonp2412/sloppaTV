#pragma once

#include "app_screen.hpp"
#include "app_settings.hpp"
#include "content_mutation_flow.hpp"
#include "details_flow.hpp"
#include "navigation_stack.hpp"
#include "screen_navigation_key.hpp"
#include "seerr_completion_flow.hpp"
#include "seerr_domain.hpp"
#include "seerr_drive_navigation_controller.hpp"

#include <chrono>
#include <string>
#include <type_traits>
#include <utility>

template <typename T>
inline constexpr bool isSeerrAppCompletionV = std::is_same_v<std::remove_cvref_t<T>, SeerrDeleteCompletion> ||
                                              std::is_same_v<std::remove_cvref_t<T>, SeerrRequestCompletion> ||
                                              std::is_same_v<std::remove_cvref_t<T>, SeerrStorageRefreshCompletion> ||
                                              std::is_same_v<std::remove_cvref_t<T>, SeerrPendingRefreshCompletion> ||
                                              std::is_same_v<std::remove_cvref_t<T>, SeerrConnectCompletion>;

template <typename AsyncExecutor, typename ConnectionCoordinator, typename RequestCoordinator,
          typename RefreshCoordinator, typename CompletionFlow>
class SeerrAppCoordinator {
public:
    SeerrAppCoordinator(SeerrDomainState& domain, ContentMutationFlow& mutations, DetailsFlow& details,
                        AppSettings& settings, JellyfinSession& session, bool& loading,
                        NavigationStack<Screen>& navigation, Screen& screen, AsyncExecutor& async,
                        ConnectionCoordinator& connection, RequestCoordinator& request, RefreshCoordinator& refresh,
                        CompletionFlow& completions)
        : domain_(domain), mutations_(mutations), details_(details), settings_(settings), session_(session),
          loading_(loading), navigation_(navigation), screen_(screen), async_(async), connection_(connection),
          request_(request), refresh_(refresh), completions_(completions) {}

    void resetForSessionChange() {
        settings_.seerrSessionCookie.clear();
        domain_.resetStorageForSessionClear();
    }

    void invalidateStorage() { domain_.invalidateStorageTargets(); }

    [[nodiscard]] SeerrEndpoint endpoint() const {
        return SeerrEndpoint{
            .server = settings_.seerrServer,
            .auth = {.sessionCookie = settings_.seerrSessionCookie, .apiKey = settings_.seerrApiKey},
        };
    }

    [[nodiscard]] SeerrAuth auth() const { return endpoint().auth; }

    [[nodiscard]] SeerrCompletionHostEffects connect(bool announce = true) {
        SeerrCompletionHostEffects effects;
        auto plan = connection_.prepare(settings_.seerrServer, session_);
        switch (plan.action) {
        case SeerrDomainState::ConnectAction::AlreadyConnecting:
            return effects;
        case SeerrDomainState::ConnectAction::MissingServer:
            if (announce) {
                effects.notice = "SET THE SEERR SERVER FIRST";
                effects.noticeSeconds = 4;
            }
            return effects;
        case SeerrDomainState::ConnectAction::MissingJellyfin:
            if (announce) {
                effects.notice = "JELLYFIN LOGIN REQUIRED";
                effects.noticeSeconds = 4;
            }
            return effects;
        case SeerrDomainState::ConnectAction::Submit:
            break;
        }
        if (announce) {
            effects.clearError = true;
            effects.notice = "CONNECTING SEERR WITH JELLYFIN...";
            effects.noticeSeconds = 30;
        }
        if (!connection_.submit(std::move(plan), announce) && announce) {
            effects.notice = "COULD NOT START SEERR CONNECTION";
            effects.noticeSeconds = 4;
        }
        return effects;
    }

    void refreshStorage(bool force = false) {
        refresh_.refreshStorage(endpoint(), force, std::chrono::steady_clock::now());
    }

    [[nodiscard]] SeerrCompletionHostEffects refreshPending() {
        SeerrCompletionHostEffects effects;
        if (refresh_.refreshPending(endpoint()) == SeerrDomainState::RefreshStartAction::Reset) effects.syncHome = true;
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects openDrivePicker(const SeerrMediaItem& item) {
        SeerrCompletionHostEffects effects;
        const auto status = domain_.prepareStoragePicker(item);
        if (status == SeerrStorageState::PickerStatus::Loading) {
            effects.notice = "LOADING SEERR STORAGE...";
            effects.noticeSeconds = 4;
            return effects;
        }
        if (status == SeerrStorageState::PickerStatus::Unavailable) {
            effects.notice = domain_.storageError().empty() ? "NO SEERR STORAGE TARGETS ARE AVAILABLE"
                                                            : "SEERR STORAGE: " + domain_.storageError();
            effects.noticeSeconds = 5;
            return effects;
        }
        effects.openDrivePicker = item;
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects handleDrivePicker(ScreenNavigationKey key) {
        SeerrDriveNavigationAction navigation = SeerrDriveNavigationController::handle(domain_.storage(), key);
        if (navigation.type == SeerrDriveNavigationActionType::Back) {
            screen_ = navigation_.popOr(Screen::Search);
            return {};
        }
        if (navigation.type != SeerrDriveNavigationActionType::Selected || !navigation.selection) return {};

        auto selected = std::move(*navigation.selection);
        screen_ = navigation_.popOr(Screen::Search);
        SeerrCompletionHostEffects effects = requestMedia(selected.item, &selected.target, true);
        effects.log = SeerrCompletionLog{
            SeerrCompletionLogLevel::Info,
            "Seerr storage selected media=" + selected.item.mediaType +
                " server=" + std::to_string(selected.target.serverId) + " path=" + selected.target.path,
        };
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects requestMedia(const SeerrMediaItem& item,
                                                          const SeerrStorageTarget* selectedTarget = nullptr,
                                                          bool skipDrivePrompt = false) {
        SeerrCompletionHostEffects effects;
        auto plan = request_.prepare(item, endpoint(), settings_.seerrSelectDrive, skipDrivePrompt, selectedTarget);
        switch (plan.action) {
        case SeerrDomainState::RequestAction::Invalid:
            return effects;
        case SeerrDomainState::RequestAction::AlreadyRequested:
            effects.notice = item.status.empty() ? "ALREADY REQUESTED IN SEERR" : item.status;
            effects.noticeSeconds = 5;
            return effects;
        case SeerrDomainState::RequestAction::NotConfigured:
            effects.notice = "CONNECT SEERR IN SETTINGS FIRST";
            effects.noticeSeconds = 5;
            return effects;
        case SeerrDomainState::RequestAction::DeferredForConnection:
            effects.notice = "REFRESHING SEERR SESSION...";
            effects.noticeSeconds = 4;
            return effects;
        case SeerrDomainState::RequestAction::ChooseStorage:
            refreshStorage(plan.refreshStorage);
            return openDrivePicker(item);
        case SeerrDomainState::RequestAction::Submit:
            break;
        }
        mutations_.begin();
        if (!request_.submit(std::move(plan))) {
            mutations_.finish();
            effects.error = "COULD NOT START SEERR REQUEST";
        }
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects deleteCurrentRequest() {
        SeerrCompletionHostEffects effects;
        const auto pendingDelete = seerrDeleteRequestFromJellyfinItem(details_.item());
        if (loading_ || mutations_.loading() || !pendingDelete) {
            if (isSeerrItem(details_.item()) && details_.item().externalRequestId <= 0) {
                effects.error = "SEERR REQUEST ID IS NOT AVAILABLE YET";
                details_.state().setDeleteConfirmation(false);
            }
            return effects;
        }
        if (!endpoint().configured()) {
            effects.error = "SEERR IS NOT CONNECTED";
            details_.state().setDeleteConfirmation(false);
            return effects;
        }
        mutations_.begin();
        effects.clearError = true;
        if (!async_.deleteRequest(endpoint(), *pendingDelete)) {
            mutations_.finish();
            details_.state().setDeleteConfirmation(false);
            effects.error = "COULD NOT START SEERR DELETE";
        }
        return effects;
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(const SeerrDeleteCompletion& completion) {
        return completions_.complete(completion, endpoint(), screen_ == Screen::ItemMenu);
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(const SeerrRequestCompletion& completion) {
        return completions_.complete(completion, endpoint(), std::chrono::steady_clock::now());
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(SeerrStorageRefreshCompletion& completion) {
        return completions_.complete(completion, endpoint(), settings_.seerrSelectDrive,
                                     std::chrono::steady_clock::now());
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(SeerrPendingRefreshCompletion& completion) {
        return completions_.complete(completion, endpoint(), screen_ == Screen::ItemMenu,
                                     std::chrono::steady_clock::now());
    }

    [[nodiscard]] SeerrCompletionHostEffects complete(SeerrConnectCompletion& completion) {
        return completions_.complete(completion);
    }

private:
    SeerrDomainState& domain_;
    ContentMutationFlow& mutations_;
    DetailsFlow& details_;
    AppSettings& settings_;
    JellyfinSession& session_;
    bool& loading_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    AsyncExecutor& async_;
    ConnectionCoordinator& connection_;
    RequestCoordinator& request_;
    RefreshCoordinator& refresh_;
    CompletionFlow& completions_;
};
