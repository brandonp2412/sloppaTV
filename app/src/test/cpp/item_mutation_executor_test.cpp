#include "item_mutation_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<FavoriteCompletion, MetadataRefreshCompletion, DeleteItemCompletion>;

struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        if (!accept) return false;
        ++submissions;
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

struct FakeClient {
    ApiResult setFavorite(const JellyfinSession&, const JellyfinItem& item, bool desired) {
        ++favoriteCalls;
        favoriteItemId = item.id;
        favoriteDesired = desired;
        return favoriteResult;
    }

    ApiResult refreshMetadata(const JellyfinSession&, const JellyfinItem& item) {
        ++refreshCalls;
        refreshItemId = item.id;
        return refreshResult;
    }

    ApiResult deleteItem(const JellyfinSession&, const JellyfinItem& item) {
        ++deleteCalls;
        deleteItemId = item.id;
        return deleteResult;
    }

    ApiResult favoriteResult;
    ApiResult refreshResult;
    ApiResult deleteResult;
    std::string favoriteItemId;
    std::string refreshItemId;
    std::string deleteItemId;
    bool favoriteDesired = false;
    int favoriteCalls = 0;
    int refreshCalls = 0;
    int deleteCalls = 0;
};

JellyfinSession session() {
    JellyfinSession value;
    value.server = "https://jellyfin.example.nz";
    value.userId = "user-1";
    return value;
}

JellyfinItem movie() {
    JellyfinItem item;
    item.id = "movie-1";
    item.name = "Movie";
    item.type = "Movie";
    item.favorite = false;
    item.played = true;
    item.positionTicks = 123;
    return item;
}
} // namespace

int main() {
    {
        FakeClient client;
        client.favoriteResult.ok = true;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.setFavorite(session(), movie(), true, 11));
        assert(tasks.submissions == 1);
        assert(client.favoriteCalls == 1);
        assert(client.favoriteItemId == "movie-1");
        assert(client.favoriteDesired);
        assert(completions.events.size() == 1);
        const auto& completion = std::get<FavoriteCompletion>(completions.events.front());
        assert(completion.item.id == "movie-1");
        assert(completion.item.name == "Movie");
        assert(completion.item.played);
        assert(completion.item.positionTicks == 123);
        assert(completion.desired);
        assert(completion.sessionEpoch == 11);
        assert(completion.result.ok);
    }

    {
        FakeClient client;
        client.refreshResult.ok = false;
        client.refreshResult.error = "refresh failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.refreshMetadata(session(), "episode-2", 13));
        assert(client.refreshCalls == 1);
        assert(client.refreshItemId == "episode-2");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<MetadataRefreshCompletion>(completions.events.front());
        assert(completion.sessionEpoch == 13);
        assert(!completion.result.ok);
        assert(completion.result.error == "refresh failed");
    }

    {
        FakeClient client;
        client.deleteResult.ok = true;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.deleteItem(session(), "movie-3", 17));
        assert(client.deleteCalls == 1);
        assert(client.deleteItemId == "movie-3");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<DeleteItemCompletion>(completions.events.front());
        assert(completion.itemId == "movie-3");
        assert(completion.sessionEpoch == 17);
        assert(completion.result.ok);
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(!executor.setFavorite(session(), movie(), true, 19));
        assert(!executor.refreshMetadata(session(), "movie-1", 21));
        assert(!executor.deleteItem(session(), "movie-1", 23));
        assert(tasks.submissions == 0);
        assert(client.favoriteCalls == 0);
        assert(client.refreshCalls == 0);
        assert(client.deleteCalls == 0);
        assert(completions.events.empty());
    }

    return 0;
}
