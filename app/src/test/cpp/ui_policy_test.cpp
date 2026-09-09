#include "ui_policy.hpp"

#include <cassert>

int main() {
    assert(mediaGridColumns() == 5);
    assert(isTopMediaGridSelection(0));
    assert(isTopMediaGridSelection(4));
    assert(!isTopMediaGridSelection(5));
    assert(mediaCardWidth() == 320.0f);
    // Focus must remain inside the rendered viewport for every grid density.
    for (const int visibleRows : {1, 2, 4}) {
        for (int selection = 0; selection < 150; ++selection) {
            const int first = mediaFirstVisibleRow(selection, visibleRows);
            const int row = selection / mediaGridColumns();
            assert(first <= row && row < first + visibleRows);
        }
    }
    assert(mediaFirstVisibleRow(-1, 0) == 0);
    // Login's five-row keyboard must stay readable while clearing the password field.
    assert(keyboardKeyHeight(655.0f, 5, 14.0f) < 82.0f);
    assert(keyboardKeyHeight(655.0f, 5, 14.0f) >= 68.0f);
    assert(keyboardKeyHeight(270.0f, 5, 14.0f) == 82.0f);
    assert(keyboardKeyHeight(610.0f, 0, 14.0f) == 0.0f);
    // Two poster rows, including two title lines and metadata, fit the canvas.
    assert(195.0f + 430.0f + mediaPosterHeight() + 24.0f + 72.0f + 36.0f <= 1080.0f);
    assert(mediaTitleScale() == 2.45f);
    assert(usesLandscapeMediaCard("Episode"));
    assert(usesLandscapeMediaCard("CollectionFolder"));
    assert(usesLandscapeMediaCard("BoxSet"));
    assert(usesLandscapeMediaCard("Folder"));
    assert(!usesLandscapeMediaCard("Movie"));
    assert(!usesLandscapeMediaCard("Series"));
    assert(searchMediaRowHeight(true) == 430.0f);
    assert(searchMediaRowHeight(false) == 300.0f);
    assert(mediaGridTitleLineLimit(0, 2, true) == 0);
    assert(mediaGridTitleLineLimit(1, 0, true) == 0);
    assert(mediaGridTitleLineLimit(1, 1, true) == 1);
    assert(mediaGridTitleLineLimit(1, 2, true) == 1);
    assert(mediaGridTitleLineLimit(1, 2, false) == 0);
    assert(detailActionTextScale(5) == 1.8f);
    assert(detailActionTextScale(10) == 1.8f);
    assert(detailActionTextScale(11) == 1.6f);
    assert(detailActionTextScale(13) == 1.6f);
    assert(uiTextScale(0) == 1.9f);
    assert(uiTextScale(1) == 2.15f);
    assert(uiTextScale(2) == 2.4f);
    assert(homeRowImageOffset(0) == 82.0f);
    assert(homeRowImageOffset(1) == 92.0f);
    assert(homeRowImageOffset(2) == 102.0f);
    assert(homeRowStep(0) == 420.0f);
    assert(homeRowStep(1) == 440.0f);
    assert(homeRowStep(2) == 460.0f);
    assert(castRowStep(0) == 400.0f);
    assert(castRowStep(1) == 415.0f);
    assert(castRowStep(2) == 430.0f);
    assert(settingsDescriptionY(0) == 220.0f);
    assert(settingsDescriptionY(2) == 226.0f);
    assert(settingsRowsTop(0) == 280.0f);
    assert(settingsRowsTop(2) == 292.0f);
    assert(settingsFooterY(0) == 956.0f);
    assert(settingsFooterY(2) == 968.0f);
    // Keep the second row's episode metadata on-screen at the largest text size.
    assert(150.0f + homeRowStep(2) + homeRowImageOffset(2) + 202.0f + 22.0f
        + 11.0f * 2.45f * uiTextScale(2) + 4.0f + 10.0f * 1.58f * uiTextScale(2) < 1080.0f);
    // Large cast labels retain breathing room above the next focused artwork row.
    constexpr float castTitleY = 195.0f + 285.0f + 24.0f;
    constexpr float castRoleBottom = castTitleY + 11.0f * 1.8f * uiTextScale(2)
        + 4.0f + 10.0f * 1.55f * uiTextScale(2);
    constexpr float secondCastFocusHaloTop = 195.0f + castRowStep(2) - (285.0f * 0.05f * 0.5f) - 10.0f;
    assert(castRoleBottom < secondCastFocusHaloTop);
    // The second row's large role label still clears the compact footer at y=1032.
    assert(castRoleBottom + castRowStep(2) < 1032.0f);
    // Large settings copy clears the first focused row halo and the footer clears the last row halo.
    constexpr float largeSettingsDescriptionBottom = settingsDescriptionY(2) + 10.0f * 1.40f * uiTextScale(2);
    constexpr float firstSettingsHaloTop = settingsRowsTop(2) - 8.0f - (88.0f * 0.04f * 0.5f) - 10.0f;
    constexpr float lastSettingsHaloBottom = settingsRowsTop(2) + 5.0f * 112.0f - 8.0f
        + 88.0f + (88.0f * 0.04f * 0.5f) + 10.0f;
    assert(largeSettingsDescriptionBottom < firstSettingsHaloTop);
    assert(lastSettingsHaloBottom < settingsFooterY(2));
    assert(settingsFooterY(2) + 58.0f < 1080.0f);
    assert(uiSafeAreaFraction(-1) == 0.0f);
    assert(uiSafeAreaFraction(4) == 0.04f);
    assert(uiSafeAreaFraction(99) == 0.06f);
    assert(materialButtonFocusScale() == 1.05f);
    assert(materialCardFocusScale() == 1.05f);
    assert(materialListItemFocusScale() == 1.04f);
    assert(materialTabFocusScale() == 1.05f);
    assert(materialInputFocusScale() == 1.04f);
    assert(wrappedIndex(0, -1, 10) == 9);
    assert(wrappedIndex(9, 1, 10) == 0);
    assert(wrappedIndex(2, 1, 5) == 3);
    assert(wrappedIndex(4, 1, 5) == 0);
    assert(subtitleBottomY(false, false, 0) == 1000.0f);
    assert(subtitleBottomY(false, false, 1) == 905.0f);
    assert(subtitleBottomY(false, false, 2) == 810.0f);
    assert(subtitleBottomY(true, false, 0) == 790.0f);
    assert(subtitleBottomY(true, false, 2) == 600.0f);
    assert(subtitleBottomY(true, true, 0) == 670.0f);
    assert(subtitleBottomY(false, true, 2) == 480.0f);
    assert(subtitleBottomY(false, false, 0, true) == 610.0f);
    assert(subtitleBottomY(true, true, 0, true) == 610.0f);
    assert(subtitleBottomY(false, false, 2, true) == 420.0f);
    return 0;
}
