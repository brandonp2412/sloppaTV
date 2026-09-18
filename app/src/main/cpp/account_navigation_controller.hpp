#pragma once

#include "account_screen.hpp"
#include "screen_navigation_key.hpp"

enum class AccountNavigationActionType {
    None,
    FinishActivity,
    CancelQuickConnect,
    MoveKeyboard,
    ActivateKeyboard,
    EditField,
    Login,
    QuickConnect,
    Discover,
    OpenProfiles,
    ProfilesBack,
    AddAccount,
    SwitchSession,
    ForgetSession,
};

struct AccountNavigationAction {
    AccountNavigationActionType type = AccountNavigationActionType::None;
    int index = -1;
    int dx = 0;
    int dy = 0;
};

class AccountNavigationController {
public:
    [[nodiscard]] static AccountNavigationAction handleLogin(AccountScreenState& state, ScreenNavigationKey key,
                                                              bool hasSavedSessions);
    [[nodiscard]] static AccountNavigationAction handleProfiles(AccountScreenState& state, ScreenNavigationKey key,
                                                                 int savedSessionCount);
};
