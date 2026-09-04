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
constexpr float mediaTitleScale() { return 2.45f; }
constexpr bool usesLandscapeMediaCard(std::string_view itemType) {
    return itemType == "Episode" || itemType == "CollectionFolder" || itemType == "BoxSet" || itemType == "Folder";
}
constexpr float searchMediaRowHeight(bool hasPortraitCard) { return hasPortraitCard ? 430.0f : 300.0f; }
constexpr float detailActionTextScale(std::size_t labelLength) {
    // Details can expose six actions at once. Long labels must remain single-line
    // even under the Extra Large global TV text preset.
    return labelLength > 10 ? 1.6f : 1.8f;
}
constexpr float uiTextScale(int option) {
    // The baseline is intentionally TV-sized; larger presets are accessibility choices,
    // not a way to compensate for a desktop-density default.
    return option <= 0 ? 1.9f : (option == 1 ? 2.15f : 2.4f);
}
constexpr float uiSafeAreaFraction(int percent) {
    return static_cast<float>(percent < 0 ? 0 : (percent > 6 ? 6 : percent)) / 100.0f;
}


constexpr float subtitleBottomY(bool playbackOverlayVisible, int position) {
    const int clamped = std::clamp(position, 0, 2);
    const float base = playbackOverlayVisible ? 790.0f : 1000.0f;
    return base - static_cast<float>(clamped) * 95.0f;
}

constexpr int wrappedIndex(int index, int delta, int count) {
    if (count <= 0) return 0;
    const int value = (index + delta) % count;
    return value < 0 ? value + count : value;
}