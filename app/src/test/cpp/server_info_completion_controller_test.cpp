#include "server_info_completion_controller.hpp"

#include <cassert>

namespace {
DiagnosticsCompletion diagnostics(std::string version, bool ok = true) {
    DiagnosticsCompletion completion;
    completion.generation = 3;
    completion.result.ok = ok;
    completion.result.value.version = std::move(version);
    return completion;
}

ServerInfoNoticeCompletion notice(std::string version, bool ok = true) {
    ServerInfoNoticeCompletion completion;
    completion.server = "https://jellyfin.example";
    completion.userId = "user";
    completion.result.ok = ok;
    completion.result.value.version = std::move(version);
    return completion;
}
} // namespace

int main() {
    JellyfinServerInfo serverInfo;

    {
        auto completion = diagnostics("10.11.1");
        const auto effects = ServerInfoCompletionController::applyDiagnostics(completion, false, true, serverInfo);
        assert(!effects.finishLoading);
        assert(serverInfo.version.empty());
    }

    {
        auto completion = diagnostics("10.11.1");
        const auto effects = ServerInfoCompletionController::applyDiagnostics(completion, true, false, serverInfo);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(serverInfo.version.empty());
    }

    {
        auto completion = diagnostics("", false);
        completion.result.error = "offline";
        const auto effects = ServerInfoCompletionController::applyDiagnostics(completion, true, true, serverInfo);
        assert(effects.finishLoading);
        assert(effects.error == "SERVER INFO: offline");
    }

    {
        auto completion = diagnostics("10.9.11");
        const auto effects = ServerInfoCompletionController::applyDiagnostics(completion, true, true, serverInfo);
        assert(effects.finishLoading);
        assert(serverInfo.version == "10.9.11");
        assert(effects.error == "JELLYFIN 10.9.11 IS BELOW THE TESTED 10.10+ BASELINE");
    }

    {
        auto completion = diagnostics("dev");
        const auto effects = ServerInfoCompletionController::applyDiagnostics(completion, true, true, serverInfo);
        assert(effects.error == "UNRECOGNIZED JELLYFIN VERSION: dev");
    }

    {
        auto completion = diagnostics("10.10.7");
        const auto effects = ServerInfoCompletionController::applyDiagnostics(completion, true, true, serverInfo);
        assert(!effects.error);
        assert(serverInfo.version == "10.10.7");
    }

    {
        auto completion = notice("10.9.1");
        const auto effects = ServerInfoCompletionController::applyNotice(completion, false, serverInfo);
        assert(effects.finishNoticeLoading);
        assert(!effects.notice);
        assert(serverInfo.version == "10.10.7");
    }

    {
        auto completion = notice("10.9.1");
        const auto effects = ServerInfoCompletionController::applyNotice(completion, true, serverInfo);
        assert(effects.finishNoticeLoading);
        assert(serverInfo.version == "10.9.1");
        assert(effects.notice);
        assert(effects.notice->message ==
               "JELLYFIN 10.9.1 IS BELOW THE TESTED 10.10+ BASELINE - SERVER UPGRADE RECOMMENDED");
        assert(effects.notice->duration.count() == 6);
        assert(effects.notice->persistent);
    }

    {
        auto completion = notice("preview");
        const auto effects = ServerInfoCompletionController::applyNotice(completion, true, serverInfo);
        assert(effects.notice);
        assert(effects.notice->message == "UNRECOGNIZED JELLYFIN VERSION preview - PLAYBACK COMPATIBILITY MAY VARY");
        assert(effects.notice->duration.count() == 10);
        assert(!effects.notice->persistent);
    }

    {
        auto completion = notice("10.11.0");
        const auto effects = ServerInfoCompletionController::applyNotice(completion, true, serverInfo);
        assert(!effects.notice);
        assert(serverInfo.version == "10.11.0");
    }

    return 0;
}
