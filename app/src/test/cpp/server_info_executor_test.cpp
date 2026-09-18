#include "server_info_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<DiagnosticsCompletion, ServerInfoNoticeCompletion>;

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
    ApiValueResult<JellyfinServerInfo> getServerInfo(const JellyfinSession& session) {
        ++calls;
        lastServer = session.server;
        lastUserId = session.userId;
        return result;
    }

    ApiValueResult<JellyfinServerInfo> result;
    std::string lastServer;
    std::string lastUserId;
    int calls = 0;
};

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
        client.result.ok = true;
        client.result.value.version = "10.11.0";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ServerInfoExecutor executor(client, tasks, completions);

        assert(executor.loadNotice(session()));
        assert(tasks.submissions == 1);
        assert(client.calls == 1);
        assert(client.lastServer == "https://jellyfin.example.nz");
        assert(client.lastUserId == "user-1");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<ServerInfoNoticeCompletion>(completions.events.front());
        assert(completion.server == "https://jellyfin.example.nz");
        assert(completion.userId == "user-1");
        assert(completion.result.ok);
        assert(completion.result.value.version == "10.11.0");
    }

    {
        FakeClient client;
        client.result.ok = false;
        client.result.error = "server info failed";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        ServerInfoExecutor executor(client, tasks, completions);

        assert(executor.loadDiagnostics(session(), 37));
        assert(tasks.submissions == 1);
        assert(client.calls == 1);
        assert(completions.events.size() == 1);
        const auto& completion = std::get<DiagnosticsCompletion>(completions.events.front());
        assert(completion.generation == 37);
        assert(!completion.result.ok);
        assert(completion.result.error == "server info failed");
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        ServerInfoExecutor executor(client, tasks, completions);

        assert(!executor.loadNotice(session()));
        assert(!executor.loadDiagnostics(session(), 41));
        assert(tasks.submissions == 0);
        assert(client.calls == 0);
        assert(completions.events.empty());
    }

    return 0;
}
