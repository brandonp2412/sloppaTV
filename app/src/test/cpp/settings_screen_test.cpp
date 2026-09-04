#include "settings_screen.hpp"

#include <cassert>

int main() {
    SettingsScreenState screen;
    screen.reset();
    assert(!screen.advanced());
    assert(!screen.searchFocused());
    assert(screen.selection() == 18);
    assert(screen.firstVisible() == 0);

    screen.moveUp();
    assert(screen.searchFocused());
    screen.moveDown();
    assert(!screen.searchFocused());
    assert(screen.selection() == 18);

    for (int i = 0; i < 7; ++i) screen.moveDown();
    assert(screen.firstVisible() > 0);

    screen.setSearchText("subtitle");
    assert(screen.searchFocused());
    const auto subtitleMatches = screen.matches();
    assert(!subtitleMatches.empty());
    assert(screen.selection() == subtitleMatches.front());

    screen.toggleAdvanced();
    assert(screen.advanced());
    assert(screen.searchQuery().empty());
    assert(screen.selection() == 0);

    AppSettings settings;
    auto effect = adjustSetting(settings, 0, -1);
    assert(settings.maxBitrateMbps == 80);
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));

    settings.refreshRateSwitching = true;
    effect = adjustSetting(settings, 7, 1);
    assert(!settings.refreshRateSwitching);
    assert(hasSettingEffect(effect, SettingChangeEffect::RestoreDisplayMode));

    effect = adjustSetting(settings, 20, 1);
    assert(settings.screensaverMinutes == 5);
    assert(hasSettingEffect(effect, SettingChangeEffect::ResetScreensaver));

    effect = adjustSetting(settings, 21, 1);
    assert(hasSettingEffect(effect, SettingChangeEffect::CycleExternalPlayer));
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));

    const auto values = settingsValues(settings, 6, "MPV", "viewer", false);
    assert(values[0] == "80 MBIT/S");
    assert(values[14] == "DIRECT / 6CH ROUTE");
    assert(values[21] == "MPV");
    assert(values[23] == "viewer");
    assert(values[24] == "SHOW TECHNICAL");
    return 0;
}
