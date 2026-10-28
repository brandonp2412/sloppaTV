#pragma once

#include "playback_resolver.hpp"
#include "remux_startup_retry.hpp"

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

struct BeginPlaybackCompletion {
    uint64_t generation = 0;
    int queuedPlaybackIndex = -1;
    JellyfinItem selected;
    JellyfinItem playable;
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
                       bool replacingPlayer, bool startFromBeginning = false) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item), options = std::move(options),
                              generation, originScreen = std::move(originScreen), index, previousQueueIndex,
                              replacingPlayer, startFromBeginning]() mutable {
            auto resolved = awaitRemuxStartup(
                [&] { return resolvePreferredPlayback(client_, session, item, options, startFromBeginning); },
                [&] { return epoch_.active(generation); },
                [&](const PendingRemuxPlayback& pending) {
                    completions_.push(RemuxPlaybackPendingCompletion{generation, item.id, pending});
                });
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
        return tasks_.submit([this, session = std::move(session), item = std::move(item), options = std::move(options),
                              generation]() mutable {
            auto resolved = awaitRemuxStartup(
                [&] { return resolvePreferredPlayback(client_, session, item, options); },
                [&] { return epoch_.active(generation); },
                [&](const PendingRemuxPlayback& pending) {
                    completions_.push(RemuxPlaybackPendingCompletion{generation, item.id, pending});
                });
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
        return tasks_.submit([this, session = std::move(session), item = std::move(item), options = std::move(options),
                              generation, queuedNextIndex]() mutable {
            auto resolved = awaitRemuxStartup(
                [&] { return resolvePreferredPlayback(client_, session, item, options); },
                [&] { return epoch_.active(generation); },
                [&](const PendingRemuxPlayback& pending) {
                    completions_.push(RemuxPlaybackPendingCompletion{generation, item.id, pending});
                });
            if (!epoch_.active(generation)) return;
            completions_.push(AutoplayPlaybackCompletion{
                .generation = generation,
                .queuedNextIndex = queuedNextIndex,
                .item = std::move(resolved.item),
                .result = std::move(resolved.target),
            });
        });
    }

    bool resolveSelection(JellyfinSession session, JellyfinItem selected, PlaybackResolutionOptions options,
                          uint64_t generation, int queuedPlaybackIndex, bool fromBeginning = false) {
        return tasks_.submit([this, session = std::move(session), selected = std::move(selected),
                              options = std::move(options), generation, queuedPlaybackIndex, fromBeginning]() mutable {
            JellyfinItem playable = selected;
            if (fromBeginning) playable.positionTicks = 0;
            if (selected.type != "Series") {
                // Deep links and some cached navigation entries can carry only an item ID.
                // Hydrate the selection before PlaybackInfo so Remux sees the real media
                // metadata/source pool instead of an ID-only placeholder.
                auto detailed = client_.getItem(session, selected.id);
                if (detailed.ok) {
                    playable = std::move(detailed.value);
                    // The server item can lag a just-reported local stop. Keep the
                    // newest known resume point instead of replacing it with zero
                    // (or an older position) during hydration.
                    playable.positionTicks =
                        fromBeginning ? 0 : std::max(playable.positionTicks, selected.positionTicks);
                }
            }
            if (playable.type == "Series") {
                auto next = client_.getNextUpForSeries(session, playable.id);
                if (!next.ok) {
                    if (!epoch_.active(generation)) return;
                    ApiValueResult<PlaybackTarget> failed;
                    failed.error = std::move(next.error);
                    completions_.push(BeginPlaybackCompletion{
                        .generation = generation,
                        .queuedPlaybackIndex = queuedPlaybackIndex,
                        .selected = std::move(selected),
                        .playable = {},
                        .result = std::move(failed),
                    });
                    return;
                }
                playable = std::move(next.value);
                auto detailed = client_.getItem(session, playable.id);
                if (detailed.ok) playable = std::move(detailed.value);
            }

            if (options.isRemux) {
                playable.mediaSourceId.clear();
            } else if (const auto preferred = options.preferredMediaSources.find(playable.id);
                       preferred != options.preferredMediaSources.end()) {
                playable.mediaSourceId = preferred->second;
            }
            const auto tracks = selectPlaybackTracks(playable, options.audioLanguagePreference,
                                                     options.subtitleLanguagePreference, options.trackPolicy);
            const JellyfinItem negotiation =
                playbackNegotiationItem(playable, options.isRemux && playable.mediaSourceId.empty());
            auto target = awaitRemuxStartup(
                [&] {
                    return client_.resolvePlayback(session, negotiation, options.maxStreamingBitrate,
                                                   options.maxAudioChannels, options.overrides,
                                                   tracks.audioStreamIndex, tracks.subtitleStreamIndex);
                },
                [&] { return epoch_.active(generation); },
                [&](const PendingRemuxPlayback& pending) {
                    completions_.push(RemuxPlaybackPendingCompletion{generation, playable.id, pending});
                });
            if (!epoch_.active(generation)) return;
            completions_.push(BeginPlaybackCompletion{
                .generation = generation,
                .queuedPlaybackIndex = queuedPlaybackIndex,
                .selected = std::move(selected),
                .playable = std::move(playable),
                .result = std::move(target),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    const Epoch& epoch_;
};
