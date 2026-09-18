#include "settings_navigation_controller.hpp"

namespace {
SettingsScreenInput settingsInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return SettingsScreenInput::Back;
    case ScreenNavigationKey::Search:
        return SettingsScreenInput::Search;
    case ScreenNavigationKey::Up:
        return SettingsScreenInput::Up;
    case ScreenNavigationKey::Down:
        return SettingsScreenInput::Down;
    case ScreenNavigationKey::Left:
        return SettingsScreenInput::Left;
    case ScreenNavigationKey::Right:
        return SettingsScreenInput::Right;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return SettingsScreenInput::Activate;
    case ScreenNavigationKey::None:
    case ScreenNavigationKey::Context:
        return SettingsScreenInput::None;
    }
    return SettingsScreenInput::None;
}

SettingsNavigationAction action(SettingsNavigationActionType type) {
    SettingsNavigationAction result;
    result.type = type;
    return result;
}
} // namespace

SettingsNavigationAction SettingsNavigationController::handle(SettingsScreenState& state, AppSettings& settings,
                                                               ScreenNavigationKey key) {
    const SettingsScreenCommand command = state.handleInput(settingsInput(key));
    switch (command.type) {
    case SettingsScreenCommandType::None:
        return {};
    case SettingsScreenCommandType::Exit:
        return action(SettingsNavigationActionType::Exit);
    case SettingsScreenCommandType::EditSearch:
        return action(SettingsNavigationActionType::EditSearch);
    case SettingsScreenCommandType::Adjust: {
        SettingsNavigationAction result = action(SettingsNavigationActionType::ApplyEffects);
        result.effects = adjustSetting(settings, command.setting, command.direction);
        result.setting = command.setting;
        result.direction = command.direction;
        return result;
    }
    case SettingsScreenCommandType::ActivateSetting: {
        SettingsNavigationAction result = action(SettingsNavigationActionType::Activate);
        result.setting = command.setting;
        result.activation = settingActivation(command.setting);
        return result;
    }
    case SettingsScreenCommandType::ToggleSubtitleLanguage:
        return action(SettingsNavigationActionType::ToggleSubtitleLanguage);
    }
    return {};
}
