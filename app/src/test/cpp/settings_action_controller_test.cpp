#include "settings_action_controller.hpp"

#include <cassert>

namespace {
ExternalPlayerApp player(std::string component, std::string label) {
    ExternalPlayerApp value;
    value.componentName = std::move(component);
    value.label = std::move(label);
    return value;
}
} // namespace

int main() {
    SettingsScreenState screen;
    screen.reset();
    AppSettings settings;
    const std::vector<ExternalPlayerApp> players{
        player("org.example/.One", "Player One"),
        player("org.example/.Two", "Player Two"),
    };

    {
        SettingsNavigationAction navigation{
            .type = SettingsNavigationActionType::ApplyEffects,
            .effects = SettingChangeEffect::Save | SettingChangeEffect::CycleExternalPlayer,
            .direction = 1,
        };
        const auto effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(settings.externalPlayerComponent == "org.example/.One");
        assert(hasSettingEffect(effects.settingEffects, SettingChangeEffect::Save));
        assert(SettingsActionController::externalPlayerLabel(settings, players) == "Player One");
        const auto selected = SettingsActionController::selectedExternalPlayer(settings, players);
        assert(selected);
        assert(selected->componentName == "org.example/.One");

        const auto second = SettingsActionController::apply(navigation, screen, settings, players);
        assert(second.hostAction == SettingsHostAction::None);
        assert(settings.externalPlayerComponent == "org.example/.Two");

        navigation.direction = -1;
        const auto previous = SettingsActionController::apply(navigation, screen, settings, players);
        assert(previous.hostAction == SettingsHostAction::None);
        assert(settings.externalPlayerComponent == "org.example/.One");
    }

    settings.externalPlayerComponent = "missing/.Player";
    SettingsActionController::reconcileExternalPlayer(settings, players);
    assert(settings.externalPlayerComponent.empty());
    assert(SettingsActionController::externalPlayerLabel(settings, players) == "INTERNAL");
    assert(!SettingsActionController::selectedExternalPlayer(settings, players));

    {
        SettingsNavigationAction navigation{
            .type = SettingsNavigationActionType::Activate,
            .activation = SettingActivation::OpenSubtitleLanguages,
        };
        auto effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(screen.subtitleLanguagePicker());
        assert(effects.hostAction == SettingsHostAction::None);

        screen.moveSubtitleLanguage(1);
        navigation = {.type = SettingsNavigationActionType::ToggleSubtitleLanguage};
        effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(effects.saveSession);
        assert(settings.subtitleLanguages.size() == 1);
        assert(settings.subtitleLanguages.front() == kSubtitleLanguageOptions.front().code);

        effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(settings.subtitleLanguages.empty());
    }

    {
        settings.seerrSelectDrive = false;
        SettingsNavigationAction navigation{
            .type = SettingsNavigationActionType::Activate,
            .activation = SettingActivation::ToggleSeerrDriveSelection,
        };
        auto effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(settings.seerrSelectDrive);
        assert(effects.saveSession);
        assert(effects.refreshSeerrStorage);

        effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(!settings.seerrSelectDrive);
        assert(effects.saveSession);
        assert(!effects.refreshSeerrStorage);
    }

    {
        SettingsNavigationAction navigation{
            .type = SettingsNavigationActionType::Activate,
            .activation = SettingActivation::OpenDiagnostics,
        };
        const auto effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(effects.hostAction == SettingsHostAction::OpenDiagnostics);
    }

    {
        SettingsNavigationAction navigation{.type = SettingsNavigationActionType::Exit};
        const auto effects = SettingsActionController::apply(navigation, screen, settings, players);
        assert(effects.hostAction == SettingsHostAction::Exit);
    }

    return 0;
}
