#include "font_ink_bounds.hpp"

#include <cassert>

int main() {
    std::vector<uint8_t> pixels(1024 * 384 * 4);
    // Deliberately asymmetric font-cell padding: advancing or centering the
    // entire cell would put this initial off center.
    constexpr int glyph = 'J' - 32;
    for (int y = 13; y < 49; ++y)
        for (int x = 7; x < 28; ++x) pixels[((glyph / 16 * 64 + y) * 1024 + glyph % 16 * 64 + x) * 4 + 3] = 255;
    const auto bounds = fontInkBounds(pixels, 1024, 384);
    assert((bounds[glyph] == std::array<int, 4>{7, 13, 21, 36}));
    assert((bounds[0] == std::array<int, 4>{}));
    assert((fontInkBounds({}, 1024, 384)[glyph] == std::array<int, 4>{}));
}
