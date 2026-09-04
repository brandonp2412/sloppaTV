#include "home_screen.hpp"

#include <cassert>

int main() {
    assert(homeImageKind(true, true, true) == ArtworkKind::Primary);
    assert(homeImageKind(false, true, true) == ArtworkKind::Thumb);
    assert(homeImageKind(false, false, true) == ArtworkKind::Backdrop);
    assert(preferHomeLandscapeArtwork("Movie"));
    assert(preferHomeLandscapeArtwork("Episode"));
    assert(preferHomeLandscapeArtwork("Series"));
    assert(preferHomeLandscapeArtwork("BoxSet"));
    assert(!preferHomeLandscapeArtwork("UserView"));
    assert(!preferHomeLandscapeArtwork("CollectionFolder"));
    assert(!preferHomeLandscapeArtwork("Folder"));

    const ArtworkReference episodeArtwork = homeArtworkReference(
        "episode-id", "episode-tag", "series-id", "series-tag", true, "thumb-tag", "backdrop-tag", "episode-id"
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

    assert(homeFirstVisibleRow(0, 0, 5) == 0);
    assert(homeFirstVisibleRow(0, 1, 5) == 0);
    assert(homeFirstVisibleRow(0, 2, 5) == 1);
    assert(homeFirstVisibleRow(1, 1, 5) == 1);
    assert(homeFirstVisibleRow(1, 0, 5) == 0);
    assert(homeFirstVisibleRow(3, -1, 5) == 3);

    HomeScreenState state;
    state.reset();
    state.setSelections({0, 1, 2});
    assert(state.row() == 0);
    assert(state.navIndex() == 1);
    assert(state.selection(1, 4) == 1);
    state.moveSelection(1, 1, 4);
    assert(state.selection(1, 4) == 2);
    state.setRow(2);
    state.updateViewport(5);
    assert(state.firstVisibleRow() == 1);
    state.focusToolbar(3);
    assert(state.row() == -1);
    assert(state.navIndex() == 3);
    state.moveToolbar(-1);
    assert(state.navIndex() == 2);
    state.beginCenterPress();
    assert(state.centerPending());
    assert(state.consumeCenterRelease(true));
    state.beginCenterPress();
    state.markCenterLongPressed();
    assert(state.centerLongPressed());
    assert(!state.consumeCenterRelease(true));
    return 0;
}
