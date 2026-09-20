#include "seerr_jellyfin_adapter.hpp"

#include <cassert>

int main() {
    SeerrMediaItem media;
    media.id = seerrMediaId("tv", 300);
    media.name = "Example series";
    media.mediaType = "tv";
    media.tmdbId = 300;
    media.overview = "Overview";
    media.posterUrl = "https://images.example/poster.jpg";
    media.backdropUrl = "https://images.example/backdrop.jpg";
    media.jellyfinId = "jellyfin-300";
    media.status = "Queued";
    media.progressLabel = "S01E02 · 40%";
    media.progressEta = "12 min left";
    media.productionYear = 2026;
    media.progressPercent = 40;
    media.requestId = 42;
    media.mediaStatus = 4;
    media.requested = true;

    assert(media.valid());
    assert(media.television());
    assert(seerrMediaId("movie", 10) == "seerr:movie:10");
    assert(seerrMediaId("person", 10).empty());
    assert(seerrMediaId("tv", 0).empty());

    const JellyfinItem item = jellyfinItemFromSeerrMedia(media);
    assert(isSeerrItem(item));
    assert(item.id == media.id);
    assert(item.type == "Series");
    assert(item.tmdbId == "300");
    assert(item.externalMediaType == "tv");
    assert(item.externalPosterUrl == media.posterUrl);
    assert(item.externalBackdropUrl == media.backdropUrl);
    assert(item.externalJellyfinId == media.jellyfinId);
    assert(item.externalProgressPercent == 40);
    assert(item.externalRequestId == 42);
    assert(item.externalRequested);

    const auto deleteRequest = seerrDeleteRequestFromJellyfinItem(item);
    assert(deleteRequest.has_value());
    assert(deleteRequest->itemId == media.id);
    assert(deleteRequest->requestId == 42);

    const auto restored = seerrMediaFromJellyfinItem(item);
    assert(restored.has_value());
    assert(restored->id == media.id);
    assert(restored->mediaType == media.mediaType);
    assert(restored->tmdbId == media.tmdbId);
    assert(restored->requestId == media.requestId);
    assert(restored->status == media.status);
    assert(restored->progressLabel == media.progressLabel);

    JellyfinItem ordinary;
    ordinary.id = "movie-1";
    ordinary.tmdbId = "10";
    assert(!seerrDeleteRequestFromJellyfinItem(ordinary).has_value());
    assert(!seerrMediaFromJellyfinItem(ordinary).has_value());

    JellyfinItem invalid = item;
    invalid.tmdbId = "not-a-number";
    assert(seerrDeleteRequestFromJellyfinItem(invalid).has_value());
    assert(!seerrMediaFromJellyfinItem(invalid).has_value());

    invalid.tmdbId = "300junk";
    assert(!seerrMediaFromJellyfinItem(invalid).has_value());
    invalid.tmdbId = " 300";
    assert(!seerrMediaFromJellyfinItem(invalid).has_value());
    invalid.tmdbId = "0";
    assert(!seerrMediaFromJellyfinItem(invalid).has_value());
    invalid.tmdbId = "-300";
    assert(!seerrMediaFromJellyfinItem(invalid).has_value());
    invalid.tmdbId = "999999999999999999999999";
    assert(!seerrMediaFromJellyfinItem(invalid).has_value());

    JellyfinItem pendingId = item;
    pendingId.externalRequestId = 0;
    assert(!seerrDeleteRequestFromJellyfinItem(pendingId).has_value());
    return 0;
}
