#include "quick_connect_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<QuickConnectStartedCompletion, QuickConnectFailedCompletion,
                                QuickConnectAuthenticatedCompletion, QuickConnectTimedOutCompletion>;

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
    ApiValueResult<QuickConnectRequest> initiateQuickConnect(const std::string& server, const std::string& deviceId) {
        ++initiateCalls;
        lastServer = server;
        lastDeviceId = deviceId;
        return initiated;
    }

    ApiValueResult<bool> pollQuickConnect(const QuickConnectRequest&, const std::string&) {
        ++pollCalls;
        if (pollIndex < polls.size()) return polls[pollIndex++];
        ApiValueResult<bool> pending;
        pending.ok = true;
        pending.value = false;
        return pending;
    }

    ApiValueResult<JellyfinSession> completeQuickConnect(const QuickConnectRequest&, const std::string&) {
        ++completeCalls;
        return authenticated;
    }

    ApiValueResult<QuickConnectRequest> initiated;
    std::vector<ApiValueResult<bool>> polls;
    size_t pollIndex = 0;
    ApiValueResult<JellyfinSession> authenticated;
    int initiateCalls = 0;
    int pollCalls = 0;
    int completeCalls = 0;
    std::string lastServer;
    std::string lastDeviceId;
};

struct NoWait {
    void operator()(std::chrono::milliseconds) {}
};

struct CancelOnWait {
    RequestEpoch* epoch = nullptr;

    void operator()(std::chrono::milliseconds) {
        if (epoch != nullptr) epoch->invalidate();
    }
};

ApiValueResult<bool> pollResult(bool ready) {
    ApiValueResult<bool> result;
    result.ok = true;
    result.value = ready;
    return result;
}

FakeClient successfulClient() {
    FakeClient client;
    client.initiated.ok = true;
    client.initiated.value = {
        .server = "https://jellyfin.example.nz",
        .secret = "secret",
        .code = "ABCD",
    };
    client.polls = {pollResult(false), pollResult(true)};
    client.authenticated.ok = true;
    client.authenticated.value.server = "https://jellyfin.example.nz";
    client.authenticated.value.userId = "user-1";
    return client;
}
} // namespace

int main() {
    {
        FakeClient client = successfulClient();
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, NoWait> executor(client, tasks,
                                                                                               completions);

        const auto token = epoch.beginToken();
        assert(executor.connect("https://jellyfin.example.nz", "device-1", token));
        assert(tasks.submissions == 1);
        assert(client.lastServer == "https://jellyfin.example.nz");
        assert(client.lastDeviceId == "device-1");
        assert(client.pollCalls == 2);
        assert(client.completeCalls == 1);
        assert(completions.events.size() == 2);
        const auto& started = std::get<QuickConnectStartedCompletion>(completions.events[0]);
        assert(started.generation == token.value());
        assert(started.request.code == "ABCD");
        const auto& authenticated = std::get<QuickConnectAuthenticatedCompletion>(completions.events[1]);
        assert(authenticated.generation == token.value());
        assert(authenticated.session.userId == "user-1");
    }

    {
        FakeClient client;
        client.initiated.ok = false;
        client.initiated.error = "init failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, NoWait> executor(client, tasks,
                                                                                               completions);

        const auto token = epoch.beginToken();
        assert(executor.connect("https://jellyfin.example.nz", "device-2", token));
        assert(completions.events.size() == 1);
        const auto& failed = std::get<QuickConnectFailedCompletion>(completions.events.back());
        assert(failed.generation == token.value());
        assert(failed.error == "init failed");
        assert(client.pollCalls == 0);
    }

    {
        FakeClient client = successfulClient();
        client.polls.clear();
        ApiValueResult<bool> failedPoll;
        failedPoll.ok = false;
        failedPoll.error = "poll failed";
        client.polls.push_back(failedPoll);
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, NoWait> executor(client, tasks,
                                                                                               completions);

        assert(executor.connect("https://jellyfin.example.nz", "device-3", epoch.beginToken()));
        assert(completions.events.size() == 2);
        assert(std::get<QuickConnectFailedCompletion>(completions.events.back()).error == "poll failed");
        assert(client.completeCalls == 0);
    }

    {
        FakeClient client = successfulClient();
        client.polls = {pollResult(true)};
        client.authenticated.ok = false;
        client.authenticated.error = "auth failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, NoWait> executor(client, tasks,
                                                                                               completions);

        assert(executor.connect("https://jellyfin.example.nz", "device-4", epoch.beginToken()));
        assert(completions.events.size() == 2);
        assert(std::get<QuickConnectFailedCompletion>(completions.events.back()).error == "auth failed");
    }

    {
        FakeClient client = successfulClient();
        client.polls.clear();
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, NoWait> executor(client, tasks,
                                                                                               completions);

        const auto token = epoch.beginToken();
        assert(executor.connect("https://jellyfin.example.nz", "device-5", token));
        assert(client.pollCalls == 60);
        assert(completions.events.size() == 2);
        assert(std::get<QuickConnectTimedOutCompletion>(completions.events.back()).generation == token.value());
    }

    {
        FakeClient client = successfulClient();
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, CancelOnWait> executor(
            client, tasks, completions, CancelOnWait{.epoch = &epoch});

        assert(executor.connect("https://jellyfin.example.nz", "device-6", epoch.beginToken()));
        assert(completions.events.size() == 1);
        assert(std::holds_alternative<QuickConnectStartedCompletion>(completions.events.front()));
        assert(client.pollCalls == 0);
        assert(client.completeCalls == 0);
    }

    {
        FakeClient client = successfulClient();
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        RequestEpoch epoch;
        QuickConnectExecutor<FakeClient, ImmediateTaskRunner, CompletionSink, NoWait> executor(client, tasks,
                                                                                               completions);

        assert(!executor.connect("https://jellyfin.example.nz", "device-7", epoch.beginToken()));
        assert(completions.events.empty());
        assert(client.initiateCalls == 0);
    }

    return 0;
}
