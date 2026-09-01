#pragma once

#include <cstddef>
#include <string>

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

struct ArtworkReference {
    std::string itemId;
    std::string tag;
    ArtworkKind kind = ArtworkKind::None;
};

constexpr std::size_t playerControlCount() {
    return 3;
}

constexpr int mediaGridColumns() { return 5; }
constexpr bool isTopMediaGridSelection(int selection) {
    return selection >= 0 && selection < mediaGridColumns();
}
constexpr float mediaCardWidth() { return 310.0f; }
constexpr float mediaTitleScale() { return 1.8f; }

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

inline ArtworkReference homeArtworkReference(
    const std::string& itemId,
    const std::string& primaryTag,
    const std::string& seriesId,
    const std::string& seriesPrimaryTag,
    bool preferSeries,
    const std::string& thumbTag,
    const std::string& backdropTag
) {
    if (preferSeries && !seriesId.empty() && !seriesPrimaryTag.empty()) {
        return {seriesId, seriesPrimaryTag, ArtworkKind::Primary};
    }
    const ArtworkKind kind = homeImageKind(!primaryTag.empty(), !thumbTag.empty(), !backdropTag.empty());
    const std::string& tag = kind == ArtworkKind::Primary ? primaryTag
        : (kind == ArtworkKind::Thumb ? thumbTag : backdropTag);
    return {itemId, tag, kind};
}

constexpr bool subtitleLoadCompleted(bool /*downloadSucceeded*/, bool /*cuesParsed*/) {
    return true;
}
