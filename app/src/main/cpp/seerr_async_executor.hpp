#pragma once

#include "jellyfin_types.hpp"
#include "seerr_auth.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "seerr_media.hpp"
#include "seerr_quick_connect.hpp"
#include "seerr_storage.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SeerrDeleteCompletion {
    SeerrEndpoint endpoint;
    SeerrDeleteRequest request;
    ApiResult result;
};

struct SeerrRequestCompletion {
    SeerrEndpoint endpoint;
    SeerrMediaItem requestedItem;
    ApiValueResult<int> result;
};

struct SeerrStorageRefreshCompletion {
    SeerrEndpoint endpoint;
    ApiValueResult<std::vector<SeerrStorageTarget>> result;
};

struct SeerrPendingRefreshCompletion {
    SeerrEndpoint endpoint;
    ApiValueResult<std::vector<SeerrMediaItem>> result;
};

struct SeerrSearchCompletion {
    std::string query;
    uint64_t generation = 0;
    ApiValueResult<std::vector<SeerrMediaItem>> result;
};

struct SeerrConnectCompletion {
    std::string server;
    std::string jellyfinUserId;
    bool announce = false;
    SeerrQuickConnectResult result;
};

template <typename RequestClient, typename SearchClient, typename JellyfinClient, typename TaskRunner,
          typename CompletionSink>
class SeerrAsyncExecutor {
public:
    SeerrAsyncExecutor(RequestClient& requestClient, SearchClient& searchClient, JellyfinClient& jellyfinClient,
                       TaskRunner& tasks, CompletionSink& completions)
        : requestClient_(requestClient), searchClient_(searchClient), jellyfinClient_(jellyfinClient), tasks_(tasks),
          completions_(completions) {}

    void connect(std::string server, JellyfinSession jellyfin, bool announce) {
        tasks_.submit([this, server = std::move(server), jellyfin = std::move(jellyfin), announce] {
            auto result = runSeerrQuickConnect(
                [&] { return requestClient_.initiateQuickConnect(server); },
                [&](const std::string& code) { return jellyfinClient_.authorizeQuickConnectCode(jellyfin, code); },
                [&](const SeerrQuickConnectRequest& request) {
                    return requestClient_.authenticateQuickConnect(server, request);
                });
            completions_.push(SeerrConnectCompletion{
                .server = server,
                .jellyfinUserId = jellyfin.userId,
                .announce = announce,
                .result = std::move(result),
            });
        });
    }

    void deleteRequest(SeerrEndpoint endpoint, SeerrDeleteRequest request) {
        tasks_.submit([this, endpoint = std::move(endpoint), request = std::move(request)] {
            ApiResult result = requestClient_.deleteRequest(endpoint.server, endpoint.auth, request.requestId);
            completions_.push(SeerrDeleteCompletion{
                .endpoint = endpoint,
                .request = request,
                .result = std::move(result),
            });
        });
    }

    void requestMedia(SeerrEndpoint endpoint, SeerrMediaItem item, std::optional<SeerrStorageTarget> target) {
        tasks_.submit([this, endpoint = std::move(endpoint), item = std::move(item), target = std::move(target)] {
            auto result =
                requestClient_.requestMedia(endpoint.server, endpoint.auth, item, target ? &*target : nullptr);
            completions_.push(SeerrRequestCompletion{
                .endpoint = endpoint,
                .requestedItem = item,
                .result = std::move(result),
            });
        });
    }

    void refreshStorage(SeerrEndpoint endpoint) {
        tasks_.submit([this, endpoint = std::move(endpoint)] {
            auto result = requestClient_.storageTargets(endpoint.server, endpoint.auth);
            completions_.push(SeerrStorageRefreshCompletion{
                .endpoint = endpoint,
                .result = std::move(result),
            });
        });
    }

    void refreshPending(SeerrEndpoint endpoint, int limit = 20) {
        tasks_.submit([this, endpoint = std::move(endpoint), limit] {
            auto result = requestClient_.pendingRequests(endpoint.server, endpoint.auth, limit);
            completions_.push(SeerrPendingRefreshCompletion{
                .endpoint = endpoint,
                .result = std::move(result),
            });
        });
    }

    bool search(SeerrEndpoint endpoint, std::string query, uint64_t generation) {
        return tasks_.submit([this, endpoint = std::move(endpoint), query = std::move(query), generation] {
            auto result = searchClient_.search(endpoint.server, endpoint.auth, query);
            completions_.push(SeerrSearchCompletion{
                .query = query,
                .generation = generation,
                .result = std::move(result),
            });
        });
    }

    void cancelSearch() { searchClient_.cancelPendingRequests(); }

private:
    RequestClient& requestClient_;
    SearchClient& searchClient_;
    JellyfinClient& jellyfinClient_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
