#pragma once

#include "media_player_policy.hpp"
#include "screensaver_policy.hpp"
#include "subtitle_policy.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum class VideoZoomMode {
    Fit = 0,
    Fill = 1,
    Stretch = 2,
};

struct SubtitleLanguageOption {
    const char* code;
    const char* label;
};

inline constexpr std::array<SubtitleLanguageOption, 16> kSubtitleLanguageOptions{{
    {"eng", "ENGLISH"},
    {"mri", "MAORI"},
    {"jpn", "JAPANESE"},
    {"spa", "SPANISH"},
    {"fra", "FRENCH"},
    {"deu", "GERMAN"},
    {"ita", "ITALIAN"},
    {"por", "PORTUGUESE"},
    {"kor", "KOREAN"},
    {"zho", "CHINESE"},
    {"ara", "ARABIC"},
    {"nld", "DUTCH"},
    {"rus", "RUSSIAN"},
    {"hin", "HINDI"},
    {"swe", "SWEDISH"},
    {"nor", "NORWEGIAN"},
}};

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
    bool clock24Hour = false;
    int backdropMode = 1;
    int subtitleSize = 1;
    bool subtitleBackground = false;
    int subtitlePosition = 0;
    std::vector<std::string> subtitleLanguages;
    bool autoSubtitles = false;
    std::string autoSubtitleLanguage = "eng";
    std::string autoSubtitleSourceLanguage = "any";
    int maxAudioChannels = 8;
    int avcLevelOverride = 0;
    int hevcLevelOverride = 0;
    int hdrOverride = static_cast<int>(HdrOverrideMode::Auto);
    int uiTextSize = 0;
    int safeAreaPercent = 0;
    int screensaverMinutes = 0;
    std::string externalPlayerComponent;
    std::string seerrServer;
    std::string seerrSessionCookie;
    std::string seerrApiKey;
    bool seerrSelectDrive = false;
};

enum class SettingId : uint8_t {
    MaxStreamingBitrate = 0,
    PlaybackBuffer,
    SkipBack,
    SkipAhead,
    DefaultVideoZoom,
    AutoplayNextEpisode,
    StillWatchingAfter,
    MatchVideoRefreshRate,
    WatchedIndicators,
    Clock,
    Backdrops,
    SubtitleSize,
    SubtitleBackground,
    SubtitlePosition,
    AudioOutput,
    AvcMaxLevel,
    HevcMaxLevel,
    HdrPlayback,
    UiTextSize,
    OverscanSafeArea,
    Screensaver,
    ExternalPlayer,
    Diagnostics,
    SwitchUser,
    SubtitleLanguages,
    TimeFormat,
    AutoSubtitles,
    AutoSubtitleLanguage,
    AutoSubtitleSourceAudio,
    SeerrServer,
    SeerrConnection,
    SeerrDriveSelection,
    SeerrApiKey,
    AdvancedToggle,
};

enum class SettingChangeEffect : uint8_t {
    None = 0,
    Save = 1 << 0,
    RestoreDisplayMode = 1 << 1,
    ResetScreensaver = 1 << 2,
    CycleExternalPlayer = 1 << 3,
};

enum class SettingActivation : uint8_t {
    None = 0,
    OpenDiagnostics,
    SwitchUser,
    OpenSubtitleLanguages,
    EditSeerrServer,
    ConnectSeerr,
    ToggleSeerrDriveSelection,
    EditSeerrApiKey,
    ToggleAdvanced,
};

constexpr SettingChangeEffect operator|(SettingChangeEffect left, SettingChangeEffect right) {
    return static_cast<SettingChangeEffect>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

constexpr bool hasSettingEffect(SettingChangeEffect effects, SettingChangeEffect effect) {
    return (static_cast<uint8_t>(effects) & static_cast<uint8_t>(effect)) != 0;
}

using SettingAdjuster = SettingChangeEffect (*)(AppSettings&, int);

template <size_t N>
inline void stepSettingChoice(int& value, const std::array<int, N>& choices, int direction, int fallbackIndex) {
    const auto current = std::find(choices.begin(), choices.end(), value);
    int index = current == choices.end() ? fallbackIndex : static_cast<int>(std::distance(choices.begin(), current));
    index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
    value = choices[static_cast<size_t>(index)];
}

template <bool AppSettings::*Member>
inline SettingChangeEffect toggleSetting(AppSettings& settings, int) {
    settings.*Member = !(settings.*Member);
    return SettingChangeEffect::None;
}

template <int AppSettings::*Member, int Minimum, int Maximum>
inline SettingChangeEffect stepClampedSetting(AppSettings& settings, int direction) {
    settings.*Member = std::clamp(settings.*Member + direction, Minimum, Maximum);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustMaxStreamingBitrate(AppSettings& settings, int direction) {
    static constexpr std::array<int, 6> choices{20, 40, 80, 120, 160, 200};
    stepSettingChoice(settings.maxBitrateMbps, choices, direction, 3);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustSeekBack(AppSettings& settings, int direction) {
    static constexpr std::array<int, 6> choices{5, 10, 15, 20, 30, 60};
    stepSettingChoice(settings.seekBackSeconds, choices, direction, 1);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustSeekForward(AppSettings& settings, int direction) {
    static constexpr std::array<int, 6> choices{5, 10, 15, 20, 30, 60};
    stepSettingChoice(settings.seekForwardSeconds, choices, direction, 1);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustStillWatchingAfter(AppSettings& settings, int direction) {
    static constexpr std::array<int, 5> choices{2, 3, 4, 5, 6};
    stepSettingChoice(settings.stillWatchingAfter, choices, direction, 1);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustRefreshRateSwitching(AppSettings& settings, int) {
    settings.refreshRateSwitching = !settings.refreshRateSwitching;
    return settings.refreshRateSwitching ? SettingChangeEffect::None : SettingChangeEffect::RestoreDisplayMode;
}

inline SettingChangeEffect adjustAudioOutput(AppSettings& settings, int) {
    settings.maxAudioChannels = settings.maxAudioChannels <= 2 ? 8 : 2;
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustAvcMaxLevel(AppSettings& settings, int direction) {
    static constexpr std::array<int, 10> choices{0, 40, 41, 42, 50, 51, 52, 60, 61, 62};
    stepSettingChoice(settings.avcLevelOverride, choices, direction, 0);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustHevcMaxLevel(AppSettings& settings, int direction) {
    static constexpr std::array<int, 9> choices{0, 120, 123, 150, 153, 156, 180, 183, 186};
    stepSettingChoice(settings.hevcLevelOverride, choices, direction, 0);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustOverscanSafeArea(AppSettings& settings, int direction) {
    static constexpr std::array<int, 4> choices{0, 2, 4, 6};
    stepSettingChoice(settings.safeAreaPercent, choices, direction, 0);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustScreensaver(AppSettings& settings, int direction) {
    static constexpr std::array<int, 5> choices{0, 5, 10, 20, 30};
    stepSettingChoice(settings.screensaverMinutes, choices, direction, 0);
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustExternalPlayer(AppSettings&, int) {
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustAutoSubtitleLanguage(AppSettings& settings, int direction) {
    const std::string normalized = normalizeSubtitleLanguage(settings.autoSubtitleLanguage);
    auto current = std::find_if(kSubtitleLanguageOptions.begin(), kSubtitleLanguageOptions.end(), [&](const auto& option) {
        return normalized == option.code;
    });
    int index = current == kSubtitleLanguageOptions.end()
        ? 0
        : static_cast<int>(std::distance(kSubtitleLanguageOptions.begin(), current));
    index = std::clamp(index + direction, 0, static_cast<int>(kSubtitleLanguageOptions.size()) - 1);
    settings.autoSubtitleLanguage = kSubtitleLanguageOptions[static_cast<size_t>(index)].code;
    return SettingChangeEffect::None;
}

inline SettingChangeEffect adjustAutoSubtitleSourceAudio(AppSettings& settings, int direction) {
    std::vector<std::string> choices{"any", "different"};
    for (const auto& option : kSubtitleLanguageOptions) choices.emplace_back(option.code);
    const std::string normalized = settings.autoSubtitleSourceLanguage == "any" || settings.autoSubtitleSourceLanguage == "different"
        ? settings.autoSubtitleSourceLanguage
        : normalizeSubtitleLanguage(settings.autoSubtitleSourceLanguage);
    auto current = std::find(choices.begin(), choices.end(), normalized);
    int index = current == choices.end() ? 0 : static_cast<int>(std::distance(choices.begin(), current));
    index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
    settings.autoSubtitleSourceLanguage = choices[static_cast<size_t>(index)];
    return SettingChangeEffect::None;
}

struct SettingDescriptor {
    SettingId id;
    std::string_view label;
    int commonOrder;
    int advancedOrder;
    bool boolean;
    bool action;
    SettingChangeEffect changeEffects;
    SettingAdjuster adjuster;
    SettingActivation activation = SettingActivation::None;
};

inline constexpr int kNoSettingOrder = -1;
inline constexpr size_t kSettingCount = 34;

constexpr size_t settingIndex(SettingId setting) {
    return static_cast<size_t>(setting);
}

inline constexpr std::array<SettingDescriptor, kSettingCount> kSettingDescriptors{{
    {SettingId::MaxStreamingBitrate, "MAX STREAMING BITRATE", kNoSettingOrder, 0, false, false, SettingChangeEffect::Save, adjustMaxStreamingBitrate},
    {SettingId::PlaybackBuffer, "PLAYBACK BUFFER", kNoSettingOrder, 1, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::playbackBufferPreset, 0, 2>},
    {SettingId::SkipBack, "SKIP BACK", 14, kNoSettingOrder, false, false, SettingChangeEffect::Save, adjustSeekBack},
    {SettingId::SkipAhead, "SKIP AHEAD", 15, kNoSettingOrder, false, false, SettingChangeEffect::Save, adjustSeekForward},
    {SettingId::DefaultVideoZoom, "DEFAULT VIDEO ZOOM", kNoSettingOrder, 2, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::zoomMode, 0, 2>},
    {SettingId::AutoplayNextEpisode, "AUTOPLAY NEXT EPISODE", 12, kNoSettingOrder, true, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::autoplayNext>},
    {SettingId::StillWatchingAfter, "STILL WATCHING AFTER", 13, kNoSettingOrder, false, false, SettingChangeEffect::Save, adjustStillWatchingAfter},
    {SettingId::MatchVideoRefreshRate, "MATCH VIDEO REFRESH RATE", kNoSettingOrder, 3, true, false, SettingChangeEffect::Save, adjustRefreshRateSwitching},
    {SettingId::WatchedIndicators, "WATCHED INDICATORS", 16, kNoSettingOrder, true, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::showWatchedIndicators>},
    {SettingId::Clock, "CLOCK", 17, kNoSettingOrder, true, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::showClock>},
    {SettingId::Backdrops, "BACKDROPS", 1, kNoSettingOrder, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::backdropMode, 0, 2>},
    {SettingId::SubtitleSize, "SUBTITLE SIZE", 2, kNoSettingOrder, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::subtitleSize, 0, 2>},
    {SettingId::SubtitleBackground, "SUBTITLE BACKGROUND", 4, kNoSettingOrder, true, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::subtitleBackground>},
    {SettingId::SubtitlePosition, "SUBTITLE POSITION", 3, kNoSettingOrder, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::subtitlePosition, 0, 2>},
    {SettingId::AudioOutput, "AUDIO OUTPUT", 19, kNoSettingOrder, false, false, SettingChangeEffect::Save, adjustAudioOutput},
    {SettingId::AvcMaxLevel, "AVC / H.264 MAX LEVEL", kNoSettingOrder, 6, false, false, SettingChangeEffect::Save, adjustAvcMaxLevel},
    {SettingId::HevcMaxLevel, "HEVC / H.265 MAX LEVEL", kNoSettingOrder, 7, false, false, SettingChangeEffect::Save, adjustHevcMaxLevel},
    {SettingId::HdrPlayback, "HDR PLAYBACK", kNoSettingOrder, 4, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::hdrOverride, 0, 2>},
    {SettingId::UiTextSize, "UI TEXT SIZE", 0, kNoSettingOrder, false, false, SettingChangeEffect::Save, stepClampedSetting<&AppSettings::uiTextSize, 0, 2>},
    {SettingId::OverscanSafeArea, "OVERSCAN SAFE AREA", kNoSettingOrder, 5, false, false, SettingChangeEffect::Save, adjustOverscanSafeArea},
    {SettingId::Screensaver, "IN-APP SCREENSAVER", 20, kNoSettingOrder, false, false, SettingChangeEffect::Save | SettingChangeEffect::ResetScreensaver, adjustScreensaver},
    {SettingId::ExternalPlayer, "EXTERNAL PLAYER", kNoSettingOrder, 8, false, false, SettingChangeEffect::Save | SettingChangeEffect::CycleExternalPlayer, adjustExternalPlayer},
    {SettingId::Diagnostics, "DIAGNOSTICS", kNoSettingOrder, 9, false, true, SettingChangeEffect::None, nullptr, SettingActivation::OpenDiagnostics},
    {SettingId::SwitchUser, "SWITCH USER", 21, 10, false, true, SettingChangeEffect::None, nullptr, SettingActivation::SwitchUser},
    {SettingId::SubtitleLanguages, "SUBTITLE LANGUAGES", 5, kNoSettingOrder, false, true, SettingChangeEffect::None, nullptr, SettingActivation::OpenSubtitleLanguages},
    {SettingId::TimeFormat, "TIME FORMAT", 18, kNoSettingOrder, false, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::clock24Hour>},
    {SettingId::AutoSubtitles, "AUTO SUBTITLES", 6, kNoSettingOrder, true, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::autoSubtitles>},
    {SettingId::AutoSubtitleLanguage, "AUTO SUBTITLE LANGUAGE", 7, kNoSettingOrder, false, false, SettingChangeEffect::Save, adjustAutoSubtitleLanguage},
    {SettingId::AutoSubtitleSourceAudio, "AUTO SUBTITLE SOURCE AUDIO", 8, kNoSettingOrder, false, false, SettingChangeEffect::Save, adjustAutoSubtitleSourceAudio},
    {SettingId::SeerrServer, "SEERR SERVER", 9, kNoSettingOrder, false, true, SettingChangeEffect::None, nullptr, SettingActivation::EditSeerrServer},
    {SettingId::SeerrConnection, "SEERR CONNECTION", 10, kNoSettingOrder, false, true, SettingChangeEffect::None, nullptr, SettingActivation::ConnectSeerr},
    {SettingId::SeerrDriveSelection, "SEERR DRIVE SELECTION", 11, kNoSettingOrder, true, false, SettingChangeEffect::Save, toggleSetting<&AppSettings::seerrSelectDrive>, SettingActivation::ToggleSeerrDriveSelection},
    {SettingId::SeerrApiKey, "SEERR API KEY (LEGACY)", kNoSettingOrder, 11, false, true, SettingChangeEffect::None, nullptr, SettingActivation::EditSeerrApiKey},
    {SettingId::AdvancedToggle, "ADVANCED SETTINGS", 22, 12, false, true, SettingChangeEffect::None, nullptr, SettingActivation::ToggleAdvanced},
}};

constexpr const SettingDescriptor& settingDescriptor(SettingId setting) {
    return kSettingDescriptors[settingIndex(setting)];
}

constexpr bool isBooleanSetting(SettingId setting) {
    return settingDescriptor(setting).boolean;
}

constexpr bool isActionSetting(SettingId setting) {
    return settingDescriptor(setting).action;
}

constexpr SettingChangeEffect settingChangeEffects(SettingId setting) {
    return settingDescriptor(setting).changeEffects;
}

constexpr SettingActivation settingActivation(SettingId setting) {
    return settingDescriptor(setting).activation;
}

constexpr bool isAdjustableSetting(SettingId setting) {
    return settingDescriptor(setting).adjuster != nullptr;
}

inline SettingChangeEffect adjustSetting(AppSettings& settings, SettingId setting, int direction) {
    const auto& descriptor = settingDescriptor(setting);
    if (descriptor.adjuster == nullptr) return SettingChangeEffect::None;
    direction = direction >= 0 ? 1 : -1;
    return descriptor.changeEffects | descriptor.adjuster(settings, direction);
}

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

inline std::string clockFormatName(bool clock24Hour) {
    return clock24Hour ? "24 HOUR" : "12 HOUR (AM/PM)";
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

inline std::string subtitleLanguageLabel(std::string code) {
    code = normalizeSubtitleLanguage(std::move(code));
    const auto match = std::find_if(kSubtitleLanguageOptions.begin(), kSubtitleLanguageOptions.end(), [&](const auto& option) {
        return code == option.code;
    });
    return match == kSubtitleLanguageOptions.end() ? code : match->label;
}

inline std::string autoSubtitleSourceName(const AppSettings& settings) {
    if (settings.autoSubtitleSourceLanguage.empty() || settings.autoSubtitleSourceLanguage == "any") return "ANY AUDIO";
    if (settings.autoSubtitleSourceLanguage == "different") {
        return "AUDIO NOT " + subtitleLanguageLabel(settings.autoSubtitleLanguage);
    }
    return subtitleLanguageLabel(settings.autoSubtitleSourceLanguage) + " AUDIO";
}

inline std::string subtitleLanguageSummary(const AppSettings& settings) {
    if (settings.subtitleLanguages.empty()) return "ALL LANGUAGES";
    if (settings.subtitleLanguages.size() == 1) {
        const auto match = std::find_if(kSubtitleLanguageOptions.begin(), kSubtitleLanguageOptions.end(), [&](const auto& option) {
            return settings.subtitleLanguages.front() == option.code;
        });
        return match == kSubtitleLanguageOptions.end() ? settings.subtitleLanguages.front() : match->label;
    }
    return std::to_string(settings.subtitleLanguages.size()) + " SELECTED";
}

inline std::string_view settingLabel(SettingId setting, bool advanced) {
    if (setting == SettingId::AdvancedToggle && advanced) return "BASIC SETTINGS";
    return settingDescriptor(setting).label;
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

template <size_t N>
consteval std::array<SettingId, N> makeSettingOrder(bool advanced) {
    std::array<SettingId, N> result{};
    size_t count = 0;
    for (const auto& descriptor : kSettingDescriptors) {
        const int order = advanced ? descriptor.advancedOrder : descriptor.commonOrder;
        if (order == kNoSettingOrder) continue;
        result[static_cast<size_t>(order)] = descriptor.id;
        ++count;
    }
    if (count != N) throw "setting order count mismatch";
    return result;
}

inline constexpr auto kCommonSettings = makeSettingOrder<23>(false);
inline constexpr auto kAdvancedSettings = makeSettingOrder<13>(true);

inline std::vector<SettingId> matchingSettings(const std::string& query, bool advanced) {
    std::vector<SettingId> matches;
    matches.reserve(advanced ? kAdvancedSettings.size() : kCommonSettings.size());
    const auto appendMatches = [&](const auto& candidates) {
        for (const SettingId setting : candidates) {
            if (query.empty() || settingLabelContains(settingLabel(setting, advanced), query)) {
                matches.push_back(setting);
            }
        }
    };
    if (advanced) appendMatches(kAdvancedSettings);
    else appendMatches(kCommonSettings);
    return matches;
}
