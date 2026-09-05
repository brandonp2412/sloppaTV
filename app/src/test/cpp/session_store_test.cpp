#include "session_store.hpp"

#include <cassert>
#include <filesystem>
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

    const std::string generated = generateDeviceId();
    assert(generated.rfind("sloppatv-", 0) == 0);

    std::filesystem::remove_all(directory, ec);
    return 0;
}
