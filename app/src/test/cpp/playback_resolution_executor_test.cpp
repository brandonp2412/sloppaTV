#include "playback_resolution_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace {
enum class Origin {
    Home,
    Player,
};

using QueuedCompletion = QueuedPlaybackResolutionCompletion<Origin>;
using Completion = std::variant<QueuedCompletion, PlayerItemPlaybackCompletion, AutoplayPlaybackCompletion>;

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

    uint64_t activeGeneration = 7;
};

struct FakeClient {
    ApiValueResult<JellyfinItem> getItem(const JellyfinSession&, const std::string& itemId) {
        ++detailRequests;
        ApiValueResult<JellyfinItem> result;
        result.ok = detailOk;
        if (!detailOk) return result;
        result.value.id = itemId;
        result.value.name = "Detailed";
        result.value.audios = {
            {.index = 4, .channels = 2, .codec = "aac", .language = "eng", .title = "English", .isDefault = true},
        };
        return result;
    }

    ApiValueResult<PlaybackTarget> resolvePlayback(const JellyfinSession&, const JellyfinItem& item, int bitrate,
                                                   int channels, const PlaybackOverrides&, int audioStreamIndex,
                                                   int subtitleStreamIndex) {
        lastResolvedItemId = item.id;
        lastBitrate = bitrate;
        lastChannels = channels;
        lastAudioStreamIndex = audioStreamIndex;
        lastSubtitleStreamIndex = subtitleStreamIndex;
        ApiValueResult<PlaybackTarget> result;
        result.ok = resolveOk;
        if (!resolveOk) {
            result.error = "resolve failed";
            return result;
        }
        result.value.url = "https://media.example/" + item.id;
        return result;
    }

    bool detailOk = true;
    bool resolveOk = true;
    int detailRequests = 0;
    std::string lastResolvedItemId;
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

    PlaybackResolutionOptions options;
    options.maxStreamingBitrate = 42'000'000;
    options.maxAudioChannels = 6;
    options.audioLanguagePreference = std::string{"eng"};

    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    Epoch epoch;
    PlaybackResolutionExecutor executor(client, tasks, completions, epoch);

    JellyfinItem queued;
    queued.id = "queued";
    assert(executor.resolveQueued(session, queued, options, 7, Origin::Player, 3, 2, true));
    assert(tasks.submissions == 1);
    const auto& queuedCompletion = std::get<QueuedCompletion>(completions.events.back());
    assert(queuedCompletion.generation == 7);
    assert(queuedCompletion.originScreen == Origin::Player);
    assert(queuedCompletion.index == 3);
    assert(queuedCompletion.previousQueueIndex == 2);
    assert(queuedCompletion.replacingPlayer);
    assert(queuedCompletion.item.name == "Detailed");
    assert(queuedCompletion.result.ok);
    assert(queuedCompletion.result.value.url == "https://media.example/queued");
    assert(client.lastBitrate == 42'000'000);
    assert(client.lastChannels == 6);
    assert(client.lastAudioStreamIndex == 4);

    JellyfinItem playerItem;
    playerItem.id = "player";
    assert(executor.resolvePlayerItem(session, playerItem, options, 7));
    const auto& playerCompletion = std::get<PlayerItemPlaybackCompletion>(completions.events.back());
    assert(playerCompletion.item.id == "player");
    assert(playerCompletion.result.ok);

    JellyfinItem autoplay;
    autoplay.id = "autoplay";
    assert(executor.resolveAutoplay(session, autoplay, options, 7, 9));
    const auto& autoplayCompletion = std::get<AutoplayPlaybackCompletion>(completions.events.back());
    assert(autoplayCompletion.queuedNextIndex == 9);
    assert(autoplayCompletion.item.id == "autoplay");

    const size_t completionCount = completions.events.size();
    epoch.activeGeneration = 8;
    JellyfinItem stale;
    stale.id = "stale";
    assert(executor.resolvePlayerItem(session, stale, options, 7));
    assert(completions.events.size() == completionCount);

    epoch.activeGeneration = 8;
    client.resolveOk = false;
    JellyfinItem failed;
    failed.id = "failed";
    assert(executor.resolvePlayerItem(session, failed, options, 8));
    const auto& failedCompletion = std::get<PlayerItemPlaybackCompletion>(completions.events.back());
    assert(!failedCompletion.result.ok);
    assert(failedCompletion.result.error == "resolve failed");

    tasks.accept = false;
    JellyfinItem rejected;
    rejected.id = "rejected";
    assert(!executor.resolvePlayerItem(session, rejected, options, 8));
    assert(!executor.resolveAutoplay(session, rejected, options, 8, 10));
    assert(!executor.resolveQueued(session, rejected, options, 8, Origin::Home, 0, -1, false));

    return 0;
}
