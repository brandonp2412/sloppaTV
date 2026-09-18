#pragma once

#include "jellyfin_types.hpp"

#include <cstdint>
#include <string>
#include <utility>

struct DiagnosticsCompletion {
    uint64_t generation = 0;
    ApiValueResult<JellyfinServerInfo> result;
};

struct ServerInfoNoticeCompletion {
    std::string server;
    std::string userId;
    ApiValueResult<JellyfinServerInfo> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink>
class ServerInfoExecutor {
public:
    ServerInfoExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool loadNotice(JellyfinSession session) {
        return tasks_.submit([this, session = std::move(session)]() mutable {
            auto result = client_.getServerInfo(session);
            completions_.push(ServerInfoNoticeCompletion{
                .server = std::move(session.server),
                .userId = std::move(session.userId),
                .result = std::move(result),
            });
        });
    }

    bool loadDiagnostics(JellyfinSession session, uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), generation]() mutable {
            auto result = client_.getServerInfo(session);
            completions_.push(DiagnosticsCompletion{
                .generation = generation,
                .result = std::move(result),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
