#pragma once

#include "app_screen.hpp"
#include "display_mode.hpp"
#include "home_screen.hpp"
#include "navigation_stack.hpp"
#include "playback_coordinator.hpp"
#include "settings_flow.hpp"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

struct SettingsScreenEffects {
    SettingsHostAction hostAction = SettingsHostAction::None;
    bool persistSession = false;
    bool refreshSeerrStorage = false;
};

class SettingsScreenCoordinator {
public:
    SettingsScreenCoordinator(SettingsFlow& flow, AppSettings& settings, PlaybackCoordinator& playback,
                              DisplayModeController& displayMode, HomeScreenState& homeState,
                              NavigationStack<Screen>& navigation, Screen& screen,
                              std::chrono::steady_clock::time_point& lastInteraction, bool& screensaverActive,
                              std::string& error)
        : flow_(flow), settings_(settings), playback_(playback), displayMode_(displayMode), homeState_(homeState),
          navigation_(navigation), screen_(screen), lastInteraction_(lastInteraction),
          screensaverActive_(screensaverActive), error_(error) {}

    void open(std::vector<ExternalPlayerApp> players) {
        flow_.refreshExternalPlayers(std::move(players));
        navigation_.push(Screen::Settings);
        screen_ = navigation_.current();
        flow_.reset();
        error_.clear();
    }

    [[nodiscard]] SettingsScreenEffects handle(ScreenNavigationKey key) {
        const SettingsActionEffects effects = flow_.handle(key);
        if (hasSettingEffect(effects.settingEffects, SettingChangeEffect::ApplyVideoZoom))
            playback_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
        if (hasSettingEffect(effects.settingEffects, SettingChangeEffect::RestoreDisplayMode)) displayMode_.restore();
        if (hasSettingEffect(effects.settingEffects, SettingChangeEffect::ResetScreensaver)) {
            lastInteraction_ = std::chrono::steady_clock::now();
            screensaverActive_ = false;
        }

        SettingsScreenEffects result{
            .hostAction = effects.hostAction,
            .persistSession =
                effects.saveSession || hasSettingEffect(effects.settingEffects, SettingChangeEffect::Save),
            .refreshSeerrStorage = effects.refreshSeerrStorage,
        };
        if (result.hostAction == SettingsHostAction::Exit) {
            screen_ = navigation_.popOr(Screen::Home);
            if (screen_ == Screen::Home) homeState_.focusToolbar(3);
            result.hostAction = SettingsHostAction::None;
        }
        return result;
    }

private:
    SettingsFlow& flow_;
    AppSettings& settings_;
    PlaybackCoordinator& playback_;
    DisplayModeController& displayMode_;
    HomeScreenState& homeState_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    std::chrono::steady_clock::time_point& lastInteraction_;
    bool& screensaverActive_;
    std::string& error_;
};
