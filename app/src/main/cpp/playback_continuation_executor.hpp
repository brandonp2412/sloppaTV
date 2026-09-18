#pragma once

#include "jellyfin_types.hpp"
#include "playback_continuation.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct MediaSegmentsCompletion {
    std::string itemId;
    bool ok = false;
    std::vector<JellyfinMediaSegment> segments;
    std::string error;
    std::chrono::steady_clock::time_point completedAt;
};

struct NextEpisodeCompletion {
    std::string currentItemId;
    bool ok = false;
    JellyfinItem item;
    std::string error;
    std::chrono::steady_clock::time_point completedAt;
};

struct PlaybackAdjacentCompletion {
    std::string currentItemId;
    int direction = 0;
    bool ok = false;
    std::optional<JellyfinItem> item;
};

template <typename Client, typename TaskRunner, typename CompletionSink> class PlaybackContinuationExecutor {
public:
    PlaybackContinuationExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool requestMediaSegments(JellyfinSession session, std::string itemId) {
        return tasks_.submit([this, session = std::move(session), itemId = std::move(itemId)] {
            auto result = client_.getMediaSegments(session, itemId);
            completions_.push(MediaSegmentsCompletion{
                .itemId = itemId,
                .ok = result.ok,
                .segments = result.ok ? std::move(result.value) : std::vector<JellyfinMediaSegment>{},
                .error = result.ok ? std::string{} : std::move(result.error),
                .completedAt = std::chrono::steady_clock::now(),
            });
        });
    }

    bool requestNextEpisode(JellyfinSession session, std::string seriesId, std::string currentItemId) {
        return tasks_.submit([this, session = std::move(session), seriesId = std::move(seriesId),
                              currentItemId = std::move(currentItemId)] {
            auto next = client_.getFollowingEpisodeForSeries(session, seriesId, currentItemId);
            if (!next.ok) {
                completions_.push(NextEpisodeCompletion{
                    .currentItemId = currentItemId,
                    .ok = false,
                    .item = {},
                    .error = std::move(next.error),
                    .completedAt = std::chrono::steady_clock::now(),
                });
                return;
            }
            if (next.value.id.empty() || next.value.id == currentItemId) return;
            auto detailed = client_.getItem(session, next.value.id);
            completions_.push(NextEpisodeCompletion{
                .currentItemId = currentItemId,
                .ok = true,
                .item = detailed.ok ? std::move(detailed.value) : std::move(next.value),
                .error = {},
                .completedAt = std::chrono::steady_clock::now(),
            });
        });
    }

    bool requestAdjacentEpisode(JellyfinSession session, std::string seriesId, std::string currentItemId,
                                int currentSeason, int currentEpisode, int direction) {
        return tasks_.submit([this, session = std::move(session), seriesId = std::move(seriesId),
                              currentItemId = std::move(currentItemId), currentSeason, currentEpisode, direction] {
            auto episodes = client_.getSeriesEpisodes(session, seriesId, 1000);
            auto adjacent = episodes.ok ? selectAdjacentPlaybackEpisode(std::move(episodes.value), currentItemId,
                                                                        currentSeason, currentEpisode, direction)
                                        : std::nullopt;
            completions_.push(PlaybackAdjacentCompletion{
                .currentItemId = currentItemId,
                .direction = direction,
                .ok = episodes.ok,
                .item = std::move(adjacent),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
