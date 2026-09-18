#pragma once

#include "playback_resolver.hpp"

#include <cstdint>
#include <utility>

template <typename Origin> struct QueuedPlaybackResolutionCompletion {
    uint64_t generation = 0;
    Origin originScreen{};
    int index = -1;
    int previousQueueIndex = -1;
    bool replacingPlayer = false;
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> result;
};

struct PlayerItemPlaybackCompletion {
    uint64_t generation = 0;
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> result;
};

struct AutoplayPlaybackCompletion {
    uint64_t generation = 0;
    int queuedNextIndex = -1;
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink, typename Epoch>
class PlaybackResolutionExecutor {
public:
    PlaybackResolutionExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions, const Epoch& epoch)
        : client_(client), tasks_(tasks), completions_(completions), epoch_(epoch) {}

    template <typename Origin>
    bool resolveQueued(JellyfinSession session, JellyfinItem item, PlaybackResolutionOptions options,
                       uint64_t generation, Origin originScreen, int index, int previousQueueIndex,
                       bool replacingPlayer) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item),
                              options = std::move(options), generation, originScreen = std::move(originScreen), index,
                              previousQueueIndex, replacingPlayer]() mutable {
            auto resolved = resolvePreferredPlayback(client_, session, std::move(item), options);
            if (!epoch_.active(generation)) return;
            completions_.push(QueuedPlaybackResolutionCompletion<Origin>{
                .generation = generation,
                .originScreen = std::move(originScreen),
                .index = index,
                .previousQueueIndex = previousQueueIndex,
                .replacingPlayer = replacingPlayer,
                .item = std::move(resolved.item),
                .result = std::move(resolved.target),
            });
        });
    }

    bool resolvePlayerItem(JellyfinSession session, JellyfinItem item, PlaybackResolutionOptions options,
                           uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item),
                              options = std::move(options), generation]() mutable {
            auto resolved = resolvePreferredPlayback(client_, session, std::move(item), options);
            if (!epoch_.active(generation)) return;
            completions_.push(PlayerItemPlaybackCompletion{
                .generation = generation,
                .item = std::move(resolved.item),
                .result = std::move(resolved.target),
            });
        });
    }

    bool resolveAutoplay(JellyfinSession session, JellyfinItem item, PlaybackResolutionOptions options,
                         uint64_t generation, int queuedNextIndex) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item),
                              options = std::move(options), generation, queuedNextIndex]() mutable {
            auto resolved = resolvePreferredPlayback(client_, session, std::move(item), options);
            if (!epoch_.active(generation)) return;
            completions_.push(AutoplayPlaybackCompletion{
                .generation = generation,
                .queuedNextIndex = queuedNextIndex,
                .item = std::move(resolved.item),
                .result = std::move(resolved.target),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    const Epoch& epoch_;
};
