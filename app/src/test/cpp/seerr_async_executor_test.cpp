#include "seerr_async_executor.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<SeerrDeleteCompletion, SeerrRequestCompletion, SeerrStorageRefreshCompletion,
                                SeerrPendingRefreshCompletion, SeerrSearchCompletion>;

struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        ++submissions;
        task();
        return true;
    }

    int submissions = 0;
};

struct CompletionSink {
    void push(Completion completion) { events.push_back(std::move(completion)); }

    std::vector<Completion> events;
};

struct FakeSeerrClient {
    ApiResult deleteRequest(const std::string& server, const SeerrAuth& auth, int requestId) {
        lastServer = server;
        lastAuth = auth;
        lastRequestId = requestId;
        ApiResult result;
        result.ok = true;
        return result;
    }

    ApiValueResult<int> requestMedia(const std::string& server, const SeerrAuth& auth, const SeerrMediaItem& item,
                                     const SeerrStorageTarget* target) {
        lastServer = server;
        lastAuth = auth;
        lastItemId = item.id;
        lastTargetServerId = target == nullptr ? -1 : target->serverId;
        ApiValueResult<int> result;
        result.ok = true;
        result.value = 91;
        return result;
    }

    ApiValueResult<std::vector<SeerrStorageTarget>> storageTargets(const std::string& server, const SeerrAuth& auth) {
        lastServer = server;
        lastAuth = auth;
        SeerrStorageTarget target;
        target.mediaType = "movie";
        target.serverId = 7;
        target.path = "/media";
        ApiValueResult<std::vector<SeerrStorageTarget>> result;
        result.ok = true;
        result.value = {target};
        return result;
    }

    ApiValueResult<std::vector<SeerrMediaItem>> pendingRequests(const std::string& server, const SeerrAuth& auth,
                                                                int limit) {
        lastServer = server;
        lastAuth = auth;
        lastLimit = limit;
        SeerrMediaItem item;
        item.id = "seerr:movie:20";
        item.name = "Pending";
        item.mediaType = "movie";
        item.tmdbId = 20;
        ApiValueResult<std::vector<SeerrMediaItem>> result;
        result.ok = true;
        result.value = {item};
        return result;
    }

    ApiValueResult<std::vector<SeerrMediaItem>> search(const std::string& server, const SeerrAuth& auth,
                                                       const std::string& query) {
        lastServer = server;
        lastAuth = auth;
        lastQuery = query;
        SeerrMediaItem item;
        item.id = "seerr:movie:30";
        item.name = "Search";
        item.mediaType = "movie";
        item.tmdbId = 30;
        ApiValueResult<std::vector<SeerrMediaItem>> result;
        result.ok = true;
        result.value = {item};
        return result;
    }

    std::string lastServer;
    SeerrAuth lastAuth;
    int lastRequestId = -1;
    int lastTargetServerId = -1;
    int lastLimit = -1;
    std::string lastItemId;
    std::string lastQuery;
};

SeerrEndpoint endpoint() {
    return {
        .server = "https://seerr.example.nz",
        .auth =
            {
                .sessionCookie = "session",
                .apiKey = "",
            },
    };
}

SeerrMediaItem media() {
    SeerrMediaItem item;
    item.id = "seerr:movie:10";
    item.name = "Requested";
    item.mediaType = "movie";
    item.tmdbId = 10;
    return item;
}

SeerrStorageTarget target() {
    SeerrStorageTarget value;
    value.mediaType = "movie";
    value.serverId = 4;
    value.path = "/tv";
    return value;
}
} // namespace

int main() {
    FakeSeerrClient requestClient;
    FakeSeerrClient searchClient;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    SeerrAsyncExecutor executor(requestClient, searchClient, tasks, completions);

    executor.deleteRequest(endpoint(), {.itemId = "seerr:movie:10", .requestId = 44});
    assert(tasks.submissions == 1);
    assert(requestClient.lastRequestId == 44);
    const auto& deleted = std::get<SeerrDeleteCompletion>(completions.events.back());
    assert(deleted.endpoint.server == "https://seerr.example.nz");
    assert(deleted.request.itemId == "seerr:movie:10");
    assert(deleted.result.ok);

    executor.requestMedia(endpoint(), media(), target());
    assert(tasks.submissions == 2);
    assert(requestClient.lastItemId == "seerr:movie:10");
    assert(requestClient.lastTargetServerId == 4);
    const auto& requested = std::get<SeerrRequestCompletion>(completions.events.back());
    assert(requested.requestedItem.id == "seerr:movie:10");
    assert(requested.result.ok && requested.result.value == 91);

    executor.refreshStorage(endpoint());
    assert(tasks.submissions == 3);
    const auto& storage = std::get<SeerrStorageRefreshCompletion>(completions.events.back());
    assert(storage.result.ok && storage.result.value.size() == 1);
    assert(storage.result.value.front().serverId == 7);

    executor.refreshPending(endpoint());
    assert(tasks.submissions == 4);
    assert(requestClient.lastLimit == 20);
    const auto& pending = std::get<SeerrPendingRefreshCompletion>(completions.events.back());
    assert(pending.result.ok && pending.result.value.front().id == "seerr:movie:20");

    executor.search(endpoint(), "arrival", 12);
    assert(tasks.submissions == 5);
    assert(searchClient.lastQuery == "arrival");
    assert(requestClient.lastQuery.empty());
    const auto& searched = std::get<SeerrSearchCompletion>(completions.events.back());
    assert(searched.query == "arrival");
    assert(searched.generation == 12);
    assert(searched.result.ok && searched.result.value.front().id == "seerr:movie:30");

    return 0;
}
