#include "series_playback_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    void push(SeriesPlayAllCompletion completion) { events.push_back(std::move(completion)); }

    std::vector<SeriesPlayAllCompletion> events;
};

struct Epoch {
    bool active(uint64_t generation) const { return generation == activeGeneration; }

    uint64_t activeGeneration = 4;
};

struct FakeClient {
    ApiValueResult<std::vector<JellyfinItem>> getSeriesEpisodes(const JellyfinSession&, const std::string& seriesId,
                                                                int limit) {
        lastSeriesId = seriesId;
        lastEpisodeLimit = limit;
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.ok = episodesOk;
        if (!episodesOk) {
            result.error = "episodes failed";
            return result;
        }
        result.value = episodes;
        return result;
    }

    ApiValueResult<JellyfinItem> getItem(const JellyfinSession&, const std::string& itemId) {
        detailRequests.push_back(itemId);
        ApiValueResult<JellyfinItem> result;
        result.ok = !detailFailures.contains(itemId);
        if (!result.ok) return result;

        const auto found = detailedItems.find(itemId);
        if (found != detailedItems.end()) {
            result.value = found->second;
            return result;
        }
        result.value.id = itemId;
        result.value.name = "Detailed " + itemId;
        return result;
    }

    bool isStaticStreamAvailable(const JellyfinSession&, const JellyfinItem& item) {
        availabilityChecks.push_back(item.id);
        return availableItems.contains(item.id);
    }

    ApiValueResult<PlaybackTarget> resolvePlayback(const JellyfinSession&, const JellyfinItem& item, int bitrate,
                                                   int channels, const PlaybackOverrides&) {
        resolvedItemId = item.id;
        resolvedBitrate = bitrate;
        resolvedChannels = channels;
        ApiValueResult<PlaybackTarget> result;
        result.ok = resolveOk;
        if (!resolveOk) {
            result.error = "resolve failed";
            return result;
        }
        result.value.url = "https://media.example/" + item.id;
        return result;
    }

    bool episodesOk = true;
    bool resolveOk = true;
    std::vector<JellyfinItem> episodes;
    std::unordered_map<std::string, JellyfinItem> detailedItems;
    std::unordered_set<std::string> detailFailures;
    std::unordered_set<std::string> availableItems;
    std::vector<std::string> detailRequests;
    std::vector<std::string> availabilityChecks;
    std::string lastSeriesId;
    int lastEpisodeLimit = 0;
    std::string resolvedItemId;
    int resolvedBitrate = 0;
    int resolvedChannels = 0;
};

JellyfinItem episode(std::string id, int season, int number, std::string mediaSourceId = {},
                     std::string container = {}) {
    JellyfinItem item;
    item.id = std::move(id);
    item.type = "Episode";
    item.parentIndexNumber = season;
    item.indexNumber = number;
    item.mediaSourceId = std::move(mediaSourceId);
    item.container = std::move(container);
    return item;
}
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    JellyfinItem series;
    series.id = "series-1";
    series.type = "Series";

    SeriesPlayAllOptions options;
    options.maxStreamingBitrate = 50'000'000;
    options.maxAudioChannels = 6;

    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    Epoch epoch;
    std::vector<SeriesEpisodeSlot> warnings;
    SeriesPlaybackExecutor executor(client, tasks, completions, epoch,
                                    [&](const SeriesEpisodeSlot& slot) { warnings.push_back(slot); });

    client.episodesOk = false;
    assert(executor.playAll(session, series, options, 4));
    assert(completions.events.back().error == "PLAY ALL: episodes failed");
    assert(completions.events.back().series.id == series.id);
    assert(client.lastSeriesId == series.id);
    assert(client.lastEpisodeLimit == 1000);

    client.episodesOk = true;
    client.episodes.clear();
    assert(executor.playAll(session, series, options, 4));
    assert(completions.events.back().error == "PLAY ALL: NO EPISODES");

    JellyfinItem unavailable = episode("s1e1-a", 1, 1);
    JellyfinItem available = episode("s1e1-b", 1, 1, "source-b", "mkv");
    JellyfinItem second = episode("s1e2", 1, 2, "source-2", "mkv");
    JellyfinItem special = episode("special", 0, 1, "special-source", "mkv");
    client.episodes = {second, special, unavailable, available};
    JellyfinItem unavailableDetailed = unavailable;
    unavailableDetailed.mediaSourceId = "source-a";
    unavailableDetailed.container = "mkv";
    client.detailedItems[unavailable.id] = unavailableDetailed;
    client.availableItems.insert(available.id);
    JellyfinItem firstDetailed = available;
    firstDetailed.name = "Detailed chosen episode";
    client.detailedItems[available.id] = firstDetailed;

    assert(executor.playAll(session, series, options, 4));
    const auto& success = completions.events.back();
    assert(success.error.empty());
    assert(success.episodes.size() == 2);
    assert(success.episodes[0].id == available.id);
    assert(success.episodes[1].id == second.id);
    assert(success.first.id == available.id);
    assert(success.first.name == "Detailed chosen episode");
    assert(success.target);
    assert(success.target->url == "https://media.example/s1e1-b");
    assert(client.resolvedItemId == available.id);
    assert(client.resolvedBitrate == 50'000'000);
    assert(client.resolvedChannels == 6);
    assert((client.availabilityChecks == std::vector<std::string>{"s1e1-a", "s1e1-b"}));
    assert(warnings.empty());

    client.availabilityChecks.clear();
    client.availableItems.clear();
    client.episodes = {unavailable, available};
    warnings.clear();
    assert(executor.playAll(session, series, options, 4));
    assert(warnings.size() == 1);
    assert(warnings[0].season == 1);
    assert(warnings[0].episode == 1);
    assert(completions.events.back().episodes.front().id == unavailable.id);

    client.resolveOk = false;
    client.episodes = {second};
    assert(executor.playAll(session, series, options, 4));
    assert(!completions.events.back().target);
    assert(completions.events.back().error == "PLAY ALL: resolve failed");
    client.resolveOk = true;

    const size_t completionCount = completions.events.size();
    epoch.activeGeneration = 5;
    client.episodes = {second};
    assert(executor.playAll(session, series, options, 4));
    assert(completions.events.size() == completionCount);

    tasks.accept = false;
    assert(!executor.playAll(session, series, options, 5));

    return 0;
}
