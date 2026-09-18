#pragma once

#include "account_async_executor.hpp"
#include "account_screen.hpp"
#include "quick_connect_executor.hpp"

#include <optional>
#include <string>

struct AccountCompletionEffects {
    bool finishLoading = false;
    bool clearError = false;
    std::optional<std::string> error;
    std::optional<JellyfinSession> authenticatedSession;
};

class AccountCompletionController {
public:
    [[nodiscard]] static AccountCompletionEffects apply(DiscoveryCompletion& completion, bool active,
                                                        AccountScreenState& state);
    [[nodiscard]] static AccountCompletionEffects apply(LoginCompletion& completion, bool active,
                                                        AccountScreenState& state);
    [[nodiscard]] static AccountCompletionEffects apply(QuickConnectStartedCompletion& completion, bool active,
                                                        AccountScreenState& state);
    [[nodiscard]] static AccountCompletionEffects apply(QuickConnectFailedCompletion& completion, bool active,
                                                        AccountScreenState& state);
    [[nodiscard]] static AccountCompletionEffects apply(QuickConnectAuthenticatedCompletion& completion, bool active,
                                                        AccountScreenState& state);
    [[nodiscard]] static AccountCompletionEffects apply(QuickConnectTimedOutCompletion& completion, bool active,
                                                        AccountScreenState& state);
};
