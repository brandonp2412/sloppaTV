#include "account_completion_controller.hpp"

#include <cassert>
#include <string>

namespace {
DiscoveredJellyfinServer server(std::string address, std::string name) {
    return {
        .address = std::move(address),
        .id = "server-id",
        .name = std::move(name),
    };
}

JellyfinSession session(std::string serverAddress, std::string username) {
    JellyfinSession value;
    value.server = std::move(serverAddress);
    value.username = std::move(username);
    value.userId = "user-id";
    value.token = "token";
    return value;
}
} // namespace

int main() {
    {
        AccountScreenState state;
        state.setDiscoveryStatus("KEEP");
        DiscoveryCompletion completion{
            .generation = 3,
            .servers = {server("https://ignored.example", "Ignored")},
        };

        const auto effects = AccountCompletionController::apply(completion, false, state);
        assert(!effects.finishLoading);
        assert(!effects.clearError);
        assert(!effects.error);
        assert(!effects.authenticatedSession);
        assert(state.discoveryStatus() == "KEEP");
        assert(state.field(AccountScreenState::kServerField).empty());
    }

    {
        AccountScreenState state;
        state.setDiscoveryStatus("SEARCHING");
        DiscoveryCompletion completion{
            .generation = 4,
            .servers = {},
        };

        const auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(!effects.clearError);
        assert(effects.error == "NO JELLYFIN SERVER FOUND ON THIS NETWORK");
        assert(!effects.authenticatedSession);
        assert(state.discoveryStatus().empty());
    }

    {
        AccountScreenState state;
        DiscoveryCompletion completion{
            .generation = 5,
            .servers = {
                server("https://living-room.example", "Living Room"),
                server("https://bedroom.example", "Bedroom"),
                server("https://office.example", ""),
            },
        };

        const auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(effects.clearError);
        assert(!effects.error);
        assert(state.field(AccountScreenState::kServerField) == "https://living-room.example");
        assert(state.discoveryStatus() == "FOUND Living Room + 2 MORE");
        assert(state.loginFocus() == AccountScreenState::kUsernameField);
    }

    {
        AccountScreenState state;
        LoginCompletion completion;
        completion.generation = 6;
        completion.result.ok = false;
        completion.result.error = "bad credentials";

        const auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(effects.error == "bad credentials");
        assert(!effects.authenticatedSession);
    }

    {
        AccountScreenState state;
        LoginCompletion completion;
        completion.generation = 7;
        completion.result.ok = true;
        completion.result.value = session("https://login.example", "brandon");

        auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(effects.clearError);
        assert(effects.authenticatedSession);
        assert(effects.authenticatedSession->server == "https://login.example");
        assert(effects.authenticatedSession->username == "brandon");
        assert(state.field(AccountScreenState::kServerField) == "https://login.example");
        assert(state.field(AccountScreenState::kUsernameField) == "brandon");
        assert(state.field(AccountScreenState::kPasswordField).empty());
    }

    {
        AccountScreenState state;
        state.beginQuickConnect();
        QuickConnectStartedCompletion completion;
        completion.generation = 8;
        completion.request.server = "https://quick.example";
        completion.request.code = "ABCD";

        const auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(state.quickConnectActive());
        assert(state.quickConnectCode() == "ABCD");
        assert(state.field(AccountScreenState::kServerField) == "https://quick.example");
    }

    {
        AccountScreenState state;
        state.beginQuickConnect("ABCD");
        QuickConnectFailedCompletion completion{.generation = 9, .error = "poll failed"};

        const auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(effects.error == "poll failed");
        assert(!state.quickConnectActive());
    }

    {
        AccountScreenState state;
        state.beginQuickConnect("ABCD");
        QuickConnectAuthenticatedCompletion completion{
            .generation = 10,
            .session = session("https://quick.example", "quick-user"),
        };

        auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(effects.clearError);
        assert(effects.authenticatedSession);
        assert(effects.authenticatedSession->username == "quick-user");
        assert(!state.quickConnectActive());
        assert(state.field(AccountScreenState::kUsernameField) == "quick-user");
    }

    {
        AccountScreenState state;
        state.beginQuickConnect("ABCD");
        QuickConnectTimedOutCompletion completion{.generation = 11};

        const auto effects = AccountCompletionController::apply(completion, true, state);
        assert(effects.finishLoading);
        assert(effects.error == "QUICK CONNECT TIMED OUT - TRY AGAIN");
        assert(!state.quickConnectActive());
    }

    return 0;
}
