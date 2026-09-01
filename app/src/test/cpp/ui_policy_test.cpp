#include "ui_policy.hpp"

#include <cassert>

int main() {
    assert(playerControlCount() == 3);
    assert(playerControlKind(0) == PlayerControlKind::PlayPause);
    assert(playerControlKind(1) == PlayerControlKind::Audio);
    assert(playerControlKind(2) == PlayerControlKind::Subtitles);
    assert(mediaGridColumns() == 4);
    assert(isTopMediaGridSelection(0));
    assert(isTopMediaGridSelection(3));
    assert(!isTopMediaGridSelection(4));
    assert(mediaCardWidth() >= 400.0f);
    assert(mediaTitleScale() >= 2.1f);
    assert(uiTextScale(0) == 1.0f);
    assert(uiTextScale(1) > uiTextScale(0));
    assert(uiTextScale(2) > uiTextScale(1));
    assert(uiSafeAreaFraction(-1) == 0.0f);
    assert(uiSafeAreaFraction(4) == 0.04f);
    assert(uiSafeAreaFraction(99) == 0.06f);

    assert(homeImageKind(true, true, true) == ArtworkKind::Primary);
    assert(homeImageKind(false, true, true) == ArtworkKind::Thumb);
    assert(homeImageKind(false, false, true) == ArtworkKind::Backdrop);

    const ArtworkReference episodeArtwork = homeArtworkReference(
        "episode-id",
        "episode-tag",
        "series-id",
        "series-tag",
        true,
        "thumb-tag",
        "backdrop-tag"
    );
    assert(episodeArtwork.itemId == "series-id");
    assert(episodeArtwork.tag == "series-tag");
    assert(episodeArtwork.kind == ArtworkKind::Primary);

    const ArtworkReference thumbArtwork = homeArtworkReference(
        "movie-id", "", "", "", false, "thumb-tag", "backdrop-tag"
    );
    assert(thumbArtwork.itemId == "movie-id");
    assert(thumbArtwork.tag == "thumb-tag");
    assert(thumbArtwork.kind == ArtworkKind::Thumb);

    assert(subtitleLoadCompleted(false, false));
    assert(subtitleLoadCompleted(true, false));
    return 0;
}
