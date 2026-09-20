#pragma once

#include "discovery.hpp"
#include "jellyfin_types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct DiscoveryCompletion {
    uint64_t generation = 0;
    std::vector<DiscoveredJellyfinServer> servers;
};

struct LoginCompletion {
    uint64_t generation = 0;
    ApiValueResult<JellyfinSession> result;
};

template <typename Client, typename TaskRunner, typename CompletionSink> class AccountAsyncExecutor {
public:
    using DiscoveryFunction = std::vector<DiscoveredJellyfinServer> (*)(int);

    AccountAsyncExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions,
                         DiscoveryFunction discovery = discoverJellyfinServers)
        : client_(client), tasks_(tasks), completions_(completions), discovery_(discovery) {}

    bool discover(uint64_t generation, int timeoutMs) {
        return tasks_.submit([this, generation, timeoutMs] {
            completions_.push(DiscoveryCompletion{
                .generation = generation,
                .servers = discovery_(timeoutMs),
            });
        });
    }

    bool login(std::array<std::string, 3> fields, std::string deviceId, uint64_t generation) {
        return tasks_.submit([this, fields = std::move(fields), deviceId = std::move(deviceId), generation]() mutable {
            auto result = client_.login(std::move(fields[0]), fields[1], fields[2], deviceId);
            completions_.push(LoginCompletion{
                .generation = generation,
                .result = std::move(result),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    DiscoveryFunction discovery_;
};
