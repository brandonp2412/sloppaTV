#include "settings_action_controller.hpp"

#include <algorithm>

namespace {
auto findExternalPlayer(const AppSettings& settings, const std::vector<ExternalPlayerApp>& externalPlayers) {
    return std::find_if(externalPlayers.begin(), externalPlayers.end(), [&](const ExternalPlayerApp& player) {
        return player.componentName == settings.externalPlayerComponent;
    });
}

void cycleExternalPlayer(AppSettings& settings, const std::vector<ExternalPlayerApp>& externalPlayers, int direction) {
    if (externalPlayers.empty()) {
        settings.externalPlayerComponent.clear();
        return;
    }

    int index = 0;
    if (!settings.externalPlayerComponent.empty()) {
        const auto selected = findExternalPlayer(settings, externalPlayers);
        if (selected != externalPlayers.end())
            index = static_cast<int>(std::distance(externalPlayers.begin(), selected)) + 1;
    }
    index = std::clamp(index + direction, 0, static_cast<int>(externalPlayers.size()));
    settings.externalPlayerComponent =
        index == 0 ? std::string{} : externalPlayers[static_cast<size_t>(index - 1)].componentName;
}

void toggleSubtitleLanguage(SettingsScreenState& screen, AppSettings& settings) {
    const int selection = screen.subtitleLanguageSelection();
    if (selection <= 0) {
        settings.subtitleLanguages.clear();
        return;
    }

    const std::string code = kSubtitleLanguageOptions[static_cast<size_t>(selection - 1)].code;
    const auto current = std::find(settings.subtitleLanguages.begin(), settings.subtitleLanguages.end(), code);
    if (settings.subtitleLanguages.empty() || current == settings.subtitleLanguages.end())
        settings.subtitleLanguages.push_back(code);
    else
        settings.subtitleLanguages.erase(current);
}

SettingsActionEffects hostAction(SettingsHostAction action) {
    SettingsActionEffects effects;
    effects.hostAction = action;
    return effects;
}
} // namespace

SettingsActionEffects SettingsActionController::apply(const SettingsNavigationAction& navigation,
                                                      SettingsScreenState& screen, AppSettings& settings,
                                                      const std::vector<ExternalPlayerApp>& externalPlayers) {
    switch (navigation.type) {
    case SettingsNavigationActionType::None:
        return {};
    case SettingsNavigationActionType::Exit:
        return hostAction(SettingsHostAction::Exit);
    case SettingsNavigationActionType::EditSearch:
        return hostAction(SettingsHostAction::EditSearch);
    case SettingsNavigationActionType::ApplyEffects: {
        SettingsActionEffects effects;
        effects.settingEffects = navigation.effects;
        if (hasSettingEffect(navigation.effects, SettingChangeEffect::CycleExternalPlayer))
            cycleExternalPlayer(settings, externalPlayers, navigation.direction);
        return effects;
    }
    case SettingsNavigationActionType::Activate:
        switch (navigation.activation) {
        case SettingActivation::None:
            return {};
        case SettingActivation::OpenDiagnostics:
            return hostAction(SettingsHostAction::OpenDiagnostics);
        case SettingActivation::SwitchUser:
            return hostAction(SettingsHostAction::SwitchUser);
        case SettingActivation::OpenSubtitleLanguages:
            screen.openSubtitleLanguagePicker();
            return {};
        case SettingActivation::EditSeerrServer:
            return hostAction(SettingsHostAction::EditSeerrServer);
        case SettingActivation::ConnectSeerr:
            return hostAction(SettingsHostAction::ConnectSeerr);
        case SettingActivation::ToggleSeerrDriveSelection: {
            SettingsActionEffects effects;
            settings.seerrSelectDrive = !settings.seerrSelectDrive;
            effects.saveSession = true;
            effects.refreshSeerrStorage = settings.seerrSelectDrive;
            return effects;
        }
        case SettingActivation::EditSeerrApiKey:
            return hostAction(SettingsHostAction::EditSeerrApiKey);
        case SettingActivation::ToggleAdvanced:
            screen.toggleAdvanced();
            return {};
        }
        return {};
    case SettingsNavigationActionType::ToggleSubtitleLanguage: {
        SettingsActionEffects effects;
        toggleSubtitleLanguage(screen, settings);
        effects.saveSession = true;
        return effects;
    }
    }
    return {};
}

void SettingsActionController::reconcileExternalPlayer(AppSettings& settings,
                                                       const std::vector<ExternalPlayerApp>& externalPlayers) {
    if (settings.externalPlayerComponent.empty()) return;
    if (findExternalPlayer(settings, externalPlayers) == externalPlayers.end())
        settings.externalPlayerComponent.clear();
}

std::string SettingsActionController::externalPlayerLabel(const AppSettings& settings,
                                                          const std::vector<ExternalPlayerApp>& externalPlayers) {
    if (settings.externalPlayerComponent.empty()) return "INTERNAL";
    const auto selected = findExternalPlayer(settings, externalPlayers);
    return selected == externalPlayers.end() ? "INTERNAL" : selected->label;
}

std::optional<ExternalPlayerApp>
SettingsActionController::selectedExternalPlayer(const AppSettings& settings,
                                                 const std::vector<ExternalPlayerApp>& externalPlayers) {
    if (settings.externalPlayerComponent.empty()) return std::nullopt;
    const auto selected = findExternalPlayer(settings, externalPlayers);
    if (selected == externalPlayers.end()) return std::nullopt;
    return *selected;
}
