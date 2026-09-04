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

    state.moveLoginVertical(1);
    assert(state.loginFocus() == AccountScreenState::kUsernameField);
    state.moveLoginVertical(1);
    assert(state.loginFocus() == AccountScreenState::kPasswordField);
    state.moveLoginVertical(1);
    assert(state.loginFocus() == AccountScreenState::kLoginAction);
    state.moveLoginAction(-1, false);
    assert(state.loginFocus() == AccountScreenState::kDiscoverAction);
    state.moveLoginAction(1, false);
    assert(state.loginFocus() == AccountScreenState::kLoginAction);
    state.moveLoginAction(-1, true);
    assert(state.loginFocus() == AccountScreenState::kSavedUsersAction);
    state.moveLoginVertical(-1);
    assert(state.loginFocus() == AccountScreenState::kPasswordField);

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
    state.moveProfile(1, 3);
    state.moveProfile(1, 3);
    assert(state.profileSelection() == 2);
    state.toggleProfileAction(3);
    assert(state.profileAction() == 1);
    state.moveProfile(1, 3);
    assert(state.profileSelection() == 3);
    assert(state.profileAction() == 0);
    state.toggleProfileAction(3);
    assert(state.profileAction() == 0);
    state.moveProfile(-1, 3);
    assert(state.profileSelection() == 2);

    return 0;
}
