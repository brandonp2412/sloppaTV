#pragma once

#include "account_screen.hpp"
#include "app_screen.hpp"
#include "details_navigation_controller.hpp"
#include "search_screen.hpp"
#include "virtual_keyboard.hpp"
#include "screen_navigation_key.hpp"

#include <android/input.h>

inline bool isItemContextKey(int32_t key) {
    return key == AKEYCODE_MENU || key == AKEYCODE_INFO;
}

inline ScreenNavigationKey screenNavigationKeyForAndroidKey(int32_t key) {
    if (key == AKEYCODE_BACK) return ScreenNavigationKey::Back;
    if (key == AKEYCODE_SEARCH) return ScreenNavigationKey::Search;
    if (isItemContextKey(key)) return ScreenNavigationKey::Context;
    if (key == AKEYCODE_DPAD_LEFT) return ScreenNavigationKey::Left;
    if (key == AKEYCODE_DPAD_RIGHT) return ScreenNavigationKey::Right;
    if (key == AKEYCODE_DPAD_UP) return ScreenNavigationKey::Up;
    if (key == AKEYCODE_DPAD_DOWN) return ScreenNavigationKey::Down;
    if (key == AKEYCODE_DPAD_CENTER) return ScreenNavigationKey::Activate;
    if (key == AKEYCODE_ENTER) return ScreenNavigationKey::Submit;
    return ScreenNavigationKey::None;
}

inline DetailsNavigationKey detailsNavigationKeyForAndroidKey(int32_t key) {
    if (key == AKEYCODE_BACK) return DetailsNavigationKey::Back;
    if (isItemContextKey(key)) return DetailsNavigationKey::Context;
    if (key == AKEYCODE_DPAD_LEFT) return DetailsNavigationKey::Left;
    if (key == AKEYCODE_DPAD_RIGHT) return DetailsNavigationKey::Right;
    if (key == AKEYCODE_DPAD_UP) return DetailsNavigationKey::Up;
    if (key == AKEYCODE_DPAD_DOWN) return DetailsNavigationKey::Down;
    if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) return DetailsNavigationKey::Activate;
    return DetailsNavigationKey::None;
}

struct TextEntryResult {
    bool handled = false;
    bool searchChanged = false;
};

inline TextEntryResult appendPhysicalText(Screen screen, AccountScreenState& account, SearchScreenState& search,
                                          char value) {
    if (screen == Screen::Login && account.loginFocus() >= 0 && account.loginFocus() < 3) {
        account.appendToFocusedField(value);
        return {.handled = true};
    }
    if (screen == Screen::Search) {
        search.append(value);
        return {.handled = true, .searchChanged = true};
    }
    return {};
}

inline TextEntryResult backspacePhysicalText(Screen screen, AccountScreenState& account, SearchScreenState& search) {
    if (screen == Screen::Login && account.backspaceFocusedField()) return {.handled = true};
    if (screen == Screen::Search && search.backspace()) return {.handled = true, .searchChanged = true};
    return {};
}

inline TextEntryResult handlePhysicalTextInput(Screen screen, AccountScreenState& account, SearchScreenState& search,
                                               int32_t key, int32_t metaState) {
    const char value = keyCodeToChar(key, metaState);
    if (value != 0) return appendPhysicalText(screen, account, search, value);
    if (key == AKEYCODE_DEL) return backspacePhysicalText(screen, account, search);
    return {};
}
