#include "details_async_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<DetailsItemCompletion, DetailsSimilarCompletion, EpisodeSeriesContextRequestCompletion,
                                EpisodeSeriesContextCompletion>;

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
    ApiValueResult<JellyfinItem> getItem(const JellyfinSession&, const std::string& itemId) {
        ++getItemCalls;
        requestedItems.push_back(itemId);
        if (onGetItem) onGetItem();
        if (itemId == "series-1") return seriesResult;
        return itemResult;
    }

    ApiValueResult<std::vector<JellyfinItem>> getSimilar(const JellyfinSession&, const std::string& itemId, int limit) {
        ++similarCalls;
        similarItemId = itemId;
        similarLimit = limit;
        return similarResult;
    }

    ApiValueResult<std::vector<JellyfinItem>> getSeasons(const JellyfinSession&, const std::string& seriesId) {
        ++seasonsCalls;
        seasonsSeriesId = seriesId;
        return seasonsResult;
    }

    ApiValueResult<JellyfinItem> itemResult;
    ApiValueResult<JellyfinItem> seriesResult;
    ApiValueResult<std::vector<JellyfinItem>> similarResult;
    ApiValueResult<std::vector<JellyfinItem>> seasonsResult;
    std::function<void()> onGetItem;
    std::vector<std::string> requestedItems;
    std::string similarItemId;
    std::string seasonsSeriesId;
    int similarLimit = -1;
    int getItemCalls = 0;
    int similarCalls = 0;
    int seasonsCalls = 0;
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
    item.type = "Episode";
    item.seriesId = "series-1";
    item.name = "Episode";
    return item;
}

JellyfinItem movie() {
    JellyfinItem item;
    item.id = "movie-1";
    item.type = "Movie";
    item.name = "Movie";
    return item;
}

JellyfinItem similarItem() {
    JellyfinItem item;
    item.id = "similar-1";
    item.type = "Movie";
    return item;
}
} // namespace

int main() {
    {
        FakeClient client;
        client.itemResult.ok = true;
        client.itemResult.value = episode();
        client.similarResult.ok = true;
        client.similarResult.value = {similarItem()};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        DetailsAsyncExecutor executor(client, tasks, completions);

        const auto token = epoch.beginToken();
        assert(executor.load(session(), "episode-1", token));
        assert(tasks.submissions == 1);
        assert(client.getItemCalls == 1);
        assert(client.similarCalls == 1);
        assert(client.similarItemId == "episode-1");
        assert(client.similarLimit == 18);
        assert(completions.events.size() == 3);

        const auto& item = std::get<DetailsItemCompletion>(completions.events[0]);
        assert(item.itemId == "episode-1");
        assert(item.generation == token.value());
        assert(item.result.ok);
        assert(item.result.value.seriesId == "series-1");

        const auto& similar = std::get<DetailsSimilarCompletion>(completions.events[1]);
        assert(similar.itemId == "episode-1");
        assert(similar.generation == token.value());
        assert(similar.items.size() == 1);
        assert(similar.items.front().id == "similar-1");

        const auto& context = std::get<EpisodeSeriesContextRequestCompletion>(completions.events[2]);
        assert(context.generation == token.value());
        assert(context.session.userId == "user-1");
        assert(context.request.itemId == "episode-1");
        assert(context.request.seriesId == "series-1");
    }

    {
        FakeClient client;
        client.itemResult.ok = true;
        client.itemResult.value = movie();
        client.similarResult.ok = false;
        client.similarResult.error = "similar failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        DetailsAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(session(), "movie-1", epoch.beginToken()));
        assert(client.similarCalls == 1);
        assert(completions.events.size() == 1);
        assert(std::holds_alternative<DetailsItemCompletion>(completions.events.front()));
    }

    {
        FakeClient client;
        client.itemResult.ok = false;
        client.itemResult.error = "details failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        DetailsAsyncExecutor executor(client, tasks, completions);

        const auto token = epoch.beginToken();
        assert(executor.load(session(), "missing-1", token));
        assert(client.similarCalls == 0);
        assert(completions.events.size() == 1);
        const auto& failed = std::get<DetailsItemCompletion>(completions.events.front());
        assert(failed.generation == token.value());
        assert(failed.result.error == "details failed");
    }

    {
        FakeClient client;
        client.itemResult.ok = true;
        client.itemResult.value = episode();
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        client.onGetItem = [&] { epoch.invalidate(); };
        DetailsAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(session(), "episode-1", epoch.beginToken()));
        assert(completions.events.empty());
        assert(client.similarCalls == 0);
    }

    {
        FakeClient client;
        client.seriesResult.ok = true;
        client.seriesResult.value.id = "series-1";
        client.seriesResult.value.type = "Series";
        client.seasonsResult.ok = true;
        JellyfinItem season;
        season.id = "season-1";
        season.type = "Season";
        client.seasonsResult.value = {season};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        DetailsAsyncExecutor executor(client, tasks, completions);

        const auto token = epoch.beginToken();
        assert(executor.loadSeriesContext(
            session(), EpisodeSeriesContextRequest{.itemId = "episode-1", .seriesId = "series-1"}, token));
        assert(tasks.submissions == 1);
        assert(client.requestedItems.size() == 1);
        assert(client.requestedItems.front() == "series-1");
        assert(client.seasonsCalls == 1);
        assert(client.seasonsSeriesId == "series-1");
        assert(completions.events.size() == 1);
        const auto& context = std::get<EpisodeSeriesContextCompletion>(completions.events.front());
        assert(context.itemId == "episode-1");
        assert(context.generation == token.value());
        assert(context.series.id == "series-1");
        assert(context.seasons.size() == 1);
        assert(context.seasons.front().id == "season-1");
    }

    {
        FakeClient client;
        client.seriesResult.ok = false;
        client.seasonsResult.ok = true;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        DetailsAsyncExecutor executor(client, tasks, completions);

        assert(executor.loadSeriesContext(
            session(), EpisodeSeriesContextRequest{.itemId = "episode-2", .seriesId = "series-1"},
            epoch.beginToken()));
        assert(completions.events.empty());
    }

    {
        FakeClient client;
        client.itemResult.ok = true;
        client.itemResult.value = movie();
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        RequestEpoch epoch;
        DetailsAsyncExecutor executor(client, tasks, completions);

        assert(!executor.load(session(), "movie-1", epoch.beginToken()));
        assert(client.getItemCalls == 0);
        assert(completions.events.empty());
    }

    return 0;
}
