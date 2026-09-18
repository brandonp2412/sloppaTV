#include "server_info_completion_controller.hpp"

#include "version_policy.hpp"

#include <utility>

using namespace std::chrono_literals;

namespace {
std::optional<std::string> diagnosticsCompatibilityError(const JellyfinServerInfo& serverInfo) {
    const auto compatibility = jellyfinServerCompatibility(serverInfo.version);
    if (compatibility == ServerCompatibility::TooOld)
        return "JELLYFIN " + serverInfo.version + " IS BELOW THE TESTED 10.10+ BASELINE";
    if (compatibility == ServerCompatibility::Unknown && !serverInfo.version.empty())
        return "UNRECOGNIZED JELLYFIN VERSION: " + serverInfo.version;
    return std::nullopt;
}

std::optional<ServerInfoNotice> compatibilityNotice(const JellyfinServerInfo& serverInfo) {
    const auto compatibility = jellyfinServerCompatibility(serverInfo.version);
    if (compatibility == ServerCompatibility::TooOld) {
        return ServerInfoNotice{
            .message =
                "JELLYFIN " + serverInfo.version + " IS BELOW THE TESTED 10.10+ BASELINE - SERVER UPGRADE RECOMMENDED",
            .duration = 6s,
            .persistent = true,
        };
    }
    if (compatibility == ServerCompatibility::Unknown && !serverInfo.version.empty()) {
        return ServerInfoNotice{
            .message = "UNRECOGNIZED JELLYFIN VERSION " + serverInfo.version + " - PLAYBACK COMPATIBILITY MAY VARY",
            .duration = 10s,
        };
    }
    return std::nullopt;
}
} // namespace

ServerInfoCompletionEffects ServerInfoCompletionController::applyDiagnostics(DiagnosticsCompletion& completion,
                                                                             bool activeGeneration, bool activeScreen,
                                                                             JellyfinServerInfo& serverInfo) {
    if (!activeGeneration) return {};

    ServerInfoCompletionEffects effects;
    effects.finishLoading = true;
    if (!activeScreen) return effects;
    if (!completion.result.ok) {
        effects.error = "SERVER INFO: " + completion.result.error;
        return effects;
    }

    serverInfo = std::move(completion.result.value);
    effects.error = diagnosticsCompatibilityError(serverInfo);
    return effects;
}

ServerInfoCompletionEffects ServerInfoCompletionController::applyNotice(ServerInfoNoticeCompletion& completion,
                                                                        bool activeSession,
                                                                        JellyfinServerInfo& serverInfo) {
    ServerInfoCompletionEffects effects;
    effects.finishNoticeLoading = true;
    if (!activeSession || !completion.result.ok) return effects;

    serverInfo = std::move(completion.result.value);
    effects.notice = compatibilityNotice(serverInfo);
    return effects;
}
