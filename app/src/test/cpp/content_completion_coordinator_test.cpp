#include "content_completion_coordinator.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
struct FakeDetailsAsync {
    bool accepted = true;
    int calls = 0;
    uint64_t generation = 0;
    std::string itemId;
    std::string seriesId;

    bool loadSeriesContext(JellyfinSession, EpisodeSeriesContextRequest request, RequestEpoch::Token token) {
        ++calls;
        generation = token.value();
        itemId = std::move(request.itemId);
        seriesId = std::move(request.seriesId);
        return accepted;
    }
};

struct Fixture {
    RequestEpoch contentEpoch;
    RequestEpoch sessionEpoch;
    Screen screen = Screen::Details;
    bool loading = false;
    std::string error;
    DetailsFlow details;
    ContentMutationFlow mutations;
    JellyfinSession session;
    std::unordered_set<std::string> hiddenItems;
    HomeVisibility homeVisibility{session, hiddenItems};
    JellyfinHomeData home;
    HomeScreenState homeState;
    BrowseScreenState browseState;
    std::vector<SeerrMediaItem> seerrResults;
    SearchScreenState searchState{seerrResults};
    PlaybackQueueState queueState;
    FakeDetailsAsync detailsAsync;
    SimilarPrefetchController similarPrefetch;
    ContentCompletionCoordinator<FakeDetailsAsync> coordinator{
        contentEpoch, sessionEpoch, screen,      loading,     error,      details,      mutations,      homeVisibility,
        home,         homeState,    browseState, searchState, queueState, detailsAsync, similarPrefetch};

    EpisodeSeriesContextRequestCompletion completion(uint64_t generation) {
        JellyfinItem episode;
        episode.id = "episode-1";
        episode.type = "Episode";
        episode.seriesId = "series-1";
        details.item() = episode;

        EpisodeSeriesContextRequestCompletion result;
        result.generation = generation;
        result.request.itemId = episode.id;
        result.request.seriesId = episode.seriesId;
        return result;
    }
};
} // namespace

int main() {
    {
        Fixture fixture;
        const uint64_t generation = fixture.contentEpoch.begin();
        auto completion = fixture.completion(generation);

        const auto effects = fixture.coordinator.complete(completion);

        assert(!effects.closeDeletedItem);
        assert(!effects.notice);
        assert(fixture.error.empty());
        assert(fixture.detailsAsync.calls == 1);
        assert(fixture.detailsAsync.generation == generation);
        assert(fixture.detailsAsync.itemId == "episode-1");
        assert(fixture.detailsAsync.seriesId == "series-1");
    }

    {
        Fixture fixture;
        fixture.detailsAsync.accepted = false;
        const uint64_t generation = fixture.contentEpoch.begin();
        auto completion = fixture.completion(generation);

        static_cast<void>(fixture.coordinator.complete(completion));

        assert(fixture.detailsAsync.calls == 1);
        assert(fixture.error == "EPISODE CONTEXT COULD NOT BE STARTED");
    }

    {
        Fixture fixture;
        const uint64_t staleGeneration = fixture.contentEpoch.begin();
        static_cast<void>(fixture.contentEpoch.begin());
        auto completion = fixture.completion(staleGeneration);

        static_cast<void>(fixture.coordinator.complete(completion));

        assert(fixture.detailsAsync.calls == 0);
        assert(fixture.error.empty());
    }

    {
        Fixture fixture;
        const uint64_t generation = fixture.contentEpoch.begin();
        auto completion = fixture.completion(generation);
        fixture.details.item().id = "different-episode";

        static_cast<void>(fixture.coordinator.complete(completion));

        assert(fixture.detailsAsync.calls == 0);
        assert(fixture.error.empty());
    }

    return 0;
}
