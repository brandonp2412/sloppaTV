#pragma once

#include "jellyfin_types.hpp"
#include "request_epoch.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>

struct QuickConnectStartedCompletion {
    uint64_t generation = 0;
    QuickConnectRequest request;
};

struct QuickConnectFailedCompletion {
    uint64_t generation = 0;
    std::string error;
};

struct QuickConnectAuthenticatedCompletion {
    uint64_t generation = 0;
    JellyfinSession session;
};

struct QuickConnectTimedOutCompletion {
    uint64_t generation = 0;
};

struct QuickConnectWaiter {
    void operator()(std::chrono::milliseconds delay) const { std::this_thread::sleep_for(delay); }
};

template <typename Client, typename TaskRunner, typename CompletionSink, typename Waiter = QuickConnectWaiter>
class QuickConnectExecutor {
public:
    QuickConnectExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions, Waiter waiter = Waiter{})
        : client_(client), tasks_(tasks), completions_(completions), waiter_(std::move(waiter)) {}

    bool connect(std::string server, std::string deviceId, RequestEpoch::Token requestToken) {
        return tasks_.submit(
            [this, server = std::move(server), deviceId = std::move(deviceId), requestToken]() mutable {
                auto initiated = client_.initiateQuickConnect(server, deviceId);
                if (!requestToken.active()) return;
                if (!initiated.ok) {
                    completions_.push(QuickConnectFailedCompletion{
                        .generation = requestToken.value(),
                        .error = std::move(initiated.error),
                    });
                    return;
                }

                QuickConnectRequest request = std::move(initiated.value);
                completions_.push(QuickConnectStartedCompletion{
                    .generation = requestToken.value(),
                    .request = request,
                });

                for (int attempt = 0; attempt < 60; ++attempt) {
                    waiter_(std::chrono::seconds{5});
                    if (!requestToken.active()) return;

                    auto state = client_.pollQuickConnect(request, deviceId);
                    if (!state.ok) {
                        completions_.push(QuickConnectFailedCompletion{
                            .generation = requestToken.value(),
                            .error = std::move(state.error),
                        });
                        return;
                    }
                    if (!state.value) continue;

                    auto authenticated = client_.completeQuickConnect(request, deviceId);
                    if (!requestToken.active()) return;
                    if (!authenticated.ok) {
                        completions_.push(QuickConnectFailedCompletion{
                            .generation = requestToken.value(),
                            .error = std::move(authenticated.error),
                        });
                        return;
                    }

                    completions_.push(QuickConnectAuthenticatedCompletion{
                        .generation = requestToken.value(),
                        .session = std::move(authenticated.value),
                    });
                    return;
                }

                if (requestToken.active()) {
                    completions_.push(QuickConnectTimedOutCompletion{
                        .generation = requestToken.value(),
                    });
                }
            });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    Waiter waiter_;
};
