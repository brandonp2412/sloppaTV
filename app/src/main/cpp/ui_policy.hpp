#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

enum class PlayerControlKind {
    PlayPause,
    Audio,
    Subtitles,
};

enum class ArtworkKind {
    Primary,
    Thumb,
    Backdrop,
    None,
};

enum class PlayerBackAction {
    DismissOverlay,
    ExitPlayback,
};

struct ArtworkReference {
    std::string itemId;
    std::string tag;
    ArtworkKind kind = ArtworkKind::None;
};

constexpr std::size_t playerControlCount() {
    return 3;
}

constexpr int mediaGridColumns() { return 4; }
constexpr bool isTopMediaGridSelection(int selection) {
    return selection >= 0 && selection < mediaGridColumns();
}
constexpr float mediaCardWidth() { return 405.0f; }
constexpr float mediaTitleScale() { return 2.15f; }
constexpr float detailActionTextScale(std::size_t labelLength) {
    // Details can expose six actions at once. Long labels must remain single-line
    // even under the Extra Large global TV text preset.
    return labelLength > 10 ? 1.6f : 1.8f;
}
constexpr float uiTextScale(int option) {
    // Couch-distance TV UI needs materially larger type than desktop/mobile UI.
    return option <= 0 ? 1.45f : (option == 1 ? 1.65f : 1.85f);
}
constexpr float uiSafeAreaFraction(int percent) {
    return static_cast<float>(percent < 0 ? 0 : (percent > 6 ? 6 : percent)) / 100.0f;
}

constexpr PlayerBackAction playerBackAction(bool controlsActive, bool timedOverlayVisible) {
    return controlsActive || timedOverlayVisible
        ? PlayerBackAction::DismissOverlay
        : PlayerBackAction::ExitPlayback;
}

constexpr bool queueOverlayShouldShowError(bool /*hasItems*/) { return false; }

constexpr float homeRowTop(int slot) {
    return slot <= 0 ? 145.0f : (slot == 1 ? 430.0f : 825.0f);
}

constexpr float syntheticTileTextX(float tileX) { return tileX + 28.0f; }
constexpr float syntheticTileTextY(float tileY) { return tileY + 82.0f; }

constexpr int wrappedIndex(int index, int delta, int count) {
    if (count <= 0) return 0;
    const int value = (index + delta) % count;
    return value < 0 ? value + count : value;
}

inline bool containsCaseInsensitive(std::string_view text, std::string_view query) {
    if (query.empty()) return true;
    if (query.size() > text.size()) return false;
    return std::search(
        text.begin(), text.end(),
        query.begin(), query.end(),
        [](unsigned char left, unsigned char right) {
            return std::toupper(left) == std::toupper(right);
        }
    ) != text.end();
}

constexpr PlayerControlKind playerControlKind(std::size_t index) {
    return index == 0 ? PlayerControlKind::PlayPause
        : (index == 1 ? PlayerControlKind::Audio : PlayerControlKind::Subtitles);
}

constexpr ArtworkKind homeImageKind(bool hasPrimary, bool hasThumb, bool hasBackdrop) {
    if (hasPrimary) return ArtworkKind::Primary;
    if (hasThumb) return ArtworkKind::Thumb;
    if (hasBackdrop) return ArtworkKind::Backdrop;
    return ArtworkKind::None;
}

inline bool preferHomeLandscapeArtwork(const std::string& itemType) {
    return itemType != "UserView" && itemType != "CollectionFolder" && itemType != "Folder";
}

inline ArtworkReference homeArtworkReference(
    const std::string& itemId,
    const std::string& primaryTag,
    const std::string& seriesId,
    const std::string& seriesPrimaryTag,
    bool preferSeries,
    const std::string& thumbTag,
    const std::string& backdropTag,
    const std::string& backdropItemId
) {
    const std::string backdropOwner = backdropItemId.empty() ? itemId : backdropItemId;
    const bool ownBackdrop = !backdropTag.empty() && backdropOwner == itemId;

    // Episode Home cards are landscape. Prefer item-specific art first. A parsed
    // ParentBackdropImageTag belongs to ParentBackdropItemId, not the episode ID.
    if (preferSeries && !thumbTag.empty()) return {itemId, thumbTag, ArtworkKind::Thumb};
    if (preferSeries && ownBackdrop) return {itemId, backdropTag, ArtworkKind::Backdrop};
    if (preferSeries && !primaryTag.empty()) return {itemId, primaryTag, ArtworkKind::Primary};
    if (preferSeries && !backdropTag.empty()) return {backdropOwner, backdropTag, ArtworkKind::Backdrop};
    if (preferSeries && !seriesId.empty() && !seriesPrimaryTag.empty()) {
        return {seriesId, seriesPrimaryTag, ArtworkKind::Primary};
    }

    const ArtworkKind kind = homeImageKind(!primaryTag.empty(), !thumbTag.empty(), !backdropTag.empty());
    const std::string& tag = kind == ArtworkKind::Primary ? primaryTag
        : (kind == ArtworkKind::Thumb ? thumbTag : backdropTag);
    return {kind == ArtworkKind::Backdrop ? backdropOwner : itemId, tag, kind};
}

constexpr bool subtitleLoadCompleted(bool /*downloadSucceeded*/, bool /*cuesParsed*/) {
    return true;
}
