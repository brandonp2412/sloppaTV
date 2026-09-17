#include "account_screen.hpp"

#include <cassert>

int main() {
    AccountScreenState state;

    state.setField(AccountScreenState::kServerField, "https://jellyfin.example");
    state.setField(AccountScreenState::kUsernameField, "user");
    state.setField(AccountScreenState::kPasswordField, "secret");
    assert(state.field(AccountScreenState::kServerField) == "https://jellyfin.example");

    state.setLoginFocus(AccountScreenState::kServerField);
    state.appendToFocusedField('x');
    assert(state.field(AccountScreenState::kServerField).ends_with('x'));
    assert(state.backspaceFocusedField());
    assert(!state.field(AccountScreenState::kServerField).ends_with('x'));

    state.setField(AccountScreenState::kServerField, "https://example/\xC4\x81");
    assert(state.backspaceFocusedField());
    assert(state.field(AccountScreenState::kServerField) == "https://example/");

    auto loginCommand = state.handleLoginFormInput(LoginFormInput::Down, false);
    assert(loginCommand.type == LoginFormCommandType::None);
    assert(state.loginFocus() == AccountScreenState::kUsernameField);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Down, false);
    assert(state.loginFocus() == AccountScreenState::kPasswordField);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Down, false);
    assert(state.loginFocus() == AccountScreenState::kLoginAction);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Left, false);
    assert(state.loginFocus() == AccountScreenState::kDiscoverAction);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Right, false);
    assert(state.loginFocus() == AccountScreenState::kLoginAction);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Left, true);
    assert(state.loginFocus() == AccountScreenState::kSavedUsersAction);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Up, true);
    assert(state.loginFocus() == AccountScreenState::kPasswordField);

    state.setLoginFocus(AccountScreenState::kServerField);
    loginCommand = state.handleLoginFormInput(LoginFormInput::Activate, true);
    assert(loginCommand.type == LoginFormCommandType::EditField);
    assert(loginCommand.fieldIndex == AccountScreenState::kServerField);
    state.setLoginFocus(AccountScreenState::kLoginAction);
    assert(state.handleLoginFormInput(LoginFormInput::Activate, true).type == LoginFormCommandType::Login);
    state.setLoginFocus(AccountScreenState::kQuickConnectAction);
    assert(state.handleLoginFormInput(LoginFormInput::Activate, true).type == LoginFormCommandType::QuickConnect);
    state.setLoginFocus(AccountScreenState::kDiscoverAction);
    assert(state.handleLoginFormInput(LoginFormInput::Activate, true).type == LoginFormCommandType::Discover);
    state.setLoginFocus(AccountScreenState::kSavedUsersAction);
    assert(state.handleLoginFormInput(LoginFormInput::Activate, true).type == LoginFormCommandType::OpenProfiles);

    state.finishTextField(AccountScreenState::kUsernameField, "new-user");
    assert(state.field(AccountScreenState::kUsernameField) == "new-user");
    assert(state.loginFocus() == AccountScreenState::kPasswordField);

    state.setKeyboardActive(true);
    assert(state.keyboardActive());
    state.beginQuickConnect();
    assert(state.quickConnectActive());
    assert(state.quickConnectCode() == "------");
    state.setQuickConnectCode("ABC123");
    assert(state.quickConnectCode() == "ABC123");
    state.endQuickConnect(true);
    assert(!state.quickConnectActive());
    assert(state.loginFocus() == AccountScreenState::kQuickConnectAction);

    state.setDiscoveryStatus("FOUND SERVER");
    assert(state.discoveryStatus() == "FOUND SERVER");
    state.clearDiscoveryStatus();
    assert(state.discoveryStatus().empty());

    state.setField(AccountScreenState::kUsernameField, "someone");
    state.setField(AccountScreenState::kPasswordField, "password");
    state.beginAddAccount("https://existing.example");
    assert(state.field(AccountScreenState::kServerField) == "https://existing.example");
    assert(state.field(AccountScreenState::kUsernameField).empty());
    assert(state.field(AccountScreenState::kPasswordField).empty());
    assert(state.loginFocus() == AccountScreenState::kServerField);
    assert(!state.keyboardActive());

    state.setAuthenticatedAccount("https://server.example", "viewer");
    assert(state.field(AccountScreenState::kServerField) == "https://server.example");
    assert(state.field(AccountScreenState::kUsernameField) == "viewer");
    assert(state.field(AccountScreenState::kPasswordField).empty());

    state.beginProfiles(3);
    assert(state.profileSelection() == 0);
    auto profileCommand = state.handleProfilesInput(ProfilesScreenInput::Down, 3);
    assert(profileCommand.type == ProfilesScreenCommandType::None);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Down, 3);
    assert(state.profileSelection() == 2);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Horizontal, 3);
    assert(state.profileAction() == 1);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Activate, 3);
    assert(profileCommand.type == ProfilesScreenCommandType::ForgetSession);
    assert(profileCommand.sessionIndex == 2);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Horizontal, 3);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Activate, 3);
    assert(profileCommand.type == ProfilesScreenCommandType::SwitchSession);
    assert(profileCommand.sessionIndex == 2);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Down, 3);
    assert(state.profileSelection() == 3);
    assert(state.profileAction() == 0);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Activate, 3);
    assert(profileCommand.type == ProfilesScreenCommandType::AddAccount);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Horizontal, 3);
    assert(state.profileAction() == 0);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Up, 3);
    assert(state.profileSelection() == 2);
    profileCommand = state.handleProfilesInput(ProfilesScreenInput::Back, 3);
    assert(profileCommand.type == ProfilesScreenCommandType::Back);

    return 0;
}
