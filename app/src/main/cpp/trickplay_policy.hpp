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

struct TrickplayUvRegion {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;

    [[nodiscard]] constexpr bool valid() const {
        return u1 > u0 && v1 > v0;
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
    const int64_t tileSize = static_cast<int64_t>(tileWidth) * static_cast<int64_t>(tileHeight);
    const int64_t tileOffset = static_cast<int64_t>(thumbnailIndex) % tileSize;
    return {
        .thumbnailIndex = thumbnailIndex,
        .tileIndex = static_cast<int>(static_cast<int64_t>(thumbnailIndex) / tileSize),
        .cellX = static_cast<int>(tileOffset % tileWidth),
        .cellY = static_cast<int>(tileOffset / tileWidth),
    };
}

constexpr TrickplayUvRegion trickplayUvRegion(
    const TrickplayFrame& frame,
    int cellWidth,
    int cellHeight,
    int sourceWidth,
    int sourceHeight
) {
    if (!frame.valid() || cellWidth <= 0 || cellHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0) return {};
    const float width = static_cast<float>(sourceWidth);
    const float height = static_cast<float>(sourceHeight);
    return {
        .u0 = std::clamp(static_cast<float>(frame.cellX) * static_cast<float>(cellWidth) / width, 0.0f, 1.0f),
        .v0 = std::clamp(static_cast<float>(frame.cellY) * static_cast<float>(cellHeight) / height, 0.0f, 1.0f),
        .u1 = std::clamp(static_cast<float>(frame.cellX + 1) * static_cast<float>(cellWidth) / width, 0.0f, 1.0f),
        .v1 = std::clamp(static_cast<float>(frame.cellY + 1) * static_cast<float>(cellHeight) / height, 0.0f, 1.0f),
    };
}
