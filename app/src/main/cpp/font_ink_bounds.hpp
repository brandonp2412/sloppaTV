#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Visible bounds in each 64px font cell, independent of advance/baseline padding.
inline std::array<std::array<int, 4>, 95> fontInkBounds(const std::vector<uint8_t>& rgba, int width, int height) {
    std::array<std::array<int, 4>, 95> bounds{};
    if (width != 1024 || height != 384 || rgba.size() != static_cast<size_t>(width * height * 4)) return bounds;
    for (int glyph = 0; glyph < 95; ++glyph) {
        int left = 64, top = 64, right = -1, bottom = -1;
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                const auto pixel = ((glyph / 16 * 64 + y) * width + glyph % 16 * 64 + x) * 4;
                if (rgba[pixel + 3] < 16) continue;
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
        if (right >= left) bounds[glyph] = {left, top, right - left + 1, bottom - top + 1};
    }
    return bounds;
}
