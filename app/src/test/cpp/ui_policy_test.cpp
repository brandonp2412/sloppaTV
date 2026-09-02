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
    assert(detailActionTextScale(5) == 1.8f);
    assert(detailActionTextScale(10) == 1.8f);
    assert(detailActionTextScale(11) == 1.6f);
    assert(detailActionTextScale(13) == 1.6f);
    assert(uiTextScale(0) > 1.0f);
    assert(uiTextScale(1) > uiTextScale(0));
    assert(uiTextScale(2) > uiTextScale(1));
    assert(uiSafeAreaFraction(-1) == 0.0f);
    assert(uiSafeAreaFraction(4) == 0.04f);
    assert(uiSafeAreaFraction(99) == 0.06f);
    assert(wrappedIndex(0, -1, 10) == 9);
    assert(wrappedIndex(9, 1, 10) == 0);
    assert(wrappedIndex(2, 1, 5) == 3);
    assert(wrappedIndex(4, 1, 5) == 0);
    assert(containsCaseInsensitive("AUDIO OUTPUT", "audio"));
    assert(containsCaseInsensitive("PLAYBACK BUFFER", "Buffer"));
    assert(!containsCaseInsensitive("SUBTITLE SIZE", "audio"));
    assert(containsCaseInsensitive("CLOCK", ""));

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
        "backdrop-tag",
        "episode-id"
    );
    assert(episodeArtwork.itemId == "episode-id");
    assert(episodeArtwork.tag == "thumb-tag");
    assert(episodeArtwork.kind == ArtworkKind::Thumb);

    const ArtworkReference episodePrimaryArtwork = homeArtworkReference(
        "episode-id", "episode-primary", "series-id", "series-tag", true, "", "", "series-id"
    );
    assert(episodePrimaryArtwork.itemId == "episode-id");
    assert(episodePrimaryArtwork.tag == "episode-primary");
    assert(episodePrimaryArtwork.kind == ArtworkKind::Primary);

    const ArtworkReference parentBackdropArtwork = homeArtworkReference(
        "episode-id", "", "series-id", "series-tag", true, "", "backdrop-tag", "series-id"
    );
    assert(parentBackdropArtwork.itemId == "series-id");
    assert(parentBackdropArtwork.tag == "backdrop-tag");
    assert(parentBackdropArtwork.kind == ArtworkKind::Backdrop);

    const ArtworkReference thumbArtwork = homeArtworkReference(
        "movie-id", "", "", "", false, "thumb-tag", "backdrop-tag", "movie-id"
    );
    assert(thumbArtwork.itemId == "movie-id");
    assert(thumbArtwork.tag == "thumb-tag");
    assert(thumbArtwork.kind == ArtworkKind::Thumb);

    assert(subtitleLoadCompleted(false, false));
    assert(subtitleLoadCompleted(true, false));
    return 0;
}
