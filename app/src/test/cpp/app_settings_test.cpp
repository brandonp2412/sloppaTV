#include "app_settings.hpp"

#include <cassert>

int main() {
    AppSettings settings;
    assert(settings.maxBitrateMbps == 120);
    assert(settings.zoomMode == static_cast<int>(VideoZoomMode::Fit));
    assert(playbackOverridesFor(settings).hdrMode == HdrOverrideMode::Auto);
    assert(videoZoomName(VideoZoomMode::Fill) == "FILL");
    assert(subtitleSizeName(2) == "LARGE");
    assert(subtitlePositionName(0) == "LOW");
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
    assert(filtered.size() == 3);
    assert(filtered[0] == 11);
    assert(filtered[1] == 13);
    assert(filtered[2] == 12);

    const auto advanced = matchingSettings("", true);
    assert(!advanced.empty());
    assert(advanced.back() == kAdvancedSettingsToggle);
    return 0;
}
