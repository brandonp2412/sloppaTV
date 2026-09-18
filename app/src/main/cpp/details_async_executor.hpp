#pragma once

#include "details_screen.hpp"
#include "jellyfin_types.hpp"
#include "request_epoch.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct DetailsItemCompletion {
    std::string itemId;
    uint64_t generation = 0;
    ApiValueResult<JellyfinItem> result;
};

struct DetailsSimilarCompletion {
    std::string itemId;
    uint64_t generation = 0;
    std::vector<JellyfinItem> items;
};

struct EpisodeSeriesContextRequestCompletion {
    JellyfinSession session;
    EpisodeSeriesContextRequest request;
    uint64_t generation = 0;
};

struct EpisodeSeriesContextCompletion {
    std::string itemId;
    uint64_t generation = 0;
    JellyfinItem series;
    std::vector<JellyfinItem> seasons;
};

struct SeasonsCompletion {
    std::string seriesId;
    uint64_t generation = 0;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

struct EpisodesCompletion {
    std::string seriesId;
    std::string seasonId;
    uint64_t generation = 0;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

struct ItemMenuDetailCompletion {
    std::string itemId;
    ApiValueResult<JellyfinItem> result;
};

struct PersonItemsCompletion {
    std::string personId;
    uint64_t generation = 0;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink>
class DetailsAsyncExecutor {
public:
    DetailsAsyncExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool load(JellyfinSession session, std::string itemId, RequestEpoch::Token requestToken) {
        return tasks_.submit(
            [this, session = std::move(session), itemId = std::move(itemId), requestToken]() mutable {
                auto result = client_.getItem(session, itemId);
                if (!requestToken.active()) return;
                if (!result.ok) {
                    completions_.push(DetailsItemCompletion{
                        .itemId = itemId,
                        .generation = requestToken.value(),
                        .result = std::move(result),
                    });
                    return;
                }

                auto contextRequest = episodeSeriesContextRequest(result.value);
                completions_.push(DetailsItemCompletion{
                    .itemId = itemId,
                    .generation = requestToken.value(),
                    .result = std::move(result),
                });

                auto similar = client_.getSimilar(session, itemId, 18);
                if (!requestToken.active()) return;
                if (similar.ok) {
                    completions_.push(DetailsSimilarCompletion{
                        .itemId = itemId,
                        .generation = requestToken.value(),
                        .items = std::move(similar.value),
                    });
                }

                if (!contextRequest) return;
                completions_.push(EpisodeSeriesContextRequestCompletion{
                    .session = std::move(session),
                    .request = std::move(*contextRequest),
                    .generation = requestToken.value(),
                });
            });
    }

    bool loadSeriesContext(JellyfinSession session, EpisodeSeriesContextRequest request,
                           RequestEpoch::Token requestToken) {
        return tasks_.submit(
            [this, session = std::move(session), request = std::move(request), requestToken]() mutable {
                auto series = client_.getItem(session, request.seriesId);
                auto seasons = client_.getSeasons(session, request.seriesId);
                if (!requestToken.active() || !series.ok || !seasons.ok) return;
                completions_.push(EpisodeSeriesContextCompletion{
                    .itemId = std::move(request.itemId),
                    .generation = requestToken.value(),
                    .series = std::move(series.value),
                    .seasons = std::move(seasons.value),
                });
            });
    }

    bool loadSeasons(JellyfinSession session, std::string seriesId, uint64_t generation) {
        return tasks_.submit(
            [this, session = std::move(session), seriesId = std::move(seriesId), generation]() mutable {
                auto result = client_.getSeasons(session, seriesId);
                completions_.push(SeasonsCompletion{
                    .seriesId = std::move(seriesId),
                    .generation = generation,
                    .result = std::move(result),
                });
            });
    }

    bool loadEpisodes(JellyfinSession session, std::string seriesId, std::string seasonId, uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), seriesId = std::move(seriesId),
                              seasonId = std::move(seasonId), generation]() mutable {
            auto result = client_.getEpisodes(session, seriesId, seasonId);
            completions_.push(EpisodesCompletion{
                .seriesId = std::move(seriesId),
                .seasonId = std::move(seasonId),
                .generation = generation,
                .result = std::move(result),
            });
        });
    }

    bool loadItemMenuDetail(JellyfinSession session, std::string itemId) {
        return tasks_.submit([this, session = std::move(session), itemId = std::move(itemId)]() mutable {
            auto result = client_.getItem(session, itemId);
            completions_.push(ItemMenuDetailCompletion{
                .itemId = std::move(itemId),
                .result = std::move(result),
            });
        });
    }

    bool loadPersonItems(JellyfinSession session, std::string personId, uint64_t generation, int limit) {
        return tasks_.submit([this, session = std::move(session), personId = std::move(personId), generation, limit]() mutable {
            auto result = client_.getItemsForPerson(session, personId, limit);
            completions_.push(PersonItemsCompletion{
                .personId = std::move(personId),
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
