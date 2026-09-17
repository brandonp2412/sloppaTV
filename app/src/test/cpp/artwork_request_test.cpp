#include "artwork_request.hpp"

#include <cassert>

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example";
    session.userId = "user-1";

    JellyfinItem item;
    item.id = "movie-1";
    item.name = "Large object data should not be copied into artwork requests";
    item.type = "Movie";
    item.imageTag = "primary-tag";
    item.thumbTag = "thumb-tag";
    item.backdropTag = "backdrop-tag";
    item.backdropItemId = "backdrop-owner";
    item.logoTag = "logo-tag";
    item.logoItemId = "logo-owner";

    assert(profileArtworkKey(session) == "https://jellyfin.example:user:user-1");
    assert(posterArtworkKey(session, item, false) ==
           "https://jellyfin.example:user:user-1:movie-1:primary:primary-tag");
    assert(backdropArtworkKey(session, item, 2) ==
           "https://jellyfin.example:user:user-1:backdrop-owner:backdrop:backdrop-tag:mode:2");
    assert(logoArtworkKey(session, item) == "https://jellyfin.example:user:user-1:logo-owner:logo:logo-tag");
    assert(homeArtworkKey(session, item, false) ==
           "https://jellyfin.example:user:user-1:movie-1:home:v5-480x270:1:thumb-tag");

    const PosterArtworkRequest poster = posterArtworkRequest(session, item, false);
    assert(poster.itemId == item.id);
    assert(poster.imageTag == item.imageTag);
    assert(!poster.external);
    assert(poster.externalUrl.empty());

    const HomeArtworkRequest home = homeArtworkRequest(session, item, false);
    assert(home.itemId == item.id);
    assert(home.itemType == item.type);
    assert(home.artwork.itemId == item.id);
    assert(home.artwork.tag == item.thumbTag);
    assert(home.artwork.kind == ArtworkKind::Thumb);
    assert(home.key == homeArtworkKey(session, home.artwork));
    assert(!home.external);
    assert(home.externalUrl.empty());

    const BackdropArtworkRequest backdrop = backdropArtworkRequest(session, item, 2);
    assert(backdrop.artworkItemId == item.backdropItemId);
    assert(backdrop.artworkTag == item.backdropTag);

    const LogoArtworkRequest logo = logoArtworkRequest(session, item);
    assert(logo.artworkItemId == item.logoItemId);
    assert(logo.artworkTag == item.logoTag);

    item.externalPosterUrl = "https://images.example/poster.jpg";
    item.externalBackdropUrl = "https://images.example/backdrop.jpg";
    assert(posterArtworkKey(session, item, true) == "seerr:poster:https://images.example/poster.jpg");
    assert(homeArtworkKey(session, item, true) == "seerr:home:https://images.example/backdrop.jpg");
    const PosterArtworkRequest externalPoster = posterArtworkRequest(session, item, true);
    assert(externalPoster.external);
    assert(externalPoster.externalUrl == item.externalPosterUrl);
    const HomeArtworkRequest externalHome = homeArtworkRequest(session, item, true);
    assert(externalHome.external);
    assert(externalHome.externalUrl == item.externalBackdropUrl);

    item.externalBackdropUrl.clear();
    const HomeArtworkRequest externalHomeFallback = homeArtworkRequest(session, item, true);
    assert(externalHomeFallback.externalUrl == item.externalPosterUrl);
    assert(externalHomeFallback.key == "seerr:home:https://images.example/poster.jpg");
    return 0;
}
