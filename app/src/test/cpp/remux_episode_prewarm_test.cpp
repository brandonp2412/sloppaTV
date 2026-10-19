#include "remux_episode_prewarm.hpp"

#include <cassert>

int main() {
    HttpResponse media;
    media.status = 206;
    media.body = "\x1a\x45\xdf\xa3";
    assert(isUsableRemuxPrefetchRange(media));
    media.status = 200;
    assert(!isUsableRemuxPrefetchRange(media));
    media.status = 206;
    media.body.clear();
    assert(!isUsableRemuxPrefetchRange(media));
    media.body.assign(65537, 'x');
    assert(!isUsableRemuxPrefetchRange(media));
    for (const char* document : {"<!DOCTYPE html><html>", "  <html>login</html>", "{\"error\":\"expired\"}",
                                 "[\"unauthorized\"]", "\xef\xbb\xbf   {\"message\":\"bad\"}"}) {
        media.body = document;
        assert(!isUsableRemuxPrefetchRange(media));
    }
    media.body = std::string("\x00\x00\x00\x18", 4) + "ftypisom";
    assert(isUsableRemuxPrefetchRange(media));
    RemuxEpisodePrewarmState warmup;
    JellyfinItem next;
    next.id = "episode-2";
    next.type = "Episode";
    warmup.remember("episode-1", next);

    assert(!warmup.takeDue("episode-1", 4'000, 100'000, true, true));
    assert(!warmup.takeDue("episode-1", 75'000, 100'000, true, false));
    assert(!warmup.takeDue("episode-1", 75'000, 100'000, false, true));
    assert(!warmup.takeDue("episode-1", 100'000, 100'000, true, true));
    assert(!warmup.takeDue("episode-1", 4'000, 0, true, true));

    auto ready = warmup.takeDue("episode-1", 5'000, 0, true, true);
    assert(ready && ready->id == "episode-2"); // Can prefetch before duration is known.
    warmup.remember("episode-1", next);
    assert(!warmup.takeDue("episode-1", 85'000, 100'000, true, true));

    next.id = "episode-3";
    warmup.remember("episode-2", next);
    assert(!warmup.takeDue("episode-1", 90'000, 100'000, true, true));
    assert(!warmup.takeDue("episode-2", 90'000, 100'000, true, true));
    warmup.remember("episode-2", next);
    ready = warmup.takeDue("episode-2", 90'000, 100'000, true, true);
    assert(ready && ready->id == "episode-3");
    assert(!warmup.takeDue("episode-2", 99'000, 100'000, true, true));

    JellyfinItem current;
    current.id = "episode-1";
    current.type = "Episode";
    current.seriesId = "series-1";
    JellyfinItem queued = next;
    queued.id = "episode-2";
    queued.seriesId = current.seriesId;
    PlaybackQueueState queue;
    queue.replace({current, queued}, 0);
    assert(remuxQueuedNextEpisode(current, queue)->id == "episode-2");
    queue.setRepeatMode(QueueRepeatMode::One);
    assert(remuxQueuedNextEpisode(current, queue) == nullptr);
    queue.setRepeatMode(QueueRepeatMode::Off);
    queued.seriesId = "different-series";
    queue.replace({current, queued}, 0);
    assert(remuxQueuedNextEpisode(current, queue) == nullptr);
    queued.seriesId = current.seriesId;
    queued.type = "Movie";
    queue.replace({current, queued}, 0);
    assert(remuxQueuedNextEpisode(current, queue) == nullptr);

    warmup.clear();
    warmup.remember("episode-3", next); // Same item is never prewarmed as its own successor.
    assert(!warmup.takeDue("episode-3", 90'000, 100'000, true, true));
    return 0;
}
