#pragma once

#include "jellyfin_types.hpp"
#include "media_player_policy.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

struct PlaybackStreamResolutionOptions {
    int maxStreamingBitrate = 0;
    int maxAudioChannels = 0;
    PlaybackOverrides overrides;
    int audioStreamIndex = -1;
    int subtitleStreamIndex = kSubtitleServerDefaultIndex;
};

struct StreamRestartCompletion {
    uint64_t generation = 0;
    int audioStreamIndex = -1;
    bool wasPaused = false;
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> result;
};

struct FallbackPlaybackCompletion {
    uint64_t generation = 0;
    int audioStreamIndex = -1;
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink, typename Epoch>
class PlaybackStreamExecutor {
public:
    using ReportFailureSink = std::function<void(const char*, const std::string&, const ApiResult&)>;

    PlaybackStreamExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions, const Epoch& epoch,
                           ReportFailureSink reportFailure)
        : client_(client), tasks_(tasks), completions_(completions), epoch_(epoch),
          reportFailure_(std::move(reportFailure)) {}

    bool restart(JellyfinSession session, JellyfinItem item, PlaybackTarget previousTarget, bool reportPrevious,
                 PlaybackStreamResolutionOptions options, bool wasPaused, uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item),
                              previousTarget = std::move(previousTarget), reportPrevious, options = std::move(options),
                              wasPaused, generation]() mutable {
            reportStopIfNeeded("stop", session, item, previousTarget, item.positionTicks, reportPrevious);
            auto target = resolve(session, item, options);
            if (!epoch_.active(generation)) return;
            completions_.push(StreamRestartCompletion{
                .generation = generation,
                .audioStreamIndex = options.audioStreamIndex,
                .wasPaused = wasPaused,
                .item = std::move(item),
                .result = std::move(target),
            });
        });
    }

    bool resolveFallback(JellyfinSession session, JellyfinItem item, PlaybackTarget failedTarget, bool reportPrevious,
                         int64_t resumeTicks, PlaybackStreamResolutionOptions options, uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item),
                              failedTarget = std::move(failedTarget), reportPrevious, resumeTicks,
                              options = std::move(options), generation]() mutable {
            reportStopIfNeeded("stop-after-failure", session, item, failedTarget, resumeTicks, reportPrevious);
            auto target = resolve(session, item, options);
            if (!epoch_.active(generation)) return;
            completions_.push(FallbackPlaybackCompletion{
                .generation = generation,
                .audioStreamIndex = options.audioStreamIndex,
                .item = std::move(item),
                .result = std::move(target),
            });
        });
    }

    bool reportPreviousStop(JellyfinSession session, JellyfinItem item, PlaybackTarget target, int64_t ticks,
                            const char* stage) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item), target = std::move(target),
                              ticks, stage] { reportStopIfNeeded(stage, session, item, target, ticks, true); });
    }

private:
    ApiValueResult<PlaybackTarget> resolve(const JellyfinSession& session, const JellyfinItem& item,
                                           const PlaybackStreamResolutionOptions& options) {
        return client_.resolvePlayback(session, item, options.maxStreamingBitrate, options.maxAudioChannels,
                                       options.overrides, options.audioStreamIndex, options.subtitleStreamIndex);
    }

    void reportStopIfNeeded(const char* stage, const JellyfinSession& session, const JellyfinItem& item,
                            const PlaybackTarget& target, int64_t ticks, bool report) {
        if (!report) return;
        const auto result = client_.reportPlaybackStopped(session, item, target, ticks);
        reportFailure_(stage, item.id, result);
    }

    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    const Epoch& epoch_;
    ReportFailureSink reportFailure_;
};
