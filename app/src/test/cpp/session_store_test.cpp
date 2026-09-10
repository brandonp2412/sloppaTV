#include "session_store.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "sloppatv-session-store-test";
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    std::filesystem::create_directories(directory, ec);
    assert(!ec);

    StoredSessionState state;
    state.deviceId = "device-test";
    state.currentSession = {
        .server = "https://jellyfin.example",
        .username = "viewer",
        .userId = "user-1",
        .token = "token-1",
    };
    state.savedSessions.push_back(state.currentSession);
    state.hiddenHomeItems.insert("user-1:item-1");
    state.settings.maxBitrateMbps = 80;
    state.settings.subtitleSize = 2;
    state.settings.subtitleBackground = false;
    state.settings.subtitleLanguages = {"eng", "jpn"};
    state.settings.autoSubtitles = true;
    state.settings.autoSubtitleLanguage = "eng";
    state.settings.autoSubtitleSourceLanguage = "different";
    state.settings.clock24Hour = true;
    state.settings.safeAreaPercent = 4;
    state.settings.externalPlayerComponent = "org.example/.Player";

    std::string warning;
    assert(saveSessionState(directory.string(), state, warning));
    assert(warning.empty());

    StoredSessionState loaded = loadSessionState(directory.string(), "fallback", warning);
    assert(warning.empty());
    assert(loaded.deviceId == state.deviceId);
    assert(loaded.currentSession.valid());
    assert(loaded.currentSession.server == state.currentSession.server);
    assert(loaded.savedSessions.size() == 1);
    assert(loaded.savedSessions.front().userId == "user-1");
    assert(loaded.hiddenHomeItems.contains("user-1:item-1"));
    assert(loaded.settings.maxBitrateMbps == 80);
    assert(loaded.settings.subtitleSize == 2);
    assert(!loaded.settings.subtitleBackground);
    assert(loaded.settings.subtitleLanguages.size() == 2);
    assert(loaded.settings.subtitleLanguages[0] == "eng");
    assert(loaded.settings.subtitleLanguages[1] == "jpn");
    assert(loaded.settings.autoSubtitles);
    assert(loaded.settings.autoSubtitleLanguage == "eng");
    assert(loaded.settings.autoSubtitleSourceLanguage == "different");
    assert(loaded.settings.clock24Hour);
    assert(loaded.settings.safeAreaPercent == 4);
    assert(loaded.settings.externalPlayerComponent == "org.example/.Player");

    const std::filesystem::path temporaryPath = directory / "session.json.tmp";
    std::filesystem::create_directory(temporaryPath, ec);
    assert(!ec);
    StoredSessionState replacement = state;
    replacement.deviceId = "replacement-device";
    assert(!saveSessionState(directory.string(), replacement, warning));
    assert(!warning.empty());

    loaded = loadSessionState(directory.string(), "fallback", warning);
    assert(warning.empty());
    assert(loaded.deviceId == "device-test");

    {
        std::ofstream output(directory / "session.json", std::ios::trunc);
        assert(output);
        output << R"JSON({
  "server": "https://current.example",
  "username": "current",
  "userId": "current-user",
  "token": "current-token",
  "deviceId": "device-resilient",
  "savedSessions": [
    {"server": 123, "username": "broken", "userId": "broken-user", "token": "broken-token"},
    {"server": "https://saved.example", "username": "saved", "userId": "saved-user", "token": "saved-token"}
  ],
  "settings": {
    "maxBitrateMbps": 120,
    "playbackBufferPreset": "bad",
    "clock24Hour": "yes",
    "externalPlayerComponent": 42,
    "subtitleStyleDefaultsVersion": 1,
    "subtitleSize": 2
  }
})JSON";
    }
    loaded = loadSessionState(directory.string(), "fallback", warning);
    assert(warning.empty());
    assert(loaded.deviceId == "device-resilient");
    assert(loaded.currentSession.valid());
    assert(loaded.currentSession.userId == "current-user");
    assert(loaded.savedSessions.size() == 1);
    assert(loaded.savedSessions.front().userId == "saved-user");
    assert(loaded.settings.maxBitrateMbps == 120);
    assert(loaded.settings.playbackBufferPreset == AppSettings{}.playbackBufferPreset);
    assert(loaded.settings.clock24Hour == AppSettings{}.clock24Hour);
    assert(loaded.settings.externalPlayerComponent.empty());
    assert(loaded.settings.subtitleSize == 2);

    {
        std::ofstream output(directory / "session.json", std::ios::trunc);
        assert(output);
        output << R"JSON({
  "server": "https://current.example",
  "username": "current",
  "userId": "current-user",
  "token": "current-token",
  "deviceId": ""
})JSON";
    }
    loaded = loadSessionState(directory.string(), "fallback-device", warning);
    assert(warning.empty());
    assert(loaded.deviceId == "fallback-device");
    assert(loaded.currentSession.valid());

    const std::string generated = generateDeviceId();
    assert(generated.rfind("sloppatv-", 0) == 0);

    std::filesystem::remove_all(directory, ec);
    return 0;
}
