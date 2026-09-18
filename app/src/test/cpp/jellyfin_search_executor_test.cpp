#include "jellyfin_search_executor.hpp"

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
    void push(JellyfinSearchCompletion completion) { events.push_back(std::move(completion)); }

    std::vector<JellyfinSearchCompletion> events;
};

struct FakeClient {
    ApiValueResult<std::vector<JellyfinItem>> search(const JellyfinSession& session, const std::string& query) {
        lastServer = session.server;
        lastUserId = session.userId;
        lastQuery = query;
        return response;
    }

    ApiValueResult<std::vector<JellyfinItem>> response;
    std::string lastServer;
    std::string lastUserId;
    std::string lastQuery;
};

JellyfinItem item(std::string id) {
    JellyfinItem value;
    value.id = std::move(id);
    return value;
}

JellyfinSession session() {
    JellyfinSession value;
    value.server = "https://jellyfin.example.nz";
    value.userId = "user-1";
    return value;
}
} // namespace

int main() {
    {
        FakeClient client;
        client.response.ok = true;
        client.response.value = {item("movie-1"), item("series-2")};
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        JellyfinSearchExecutor executor(client, tasks, completions);

        assert(executor.search(session(), "alien", 17));
        assert(tasks.submissions == 1);
        assert(client.lastServer == "https://jellyfin.example.nz");
        assert(client.lastUserId == "user-1");
        assert(client.lastQuery == "alien");
        assert(completions.events.size() == 1);
        const auto& completion = completions.events.front();
        assert(completion.query == "alien");
        assert(completion.generation == 17);
        assert(completion.result.ok);
        assert(completion.result.value.size() == 2);
        assert(completion.result.value[0].id == "movie-1");
        assert(completion.result.value[1].id == "series-2");
    }

    {
        FakeClient client;
        client.response.ok = false;
        client.response.error = "search failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        JellyfinSearchExecutor executor(client, tasks, completions);

        assert(executor.search(session(), "failure", 23));
        assert(completions.events.size() == 1);
        const auto& completion = completions.events.front();
        assert(completion.query == "failure");
        assert(completion.generation == 23);
        assert(!completion.result.ok);
        assert(completion.result.error == "search failed");
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        JellyfinSearchExecutor executor(client, tasks, completions);

        assert(!executor.search(session(), "rejected", 29));
        assert(tasks.submissions == 0);
        assert(client.lastQuery.empty());
        assert(completions.events.empty());
    }

    return 0;
}
