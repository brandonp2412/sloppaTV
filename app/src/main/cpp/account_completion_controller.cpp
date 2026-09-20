#include "account_completion_controller.hpp"

#include <utility>

namespace {
AccountCompletionEffects finishWithError(std::string error) {
    AccountCompletionEffects effects;
    effects.finishLoading = true;
    effects.error = std::move(error);
    return effects;
}

AccountCompletionEffects authenticated(JellyfinSession session) {
    AccountCompletionEffects effects;
    effects.finishLoading = true;
    effects.clearError = true;
    effects.authenticatedSession = std::move(session);
    return effects;
}
} // namespace

AccountCompletionEffects AccountCompletionController::apply(DiscoveryCompletion& completion, bool active,
                                                            AccountScreenState& state) {
    if (!active) return {};

    AccountCompletionEffects effects;
    effects.finishLoading = true;
    if (completion.servers.empty()) {
        state.clearDiscoveryStatus();
        effects.error = "NO JELLYFIN SERVER FOUND ON THIS NETWORK";
        return effects;
    }

    const auto& server = completion.servers.front();
    state.setField(AccountScreenState::kServerField, server.address);
    std::string status = "FOUND " + (server.name.empty() ? std::string("JELLYFIN") : server.name);
    if (completion.servers.size() > 1) {
        status += " + " + std::to_string(completion.servers.size() - 1) + " MORE";
    }
    state.setDiscoveryStatus(std::move(status));
    state.setLoginFocus(AccountScreenState::kUsernameField);
    effects.clearError = true;
    return effects;
}

AccountCompletionEffects AccountCompletionController::apply(LoginCompletion& completion, bool active,
                                                            AccountScreenState& state) {
    if (!active) return {};
    if (!completion.result.ok) return finishWithError(completion.result.error);

    state.setAuthenticatedAccount(completion.result.value.server, completion.result.value.username);
    return authenticated(std::move(completion.result.value));
}

AccountCompletionEffects AccountCompletionController::apply(QuickConnectStartedCompletion& completion, bool active,
                                                            AccountScreenState& state) {
    if (!active) return {};

    state.setField(AccountScreenState::kServerField, completion.request.server);
    state.setQuickConnectCode(completion.request.code);

    AccountCompletionEffects effects;
    effects.finishLoading = true;
    return effects;
}

AccountCompletionEffects AccountCompletionController::apply(QuickConnectFailedCompletion& completion, bool active,
                                                            AccountScreenState& state) {
    if (!active) return {};
    state.endQuickConnect();
    return finishWithError(std::move(completion.error));
}

AccountCompletionEffects AccountCompletionController::apply(QuickConnectAuthenticatedCompletion& completion,
                                                            bool active, AccountScreenState& state) {
    if (!active) return {};

    state.setAuthenticatedAccount(completion.session.server, completion.session.username);
    return authenticated(std::move(completion.session));
}

AccountCompletionEffects AccountCompletionController::apply(QuickConnectTimedOutCompletion&, bool active,
                                                            AccountScreenState& state) {
    if (!active) return {};
    state.endQuickConnect();
    return finishWithError("QUICK CONNECT TIMED OUT - TRY AGAIN");
}
