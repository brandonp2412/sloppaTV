#pragma once

#include "account_screen.hpp"
#include "app_settings.hpp"
#include "search_screen.hpp"
#include "settings_screen.hpp"
#include "system_text_input.hpp"

#include <chrono>
#include <string>
#include <string_view>

inline constexpr int kTextInputSearch = 1;
inline constexpr int kTextInputSettingsSearch = 2;
inline constexpr int kTextInputLoginServer = 10;
inline constexpr int kTextInputLoginPassword = 12;
inline constexpr int kTextInputSeerrServer = 20;
inline constexpr int kTextInputSeerrApiKey = 21;

enum class SystemTextInputNotice {
    None,
    SeerrDisconnected,
    SeerrServerSaved,
    SeerrApiKeyCleared,
    SeerrApiKeySaved,
};

struct SystemTextInputEffects {
    bool scheduleSearch = false;
    bool cancelSeerrSearch = false;
    bool submitSearch = false;
    bool invalidateSeerrStorage = false;
    bool saveSession = false;
    bool refreshSeerr = false;
    SystemTextInputNotice notice = SystemTextInputNotice::None;
    std::chrono::milliseconds renderBurst{};
};

class SystemTextInputController {
public:
    [[nodiscard]] bool active() const { return mode_ >= 0; }

    [[nodiscard]] int mode() const { return mode_; }

    void begin(int mode, std::string_view initial);
    void hide();

    [[nodiscard]] SystemTextInputEffects apply(const SystemTextInputEvent& event, SearchScreenState& search,
                                               SettingsScreenState& settingsScreen, AppSettings& settings,
                                               AccountScreenState& account);

private:
    int mode_ = -1;
    std::string original_;
};
