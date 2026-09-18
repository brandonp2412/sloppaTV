#include "settings_screen.hpp"

#include <cassert>

int main() {
    SettingsScreenState screen;
    screen.reset();
    assert(!screen.advanced());
    assert(!screen.searchFocused());
    assert(screen.selection() == SettingId::UiTextSize);
    assert(screen.firstVisible() == 0);

    auto command = screen.handleInput(SettingsScreenInput::Up);
    assert(command.type == SettingsScreenCommandType::None);
    assert(screen.searchFocused());
    command = screen.handleInput(SettingsScreenInput::Down);
    assert(command.type == SettingsScreenCommandType::None);
    assert(!screen.searchFocused());
    assert(screen.selection() == SettingId::UiTextSize);

    command = screen.handleInput(SettingsScreenInput::Left);
    assert(command.type == SettingsScreenCommandType::Adjust);
    assert(command.setting == SettingId::UiTextSize);
    assert(command.direction == -1);
    command = screen.handleInput(SettingsScreenInput::Right);
    assert(command.type == SettingsScreenCommandType::Adjust);
    assert(command.setting == SettingId::UiTextSize);
    assert(command.direction == 1);
    command = screen.handleInput(SettingsScreenInput::Activate);
    assert(command.type == SettingsScreenCommandType::ActivateSetting);
    assert(command.setting == SettingId::UiTextSize);

    screen.reset();
    command = screen.handleInput(SettingsScreenInput::Search);
    assert(command.type == SettingsScreenCommandType::EditSearch);
    assert(screen.searchFocused());
    command = screen.handleInput(SettingsScreenInput::Activate);
    assert(command.type == SettingsScreenCommandType::EditSearch);
    command = screen.handleInput(SettingsScreenInput::Back);
    assert(command.type == SettingsScreenCommandType::Exit);

    screen.reset();
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
    assert(screen.selection() == SettingId::MaxStreamingBitrate);

    AppSettings settings;
    auto effect = adjustSetting(settings, SettingId::MaxStreamingBitrate, -1);
    assert(settings.maxBitrateMbps == 80);
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));

    settings.refreshRateSwitching = true;
    effect = adjustSetting(settings, SettingId::MatchVideoRefreshRate, 1);
    assert(!settings.refreshRateSwitching);
    assert(hasSettingEffect(effect, SettingChangeEffect::RestoreDisplayMode));

    effect = adjustSetting(settings, SettingId::Screensaver, 1);
    assert(settings.screensaverMinutes == 5);
    assert(hasSettingEffect(effect, SettingChangeEffect::ResetScreensaver));

    effect = adjustSetting(settings, SettingId::ExternalPlayer, 1);
    assert(hasSettingEffect(effect, SettingChangeEffect::CycleExternalPlayer));
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));

    assert(!settings.clock24Hour);
    effect = adjustSetting(settings, SettingId::TimeFormat, 1);
    assert(settings.clock24Hour);
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));

    effect = adjustSetting(settings, SettingId::AutoSubtitles, 1);
    assert(settings.autoSubtitles);
    assert(hasSettingEffect(effect, SettingChangeEffect::Save));
    effect = adjustSetting(settings, SettingId::AutoSubtitleLanguage, 1);
    assert(settings.autoSubtitleLanguage == "mri");
    effect = adjustSetting(settings, SettingId::AutoSubtitleSourceAudio, 1);
    assert(settings.autoSubtitleSourceLanguage == "different");

    settings.seerrServer = "https://seerr.example.nz";
    settings.seerrSessionCookie = "connect.sid=session";
    settings.seerrApiKey = "secret";
    settings.seerrSelectDrive = true;
    const auto valueFor = [&](SettingId setting) { return settingValue(settings, setting, 6, "MPV", "viewer", false); };
    assert(valueFor(SettingId::MaxStreamingBitrate) == "80 MBIT/S");
    assert(valueFor(SettingId::AudioOutput) == "DIRECT / 6CH ROUTE");
    assert(valueFor(SettingId::ExternalPlayer) == "MPV");
    assert(valueFor(SettingId::SwitchUser) == "viewer");
    assert(valueFor(SettingId::SubtitleLanguages) == "ALL LANGUAGES");
    assert(valueFor(SettingId::TimeFormat) == "24 HOUR");
    assert(valueFor(SettingId::AutoSubtitles) == "ON");
    assert(valueFor(SettingId::AutoSubtitleLanguage) == "MAORI");
    assert(valueFor(SettingId::AutoSubtitleSourceAudio) == "AUDIO NOT MAORI");
    assert(valueFor(SettingId::SeerrServer) == "https://seerr.example.nz");
    assert(valueFor(SettingId::SeerrConnection) == "CONNECTED");
    assert(valueFor(SettingId::SeerrDriveSelection) == "ON");
    assert(valueFor(SettingId::SeerrApiKey) == "SET");
    assert(valueFor(SettingId::AdvancedToggle) == "SHOW TECHNICAL");

    screen.reset();
    auto row = settingsScreenRow(screen, settings, 6, "MPV", "viewer", false, 0);
    assert(row);
    assert(row->setting == SettingId::UiTextSize);
    assert(row->label == "UI TEXT SIZE");
    assert(row->focused);
    assert(!row->action);
    assert(!row->boolean);
    assert(settingsScreenRow(screen, settings, 6, "MPV", "viewer", false, 5));
    assert(!settingsScreenRow(screen, settings, 6, "MPV", "viewer", false, 6));

    screen.toggleAdvanced();
    screen.setSearchText("Seerr API key");
    row = settingsScreenRow(screen, settings, 6, "MPV", "viewer", true, 0);
    assert(row);
    assert(row->setting == SettingId::SeerrApiKey);
    assert(row->label == "SEERR API KEY (LEGACY)");
    assert(row->value == "******");
    assert(!row->focused);
    assert(row->action);
    assert(!row->boolean);
    assert(!settingsScreenRow(screen, settings, 6, "MPV", "viewer", true, 1));

    screen.reset();
    screen.setSearchText("seerr");
    const auto seerrMatches = screen.matches();
    assert(seerrMatches.size() == 3);
    assert(seerrMatches[0] == SettingId::SeerrServer);
    assert(seerrMatches[1] == SettingId::SeerrConnection);
    assert(seerrMatches[2] == SettingId::SeerrDriveSelection);

    screen.openSubtitleLanguagePicker();
    assert(screen.subtitleLanguagePicker());
    assert(screen.subtitleLanguageSelection() == 0);
    command = screen.handleInput(SettingsScreenInput::Down);
    assert(command.type == SettingsScreenCommandType::None);
    assert(screen.subtitleLanguageSelection() == 1);
    command = screen.handleInput(SettingsScreenInput::Activate);
    assert(command.type == SettingsScreenCommandType::ToggleSubtitleLanguage);
    assert(screen.subtitleLanguagePicker());
    command = screen.handleInput(SettingsScreenInput::Back);
    assert(command.type == SettingsScreenCommandType::None);
    assert(!screen.subtitleLanguagePicker());
    return 0;
}
