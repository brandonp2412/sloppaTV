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

    assert(!settings.clock24Hour);
    effect = adjustSetting(settings, kTimeFormatSetting, 1);
    assert(settings.clock24Hour);
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));

    effect = adjustSetting(settings, kAutoSubtitlesSetting, 1);
    assert(settings.autoSubtitles);
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));
    effect = adjustSetting(settings, kAutoSubtitleLanguageSetting, 1);
    assert(settings.autoSubtitleLanguage == "mri");
    effect = adjustSetting(settings, kAutoSubtitleSourceSetting, 1);
    assert(settings.autoSubtitleSourceLanguage == "different");

    assert(settingValue(settings, 0, 6, "MPV", "viewer", false) == "80 MBIT/S");
    assert(settingValue(settings, 14, 6, "MPV", "viewer", false) == "DIRECT / 6CH ROUTE");
    assert(settingValue(settings, 21, 6, "MPV", "viewer", false) == "MPV");
    assert(settingValue(settings, 23, 6, "MPV", "viewer", false) == "viewer");
    assert(settingValue(settings, 24, 6, "MPV", "viewer", false) == "ALL LANGUAGES");
    assert(settingValue(settings, 25, 6, "MPV", "viewer", false) == "24 HOUR");
    assert(settingValue(settings, 26, 6, "MPV", "viewer", false) == "ON");
    assert(settingValue(settings, 27, 6, "MPV", "viewer", false) == "MAORI");
    assert(settingValue(settings, 28, 6, "MPV", "viewer", false) == "AUDIO NOT MAORI");
    assert(settingValue(settings, 29, 6, "MPV", "viewer", false) == "SHOW TECHNICAL");

    screen.openSubtitleLanguagePicker();
    assert(screen.subtitleLanguagePicker());
    assert(screen.subtitleLanguageSelection() == 0);
    screen.moveSubtitleLanguage(1);
    assert(screen.subtitleLanguageSelection() == 1);
    screen.closeSubtitleLanguagePicker();
    assert(!screen.subtitleLanguagePicker());
    return 0;
}
