#include "seerr_app_coordinator.hpp"

#include <cassert>

namespace {
struct Async {
    int loads = 0;
    int requests = 0;
    bool accept = true;
    bool is4k = false;
    uint64_t generation = 0;
    SeerrMediaItem requested;
    std::optional<SeerrStorageTarget> storage;

    bool loadSeasons(SeerrEndpoint, int, bool quality, uint64_t token) {
        ++loads;
        is4k = quality;
        generation = token;
        return accept;
    }

    bool requestMedia(SeerrEndpoint, SeerrMediaItem item, std::optional<SeerrStorageTarget> target) {
        ++requests;
        requested = std::move(item);
        storage = std::move(target);
        return accept;
    }

    bool refreshStorage(SeerrEndpoint) { return true; }
};

struct Connection {};

struct Completions {
    ContentMutationFlow& mutations;

    SeerrCompletionHostEffects complete(const SeerrRequestCompletion&, const SeerrEndpoint&,
                                        SeerrRequestState::TimePoint) {
        mutations.finish();
        return {};
    }
};
} // namespace

int main() {
    SeerrDomainState domain;
    ContentMutationFlow mutations;
    DetailsFlow details;
    AppSettings settings;
    settings.seerrServer = "https://seerr.example.test";
    settings.seerrApiKey = "test";
    JellyfinSession session;
    bool loading = false;
    NavigationStack<Screen> navigation(Screen::Search);
    Screen screen = Screen::Search;
    Async async;
    Connection connection;
    SeerrRequestCoordinator request(domain, async);
    SeerrRefreshCoordinator refresh(domain, async);
    Completions completions{mutations};
    SeerrAppCoordinator app(domain, mutations, details, settings, session, loading, navigation, screen, async,
                            connection, request, refresh, completions);
    SeerrMediaItem series;
    series.id = "seerr:tv:10";
    series.mediaType = "tv";
    series.tmdbId = 10;
    series.requested = true; // Other seasons can already be requested.
    SeerrStorageTarget target;
    target.mediaType = "tv";
    target.serverId = 2;
    target.path = "/tv";
    target.is4k = true;
    (void)app.requestMedia(series, &target, true);
    assert(screen == Screen::SeerrSeasonPicker && async.loads == 1 && async.requests == 0);
    assert(async.is4k);
    const auto oldGeneration = async.generation;
    (void)app.handleSeasonPicker(ScreenNavigationKey::Back);
    assert(screen == Screen::Search);
    SeerrSeasonsCompletion response;
    response.endpoint = app.endpoint();
    response.generation = oldGeneration;
    response.result.ok = true;
    SeerrSeason season;
    season.number = 3;
    season.episodes = 8;
    response.result.value = {season};
    (void)app.complete(response);
    assert(domain.seasonPicker.seasons.empty());
    (void)app.requestMedia(series, &target, true);
    (void)app.complete(response); // Previous load cannot fill the new picker.
    assert(domain.seasonPicker.loading);
    response.generation = async.generation;
    (void)app.complete(response);
    assert(!domain.seasonPicker.loading);
    (void)app.handleSeasonPicker(ScreenNavigationKey::Activate);
    (void)app.handleSeasonPicker(ScreenNavigationKey::Up);
    (void)app.handleSeasonPicker(ScreenNavigationKey::Up);
    (void)app.handleSeasonPicker(ScreenNavigationKey::Activate);
    assert(async.requests == 1 && mutations.loading());
    assert(async.requested.selectedSeasons == std::vector<int>{3});
    assert(async.storage && async.storage->is4k && async.storage->path == "/tv");
    (void)app.handleSeasonPicker(ScreenNavigationKey::Activate);
    (void)app.requestMedia(series);
    assert(async.requests == 1); // Repeated remote presses cannot double-submit.
    SeerrRequestCompletion submitted;
    submitted.endpoint = app.endpoint();
    submitted.requestedItem = async.requested;
    submitted.result.ok = false;
    (void)app.complete(submitted);
    assert(screen == Screen::SeerrSeasonPicker && !mutations.loading());
    assert(domain.seasonPicker.selected() == std::vector<int>{3}); // Keep choices for retry.
    (void)app.handleSeasonPicker(ScreenNavigationKey::Activate);
    assert(async.requests == 2);
    submitted.result.ok = true;
    (void)app.complete(submitted);
    assert(screen == Screen::Search && !mutations.loading());

    async.accept = false;
    (void)app.requestMedia(series);
    assert(!domain.seasonPicker.loading && !domain.seasonPicker.error.empty());
    async.accept = true;
    (void)app.handleSeasonPicker(ScreenNavigationKey::Activate);
    assert(domain.seasonPicker.loading);
    response.generation = async.generation;
    response.endpoint.server = "https://old.example.test";
    (void)app.complete(response);
    assert(!domain.seasonPicker.loading && !domain.seasonPicker.error.empty());
}
