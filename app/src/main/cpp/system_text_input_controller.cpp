#include "system_text_input_controller.hpp"

using namespace std::chrono_literals;

namespace {
bool isLoginMode(int mode) {
    return mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword;
}

int loginField(int mode) {
    return mode - kTextInputLoginServer;
}
} // namespace

void SystemTextInputController::begin(int mode, std::string_view initial) {
    mode_ = mode;
    if (mode == kTextInputSeerrServer || mode == kTextInputSeerrApiKey) original_ = initial;
}

void SystemTextInputController::hide() {
    mode_ = -1;
}

SystemTextInputEffects SystemTextInputController::apply(const SystemTextInputEvent& event, SearchScreenState& search,
                                                        SettingsScreenState& settingsScreen, AppSettings& settings,
                                                        AccountScreenState& account) {
    SystemTextInputEffects effects;

    switch (event.phase) {
    case SystemTextInputPhase::Changed:
        mode_ = event.mode;
        if (event.mode == kTextInputSearch) {
            search.setQuery(event.value);
            search.setKeyboard(false);
            effects.scheduleSearch = true;
        } else if (event.mode == kTextInputSettingsSearch) {
            settingsScreen.setSearchText(event.value);
        } else if (event.mode == kTextInputSeerrServer) {
            settings.seerrServer = event.value;
        } else if (event.mode == kTextInputSeerrApiKey) {
            settings.seerrApiKey = event.value;
        } else if (isLoginMode(event.mode)) {
            account.setField(loginField(event.mode), event.value);
        }
        effects.renderBurst = 300ms;
        return effects;

    case SystemTextInputPhase::Cancelled:
        mode_ = -1;
        if (event.mode == kTextInputSearch) {
            search.setQuery(event.value);
            search.setKeyboard(false);
            effects.scheduleSearch = true;
        } else if (event.mode == kTextInputSettingsSearch) {
            settingsScreen.setSearchText(event.value);
        } else if (event.mode == kTextInputSeerrServer) {
            settings.seerrServer = original_;
            original_.clear();
        } else if (event.mode == kTextInputSeerrApiKey) {
            settings.seerrApiKey = original_;
            original_.clear();
        } else if (isLoginMode(event.mode)) {
            account.setField(loginField(event.mode), event.value);
        }
        effects.renderBurst = 300ms;
        return effects;

    case SystemTextInputPhase::Done:
        mode_ = -1;
        if (event.mode == kTextInputSearch) {
            search.setQuery(event.value);
            search.setKeyboard(false);
            search.cancelPending();
            effects.cancelSeerrSearch = true;
            effects.submitSearch = true;
        } else if (event.mode == kTextInputSettingsSearch) {
            settingsScreen.setSearchText(event.value);
        } else if (event.mode == kTextInputSeerrServer) {
            const bool changed = settings.seerrServer != event.value;
            settings.seerrServer = event.value;
            if (changed) {
                settings.seerrSessionCookie.clear();
                effects.invalidateSeerrStorage = true;
            }
            original_.clear();
            effects.saveSession = true;
            effects.refreshSeerr = true;
            effects.notice = settings.seerrServer.empty() ? SystemTextInputNotice::SeerrDisconnected
                                                          : SystemTextInputNotice::SeerrServerSaved;
        } else if (event.mode == kTextInputSeerrApiKey) {
            settings.seerrApiKey = event.value;
            original_.clear();
            effects.saveSession = true;
            effects.refreshSeerr = true;
            effects.notice = settings.seerrApiKey.empty() ? SystemTextInputNotice::SeerrApiKeyCleared
                                                          : SystemTextInputNotice::SeerrApiKeySaved;
        } else if (isLoginMode(event.mode)) {
            account.finishTextField(loginField(event.mode), event.value);
        }
        effects.renderBurst = 500ms;
        return effects;
    }

    return effects;
}
