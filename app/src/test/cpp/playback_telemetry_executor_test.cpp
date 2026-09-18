#include "playback_telemetry_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <vector>

namespace {
struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        ++submissions;
        task();
        return true;
    }

    int submissions = 0;
};

struct CompletionSink {
    void push(PlaybackReportCompletion completion) { events.push_back(std::move(completion)); }

    std::vector<PlaybackReportCompletion> events;
};

struct FakeClient {
    ApiResult reportPlaybackStart(const JellyfinSession&, const JellyfinItem&, const PlaybackTarget&, int64_t ticks) {
        lastTicks = ticks;
        lastCall = "start";
        ApiResult result;
        result.ok = true;
        return result;
    }

    ApiResult reportPlaybackProgress(const JellyfinSession&, const JellyfinItem&, const PlaybackTarget&, int64_t ticks,
                                     bool paused) {
        lastTicks = ticks;
        lastPaused = paused;
        lastCall = "progress";
        ApiResult result;
        result.ok = !failProgress;
        result.error = failProgress ? "progress failed" : "";
        return result;
    }

    ApiResult reportPlaybackStopped(const JellyfinSession&, const JellyfinItem&, const PlaybackTarget&, int64_t ticks) {
        lastTicks = ticks;
        lastCall = "stop";
        ApiResult result;
        result.ok = true;
        return result;
    }

    std::string lastCall;
    int64_t lastTicks = 0;
    bool lastPaused = false;
    bool failProgress = false;
};
} // namespace

int main() {
    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    PlaybackTelemetryExecutor executor(client, tasks, completions);

    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";
    JellyfinItem item;
    item.id = "episode-1";
    PlaybackTarget target;
    target.url = "https://media.example/stream";

    executor.reportStart(session, item, target, 100);
    assert(tasks.submissions == 1);
    assert(client.lastCall == "start");
    assert(client.lastTicks == 100);
    assert(completions.events.size() == 1);
    assert(completions.events.back().kind == PlaybackReportKind::Start);
    assert(completions.events.back().server == session.server);
    assert(completions.events.back().userId == session.userId);
    assert(completions.events.back().itemId == item.id);
    assert(completions.events.back().result.ok);

    executor.reportProgress(session, item, target, 200, false);
    assert(tasks.submissions == 2);
    assert(client.lastCall == "progress");
    assert(!client.lastPaused);
    assert(completions.events.back().kind == PlaybackReportKind::Progress);
    assert(completions.events.back().result.ok);

    client.failProgress = true;
    executor.reportProgress(session, item, target, 300, true);
    assert(tasks.submissions == 3);
    assert(client.lastPaused);
    assert(completions.events.back().kind == PlaybackReportKind::PausedProgress);
    assert(!completions.events.back().result.ok);
    assert(completions.events.back().result.error == "progress failed");

    executor.reportStop(session, item, target, 400);
    assert(tasks.submissions == 4);
    assert(client.lastCall == "stop");
    assert(client.lastTicks == 400);
    assert(completions.events.back().kind == PlaybackReportKind::Stop);
    assert(completions.events.back().result.ok);

    return 0;
}
