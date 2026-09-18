#pragma once

#include "jellyfin_types.hpp"

#include <cstdint>
#include <string>
#include <utility>

enum class PlaybackReportKind {
    Start,
    Progress,
    PausedProgress,
    Stop,
};

struct PlaybackReportCompletion {
    PlaybackReportKind kind = PlaybackReportKind::Progress;
    std::string server;
    std::string userId;
    std::string itemId;
    ApiResult result;
};

template <typename Client, typename TaskRunner, typename CompletionSink> class PlaybackTelemetryExecutor {
public:
    PlaybackTelemetryExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    void reportStart(JellyfinSession session, JellyfinItem item, PlaybackTarget target, int64_t ticks) {
        tasks_.submit([this, session = std::move(session), item = std::move(item), target = std::move(target), ticks] {
            auto result = client_.reportPlaybackStart(session, item, target, ticks);
            pushCompletion(PlaybackReportKind::Start, session, item, std::move(result));
        });
    }

    void reportProgress(JellyfinSession session, JellyfinItem item, PlaybackTarget target, int64_t ticks, bool paused) {
        tasks_.submit(
            [this, session = std::move(session), item = std::move(item), target = std::move(target), ticks, paused] {
                auto result = client_.reportPlaybackProgress(session, item, target, ticks, paused);
                pushCompletion(paused ? PlaybackReportKind::PausedProgress : PlaybackReportKind::Progress, session,
                               item, std::move(result));
            });
    }

    void reportStop(JellyfinSession session, JellyfinItem item, PlaybackTarget target, int64_t ticks) {
        tasks_.submit([this, session = std::move(session), item = std::move(item), target = std::move(target), ticks] {
            auto result = client_.reportPlaybackStopped(session, item, target, ticks);
            pushCompletion(PlaybackReportKind::Stop, session, item, std::move(result));
        });
    }

private:
    void pushCompletion(PlaybackReportKind kind, const JellyfinSession& session, const JellyfinItem& item,
                        ApiResult result) {
        completions_.push(PlaybackReportCompletion{
            .kind = kind,
            .server = session.server,
            .userId = session.userId,
            .itemId = item.id,
            .result = std::move(result),
        });
    }

    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
