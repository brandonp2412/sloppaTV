#include "account_navigation_controller.hpp"

namespace {
LoginScreenInput loginInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return LoginScreenInput::Back;
    case ScreenNavigationKey::Up:
        return LoginScreenInput::Up;
    case ScreenNavigationKey::Down:
        return LoginScreenInput::Down;
    case ScreenNavigationKey::Left:
        return LoginScreenInput::Left;
    case ScreenNavigationKey::Right:
        return LoginScreenInput::Right;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return LoginScreenInput::Activate;
    case ScreenNavigationKey::None:
    case ScreenNavigationKey::Search:
    case ScreenNavigationKey::Context:
        return LoginScreenInput::None;
    }
    return LoginScreenInput::None;
}

ProfilesScreenInput profilesInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return ProfilesScreenInput::Back;
    case ScreenNavigationKey::Up:
        return ProfilesScreenInput::Up;
    case ScreenNavigationKey::Down:
        return ProfilesScreenInput::Down;
    case ScreenNavigationKey::Left:
    case ScreenNavigationKey::Right:
        return ProfilesScreenInput::Horizontal;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return ProfilesScreenInput::Activate;
    case ScreenNavigationKey::None:
    case ScreenNavigationKey::Search:
    case ScreenNavigationKey::Context:
        return ProfilesScreenInput::None;
    }
    return ProfilesScreenInput::None;
}

AccountNavigationAction action(AccountNavigationActionType type) {
    AccountNavigationAction result;
    result.type = type;
    return result;
}
} // namespace

AccountNavigationAction AccountNavigationController::handleLogin(AccountScreenState& state, ScreenNavigationKey key,
                                                                  bool hasSavedSessions) {
    const LoginScreenCommand command = state.handleLoginInput(loginInput(key), hasSavedSessions);
    switch (command.type) {
    case LoginScreenCommandType::None:
        return {};
    case LoginScreenCommandType::FinishActivity:
        return action(AccountNavigationActionType::FinishActivity);
    case LoginScreenCommandType::CancelQuickConnect:
        return action(AccountNavigationActionType::CancelQuickConnect);
    case LoginScreenCommandType::MoveKeyboard: {
        AccountNavigationAction result = action(AccountNavigationActionType::MoveKeyboard);
        result.dx = command.keyboardX;
        result.dy = command.keyboardY;
        return result;
    }
    case LoginScreenCommandType::ActivateKeyboard:
        return action(AccountNavigationActionType::ActivateKeyboard);
    case LoginScreenCommandType::EditField: {
        AccountNavigationAction result = action(AccountNavigationActionType::EditField);
        result.index = command.fieldIndex;
        return result;
    }
    case LoginScreenCommandType::Login:
        return action(AccountNavigationActionType::Login);
    case LoginScreenCommandType::QuickConnect:
        return action(AccountNavigationActionType::QuickConnect);
    case LoginScreenCommandType::Discover:
        return action(AccountNavigationActionType::Discover);
    case LoginScreenCommandType::OpenProfiles:
        return action(AccountNavigationActionType::OpenProfiles);
    }
    return {};
}

AccountNavigationAction AccountNavigationController::handleProfiles(AccountScreenState& state, ScreenNavigationKey key,
                                                                     int savedSessionCount) {
    const ProfilesScreenCommand command = state.handleProfilesInput(profilesInput(key), savedSessionCount);
    AccountNavigationAction result;
    switch (command.type) {
    case ProfilesScreenCommandType::None:
        return result;
    case ProfilesScreenCommandType::Back:
        result.type = AccountNavigationActionType::ProfilesBack;
        break;
    case ProfilesScreenCommandType::AddAccount:
        result.type = AccountNavigationActionType::AddAccount;
        break;
    case ProfilesScreenCommandType::SwitchSession:
        result.type = AccountNavigationActionType::SwitchSession;
        result.index = command.sessionIndex;
        break;
    case ProfilesScreenCommandType::ForgetSession:
        result.type = AccountNavigationActionType::ForgetSession;
        result.index = command.sessionIndex;
        break;
    }
    return result;
}
