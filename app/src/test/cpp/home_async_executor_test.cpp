#include "home_async_executor.hpp"

#include <cassert>
#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<HomeCoreCompletion, HomeSecondaryCompletion>;

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
    ApiValueResult<JellyfinHomeData> loadHomeCore(const JellyfinSession& session) {
        ++coreCalls;
        lastServer = session.server;
        return coreResult;
    }

    ApiValueResult<JellyfinHomeData> loadHomeSecondary(const JellyfinSession& session,
                                                       const std::vector<JellyfinItem>& views) {
        ++secondaryCalls;
        lastServer = session.server;
        lastViews = views;
        return secondaryResult;
    }

    ApiValueResult<JellyfinHomeData> coreResult;
    ApiValueResult<JellyfinHomeData> secondaryResult;
    std::string lastServer;
    std::vector<JellyfinItem> lastViews;
    int coreCalls = 0;
    int secondaryCalls = 0;
};

JellyfinSession session() {
    JellyfinSession value;
    value.server = "https://jellyfin.example.nz";
    value.userId = "user-1";
    return value;
}

HomeSelectionSnapshot snapshot() {
    HomeSelectionSnapshot value;
    value.focusedRowTitle = "Continue Watching";
    value.selectedItemByRow["Continue Watching"] = "episode-2";
    return value;
}

JellyfinItem item(std::string id) {
    JellyfinItem value;
    value.id = std::move(id);
    return value;
}
} // namespace

int main() {
    const auto startedAt = std::chrono::steady_clock::time_point(std::chrono::milliseconds(1234));

    {
        FakeClient client;
        client.coreResult.ok = true;
        client.coreResult.value.views = {item("view-1")};
        JellyfinHomeRow row;
        row.title = "Continue Watching";
        row.items = {item("episode-1"), item("episode-2")};
        client.coreResult.value.rows = {row};

        ImmediateTaskRunner tasks;
        CompletionSink completions;
        HomeAsyncExecutor executor(client, tasks, completions);

        assert(executor.loadCore(session(), 7, snapshot(), startedAt));
        assert(tasks.submissions == 1);
        assert(client.coreCalls == 1);
        assert(client.lastServer == "https://jellyfin.example.nz");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<HomeCoreCompletion>(completions.events.front());
        assert(completion.generation == 7);
        assert(completion.snapshot.focusedRowTitle == "Continue Watching");
        assert(completion.snapshot.selectedItemByRow.at("Continue Watching") == "episode-2");
        assert(completion.startedAt == startedAt);
        assert(completion.result.ok);
        assert(completion.result.value.views.size() == 1);
        assert(completion.result.value.views.front().id == "view-1");
        assert(completion.result.value.rows.front().items.size() == 2);
    }

    {
        FakeClient client;
        client.secondaryResult.ok = false;
        client.secondaryResult.error = "secondary failed";

        ImmediateTaskRunner tasks;
        CompletionSink completions;
        HomeAsyncExecutor executor(client, tasks, completions);

        assert(executor.loadSecondary(session(), 11, {item("view-a"), item("view-b")}, snapshot(), 3, startedAt));
        assert(tasks.submissions == 1);
        assert(client.secondaryCalls == 1);
        assert(client.lastViews.size() == 2);
        assert(client.lastViews[0].id == "view-a");
        assert(client.lastViews[1].id == "view-b");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<HomeSecondaryCompletion>(completions.events.front());
        assert(completion.generation == 11);
        assert(completion.snapshot.focusedRowTitle == "Continue Watching");
        assert(completion.coreRestoredRow == 3);
        assert(completion.startedAt == startedAt);
        assert(!completion.result.ok);
        assert(completion.result.error == "secondary failed");
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        HomeAsyncExecutor executor(client, tasks, completions);

        assert(!executor.loadCore(session(), 13, snapshot(), startedAt));
        assert(!executor.loadSecondary(session(), 17, {item("view-1")}, snapshot(), 0, startedAt));
        assert(tasks.submissions == 0);
        assert(client.coreCalls == 0);
        assert(client.secondaryCalls == 0);
        assert(completions.events.empty());
    }

    return 0;
}
