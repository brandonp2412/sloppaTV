#pragma once

#include "jellyfin_types.hpp"
#include "media_player_policy.hpp"
#include "playback_queue.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SeriesPlayAllOptions {
    int maxStreamingBitrate = 0;
    int maxAudioChannels = 0;
    PlaybackOverrides overrides;
};

struct SeriesPlayAllCompletion {
    uint64_t generation = 0;
    JellyfinItem series;
    std::vector<JellyfinItem> episodes;
    JellyfinItem first;
    std::optional<PlaybackTarget> target;
    std::string error;
};

template <typename Client, typename TaskRunner, typename CompletionSink, typename Epoch> class SeriesPlaybackExecutor {
public:
    using DuplicateUnavailableWarning = std::function<void(const SeriesEpisodeSlot&)>;

    SeriesPlaybackExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions, const Epoch& epoch,
                           DuplicateUnavailableWarning duplicateUnavailableWarning = {})
        : client_(client), tasks_(tasks), completions_(completions), epoch_(epoch),
          duplicateUnavailableWarning_(std::move(duplicateUnavailableWarning)) {}

    bool playAll(JellyfinSession session, JellyfinItem series, SeriesPlayAllOptions options, uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), series = std::move(series),
                              options = std::move(options), generation]() mutable {
            auto episodes = client_.getSeriesEpisodes(session, series.id, 1000);
            if (!episodes.ok || episodes.value.empty()) {
                if (!epoch_.active(generation)) return;
                completions_.push(SeriesPlayAllCompletion{
                    .generation = generation,
                    .series = std::move(series),
                    .episodes = {},
                    .first = {},
                    .target = std::nullopt,
                    .error = episodes.ok ? "PLAY ALL: NO EPISODES" : "PLAY ALL: " + episodes.error,
                });
                return;
            }

            auto prepared = prepareSeriesPlaybackQueue(std::move(episodes.value), [&](JellyfinItem& candidate) {
                if (candidate.mediaSourceId.empty() || candidate.container.empty()) {
                    auto detailed = client_.getItem(session, candidate.id);
                    if (!detailed.ok) return false;
                    candidate = std::move(detailed.value);
                }
                return client_.isStaticStreamAvailable(session, candidate);
            });
            if (duplicateUnavailableWarning_) {
                for (const auto& slot : prepared.unavailableDuplicateSlots) duplicateUnavailableWarning_(slot);
            }
            episodes.value = std::move(prepared.episodes);
            if (episodes.value.empty()) {
                if (!epoch_.active(generation)) return;
                completions_.push(SeriesPlayAllCompletion{
                    .generation = generation,
                    .series = std::move(series),
                    .episodes = {},
                    .first = {},
                    .target = std::nullopt,
                    .error = "PLAY ALL: NO REGULAR EPISODES",
                });
                return;
            }

            JellyfinItem first = episodes.value.front();
            auto detailed = client_.getItem(session, first.id);
            if (detailed.ok) first = std::move(detailed.value);
            auto target = client_.resolvePlayback(session, first, options.maxStreamingBitrate, options.maxAudioChannels,
                                                  options.overrides);
            if (!epoch_.active(generation)) return;
            completions_.push(SeriesPlayAllCompletion{
                .generation = generation,
                .series = std::move(series),
                .episodes = std::move(episodes.value),
                .first = std::move(first),
                .target = target.ok ? std::optional<PlaybackTarget>{std::move(target.value)} : std::nullopt,
                .error = target.ok ? std::string{} : "PLAY ALL: " + target.error,
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    const Epoch& epoch_;
    DuplicateUnavailableWarning duplicateUnavailableWarning_;
};
