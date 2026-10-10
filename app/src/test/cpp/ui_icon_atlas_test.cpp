#include "ui_icon_atlas.hpp"
#include "ui_icons.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

int main() {
    assert(ui_icons::forLabel("FAVORITES") == ui_icons::Glyph::Heart);
    assert(ui_icons::forLabel("Play external") == ui_icons::Glyph::External);
    assert(!ui_icons::forLabel("Some movie title"));
    assert(ui_icons::forSetting("SUBTITLE SIZE") == ui_icons::Glyph::Subtitles);
    assert(ui_icons::forSetting("AUDIO OUTPUT") == ui_icons::Glyph::Audio);
    static_assert(sizeof(ui_icons::atlasRuns) % 2 == 0);
    std::vector<uint8_t> alpha;
    for (std::size_t i = 0; i < sizeof(ui_icons::atlasRuns); i += 2) {
        assert(ui_icons::atlasRuns[i] != 0);
        alpha.insert(alpha.end(), ui_icons::atlasRuns[i], ui_icons::atlasRuns[i + 1]);
    }
    assert(alpha.size() == ui_icons::atlasWidth * ui_icons::atlasHeight);
    // Every glyph must retain opaque strokes, coverage pixels, and transparent
    // cell borders so linear/mipmap filtering cannot pick up a neighboring glyph.
    for (int glyph = 0; glyph < static_cast<int>(ui_icons::Glyph::Count); ++glyph) {
        int opaque = 0;
        int coverage = 0;
        for (int y = 0; y < 96; ++y) {
            for (int x = 0; x < 96; ++x) {
                const auto value = alpha[(glyph / 4 * 96 + y) * ui_icons::atlasWidth + glyph % 4 * 96 + x];
                opaque += value == 255;
                coverage += value > 0 && value < 255;
                if (x < 3 || x >= 93 || y < 3 || y >= 93) assert(value == 0);
            }
        }
        assert(opaque > 0);
        assert(coverage > 0);
    }
}
