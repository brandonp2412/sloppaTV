#include "seerr_async_executor.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {
using Completion =
    std::variant<SeerrDeleteCompletion, SeerrRequestCompletion, SeerrStorageRefreshCompletion,
                 SeerrPendingRefreshCompletion, SeerrSearchCompletion, SeerrConnectCompletion, SeerrSeasonsCompletion>;

struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        ++submissions;
        if (!accept) return false;
        task();
        return true;
    }

    bool accept = true;
    int submissions = 0;
};

struct CompletionSink {
    void push(Completion completion) { events.push_back(std::move(completion)); }

    std::vector<Completion> events;
};

struct FakeJellyfinClient {
    ApiValueResult<bool> authorizeQuickConnectCode(const JellyfinSession& session, const std::string& code) {
        lastUserId = session.userId;
        lastCode = code;
        ApiValueResult<bool> result;
        result.ok = true;
        result.value = true;
        return result;
    }

    std::string lastUserId;
    std::string lastCode;
};

struct FakeSeerrClient {
    ApiValueResult<std::vector<SeerrSeason>> seasons(const std::string& server, const SeerrAuth& auth, int tmdbId,
                                                     bool is4k) {
        lastServer = server;
        lastAuth = auth;
        lastTmdbId = tmdbId;
        lastIs4k = is4k;
        ApiValueResult<std::vector<SeerrSeason>> result;
        result.ok = true;
        SeerrSeason season;
        season.number = 3;
        season.episodes = 8;
        result.value.push_back(season);
        return result;
    }

    ApiValueResult<SeerrQuickConnectRequest> initiateQuickConnect(const std::string& server) {
        lastServer = server;
        SeerrQuickConnectRequest request;
        request.code = "CODE";
        request.secret = "SECRET";
        ApiValueResult<SeerrQuickConnectRequest> result;
        result.ok = true;
        result.value = request;
        return result;
    }

    ApiValueResult<std::string> authenticateQuickConnect(const std::string& server,
                                                         const SeerrQuickConnectRequest& request) {
        lastServer = server;
        lastQuickConnectSecret = request.secret;
        ApiValueResult<std::string> result;
        result.ok = true;
        result.value = "session-cookie";
        return result;
    }

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
        lastSeasons = item.selectedSeasons;
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

    void cancelPendingRequests() { ++cancelCount; }

    std::string lastServer;
    SeerrAuth lastAuth;
    int lastRequestId = -1;
    int lastTargetServerId = -1;
    int lastLimit = -1;
    std::string lastItemId;
    std::string lastQuery;
    std::string lastQuickConnectSecret;
    int cancelCount = 0;
    int lastTmdbId = 0;
    bool lastIs4k = false;
    std::vector<int> lastSeasons;
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
    FakeJellyfinClient jellyfinClient;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    SeerrAsyncExecutor executor(requestClient, searchClient, jellyfinClient, tasks, completions);

    JellyfinSession jellyfin;
    jellyfin.userId = "jellyfin-user";
    assert(executor.connect("https://seerr.example.nz", jellyfin, true));
    assert(tasks.submissions == 1);
    assert(requestClient.lastQuickConnectSecret == "SECRET");
    assert(jellyfinClient.lastUserId == "jellyfin-user");
    assert(jellyfinClient.lastCode == "CODE");
    const auto& connected = std::get<SeerrConnectCompletion>(completions.events.back());
    assert(connected.server == "https://seerr.example.nz");
    assert(connected.jellyfinUserId == "jellyfin-user");
    assert(connected.announce);
    assert(connected.result.ok);
    assert(connected.result.sessionCookie == "session-cookie");

    assert(executor.deleteRequest(endpoint(), {.itemId = "seerr:movie:10", .requestId = 44}));
    assert(tasks.submissions == 2);
    assert(requestClient.lastRequestId == 44);
    const auto& deleted = std::get<SeerrDeleteCompletion>(completions.events.back());
    assert(deleted.endpoint.server == "https://seerr.example.nz");
    assert(deleted.request.itemId == "seerr:movie:10");
    assert(deleted.result.ok);

    assert(executor.requestMedia(endpoint(), media(), target()));
    assert(tasks.submissions == 3);
    assert(requestClient.lastItemId == "seerr:movie:10");
    assert(requestClient.lastTargetServerId == 4);
    const auto& requested = std::get<SeerrRequestCompletion>(completions.events.back());
    assert(requested.requestedItem.id == "seerr:movie:10");
    assert(requested.result.ok && requested.result.value == 91);

    assert(executor.refreshStorage(endpoint()));
    assert(tasks.submissions == 4);
    const auto& storage = std::get<SeerrStorageRefreshCompletion>(completions.events.back());
    assert(storage.result.ok && storage.result.value.size() == 1);
    assert(storage.result.value.front().serverId == 7);

    assert(executor.refreshPending(endpoint()));
    assert(tasks.submissions == 5);
    assert(requestClient.lastLimit == 20);
    const auto& pending = std::get<SeerrPendingRefreshCompletion>(completions.events.back());
    assert(pending.result.ok && pending.result.value.front().id == "seerr:movie:20");

    assert(executor.search(endpoint(), "arrival", 12));
    assert(tasks.submissions == 6);
    assert(searchClient.lastQuery == "arrival");
    assert(requestClient.lastQuery.empty());
    const auto& searched = std::get<SeerrSearchCompletion>(completions.events.back());
    assert(searched.query == "arrival");
    assert(searched.generation == 12);
    assert(searched.result.ok && searched.result.value.front().id == "seerr:movie:30");

    executor.cancelSearch();
    assert(searchClient.cancelCount == 1);
    assert(requestClient.cancelCount == 0);

    assert(executor.loadSeasons(endpoint(), 123, true, 42));
    const auto& seasons = std::get<SeerrSeasonsCompletion>(completions.events.back());
    assert(seasons.generation == 42 && seasons.result.ok);
    assert(seasons.result.value.front().number == 3);
    assert(requestClient.lastTmdbId == 123 && requestClient.lastIs4k);
    auto series = media();
    series.mediaType = "tv";
    series.selectedSeasons = {1, 3};
    assert(executor.requestMedia(endpoint(), series, target()));
    assert((requestClient.lastSeasons == std::vector<int>{1, 3}));
    assert(std::get<SeerrRequestCompletion>(completions.events.back()).requestedItem.selectedSeasons ==
           series.selectedSeasons);

    tasks.accept = false;
    const auto completionCount = completions.events.size();
    assert(!executor.connect("https://seerr.example.nz", jellyfin, false));
    assert(!executor.deleteRequest(endpoint(), {.itemId = "seerr:movie:10", .requestId = 44}));
    assert(!executor.requestMedia(endpoint(), media(), std::nullopt));
    assert(!executor.refreshStorage(endpoint()));
    assert(!executor.refreshPending(endpoint()));
    assert(!executor.search(endpoint(), "matrix", 13));
    assert(!executor.loadSeasons(endpoint(), 123, false, 43));
    assert(completions.events.size() == completionCount);

    return 0;
}
