#pragma once

#include <algorithm>
#include <cstdint>

struct TrickplayFrame {
    int thumbnailIndex = -1;
    int tileIndex = -1;
    int cellX = 0;
    int cellY = 0;

    [[nodiscard]] constexpr bool valid() const {
        return thumbnailIndex >= 0 && tileIndex >= 0;
    }
};

constexpr TrickplayFrame trickplayFrameForPosition(
    int64_t positionMs,
    int intervalMs,
    int thumbnailCount,
    int tileWidth,
    int tileHeight
) {
    if (intervalMs <= 0 || thumbnailCount <= 0 || tileWidth <= 0 || tileHeight <= 0) return {};
    const int64_t rawIndex = std::max<int64_t>(0, positionMs) / intervalMs;
    const int thumbnailIndex = static_cast<int>(std::min<int64_t>(rawIndex, thumbnailCount - 1));
    const int tileSize = tileWidth * tileHeight;
    if (tileSize <= 0) return {};
    const int tileOffset = thumbnailIndex % tileSize;
    return {
        .thumbnailIndex = thumbnailIndex,
        .tileIndex = thumbnailIndex / tileSize,
        .cellX = tileOffset % tileWidth,
        .cellY = tileOffset / tileWidth,
    };
}
