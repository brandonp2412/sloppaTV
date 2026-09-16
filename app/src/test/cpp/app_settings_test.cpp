#include "app_settings.hpp"

#include <cassert>

int main() {
    AppSettings settings;
    assert(settings.maxBitrateMbps == 120);
    assert(settings.zoomMode == static_cast<int>(VideoZoomMode::Fit));
    assert(playbackOverridesFor(settings).hdrMode == HdrOverrideMode::Auto);
    assert(videoZoomName(VideoZoomMode::Fill) == "FILL");
    assert(settings.subtitleSize == 1);
    assert(!settings.subtitleBackground);
    assert(settings.subtitleLanguages.empty());
    assert(!settings.autoSubtitles);
    assert(settings.autoSubtitleLanguage == "eng");
    assert(settings.autoSubtitleSourceLanguage == "any");
    assert(autoSubtitleSourceName(settings) == "ANY AUDIO");
    assert(!settings.clock24Hour);
    assert(clockFormatName(settings.clock24Hour) == "12 HOUR (AM/PM)");
    assert(clockFormatName(true) == "24 HOUR");
    assert(subtitleSizeName(2) == "LARGE");
    assert(subtitlePositionName(0) == "LOW");
    assert(subtitleLanguageSummary(settings) == "ALL LANGUAGES");
    assert(uiTextSizeName(2) == "EXTRA LARGE");
    assert(playbackBufferName(1) == "LARGE");
    assert(subtitleTextScale(0) == 2.55f);
    assert(settingLabelContains("AUDIO OUTPUT", "audio"));
    assert(!settingLabelContains("SUBTITLE SIZE", "audio"));
    assert(isBooleanSetting(SettingId::AutoplayNextEpisode));
    assert(isBooleanSetting(SettingId::MatchVideoRefreshRate));
    assert(isBooleanSetting(SettingId::WatchedIndicators));
    assert(isBooleanSetting(SettingId::Clock));
    assert(isBooleanSetting(SettingId::SubtitleBackground));
    assert(isBooleanSetting(SettingId::AutoSubtitles));
    assert(isBooleanSetting(SettingId::SeerrDriveSelection));
    assert(!isBooleanSetting(SettingId::Backdrops));
    assert(!isBooleanSetting(SettingId::TimeFormat));
    assert(isActionSetting(SettingId::Diagnostics));
    assert(isActionSetting(SettingId::AdvancedToggle));
    assert(!isActionSetting(SettingId::SeerrDriveSelection));
    assert(settingActivation(SettingId::MaxStreamingBitrate) == SettingActivation::None);
    assert(settingActivation(SettingId::Diagnostics) == SettingActivation::OpenDiagnostics);
    assert(settingActivation(SettingId::SwitchUser) == SettingActivation::SwitchUser);
    assert(settingActivation(SettingId::SubtitleLanguages) == SettingActivation::OpenSubtitleLanguages);
    assert(settingActivation(SettingId::SeerrServer) == SettingActivation::EditSeerrServer);
    assert(settingActivation(SettingId::SeerrConnection) == SettingActivation::ConnectSeerr);
    assert(settingActivation(SettingId::SeerrDriveSelection) == SettingActivation::ToggleSeerrDriveSelection);
    assert(settingActivation(SettingId::SeerrApiKey) == SettingActivation::EditSeerrApiKey);
    assert(settingActivation(SettingId::AdvancedToggle) == SettingActivation::ToggleAdvanced);
    assert(isAdjustableSetting(SettingId::MaxStreamingBitrate));
    assert(isAdjustableSetting(SettingId::Screensaver));
    assert(isAdjustableSetting(SettingId::ExternalPlayer));
    assert(!isAdjustableSetting(SettingId::Diagnostics));
    assert(!isAdjustableSetting(SettingId::SeerrConnection));
    assert(!isAdjustableSetting(SettingId::AdvancedToggle));
    assert(hasSettingEffect(settingChangeEffects(SettingId::Screensaver), SettingChangeEffect::Save));
    assert(hasSettingEffect(settingChangeEffects(SettingId::Screensaver), SettingChangeEffect::ResetScreensaver));
    assert(hasSettingEffect(settingChangeEffects(SettingId::ExternalPlayer), SettingChangeEffect::CycleExternalPlayer));
    assert(settingLabel(SettingId::AdvancedToggle, false) == "ADVANCED SETTINGS");
    assert(settingLabel(SettingId::AdvancedToggle, true) == "BASIC SETTINGS");
    for (size_t i = 0; i < kSettingDescriptors.size(); ++i) {
        const auto& descriptor = kSettingDescriptors[i];
        assert(settingIndex(descriptor.id) == i);
        assert((descriptor.adjuster != nullptr) == (descriptor.changeEffects != SettingChangeEffect::None));
        assert(descriptor.valueRenderer != nullptr);
    }

    AppSettings adjusted;
    auto effects = adjustSetting(adjusted, SettingId::MaxStreamingBitrate, -1);
    assert(adjusted.maxBitrateMbps == 80);
    assert(hasSettingEffect(effects, SettingChangeEffect::Save));
    adjusted.refreshRateSwitching = true;
    effects = adjustSetting(adjusted, SettingId::MatchVideoRefreshRate, 1);
    assert(!adjusted.refreshRateSwitching);
    assert(hasSettingEffect(effects, SettingChangeEffect::RestoreDisplayMode));
    effects = adjustSetting(adjusted, SettingId::ExternalPlayer, 1);
    assert(hasSettingEffect(effects, SettingChangeEffect::Save));
    assert(hasSettingEffect(effects, SettingChangeEffect::CycleExternalPlayer));
    effects = adjustSetting(adjusted, SettingId::Diagnostics, 1);
    assert(effects == SettingChangeEffect::None);

    const auto common = matchingSettings("", false);
    assert(common.size() == 23);
    assert(common.front() == SettingId::UiTextSize);
    assert(common.back() == SettingId::AdvancedToggle);

    const auto filtered = matchingSettings("subtitle", false);
    assert(filtered.size() == 7);
    assert(filtered[0] == SettingId::SubtitleSize);
    assert(filtered[1] == SettingId::SubtitlePosition);
    assert(filtered[2] == SettingId::SubtitleBackground);
    assert(filtered[3] == SettingId::SubtitleLanguages);
    assert(filtered[4] == SettingId::AutoSubtitles);
    assert(filtered[5] == SettingId::AutoSubtitleLanguage);
    assert(filtered[6] == SettingId::AutoSubtitleSourceAudio);

    const auto timeFiltered = matchingSettings("time format", false);
    assert(timeFiltered.size() == 1);
    assert(timeFiltered.front() == SettingId::TimeFormat);

    const auto advanced = matchingSettings("", true);
    assert(advanced.size() == 13);
    assert(advanced.front() == SettingId::MaxStreamingBitrate);
    assert(advanced.back() == SettingId::AdvancedToggle);
    return 0;
}
