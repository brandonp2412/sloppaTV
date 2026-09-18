#include "playback_stream_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<StreamRestartCompletion, FallbackPlaybackCompletion>;

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

struct Epoch {
    bool active(uint64_t generation) const { return generation == activeGeneration; }

    uint64_t activeGeneration = 11;
};

struct FakeClient {
    ApiResult reportPlaybackStopped(const JellyfinSession&, const JellyfinItem& item, const PlaybackTarget&,
                                    int64_t ticks) {
        calls.push_back("stop:" + item.id);
        stopTicks = ticks;
        ApiResult result;
        result.ok = reportOk;
        result.error = reportOk ? "" : "stop failed";
        return result;
    }

    ApiValueResult<PlaybackTarget> resolvePlayback(const JellyfinSession&, const JellyfinItem& item, int bitrate,
                                                   int channels, const PlaybackOverrides&, int audioStreamIndex,
                                                   int subtitleStreamIndex) {
        calls.push_back("resolve:" + item.id);
        lastBitrate = bitrate;
        lastChannels = channels;
        lastAudioStreamIndex = audioStreamIndex;
        lastSubtitleStreamIndex = subtitleStreamIndex;
        ApiValueResult<PlaybackTarget> result;
        result.ok = resolveOk;
        result.error = resolveOk ? "" : "resolve failed";
        if (resolveOk) result.value.url = "https://media.example/" + item.id;
        return result;
    }

    bool reportOk = true;
    bool resolveOk = true;
    std::vector<std::string> calls;
    int64_t stopTicks = 0;
    int lastBitrate = 0;
    int lastChannels = 0;
    int lastAudioStreamIndex = -1;
    int lastSubtitleStreamIndex = -1;
};
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    JellyfinItem item;
    item.id = "episode-1";
    item.positionTicks = 1'250'000;

    PlaybackTarget previous;
    previous.url = "https://media.example/old";

    PlaybackStreamResolutionOptions options;
    options.maxStreamingBitrate = 45'000'000;
    options.maxAudioChannels = 6;
    options.audioStreamIndex = 3;
    options.subtitleStreamIndex = 7;

    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    Epoch epoch;
    std::vector<std::string> reportFailures;
    PlaybackStreamExecutor executor(
        client, tasks, completions, epoch,
        [&](const char* stage, const std::string& itemId, const ApiResult& result) {
            if (!result.ok) reportFailures.push_back(std::string{stage} + ":" + itemId + ":" + result.error);
        });

    assert(executor.restart(session, item, previous, true, options, true, 11));
    assert((client.calls == std::vector<std::string>{"stop:episode-1", "resolve:episode-1"}));
    assert(client.stopTicks == item.positionTicks);
    assert(client.lastBitrate == 45'000'000);
    assert(client.lastChannels == 6);
    assert(client.lastAudioStreamIndex == 3);
    assert(client.lastSubtitleStreamIndex == 7);
    const auto& restart = std::get<StreamRestartCompletion>(completions.events.back());
    assert(restart.generation == 11);
    assert(restart.audioStreamIndex == 3);
    assert(restart.wasPaused);
    assert(restart.item.id == item.id);
    assert(restart.result.ok);

    client.calls.clear();
    assert(executor.restart(session, item, previous, false, options, false, 11));
    assert((client.calls == std::vector<std::string>{"resolve:episode-1"}));

    client.calls.clear();
    client.reportOk = false;
    assert(executor.resolveFallback(session, item, previous, true, 2'500'000, options, 11));
    assert((client.calls == std::vector<std::string>{"stop:episode-1", "resolve:episode-1"}));
    assert(client.stopTicks == 2'500'000);
    assert(reportFailures.size() == 1);
    assert(reportFailures.back() == "stop-after-failure:episode-1:stop failed");
    const auto& fallback = std::get<FallbackPlaybackCompletion>(completions.events.back());
    assert(fallback.generation == 11);
    assert(fallback.audioStreamIndex == 3);
    assert(fallback.item.id == item.id);
    assert(fallback.result.ok);

    client.calls.clear();
    client.reportOk = true;
    const size_t completionCount = completions.events.size();
    epoch.activeGeneration = 12;
    assert(executor.resolveFallback(session, item, previous, true, 3'000'000, options, 11));
    assert((client.calls == std::vector<std::string>{"stop:episode-1", "resolve:episode-1"}));
    assert(completions.events.size() == completionCount);

    client.calls.clear();
    assert(executor.reportPreviousStop(session, item, previous, 4'000'000, "stop-after-failure"));
    assert((client.calls == std::vector<std::string>{"stop:episode-1"}));
    assert(client.stopTicks == 4'000'000);

    tasks.accept = false;
    assert(!executor.restart(session, item, previous, true, options, false, 12));
    assert(!executor.resolveFallback(session, item, previous, true, 0, options, 12));
    assert(!executor.reportPreviousStop(session, item, previous, 0, "stop"));

    return 0;
}
