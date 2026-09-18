#include "browse_async_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {
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
    void push(BrowsePageCompletion completion) { events.push_back(std::move(completion)); }

    std::vector<BrowsePageCompletion> events;
};

struct FakeClient {
    ApiValueResult<std::vector<JellyfinItem>> browseLibrary(const JellyfinSession&, const std::string& parentId,
                                                            int startIndex, int pageSize) {
        call = "library";
        lastContainerId = parentId;
        lastStartIndex = startIndex;
        lastPageSize = pageSize;
        return libraryResult;
    }

    ApiValueResult<std::vector<JellyfinItem>>
    browseCollectionMembersFallback(const JellyfinSession&, const JellyfinItem& container) {
        ++fallbackCalls;
        fallbackContainerId = container.id;
        return fallbackResult;
    }

    ApiValueResult<std::vector<JellyfinItem>>
    browseVideoFilter(const JellyfinSession&, const JellyfinItem& container, int startIndex, int pageSize, bool favorite,
                      std::string genre = std::string{}, std::string letter = std::string{}) {
        call = "filter";
        lastContainerId = container.id;
        lastStartIndex = startIndex;
        lastPageSize = pageSize;
        lastFavorite = favorite;
        lastGenre = genre;
        lastLetter = letter;
        return filterResult;
    }

    ApiValueResult<std::vector<JellyfinItem>> listGenres(const JellyfinSession&, const JellyfinItem& container,
                                                         int limit) {
        call = "genres";
        lastContainerId = container.id;
        lastLimit = limit;
        return genresResult;
    }

    ApiValueResult<std::vector<JellyfinItem>> browseCollections(const JellyfinSession&, int startIndex, int pageSize) {
        call = "collections";
        lastStartIndex = startIndex;
        lastPageSize = pageSize;
        return collectionsResult;
    }

    std::string call;
    std::string lastContainerId;
    std::string fallbackContainerId;
    std::string lastGenre;
    std::string lastLetter;
    int lastStartIndex = -1;
    int lastPageSize = -1;
    int lastLimit = -1;
    int fallbackCalls = 0;
    bool lastFavorite = false;
    ApiValueResult<std::vector<JellyfinItem>> libraryResult;
    ApiValueResult<std::vector<JellyfinItem>> fallbackResult;
    ApiValueResult<std::vector<JellyfinItem>> filterResult;
    ApiValueResult<std::vector<JellyfinItem>> genresResult;
    ApiValueResult<std::vector<JellyfinItem>> collectionsResult;
};

JellyfinItem item(std::string id, std::string type = "Movie") {
    JellyfinItem value;
    value.id = std::move(id);
    value.type = std::move(type);
    return value;
}

BrowsePageRequest request(BrowseContentMode mode) {
    BrowsePageRequest value;
    value.session.server = "https://jellyfin.example.nz";
    value.session.userId = "user-1";
    value.container = item("library-1", "CollectionFolder");
    value.startIndex = 60;
    value.append = true;
    value.generation = 7;
    value.mode = mode;
    value.pageSize = 60;
    return value;
}

void assertCompletion(const CompletionSink& completions, const std::string& expectedItemId) {
    assert(completions.events.size() == 1);
    const auto& completion = completions.events.front();
    assert(completion.containerId == "library-1");
    assert(completion.startIndex == 60);
    assert(completion.append);
    assert(completion.generation == 7);
    assert(completion.result.ok);
    assert(completion.result.value.size() == 1);
    assert(completion.result.value.front().id == expectedItemId);
}
} // namespace

int main() {
    {
        FakeClient client;
        client.libraryResult.ok = true;
        client.libraryResult.value = {item("all-1")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(request(BrowseContentMode::All)));
        assert(tasks.submissions == 1);
        assert(client.call == "library");
        assert(client.lastContainerId == "library-1");
        assert(client.lastStartIndex == 60);
        assert(client.lastPageSize == 60);
        assertCompletion(completions, "all-1");
    }

    {
        FakeClient client;
        client.libraryResult.ok = true;
        client.libraryResult.value = {item("nested-1")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);
        auto load = request(BrowseContentMode::Favorites);
        load.nested = true;

        assert(executor.load(std::move(load)));
        assert(client.call == "library");
        assertCompletion(completions, "nested-1");
    }

    {
        FakeClient client;
        client.libraryResult.ok = true;
        client.fallbackResult.ok = true;
        client.fallbackResult.value = {item("fallback-1")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);
        auto load = request(BrowseContentMode::All);
        load.container.type = "BoxSet";
        load.startIndex = 0;
        load.append = false;

        assert(executor.load(std::move(load)));
        assert(client.fallbackCalls == 1);
        assert(client.fallbackContainerId == "library-1");
        assert(completions.events.size() == 1);
        assert(completions.events.front().result.value.front().id == "fallback-1");
    }

    {
        FakeClient client;
        client.filterResult.ok = true;
        client.filterResult.value = {item("favorite-1")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(request(BrowseContentMode::Favorites)));
        assert(client.call == "filter");
        assert(client.lastFavorite);
        assert(client.lastGenre.empty());
        assert(client.lastLetter.empty());
        assertCompletion(completions, "favorite-1");
    }

    {
        FakeClient client;
        client.genresResult.ok = true;
        client.genresResult.value = {item("genre-1", "Genre")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(request(BrowseContentMode::Genres)));
        assert(client.call == "genres");
        assert(client.lastLimit == 100);
        assertCompletion(completions, "genre-1");
    }

    {
        FakeClient client;
        client.filterResult.ok = true;
        client.filterResult.value = {item("genre-item-1")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);
        auto load = request(BrowseContentMode::GenreItems);
        load.genre = "Drama";

        assert(executor.load(std::move(load)));
        assert(client.call == "filter");
        assert(!client.lastFavorite);
        assert(client.lastGenre == "Drama");
        assert(client.lastLetter.empty());
        assertCompletion(completions, "genre-item-1");
    }

    {
        FakeClient client;
        client.filterResult.ok = true;
        client.filterResult.value = {item("letter-item-1")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);
        auto load = request(BrowseContentMode::LetterItems);
        load.letter = "B";

        assert(executor.load(std::move(load)));
        assert(client.call == "filter");
        assert(!client.lastFavorite);
        assert(client.lastGenre.empty());
        assert(client.lastLetter == "B");
        assertCompletion(completions, "letter-item-1");
    }

    {
        FakeClient client;
        client.collectionsResult.ok = true;
        client.collectionsResult.value = {item("collection-1", "BoxSet")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(request(BrowseContentMode::Collections)));
        assert(client.call == "collections");
        assert(client.lastStartIndex == 60);
        assert(client.lastPageSize == 60);
        assertCompletion(completions, "collection-1");
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);

        assert(executor.load(request(BrowseContentMode::Letters)));
        assert(client.call.empty());
        assert(completions.events.size() == 1);
        assert(completions.events.front().result.ok);
        assert(completions.events.front().result.value.empty());
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        BrowseAsyncExecutor executor(client, tasks, completions);

        assert(!executor.load(request(BrowseContentMode::All)));
        assert(client.call.empty());
        assert(completions.events.empty());
    }

    return 0;
}
