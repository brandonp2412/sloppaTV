#include "ui_policy.hpp"

#include <cassert>

int main() {
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
    assert(subtitleBottomY(false, 0) == 1000.0f);
    assert(subtitleBottomY(false, 1) == 905.0f);
    assert(subtitleBottomY(false, 2) == 810.0f);
    assert(subtitleBottomY(true, 0) == 790.0f);
    assert(subtitleBottomY(true, 2) == 600.0f);
    return 0;
}
