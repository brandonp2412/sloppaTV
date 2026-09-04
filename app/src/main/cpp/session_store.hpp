#pragma once

#include "app_settings.hpp"

#include <string>
#include <unordered_set>
#include <vector>

struct StoredSession {
    std::string server;
    std::string username;
    std::string userId;
    std::string token;

    [[nodiscard]] bool valid() const {
        return !server.empty() && !userId.empty() && !token.empty();
    }
};

struct StoredSessionState {
    std::string deviceId;
    StoredSession currentSession;
    std::vector<StoredSession> savedSessions;
    std::unordered_set<std::string> hiddenHomeItems;
    AppSettings settings;
};

std::string generateDeviceId();
StoredSessionState loadSessionState(const std::string& dataPath, std::string defaultDeviceId, std::string& warning);
bool saveSessionState(const std::string& dataPath, const StoredSessionState& state, std::string& warning);
