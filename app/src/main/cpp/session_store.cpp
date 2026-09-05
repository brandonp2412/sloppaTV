#include "session_store.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <sstream>
#include <utility>

using nlohmann::json;

namespace {
StoredSession readSession(const json& data) {
    StoredSession session;
    session.server = data.value("server", std::string{});
    session.username = data.value("username", std::string{});
    session.userId = data.value("userId", std::string{});
    session.token = data.value("token", std::string{});
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
    settings.maxBitrateMbps = std::clamp(saved.value("maxBitrateMbps", settings.maxBitrateMbps), 20, 200);
    settings.playbackBufferPreset = std::clamp(saved.value("playbackBufferPreset", settings.playbackBufferPreset), 0, 2);
    settings.seekBackSeconds = std::clamp(saved.value("seekBackSeconds", settings.seekBackSeconds), 5, 60);
    settings.seekForwardSeconds = std::clamp(saved.value("seekForwardSeconds", settings.seekForwardSeconds), 5, 60);
    settings.zoomMode = std::clamp(saved.value("zoomMode", settings.zoomMode), 0, 2);
    settings.autoplayNext = saved.value("autoplayNext", settings.autoplayNext);
    settings.stillWatchingAfter = std::clamp(saved.value("stillWatchingAfter", settings.stillWatchingAfter), 2, 6);
    settings.refreshRateSwitching = saved.value("refreshRateSwitching", settings.refreshRateSwitching);
    settings.showWatchedIndicators = saved.value("showWatchedIndicators", settings.showWatchedIndicators);
    settings.showClock = saved.value("showClock", settings.showClock);
    if (saved.contains("backdropMode")) {
        settings.backdropMode = std::clamp(saved.value("backdropMode", settings.backdropMode), 0, 2);
    } else {
        settings.backdropMode = saved.value("showBackdrops", true) ? 1 : 0;
    }
    settings.subtitleSize = std::clamp(saved.value("subtitleSize", settings.subtitleSize), 0, 2);
    settings.subtitleBackground = saved.value("subtitleBackground", settings.subtitleBackground);
    settings.subtitlePosition = std::clamp(saved.value("subtitlePosition", settings.subtitlePosition), 0, 2);
    const int savedMaxAudioChannels = saved.value("maxAudioChannels", settings.maxAudioChannels);
    settings.maxAudioChannels = savedMaxAudioChannels <= 2 ? 2 : 8;
    settings.avcLevelOverride = saved.value("avcLevelOverride", settings.avcLevelOverride);
    settings.hevcLevelOverride = saved.value("hevcLevelOverride", settings.hevcLevelOverride);
    settings.hdrOverride = std::clamp(saved.value("hdrOverride", settings.hdrOverride), 0, 2);
    settings.uiTextSize = std::clamp(saved.value("uiTextSize", settings.uiTextSize), 0, 2);
    const int savedSafeArea = saved.value("safeAreaPercent", settings.safeAreaPercent);
    settings.safeAreaPercent = savedSafeArea <= 0 ? 0 : (savedSafeArea <= 2 ? 2 : (savedSafeArea <= 4 ? 4 : 6));
    settings.screensaverMinutes = normalizedScreensaverMinutes(saved.value("screensaverMinutes", settings.screensaverMinutes));
    settings.externalPlayerComponent = saved.value("externalPlayerComponent", std::string{});
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
        {"backdropMode", settings.backdropMode},
        {"subtitleSize", settings.subtitleSize},
        {"subtitleBackground", settings.subtitleBackground},
        {"subtitlePosition", settings.subtitlePosition},
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
        state.deviceId = data.value("deviceId", state.deviceId);
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

        std::ofstream output(dataPath + "/session.json", std::ios::trunc);
        if (!output) {
            warning = "unable to open session.json for writing";
            return false;
        }
        output << data.dump(2);
        return static_cast<bool>(output);
    } catch (const std::exception& e) {
        warning = e.what();
        return false;
    }
}
