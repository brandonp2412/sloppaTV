#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

constexpr int mediaGridColumns() { return 5; }
constexpr bool isTopMediaGridSelection(int selection) {
    return selection >= 0 && selection < mediaGridColumns();
}
constexpr float mediaCardWidth() { return 320.0f; }
constexpr float mediaPosterWidth() { return 208.0f; }
constexpr float mediaPosterHeight() { return 312.0f; }
constexpr int mediaFirstVisibleRow(int selection, int visibleRows) {
    return std::max(0, std::max(0, selection) / mediaGridColumns() - std::max(1, visibleRows) + 1);
}
constexpr float mediaTitleScale() { return 2.45f; }
constexpr bool usesLandscapeMediaCard(std::string_view itemType) {
    return itemType == "Episode" || itemType == "CollectionFolder" || itemType == "BoxSet" || itemType == "Folder";
}
constexpr float searchMediaRowHeight(bool hasPortraitCard) { return hasPortraitCard ? 430.0f : 300.0f; }
constexpr int mediaGridTitleLineLimit(int visibleRow, int uiTextSize, bool hasPortraitCards) {
    return hasPortraitCards && visibleRow > 0 && uiTextSize > 0 ? 1 : 0;
}
constexpr float detailActionTextScale(std::size_t labelLength) {
    return labelLength > 10 ? 1.6f : 1.8f;
}
constexpr float uiTextScale(int option) {
    return option <= 0 ? 1.9f : (option == 1 ? 2.15f : 2.4f);
}
constexpr float homeRowImageOffset(int uiTextSize) {
    return uiTextSize <= 0 ? 82.0f : (uiTextSize == 1 ? 92.0f : 102.0f);
}
constexpr float homeRowStep(int uiTextSize) {
    return uiTextSize <= 0 ? 420.0f : (uiTextSize == 1 ? 440.0f : 460.0f);
}
constexpr float castRowStep(int uiTextSize) {
    return uiTextSize <= 0 ? 400.0f : (uiTextSize == 1 ? 415.0f : 430.0f);
}
constexpr int clampedUiTextSize(int uiTextSize) {
    return std::clamp(uiTextSize, 0, 2);
}
constexpr float settingsDescriptionY(int uiTextSize) {
    return 220.0f + 3.0f * static_cast<float>(clampedUiTextSize(uiTextSize));
}
constexpr float settingsRowsTop(int uiTextSize) {
    return 280.0f + 6.0f * static_cast<float>(clampedUiTextSize(uiTextSize));
}
constexpr float settingsFooterY(int uiTextSize) {
    return 956.0f + 6.0f * static_cast<float>(clampedUiTextSize(uiTextSize));
}
constexpr float uiSafeAreaFraction(int percent) {
    return static_cast<float>(percent < 0 ? 0 : (percent > 6 ? 6 : percent)) / 100.0f;
}

constexpr float keyboardKeyHeight(float top, int rowCount, float gap) {
    if (rowCount <= 0) return 0.0f;
    constexpr float canvasHeight = 1080.0f;
    constexpr float maxKeyHeight = 82.0f;
    constexpr float minKeyHeight = 58.0f;
    constexpr float focusedBottomReserve = 20.0f;
    const float gaps = static_cast<float>(std::max(0, rowCount - 1)) * std::max(0.0f, gap);
    const float available = canvasHeight - top - focusedBottomReserve - gaps;
    return std::clamp(available / static_cast<float>(rowCount), minKeyHeight, maxKeyHeight);
}

constexpr float materialButtonFocusScale() { return 1.05f; }
constexpr float materialCardFocusScale() { return 1.05f; }
constexpr float materialListItemFocusScale() { return 1.04f; }
constexpr float materialTabFocusScale() { return 1.05f; }
constexpr float materialInputFocusScale() { return 1.04f; }

constexpr float playbackProgressThumbCenterX(float trackX, float trackWidth, float progress, float thumbRadius) {
    if (trackWidth <= 0.0f) return trackX;
    const float radius = std::clamp(thumbRadius, 0.0f, trackWidth * 0.5f);
    const float fraction = std::clamp(progress, 0.0f, 1.0f);
    return std::clamp(trackX + trackWidth * fraction, trackX + radius, trackX + trackWidth - radius);
}

constexpr float subtitleBottomY(
    bool playbackOverlayVisible,
    bool playbackControlsActive,
    int position,
    bool skipButtonVisible = false
) {
    const int clamped = std::clamp(position, 0, 2);
    float base = playbackControlsActive ? 670.0f : (playbackOverlayVisible ? 790.0f : 1000.0f);
    if (skipButtonVisible) base = std::min(base, 610.0f);
    return base - static_cast<float>(clamped) * 95.0f;
}

constexpr int wrappedIndex(int index, int delta, int count) {
    if (count <= 0) return 0;
    const int value = (index + delta) % count;
    return value < 0 ? value + count : value;
}
