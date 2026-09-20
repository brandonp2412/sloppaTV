#pragma once

#include "home_screen.hpp"
#include "jellyfin_types.hpp"

#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

struct HomeCoreCompletion {
    uint64_t generation = 0;
    HomeSelectionSnapshot snapshot;
    std::chrono::steady_clock::time_point startedAt;
    ApiValueResult<JellyfinHomeData> result;
};

struct HomeSecondaryCompletion {
    uint64_t generation = 0;
    HomeSelectionSnapshot snapshot;
    int coreRestoredRow = -1;
    std::chrono::steady_clock::time_point startedAt;
    ApiValueResult<JellyfinHomeData> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink> class HomeAsyncExecutor {
public:
    HomeAsyncExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool loadCore(JellyfinSession session, uint64_t generation, HomeSelectionSnapshot snapshot,
                  std::chrono::steady_clock::time_point startedAt) {
        return tasks_.submit(
            [this, session = std::move(session), generation, snapshot = std::move(snapshot), startedAt]() mutable {
                auto result = client_.loadHomeCore(session);
                completions_.push(HomeCoreCompletion{
                    .generation = generation,
                    .snapshot = std::move(snapshot),
                    .startedAt = startedAt,
                    .result = std::move(result),
                });
            });
    }

    bool loadSecondary(JellyfinSession session, uint64_t generation, std::vector<JellyfinItem> views,
                       HomeSelectionSnapshot snapshot, int coreRestoredRow,
                       std::chrono::steady_clock::time_point startedAt) {
        return tasks_.submit([this, session = std::move(session), generation, views = std::move(views),
                              snapshot = std::move(snapshot), coreRestoredRow, startedAt]() mutable {
            auto result = client_.loadHomeSecondary(session, views);
            completions_.push(HomeSecondaryCompletion{
                .generation = generation,
                .snapshot = std::move(snapshot),
                .coreRestoredRow = coreRestoredRow,
                .startedAt = startedAt,
                .result = std::move(result),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
