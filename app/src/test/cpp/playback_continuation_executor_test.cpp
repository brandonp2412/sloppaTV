#include "playback_continuation_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<MediaSegmentsCompletion, NextEpisodeCompletion, PlaybackAdjacentCompletion>;

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

    ApiValueResult<std::vector<JellyfinItem>> getSeriesEpisodes(const JellyfinSession&, const std::string& seriesId,
                                                                int limit) {
        lastSeriesId = seriesId;
        lastEpisodeLimit = limit;
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.ok = seriesEpisodesOk;
        if (!seriesEpisodesOk) {
            result.error = "episodes failed";
            return result;
        }
        result.value = seriesEpisodes;
        return result;
    }

    bool mediaSegmentsOk = true;
    bool nextEpisodeOk = true;
    bool detailOk = true;
    bool seriesEpisodesOk = true;
    std::string nextEpisodeId = "episode-2";
    std::string lastSeriesId;
    std::string lastItemId;
    int lastEpisodeLimit = 0;
    int detailRequests = 0;
    std::vector<JellyfinItem> seriesEpisodes;
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

    JellyfinItem current;
    current.id = "episode-4";
    current.parentIndexNumber = 1;
    current.indexNumber = 4;
    JellyfinItem nextEpisodeItem;
    nextEpisodeItem.id = "episode-5";
    nextEpisodeItem.parentIndexNumber = 1;
    nextEpisodeItem.indexNumber = 5;
    JellyfinItem special;
    special.id = "special";
    special.parentIndexNumber = 0;
    special.indexNumber = 1;
    client.seriesEpisodes = {nextEpisodeItem, special, current};
    assert(executor.requestAdjacentEpisode(session, "series-1", current.id, 1, 4, 1));
    const auto& adjacent = std::get<PlaybackAdjacentCompletion>(completions.events.back());
    assert(adjacent.ok);
    assert(adjacent.direction == 1);
    assert(adjacent.currentItemId == current.id);
    assert(adjacent.item);
    assert(adjacent.item->id == nextEpisodeItem.id);
    assert(client.lastSeriesId == "series-1");
    assert(client.lastEpisodeLimit == 1000);

    client.seriesEpisodesOk = false;
    assert(executor.requestAdjacentEpisode(session, "series-1", current.id, 1, 4, -1));
    const auto& failedAdjacent = std::get<PlaybackAdjacentCompletion>(completions.events.back());
    assert(!failedAdjacent.ok);
    assert(failedAdjacent.direction == -1);
    assert(!failedAdjacent.item);

    tasks.accept = false;
    assert(!executor.requestMediaSegments(session, "episode-5"));
    assert(!executor.requestNextEpisode(session, "series-1", "episode-5"));
    assert(!executor.requestAdjacentEpisode(session, "series-1", "episode-5", 1, 5, 1));

    return 0;
}
