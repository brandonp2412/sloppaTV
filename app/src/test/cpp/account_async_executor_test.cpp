#include "account_async_executor.hpp"

#include <array>
#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<DiscoveryCompletion, LoginCompletion>;

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
    ApiValueResult<JellyfinSession> login(std::string server, const std::string& username,
                                          const std::string& password, const std::string& deviceId) {
        ++calls;
        lastServer = std::move(server);
        lastUsername = username;
        lastPassword = password;
        lastDeviceId = deviceId;
        return result;
    }

    ApiValueResult<JellyfinSession> result;
    std::string lastServer;
    std::string lastUsername;
    std::string lastPassword;
    std::string lastDeviceId;
    int calls = 0;
};

int discoveryTimeout = -1;

std::vector<DiscoveredJellyfinServer> fakeDiscovery(int timeoutMs) {
    discoveryTimeout = timeoutMs;
    return {
        {
            .address = "https://jellyfin.example.nz",
            .id = "server-1",
            .name = "Living Room",
        },
    };
}
} // namespace

int main() {
    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        AccountAsyncExecutor executor(client, tasks, completions, fakeDiscovery);
        discoveryTimeout = -1;

        assert(executor.discover(11, 1600));
        assert(tasks.submissions == 1);
        assert(discoveryTimeout == 1600);
        assert(completions.events.size() == 1);
        const auto& completion = std::get<DiscoveryCompletion>(completions.events.front());
        assert(completion.generation == 11);
        assert(completion.servers.size() == 1);
        assert(completion.servers.front().address == "https://jellyfin.example.nz");
        assert(completion.servers.front().name == "Living Room");
    }

    {
        FakeClient client;
        client.result.ok = true;
        client.result.value.server = "https://jellyfin.example.nz";
        client.result.value.userId = "user-1";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        AccountAsyncExecutor executor(client, tasks, completions, fakeDiscovery);
        std::array<std::string, 3> fields{
            "https://jellyfin.example.nz",
            "brandon",
            "password",
        };

        assert(executor.login(std::move(fields), "device-1", 19));
        assert(tasks.submissions == 1);
        assert(client.calls == 1);
        assert(client.lastServer == "https://jellyfin.example.nz");
        assert(client.lastUsername == "brandon");
        assert(client.lastPassword == "password");
        assert(client.lastDeviceId == "device-1");
        assert(completions.events.size() == 1);
        const auto& completion = std::get<LoginCompletion>(completions.events.front());
        assert(completion.generation == 19);
        assert(completion.result.ok);
        assert(completion.result.value.userId == "user-1");
    }

    {
        FakeClient client;
        client.result.ok = false;
        client.result.error = "bad credentials";
        ImmediateTaskRunner tasks;
        CompletionSink completions;
        AccountAsyncExecutor executor(client, tasks, completions, fakeDiscovery);
        std::array<std::string, 3> fields{"server", "user", "bad"};

        assert(executor.login(std::move(fields), "device-2", 23));
        assert(completions.events.size() == 1);
        const auto& completion = std::get<LoginCompletion>(completions.events.front());
        assert(completion.generation == 23);
        assert(!completion.result.ok);
        assert(completion.result.error == "bad credentials");
    }

    {
        FakeClient client;
        ImmediateTaskRunner tasks;
        tasks.accept = false;
        CompletionSink completions;
        AccountAsyncExecutor executor(client, tasks, completions, fakeDiscovery);
        std::array<std::string, 3> fields{"server", "user", "password"};

        assert(!executor.discover(29, 1600));
        assert(!executor.login(std::move(fields), "device-3", 31));
        assert(tasks.submissions == 0);
        assert(client.calls == 0);
        assert(completions.events.empty());
    }

    return 0;
}
