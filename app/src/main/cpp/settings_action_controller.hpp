#pragma once

#include "external_player_types.hpp"
#include "settings_navigation_controller.hpp"

#include <optional>
#include <string>
#include <vector>

enum class SettingsHostAction {
    None,
    Exit,
    EditSearch,
    OpenDiagnostics,
    SwitchUser,
    EditSeerrServer,
    ConnectSeerr,
    EditSeerrApiKey,
};

struct SettingsActionEffects {
    SettingChangeEffect settingEffects = SettingChangeEffect::None;
    SettingsHostAction hostAction = SettingsHostAction::None;
    bool saveSession = false;
    bool refreshSeerrStorage = false;
};

class SettingsActionController {
public:
    [[nodiscard]] static SettingsActionEffects apply(const SettingsNavigationAction& navigation,
                                                     SettingsScreenState& screen, AppSettings& settings,
                                                     const std::vector<ExternalPlayerApp>& externalPlayers);

    static void reconcileExternalPlayer(AppSettings& settings, const std::vector<ExternalPlayerApp>& externalPlayers);
    [[nodiscard]] static std::string externalPlayerLabel(const AppSettings& settings,
                                                         const std::vector<ExternalPlayerApp>& externalPlayers);
    [[nodiscard]] static std::optional<ExternalPlayerApp>
    selectedExternalPlayer(const AppSettings& settings, const std::vector<ExternalPlayerApp>& externalPlayers);
};
