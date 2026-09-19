#pragma once

#include "settings_action_controller.hpp"
#include "settings_navigation_controller.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

class SettingsFlow {
public:
    explicit SettingsFlow(AppSettings& settings) : settings_(settings) {}

    [[nodiscard]] SettingsScreenState& state() { return state_; }

    [[nodiscard]] const SettingsScreenState& state() const { return state_; }

    void reset() { state_.reset(); }

    void refreshExternalPlayers(std::vector<ExternalPlayerApp> players) {
        externalPlayers_ = std::move(players);
        SettingsActionController::reconcileExternalPlayer(settings_, externalPlayers_);
    }

    [[nodiscard]] const std::vector<ExternalPlayerApp>& externalPlayers() const { return externalPlayers_; }

    [[nodiscard]] std::string externalPlayerLabel() const {
        return SettingsActionController::externalPlayerLabel(settings_, externalPlayers_);
    }

    [[nodiscard]] std::optional<ExternalPlayerApp> selectedExternalPlayer() const {
        return SettingsActionController::selectedExternalPlayer(settings_, externalPlayers_);
    }

    [[nodiscard]] SettingsActionEffects handle(ScreenNavigationKey key) {
        const SettingsNavigationAction navigation = SettingsNavigationController::handle(state_, settings_, key);
        return SettingsActionController::apply(navigation, state_, settings_, externalPlayers_);
    }

private:
    AppSettings& settings_;
    SettingsScreenState state_;
    std::vector<ExternalPlayerApp> externalPlayers_;
};
