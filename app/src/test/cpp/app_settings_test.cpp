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

    const auto common = matchingSettings("", false);
    assert(!common.empty());
    assert(common.front() == 18);
    assert(common.back() == kAdvancedSettingsToggle);

    const auto filtered = matchingSettings("subtitle", false);
    assert(filtered.size() == 7);
    assert(filtered[0] == 11);
    assert(filtered[1] == 13);
    assert(filtered[2] == 12);
    assert(filtered[3] == kSubtitleLanguagesSetting);
    assert(filtered[4] == kAutoSubtitlesSetting);
    assert(filtered[5] == kAutoSubtitleLanguageSetting);
    assert(filtered[6] == kAutoSubtitleSourceSetting);

    const auto timeFiltered = matchingSettings("time format", false);
    assert(timeFiltered.size() == 1);
    assert(timeFiltered.front() == kTimeFormatSetting);

    const auto advanced = matchingSettings("", true);
    assert(!advanced.empty());
    assert(advanced.back() == kAdvancedSettingsToggle);
    return 0;
}
