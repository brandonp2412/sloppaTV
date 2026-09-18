#include "playback_continuation_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<MediaSegmentsCompletion, NextEpisodeCompletion>;

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
    ApiValueResult<std::vector<JellyfinMediaSegment>> getMediaSegments(const JellyfinSession&,
                                                                       const std::string& itemId) {
        lastItemId = itemId;
        ApiValueResult<std::vector<JellyfinMediaSegment>> result;
        result.ok = mediaSegmentsOk;
        if (!mediaSegmentsOk) {
            result.error = "segments failed";
            return result;
        }
        JellyfinMediaSegment segment;
        segment.type = "Intro";
        segment.startTicks = 100;
        segment.endTicks = 200;
        result.value = {segment};
        return result;
    }

    ApiValueResult<JellyfinItem> getFollowingEpisodeForSeries(const JellyfinSession&, const std::string& seriesId,
                                                              const std::string& currentItemId) {
        lastSeriesId = seriesId;
        lastItemId = currentItemId;
        ApiValueResult<JellyfinItem> result;
        result.ok = nextEpisodeOk;
        if (!nextEpisodeOk) {
            result.error = "next failed";
            return result;
        }
        result.value.id = nextEpisodeId;
        result.value.name = "Summary next";
        return result;
    }

    ApiValueResult<JellyfinItem> getItem(const JellyfinSession&, const std::string& itemId) {
        ++detailRequests;
        ApiValueResult<JellyfinItem> result;
        result.ok = detailOk;
        if (!detailOk) return result;
        result.value.id = itemId;
        result.value.name = "Detailed next";
        return result;
    }

    bool mediaSegmentsOk = true;
    bool nextEpisodeOk = true;
    bool detailOk = true;
    std::string nextEpisodeId = "episode-2";
    std::string lastSeriesId;
    std::string lastItemId;
    int detailRequests = 0;
};
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    PlaybackContinuationExecutor executor(client, tasks, completions);

    assert(executor.requestMediaSegments(session, "episode-1"));
    assert(tasks.submissions == 1);
    const auto& segments = std::get<MediaSegmentsCompletion>(completions.events.back());
    assert(segments.itemId == "episode-1");
    assert(segments.ok);
    assert(segments.segments.size() == 1);
    assert(segments.segments.front().type == "Intro");

    client.mediaSegmentsOk = false;
    assert(executor.requestMediaSegments(session, "episode-1"));
    const auto& failedSegments = std::get<MediaSegmentsCompletion>(completions.events.back());
    assert(!failedSegments.ok);
    assert(failedSegments.segments.empty());
    assert(failedSegments.error == "segments failed");

    assert(executor.requestNextEpisode(session, "series-1", "episode-1"));
    assert(client.lastSeriesId == "series-1");
    assert(client.lastItemId == "episode-1");
    assert(client.detailRequests == 1);
    const auto& next = std::get<NextEpisodeCompletion>(completions.events.back());
    assert(next.currentItemId == "episode-1");
    assert(next.ok);
    assert(next.item.id == "episode-2");
    assert(next.item.name == "Detailed next");

    client.detailOk = false;
    client.nextEpisodeId = "episode-3";
    assert(executor.requestNextEpisode(session, "series-1", "episode-2"));
    const auto& summaryNext = std::get<NextEpisodeCompletion>(completions.events.back());
    assert(summaryNext.ok);
    assert(summaryNext.item.id == "episode-3");
    assert(summaryNext.item.name == "Summary next");

    client.nextEpisodeOk = false;
    assert(executor.requestNextEpisode(session, "series-1", "episode-3"));
    const auto& failedNext = std::get<NextEpisodeCompletion>(completions.events.back());
    assert(!failedNext.ok);
    assert(failedNext.currentItemId == "episode-3");
    assert(failedNext.error == "next failed");

    client.nextEpisodeOk = true;
    client.nextEpisodeId = "episode-4";
    const size_t completionCount = completions.events.size();
    assert(executor.requestNextEpisode(session, "series-1", "episode-4"));
    assert(completions.events.size() == completionCount);

    tasks.accept = false;
    assert(!executor.requestMediaSegments(session, "episode-5"));
    assert(!executor.requestNextEpisode(session, "series-1", "episode-5"));

    return 0;
}
