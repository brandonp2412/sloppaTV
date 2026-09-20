#pragma once

#include "jellyfin_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

struct FavoriteCompletion {
    JellyfinItem item;
    bool desired = false;
    uint64_t sessionEpoch = 0;
    ApiResult result;
};

struct PlayedCompletion {
    JellyfinItem item;
    bool desired = false;
    uint64_t sessionEpoch = 0;
    int nextUpReplacementIndex = -1;
    std::optional<JellyfinItem> nextUpReplacement;
    ApiResult result;
};

struct MetadataRefreshCompletion {
    uint64_t sessionEpoch = 0;
    ApiResult result;
};

struct DeleteItemCompletion {
    std::string itemId;
    uint64_t sessionEpoch = 0;
    ApiResult result;
};

template <typename Client, typename TaskRunner, typename CompletionSink> class ItemMutationExecutor {
public:
    ItemMutationExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool setFavorite(JellyfinSession session, JellyfinItem item, bool desired, uint64_t sessionEpoch) {
        return tasks_.submit(
            [this, session = std::move(session), item = std::move(item), desired, sessionEpoch]() mutable {
                auto result = client_.setFavorite(session, item, desired);
                completions_.push(FavoriteCompletion{
                    .item = std::move(item),
                    .desired = desired,
                    .sessionEpoch = sessionEpoch,
                    .result = std::move(result),
                });
            });
    }

    bool setPlayed(JellyfinSession session, JellyfinItem item, bool desired, uint64_t sessionEpoch,
                   int nextUpReplacementIndex) {
        return tasks_.submit([this, session = std::move(session), item = std::move(item), desired, sessionEpoch,
                              nextUpReplacementIndex]() mutable {
            auto result = client_.setPlayed(session, item, desired);
            std::optional<JellyfinItem> nextUpReplacement;
            if (result.ok && desired && nextUpReplacementIndex >= 0 && item.type == "Episode" &&
                !item.seriesId.empty()) {
                auto next = client_.getNextUpForSeries(session, item.seriesId);
                if (next.ok && !next.value.id.empty() && next.value.id != item.id) {
                    auto detailed = client_.getItem(session, next.value.id);
                    nextUpReplacement = detailed.ok ? std::move(detailed.value) : std::move(next.value);
                }
            }
            completions_.push(PlayedCompletion{
                .item = std::move(item),
                .desired = desired,
                .sessionEpoch = sessionEpoch,
                .nextUpReplacementIndex = nextUpReplacementIndex,
                .nextUpReplacement = std::move(nextUpReplacement),
                .result = std::move(result),
            });
        });
    }

    bool refreshMetadata(JellyfinSession session, std::string itemId, uint64_t sessionEpoch) {
        return tasks_.submit([this, session = std::move(session), itemId = std::move(itemId), sessionEpoch]() mutable {
            JellyfinItem item;
            item.id = itemId;
            auto result = client_.refreshMetadata(session, item);
            completions_.push(MetadataRefreshCompletion{
                .sessionEpoch = sessionEpoch,
                .result = std::move(result),
            });
        });
    }

    bool deleteItem(JellyfinSession session, std::string itemId, uint64_t sessionEpoch) {
        return tasks_.submit([this, session = std::move(session), itemId = std::move(itemId), sessionEpoch]() mutable {
            JellyfinItem item;
            item.id = itemId;
            auto result = client_.deleteItem(session, item);
            completions_.push(DeleteItemCompletion{
                .itemId = std::move(itemId),
                .sessionEpoch = sessionEpoch,
                .result = std::move(result),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
