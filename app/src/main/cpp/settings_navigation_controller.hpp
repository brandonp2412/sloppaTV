#pragma once

#include "screen_navigation_key.hpp"
#include "settings_screen.hpp"

enum class SettingsNavigationActionType {
    None,
    Exit,
    EditSearch,
    ApplyEffects,
    Activate,
    ToggleSubtitleLanguage,
};

struct SettingsNavigationAction {
    SettingsNavigationActionType type = SettingsNavigationActionType::None;
    SettingChangeEffect effects = SettingChangeEffect::None;
    SettingActivation activation = SettingActivation::None;
    SettingId setting = SettingId::UiTextSize;
    int direction = 0;
};

class SettingsNavigationController {
public:
    [[nodiscard]] static SettingsNavigationAction handle(SettingsScreenState& state, AppSettings& settings,
                                                         ScreenNavigationKey key);
};
