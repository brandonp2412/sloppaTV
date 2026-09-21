#include "similar_prefetch_controller.hpp"

#include <cassert>
#include <chrono>
#include <utility>

int main() {
    using namespace std::chrono_literals;

    SimilarPrefetchController controller;
    JellyfinSession session;
    session.server = "https://example.invalid";
    session.userId = "user-1";
    session.token = "token";

    JellyfinItem item;
    item.id = "movie-1";
    item.name = "Movie";
    item.type = "Movie";

    const auto now = SimilarPrefetchController::Clock::now();
    assert(controller.schedule(session, item, now));
    assert(controller.dueDeadline() == now + 180ms);
    assert(!controller.takeDue(session, true, now + 179ms));

    auto work = controller.takeDue(session, true, now + 180ms);
    assert(work);
    assert(work->itemId == item.id);
    assert(work->session.userId == session.userId);

    assert(!controller.schedule(session, item, now + 181ms));
    controller.submissionFailed(work->key);
    assert(controller.schedule(session, item, now + 240ms));

    work = controller.takeDue(session, true, now + 420ms);
    assert(work);

    JellyfinItem similar;
    similar.id = "movie-2";
    similar.name = "Similar";
    similar.type = "Movie";
    SimilarPrefetchCompletion completion;
    completion.session = session;
    completion.itemId = item.id;
    completion.key = work->key;
    completion.detail.ok = true;
    completion.detail.value = item;
    completion.result.ok = true;
    completion.result.value.push_back(similar);

    auto completedItems = controller.complete(completion);
    assert(completedItems);
    assert(completedItems->size() == 1);
    assert(completedItems->front().id == similar.id);

    auto cachedDetail = controller.cachedDetail(session, item.id);
    assert(cachedDetail);
    assert(cachedDetail->name == item.name);

    auto cached = controller.cached(session, item.id);
    assert(cached);
    assert(cached->front().id == similar.id);
    assert(!controller.schedule(session, item, now + 1s));

    JellyfinItem series;
    series.id = "series-1";
    series.name = "Series";
    series.type = "Series";
    controller.rememberDetail(session, series);
    controller.rememberSimilar(session, series.id, {similar});

    JellyfinItem season;
    season.id = "season-1";
    season.name = "Season 1";
    season.type = "Season";
    season.seriesId = series.id;
    season.indexNumber = 1;
    controller.rememberSeasons(session, series.id, {season});

    JellyfinItem episode;
    episode.id = "episode-1";
    episode.name = "Episode 1";
    episode.type = "Episode";
    episode.seriesId = series.id;
    episode.parentIndexNumber = 1;
    episode.indexNumber = 1;
    controller.rememberEpisodes(session, series.id, season.id, {episode});
    controller.rememberDetail(session, episode);
    controller.rememberSimilar(session, episode.id, {similar});

    auto cachedSeasons = controller.cachedSeasons(session, series.id);
    assert(cachedSeasons);
    assert(cachedSeasons->size() == 1);
    assert(cachedSeasons->front().id == season.id);

    auto cachedEpisodes = controller.cachedEpisodes(session, series.id, season.id);
    assert(cachedEpisodes);
    assert(cachedEpisodes->size() == 1);
    assert(cachedEpisodes->front().id == episode.id);

    assert(!controller.schedule(session, series, now + 1500ms));
    assert(!controller.schedule(session, season, now + 1600ms));
    assert(!controller.schedule(session, episode, now + 1700ms));

    JellyfinItem folder;
    folder.id = "folder-1";
    folder.type = "Folder";
    assert(!controller.schedule(session, folder, now + 2s));

    JellyfinItem other;
    other.id = "movie-3";
    other.type = "Movie";
    assert(controller.schedule(session, other, now + 3s));
    assert(!controller.takeDue(session, false, now + 4s));
    assert(controller.dueDeadline() == SimilarPrefetchController::Clock::time_point{});

    return 0;
}
