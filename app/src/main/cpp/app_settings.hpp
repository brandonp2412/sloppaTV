#pragma once

#include "media_player_policy.hpp"
#include "screensaver_policy.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

enum class VideoZoomMode {
    Fit = 0,
    Fill = 1,
    Stretch = 2,
};

struct AppSettings {
    int maxBitrateMbps = 120;
    int playbackBufferPreset = 0;
    int seekBackSeconds = 10;
    int seekForwardSeconds = 10;
    int zoomMode = static_cast<int>(VideoZoomMode::Fit);
    bool autoplayNext = true;
    int stillWatchingAfter = 3;
    bool refreshRateSwitching = false;
    bool showWatchedIndicators = true;
    bool showClock = true;
    int backdropMode = 1;
    int subtitleSize = 1;
    bool subtitleBackground = true;
    int subtitlePosition = 0;
    int maxAudioChannels = 8;
    int avcLevelOverride = 0;
    int hevcLevelOverride = 0;
    int hdrOverride = static_cast<int>(HdrOverrideMode::Auto);
    int uiTextSize = 0;
    int safeAreaPercent = 0;
    int screensaverMinutes = 0;
    std::string externalPlayerComponent;
};

constexpr int kAdvancedSettingsToggle = 24;

inline PlaybackOverrides playbackOverridesFor(const AppSettings& settings) {
    return {
        .maxAvcLevel = settings.avcLevelOverride,
        .maxHevcLevel = settings.hevcLevelOverride,
        .hdrMode = static_cast<HdrOverrideMode>(std::clamp(settings.hdrOverride, 0, 2)),
    };
}

inline std::string videoZoomName(VideoZoomMode mode) {
    switch (mode) {
        case VideoZoomMode::Fit: return "FIT";
        case VideoZoomMode::Fill: return "FILL";
        case VideoZoomMode::Stretch: return "STRETCH";
    }
    return "FIT";
}

inline std::string subtitleSizeName(int size) {
    static constexpr std::array<const char*, 3> names{"SMALL", "MEDIUM", "LARGE"};
    return names[static_cast<size_t>(std::clamp(size, 0, 2))];
}

inline std::string subtitlePositionName(int position) {
    static constexpr std::array<const char*, 3> names{"LOW", "MIDDLE", "HIGH"};
    return names[static_cast<size_t>(std::clamp(position, 0, 2))];
}

inline std::string uiTextSizeName(int size) {
    static constexpr std::array<const char*, 3> names{"NORMAL", "LARGE", "EXTRA LARGE"};
    return names[static_cast<size_t>(std::clamp(size, 0, 2))];
}

inline std::string screensaverName(int minutes) {
    minutes = normalizedScreensaverMinutes(minutes);
    return minutes <= 0 ? "OFF" : std::to_string(minutes) + " MINUTES";
}

inline std::string avcLevelName(int level) {
    if (level <= 0) return "AUTO";
    return std::to_string(level / 10) + "." + std::to_string(level % 10);
}

inline std::string hevcLevelName(int level) {
    switch (level) {
        case 120: return "4.0";
        case 123: return "4.1";
        case 150: return "5.0";
        case 153: return "5.1";
        case 156: return "5.2";
        case 180: return "6.0";
        case 183: return "6.1";
        case 186: return "6.2";
        default: return "AUTO";
    }
}

inline std::string hdrOverrideName(int mode) {
    switch (static_cast<HdrOverrideMode>(std::clamp(mode, 0, 2))) {
        case HdrOverrideMode::ForceSdr: return "SDR ONLY";
        case HdrOverrideMode::AllowAllHdr: return "ALLOW ALL HDR";
        case HdrOverrideMode::Auto: return "AUTO";
    }
    return "AUTO";
}

inline std::string backdropModeName(int mode) {
    switch (std::clamp(mode, 0, 2)) {
        case 1: return "DIMMED";
        case 2: return "CLEAR";
        default: return "OFF";
    }
}

inline std::string playbackBufferName(int preset) {
    switch (std::clamp(preset, 0, 2)) {
        case 1: return "LARGE";
        case 2: return "EXTRA LARGE";
        default: return "AUTO";
    }
}

inline float subtitleTextScale(int size) {
    static constexpr std::array<float, 3> scales{2.55f, 3.1f, 3.7f};
    return scales[static_cast<size_t>(std::clamp(size, 0, 2))];
}

inline const std::array<std::string, 25>& settingsLabels() {
    static const std::array<std::string, 25> labels{
        "MAX STREAMING BITRATE",
        "PLAYBACK BUFFER",
        "SKIP BACK",
        "SKIP AHEAD",
        "DEFAULT VIDEO ZOOM",
        "AUTOPLAY NEXT EPISODE",
        "STILL WATCHING AFTER",
        "MATCH VIDEO REFRESH RATE",
        "WATCHED INDICATORS",
        "CLOCK",
        "BACKDROPS",
        "SUBTITLE SIZE",
        "SUBTITLE BACKGROUND",
        "SUBTITLE POSITION",
        "AUDIO OUTPUT",
        "AVC / H.264 MAX LEVEL",
        "HEVC / H.265 MAX LEVEL",
        "HDR PLAYBACK",
        "UI TEXT SIZE",
        "OVERSCAN SAFE AREA",
        "IN-APP SCREENSAVER",
        "EXTERNAL PLAYER",
        "DIAGNOSTICS",
        "SWITCH USER",
        "ADVANCED SETTINGS",
    };
    return labels;
}

inline bool settingLabelContains(std::string_view text, std::string_view query) {
    if (query.empty()) return true;
    if (query.size() > text.size()) return false;
    return std::search(
        text.begin(), text.end(),
        query.begin(), query.end(),
        [](unsigned char left, unsigned char right) {
            return std::toupper(left) == std::toupper(right);
        }
    ) != text.end();
}

inline std::vector<int> matchingSettings(const std::string& query, bool advanced) {
    static constexpr std::array<int, 15> commonOrder{
        18, 10, 11, 13, 12, 5, 6, 2, 3, 8, 9, 14, 20, 23, kAdvancedSettingsToggle,
    };
    static constexpr std::array<int, 12> advancedOrder{
        0, 1, 4, 7, 17, 19, 15, 16, 21, 22, 23, kAdvancedSettingsToggle,
    };

    std::vector<int> matches;
    const auto& labels = settingsLabels();
    if (!query.empty()) {
        const auto appendMatches = [&](const auto& order) {
            for (const int i : order) {
                const std::string label = advanced && i == kAdvancedSettingsToggle
                    ? "BASIC SETTINGS"
                    : labels[static_cast<size_t>(i)];
                if (settingLabelContains(label, query)) matches.push_back(i);
            }
        };
        if (advanced) appendMatches(advancedOrder);
        else appendMatches(commonOrder);
        return matches;
    }

    if (advanced) matches.assign(advancedOrder.begin(), advancedOrder.end());
    else matches.assign(commonOrder.begin(), commonOrder.end());
    return matches;
}
