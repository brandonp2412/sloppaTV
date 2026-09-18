#include "item_mutation_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<FavoriteCompletion, PlayedCompletion, MetadataRefreshCompletion, DeleteItemCompletion>;

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

    ApiResult setPlayed(const JellyfinSession&, const JellyfinItem& item, bool desired) {
        ++playedCalls;
        playedItemId = item.id;
        playedDesired = desired;
        return playedResult;
    }

    ApiValueResult<JellyfinItem> getNextUpForSeries(const JellyfinSession&, const std::string& seriesId) {
        ++nextUpCalls;
        nextUpSeriesId = seriesId;
        return nextUpResult;
    }

    ApiValueResult<JellyfinItem> getItem(const JellyfinSession&, const std::string& itemId) {
        ++getItemCalls;
        requestedItemId = itemId;
        return itemResult;
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
    ApiResult playedResult;
    ApiValueResult<JellyfinItem> nextUpResult;
    ApiValueResult<JellyfinItem> itemResult;
    ApiResult refreshResult;
    ApiResult deleteResult;
    std::string favoriteItemId;
    std::string playedItemId;
    std::string nextUpSeriesId;
    std::string requestedItemId;
    std::string refreshItemId;
    std::string deleteItemId;
    bool favoriteDesired = false;
    bool playedDesired = false;
    int favoriteCalls = 0;
    int playedCalls = 0;
    int nextUpCalls = 0;
    int getItemCalls = 0;
    int refreshCalls = 0;
    int deleteCalls = 0;
};

JellyfinSession session() {
    JellyfinSession value;
    value.server = "https://jellyfin.example.nz";
    value.userId = "user-1";
    return value;
}

JellyfinItem episode() {
    JellyfinItem item;
    item.id = "episode-1";
    item.name = "Episode";
    item.type = "Episode";
    item.seriesId = "series-1";
    return item;
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
        client.playedResult.ok = true;
        client.nextUpResult.ok = true;
        client.nextUpResult.value.id = "episode-2";
        client.itemResult.ok = true;
        client.itemResult.value.id = "episode-2";
        client.itemResult.value.name = "Detailed next episode";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.setPlayed(session(), episode(), true, 13, 2));
        assert(client.playedCalls == 1);
        assert(client.playedItemId == "episode-1");
        assert(client.playedDesired);
        assert(client.nextUpCalls == 1);
        assert(client.nextUpSeriesId == "series-1");
        assert(client.getItemCalls == 1);
        assert(client.requestedItemId == "episode-2");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<PlayedCompletion>(completions.events.front());
        assert(completion.item.id == "episode-1");
        assert(completion.desired);
        assert(completion.sessionEpoch == 13);
        assert(completion.nextUpReplacementIndex == 2);
        assert(completion.nextUpReplacement);
        assert(completion.nextUpReplacement->name == "Detailed next episode");
        assert(completion.result.ok);
    }

    {
        FakeClient client;
        client.playedResult.ok = false;
        client.playedResult.error = "played failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.setPlayed(session(), episode(), true, 15, 0));
        assert(client.nextUpCalls == 0);
        assert(client.getItemCalls == 0);
        const auto& completion = std::get<PlayedCompletion>(completions.events.front());
        assert(!completion.nextUpReplacement);
        assert(!completion.result.ok);
        assert(completion.result.error == "played failed");
    }

    {
        FakeClient client;
        client.playedResult.ok = true;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.setPlayed(session(), movie(), false, 17, -1));
        assert(client.nextUpCalls == 0);
        assert(client.getItemCalls == 0);
        const auto& completion = std::get<PlayedCompletion>(completions.events.front());
        assert(!completion.desired);
        assert(!completion.nextUpReplacement);
    }

    {
        FakeClient client;
        client.refreshResult.ok = false;
        client.refreshResult.error = "refresh failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.refreshMetadata(session(), "episode-2", 19));
        assert(client.refreshCalls == 1);
        assert(client.refreshItemId == "episode-2");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<MetadataRefreshCompletion>(completions.events.front());
        assert(completion.sessionEpoch == 19);
        assert(!completion.result.ok);
        assert(completion.result.error == "refresh failed");
    }

    {
        FakeClient client;
        client.deleteResult.ok = true;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(executor.deleteItem(session(), "movie-3", 23));
        assert(client.deleteCalls == 1);
        assert(client.deleteItemId == "movie-3");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<DeleteItemCompletion>(completions.events.front());
        assert(completion.itemId == "movie-3");
        assert(completion.sessionEpoch == 23);
        assert(completion.result.ok);
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        ItemMutationExecutor executor(client, tasks, completions);

        assert(!executor.setFavorite(session(), movie(), true, 29));
        assert(!executor.setPlayed(session(), episode(), true, 31, 0));
        assert(!executor.refreshMetadata(session(), "movie-1", 33));
        assert(!executor.deleteItem(session(), "movie-1", 35));
        assert(tasks.submissions == 0);
        assert(client.favoriteCalls == 0);
        assert(client.playedCalls == 0);
        assert(client.refreshCalls == 0);
        assert(client.deleteCalls == 0);
        assert(completions.events.empty());
    }

    return 0;
}
