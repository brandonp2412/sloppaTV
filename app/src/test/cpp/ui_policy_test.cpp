#include "ui_policy.hpp"

#include <cassert>

int main() {
    assert(playerControlCount() == 3);
    assert(playerControlKind(0) == PlayerControlKind::PlayPause);
    assert(playerControlKind(1) == PlayerControlKind::Audio);
    assert(playerControlKind(2) == PlayerControlKind::Subtitles);
    assert(mediaGridColumns() == 5);
    assert(isTopMediaGridSelection(0));
    assert(isTopMediaGridSelection(4));
    assert(!isTopMediaGridSelection(5));
    assert(mediaCardWidth() == 320.0f);
    assert(mediaTitleScale() == 2.45f);
    assert(usesLandscapeMediaCard("Episode"));
    assert(usesLandscapeMediaCard("CollectionFolder"));
    assert(usesLandscapeMediaCard("BoxSet"));
    assert(usesLandscapeMediaCard("Folder"));
    assert(!usesLandscapeMediaCard("Movie"));
    assert(!usesLandscapeMediaCard("Series"));
    assert(searchMediaRowHeight(true) == 430.0f);
    assert(searchMediaRowHeight(false) == 300.0f);
    assert(detailActionTextScale(5) == 1.8f);
    assert(detailActionTextScale(10) == 1.8f);
    assert(detailActionTextScale(11) == 1.6f);
    assert(detailActionTextScale(13) == 1.6f);
    assert(uiTextScale(0) == 1.9f);
    assert(uiTextScale(1) == 2.15f);
    assert(uiTextScale(2) == 2.4f);
    assert(uiSafeAreaFraction(-1) == 0.0f);
    assert(uiSafeAreaFraction(4) == 0.04f);
    assert(uiSafeAreaFraction(99) == 0.06f);
    assert(wrappedIndex(0, -1, 10) == 9);
    assert(wrappedIndex(9, 1, 10) == 0);
    assert(wrappedIndex(2, 1, 5) == 3);
    assert(wrappedIndex(4, 1, 5) == 0);
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

    assert(playerBackAction(true, true) == PlayerBackAction::DismissOverlay);
    assert(playerBackAction(false, true) == PlayerBackAction::DismissOverlay);
    assert(playerBackAction(false, false) == PlayerBackAction::ExitPlayback);
    assert(subtitleBottomY(false, 0) == 1000.0f);
    assert(subtitleBottomY(false, 1) == 905.0f);
    assert(subtitleBottomY(false, 2) == 810.0f);
    assert(subtitleBottomY(true, 0) == 790.0f);
    assert(subtitleBottomY(true, 2) == 600.0f);
    assert(homeRowTop(0) == 170.0f);
    assert(homeRowTop(1) == 490.0f);
    assert(homeRowTop(2) == 825.0f);
    assert(homeFirstVisibleRow(0, 0, 5) == 0);
    assert(homeFirstVisibleRow(0, 1, 5) == 0);
    assert(homeFirstVisibleRow(0, 2, 5) == 1);
    assert(homeFirstVisibleRow(1, 1, 5) == 1);
    assert(homeFirstVisibleRow(1, 0, 5) == 0);
    assert(homeFirstVisibleRow(3, -1, 5) == 3);
    assert(syntheticTileTextX(90.0f) == 118.0f);
    assert(syntheticTileTextY(260.0f) == 342.0f);
    return 0;
}
