#include "system_text_input_controller.hpp"
#include "seerr_search_state.hpp"

#include <cassert>

int main() {
    SeerrSearchState seerrSearch;
    SearchScreenState search(seerrSearch.results());
    SettingsScreenState settingsScreen;
    AccountScreenState account;
    AppSettings settings;
    SystemTextInputController controller;

    controller.begin(kTextInputSeerrServer, "https://old.example");
    assert(controller.active());
    assert(controller.mode() == kTextInputSeerrServer);

    auto effects = controller.apply(
        systemTextInputEvent(SystemTextInputPhase::Changed, kTextInputSeerrServer, "https://new.example"), search,
        settingsScreen, settings, account);
    assert(settings.seerrServer == "https://new.example");
    assert(effects.renderBurst.count() == 300);
    assert(!effects.saveSession);

    effects = controller.apply(
        systemTextInputEvent(SystemTextInputPhase::Cancelled, kTextInputSeerrServer, "https://typed.example"), search,
        settingsScreen, settings, account);
    assert(!controller.active());
    assert(settings.seerrServer == "https://old.example");
    assert(effects.renderBurst.count() == 300);

    settings.seerrServer = "https://old.example";
    settings.seerrSessionCookie = "connect.sid=stale";
    controller.begin(kTextInputSeerrServer, settings.seerrServer);
    effects =
        controller.apply(systemTextInputEvent(SystemTextInputPhase::Done, kTextInputSeerrServer, "https://new.example"),
                         search, settingsScreen, settings, account);
    assert(settings.seerrServer == "https://new.example");
    assert(settings.seerrSessionCookie.empty());
    assert(effects.invalidateSeerrStorage);
    assert(effects.saveSession);
    assert(effects.refreshSeerr);
    assert(effects.notice == SystemTextInputNotice::SeerrServerSaved);
    assert(effects.renderBurst.count() == 500);

    settings.seerrApiKey = "old-key";
    controller.begin(kTextInputSeerrApiKey, settings.seerrApiKey);
    effects = controller.apply(systemTextInputEvent(SystemTextInputPhase::Done, kTextInputSeerrApiKey, ""), search,
                               settingsScreen, settings, account);
    assert(settings.seerrApiKey.empty());
    assert(effects.saveSession);
    assert(effects.refreshSeerr);
    assert(effects.notice == SystemTextInputNotice::SeerrApiKeyCleared);

    search.setQuery("old");
    assert(search.scheduleDebounce(SearchScreenState::Clock::now()));
    controller.begin(kTextInputSearch, search.query());
    effects = controller.apply(systemTextInputEvent(SystemTextInputPhase::Changed, kTextInputSearch, "new"), search,
                               settingsScreen, settings, account);
    assert(search.query() == "new");
    assert(!search.keyboard());
    assert(effects.scheduleSearch);
    assert(controller.active());

    effects = controller.apply(systemTextInputEvent(SystemTextInputPhase::Done, kTextInputSearch, "final"), search,
                               settingsScreen, settings, account);
    assert(search.query() == "final");
    assert(!search.debouncePending());
    assert(!controller.active());
    assert(effects.cancelSeerrSearch);
    assert(effects.submitSearch);

    controller.begin(kTextInputSettingsSearch, "");
    effects = controller.apply(systemTextInputEvent(SystemTextInputPhase::Done, kTextInputSettingsSearch, "subtitle"),
                               search, settingsScreen, settings, account);
    assert(settingsScreen.searchQuery() == "subtitle");
    assert(!effects.saveSession);

    account.setLoginFocus(AccountScreenState::kServerField);
    controller.begin(kTextInputLoginServer, "");
    effects = controller.apply(
        systemTextInputEvent(SystemTextInputPhase::Changed, kTextInputLoginServer, "https://jellyfin.example"), search,
        settingsScreen, settings, account);
    assert(account.field(AccountScreenState::kServerField) == "https://jellyfin.example");
    effects = controller.apply(
        systemTextInputEvent(SystemTextInputPhase::Done, kTextInputLoginServer, "https://server.example"), search,
        settingsScreen, settings, account);
    assert(account.field(AccountScreenState::kServerField) == "https://server.example");
    assert(account.loginFocus() == AccountScreenState::kUsernameField);

    controller.begin(kTextInputSearch, "query");
    controller.hide();
    assert(!controller.active());

    return 0;
}
