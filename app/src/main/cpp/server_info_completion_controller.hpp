#pragma once

#include "server_info_executor.hpp"

#include <chrono>
#include <optional>
#include <string>

struct ServerInfoNotice {
    std::string message;
    std::chrono::seconds duration{};
    bool persistent = false;
};

struct ServerInfoCompletionEffects {
    bool finishLoading = false;
    bool finishNoticeLoading = false;
    std::optional<std::string> error;
    std::optional<ServerInfoNotice> notice;
};

class ServerInfoCompletionController {
public:
    [[nodiscard]] static ServerInfoCompletionEffects applyDiagnostics(DiagnosticsCompletion& completion,
                                                                      bool activeGeneration, bool activeScreen,
                                                                      JellyfinServerInfo& serverInfo);
    [[nodiscard]] static ServerInfoCompletionEffects applyNotice(ServerInfoNoticeCompletion& completion,
                                                                 bool activeSession, JellyfinServerInfo& serverInfo);
};
