#pragma once

#include "app_screen.hpp"
#include "navigation_stack.hpp"
#include "request_epoch.hpp"
#include "server_info_completion_controller.hpp"
#include "server_info_executor.hpp"

#include <optional>
#include <string>
#include <utility>

template <typename ServerInfoAsync> class ServerInfoScreenCoordinator {
public:
    ServerInfoScreenCoordinator(JellyfinSession& session, JellyfinServerInfo& serverInfo, bool& noticeLoading,
                                bool& loading, std::string& error, RequestEpoch& contentEpoch,
                                ServerInfoAsync& serverInfoAsync, NavigationStack<Screen>& navigation, Screen& screen)
        : session_(session), serverInfo_(serverInfo), noticeLoading_(noticeLoading), loading_(loading), error_(error),
          contentEpoch_(contentEpoch), serverInfoAsync_(serverInfoAsync), navigation_(navigation), screen_(screen) {}

    void requestNotice() {
        if (!session_.valid() || noticeLoading_ || !serverInfo_.version.empty()) return;
        const JellyfinSession session = session_;
        noticeLoading_ = true;
        serverInfoAsync_.loadNotice(session);
    }

    void openDiagnostics() {
        if (!session_.valid()) return;
        navigation_.push(Screen::Diagnostics);
        screen_ = navigation_.current();
        loading_ = true;
        error_.clear();
        serverInfo_ = {};
        const JellyfinSession session = session_;
        serverInfoAsync_.loadDiagnostics(session, contentEpoch_.begin());
    }

    [[nodiscard]] std::optional<ServerInfoNotice> complete(DiagnosticsCompletion& completion) {
        return apply(ServerInfoCompletionController::applyDiagnostics(
            completion, contentEpoch_.active(completion.generation), screen_ == Screen::Diagnostics, serverInfo_));
    }

    [[nodiscard]] std::optional<ServerInfoNotice> complete(ServerInfoNoticeCompletion& completion) {
        const bool activeSession =
            session_.valid() && session_.server == completion.server && session_.userId == completion.userId;
        return apply(ServerInfoCompletionController::applyNotice(completion, activeSession, serverInfo_));
    }

private:
    [[nodiscard]] std::optional<ServerInfoNotice> apply(ServerInfoCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.finishNoticeLoading) noticeLoading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        return std::move(effects.notice);
    }

    JellyfinSession& session_;
    JellyfinServerInfo& serverInfo_;
    bool& noticeLoading_;
    bool& loading_;
    std::string& error_;
    RequestEpoch& contentEpoch_;
    ServerInfoAsync& serverInfoAsync_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
};
