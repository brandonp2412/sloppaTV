#pragma once

#include "jellyfin_types.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JellyfinSearchCompletion {
    std::string query;
    uint64_t generation = 0;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink> class JellyfinSearchExecutor {
public:
    JellyfinSearchExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool search(JellyfinSession session, std::string query, uint64_t generation) {
        return tasks_.submit([this, session = std::move(session), query = std::move(query), generation]() mutable {
            auto result = client_.search(session, query);
            completions_.push(JellyfinSearchCompletion{
                .query = std::move(query),
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
