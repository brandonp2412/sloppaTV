#include "session_store.hpp"
#include "subtitle_policy.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <utility>

using nlohmann::json;

namespace {
template <typename T>
T valueOr(const json& data, const char* key, T fallback) {
    const auto value = data.find(key);
    if (value == data.end() || value->is_null()) return fallback;
    try {
        return value->get<T>();
    } catch (const json::exception&) {
        return fallback;
    }
}

StoredSession readSession(const json& data) {
    StoredSession session;
    session.server = valueOr<std::string>(data, "server", {});
    session.username = valueOr<std::string>(data, "username", {});
    session.userId = valueOr<std::string>(data, "userId", {});
    session.token = valueOr<std::string>(data, "token", {});
    return session;
}

json writeSession(const StoredSession& session) {
    return {
        {"server", session.server},
        {"username", session.username},
        {"userId", session.userId},
        {"token", session.token},
    };
}

void readSettings(const json& saved, AppSettings& settings) {
    settings.maxBitrateMbps = std::clamp(valueOr(saved, "maxBitrateMbps", settings.maxBitrateMbps), 20, 200);
    settings.playbackBufferPreset = std::clamp(valueOr(saved, "playbackBufferPreset", settings.playbackBufferPreset), 0, 2);
    settings.seekBackSeconds = std::clamp(valueOr(saved, "seekBackSeconds", settings.seekBackSeconds), 5, 60);
    settings.seekForwardSeconds = std::clamp(valueOr(saved, "seekForwardSeconds", settings.seekForwardSeconds), 5, 60);
    settings.zoomMode = std::clamp(valueOr(saved, "zoomMode", settings.zoomMode), 0, 2);
    settings.autoplayNext = valueOr(saved, "autoplayNext", settings.autoplayNext);
    settings.stillWatchingAfter = std::clamp(valueOr(saved, "stillWatchingAfter", settings.stillWatchingAfter), 2, 6);
    settings.refreshRateSwitching = valueOr(saved, "refreshRateSwitching", settings.refreshRateSwitching);
    settings.showWatchedIndicators = valueOr(saved, "showWatchedIndicators", settings.showWatchedIndicators);
    settings.showClock = valueOr(saved, "showClock", settings.showClock);
    settings.clock24Hour = valueOr(saved, "clock24Hour", settings.clock24Hour);
    if (saved.contains("backdropMode")) {
        settings.backdropMode = std::clamp(valueOr(saved, "backdropMode", settings.backdropMode), 0, 2);
    } else {
        settings.backdropMode = valueOr(saved, "showBackdrops", true) ? 1 : 0;
    }
    const int subtitleStyleDefaultsVersion = valueOr(saved, "subtitleStyleDefaultsVersion", 0);
    settings.subtitleSize = subtitleStyleDefaultsVersion < 1
        ? 1
        : std::clamp(valueOr(saved, "subtitleSize", settings.subtitleSize), 0, 2);
    settings.subtitleBackground = subtitleStyleDefaultsVersion < 1
        ? false
        : valueOr(saved, "subtitleBackground", settings.subtitleBackground);
    settings.subtitlePosition = std::clamp(valueOr(saved, "subtitlePosition", settings.subtitlePosition), 0, 2);
    settings.autoSubtitles = valueOr(saved, "autoSubtitles", settings.autoSubtitles);
    settings.autoSubtitleLanguage = normalizeSubtitleLanguage(valueOr(saved, "autoSubtitleLanguage", settings.autoSubtitleLanguage));
    settings.autoSubtitleSourceLanguage = valueOr(saved, "autoSubtitleSourceLanguage", settings.autoSubtitleSourceLanguage);
    if (settings.autoSubtitleSourceLanguage != "any" && settings.autoSubtitleSourceLanguage != "different") {
        settings.autoSubtitleSourceLanguage = normalizeSubtitleLanguage(settings.autoSubtitleSourceLanguage);
    }
    settings.subtitleLanguages.clear();
    if (saved.contains("subtitleLanguages") && saved["subtitleLanguages"].is_array()) {
        for (const auto& language : saved["subtitleLanguages"]) {
            if (!language.is_string()) continue;
            const std::string normalized = normalizeSubtitleLanguage(language.get<std::string>());
            if (!normalized.empty() && std::find(settings.subtitleLanguages.begin(), settings.subtitleLanguages.end(), normalized) == settings.subtitleLanguages.end()) {
                settings.subtitleLanguages.push_back(normalized);
            }
        }
    }
    const int savedMaxAudioChannels = valueOr(saved, "maxAudioChannels", settings.maxAudioChannels);
    settings.maxAudioChannels = savedMaxAudioChannels <= 2 ? 2 : 8;
    settings.avcLevelOverride = valueOr(saved, "avcLevelOverride", settings.avcLevelOverride);
    settings.hevcLevelOverride = valueOr(saved, "hevcLevelOverride", settings.hevcLevelOverride);
    settings.hdrOverride = std::clamp(valueOr(saved, "hdrOverride", settings.hdrOverride), 0, 2);
    settings.uiTextSize = std::clamp(valueOr(saved, "uiTextSize", settings.uiTextSize), 0, 2);
    const int savedSafeArea = valueOr(saved, "safeAreaPercent", settings.safeAreaPercent);
    settings.safeAreaPercent = savedSafeArea <= 0 ? 0 : (savedSafeArea <= 2 ? 2 : (savedSafeArea <= 4 ? 4 : 6));
    settings.screensaverMinutes = normalizedScreensaverMinutes(valueOr(saved, "screensaverMinutes", settings.screensaverMinutes));
    settings.externalPlayerComponent = valueOr<std::string>(saved, "externalPlayerComponent", {});
}

json writeSettings(const AppSettings& settings) {
    return {
        {"maxBitrateMbps", settings.maxBitrateMbps},
        {"playbackBufferPreset", settings.playbackBufferPreset},
        {"seekBackSeconds", settings.seekBackSeconds},
        {"seekForwardSeconds", settings.seekForwardSeconds},
        {"zoomMode", settings.zoomMode},
        {"autoplayNext", settings.autoplayNext},
        {"stillWatchingAfter", settings.stillWatchingAfter},
        {"refreshRateSwitching", settings.refreshRateSwitching},
        {"showWatchedIndicators", settings.showWatchedIndicators},
        {"showClock", settings.showClock},
        {"clock24Hour", settings.clock24Hour},
        {"backdropMode", settings.backdropMode},
        {"subtitleStyleDefaultsVersion", 1},
        {"subtitleSize", settings.subtitleSize},
        {"subtitleBackground", settings.subtitleBackground},
        {"subtitlePosition", settings.subtitlePosition},
        {"subtitleLanguages", settings.subtitleLanguages},
        {"autoSubtitles", settings.autoSubtitles},
        {"autoSubtitleLanguage", settings.autoSubtitleLanguage},
        {"autoSubtitleSourceLanguage", settings.autoSubtitleSourceLanguage},
        {"maxAudioChannels", settings.maxAudioChannels},
        {"avcLevelOverride", settings.avcLevelOverride},
        {"hevcLevelOverride", settings.hevcLevelOverride},
        {"hdrOverride", settings.hdrOverride},
        {"uiTextSize", settings.uiTextSize},
        {"safeAreaPercent", settings.safeAreaPercent},
        {"screensaverMinutes", settings.screensaverMinutes},
        {"externalPlayerComponent", settings.externalPlayerComponent},
    };
}
}

std::string generateDeviceId() {
    std::random_device rd;
    std::mt19937_64 generator(
        (static_cast<uint64_t>(rd()) << 32u)
        ^ static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    std::ostringstream out;
    out << "sloppatv-" << std::hex << generator();
    return out.str();
}

StoredSessionState loadSessionState(const std::string& dataPath, std::string defaultDeviceId, std::string& warning) {
    StoredSessionState state;
    state.deviceId = std::move(defaultDeviceId);
    warning.clear();
    if (dataPath.empty()) return state;

    std::ifstream input(dataPath + "/session.json");
    if (!input) return state;

    try {
        json data;
        input >> data;
        state.deviceId = valueOr(data, "deviceId", state.deviceId);
        if (data.contains("hiddenHomeItems") && data["hiddenHomeItems"].is_array()) {
            for (const auto& hidden : data["hiddenHomeItems"]) {
                if (hidden.is_string()) state.hiddenHomeItems.insert(hidden.get<std::string>());
            }
        }
        if (data.contains("savedSessions") && data["savedSessions"].is_array()) {
            for (const auto& saved : data["savedSessions"]) {
                if (!saved.is_object()) continue;
                StoredSession candidate = readSession(saved);
                if (candidate.valid()) state.savedSessions.push_back(std::move(candidate));
            }
        }
        state.currentSession = readSession(data);
        if (data.contains("settings") && data["settings"].is_object()) {
            readSettings(data["settings"], state.settings);
        }
    } catch (const std::exception& e) {
        warning = e.what();
    }
    return state;
}

bool saveSessionState(const std::string& dataPath, const StoredSessionState& state, std::string& warning) {
    warning.clear();
    if (dataPath.empty()) return true;
    try {
        json savedSessions = json::array();
        for (const auto& candidate : state.savedSessions) {
            if (candidate.valid()) savedSessions.push_back(writeSession(candidate));
        }
        json hiddenHome = json::array();
        for (const auto& key : state.hiddenHomeItems) hiddenHome.push_back(key);

        json data = writeSession(state.currentSession);
        data["deviceId"] = state.deviceId;
        data["hiddenHomeItems"] = std::move(hiddenHome);
        data["savedSessions"] = std::move(savedSessions);
        data["settings"] = writeSettings(state.settings);

        const std::string sessionPath = dataPath + "/session.json";
        const std::string temporaryPath = sessionPath + ".tmp";
        {
            std::ofstream output(temporaryPath, std::ios::trunc);
            if (!output) {
                warning = "unable to open temporary session file for writing";
                return false;
            }
            output << data.dump(2);
            output.flush();
            if (!output) {
                warning = "unable to write temporary session file";
                output.close();
                std::remove(temporaryPath.c_str());
                return false;
            }
        }
        if (std::rename(temporaryPath.c_str(), sessionPath.c_str()) != 0) {
            warning = "unable to replace session.json";
            std::remove(temporaryPath.c_str());
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        warning = e.what();
        return false;
    }
}
