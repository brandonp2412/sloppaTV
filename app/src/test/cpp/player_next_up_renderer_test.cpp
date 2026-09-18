#include "player_next_up_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Harness {
    int modals = 0;
    int artwork = 0;
    int placeholders = 0;
    bool artworkAvailable = true;
    std::vector<std::string> text;

    void render(const PlayerNextUpRenderState& state) {
        renderPlayerNextUp(
            state,
            PlayerNextUpRenderStyle<int>{
                .panelCornerRadius = 24.0f,
                .artworkCornerRadius = 12.0f,
                .focus = 1,
                .text = 2,
                .muted = 3,
            },
            [&](float x, float y, float width, float height, float radius) {
                ++modals;
                assert(x == 1195.0f);
                assert(y == 185.0f);
                assert(width == 625.0f);
                assert(height == 205.0f);
                assert(radius == 24.0f);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                ++artwork;
                assert(item.id == "episode-2");
                assert(x == 1210.0f);
                assert(y == 214.0f);
                assert(width == 240.0f);
                assert(height == 146.0f);
                assert(radius == 12.0f);
                return artworkAvailable;
            },
            [&](const JellyfinItem& item, float, float, float, float, float) {
                ++placeholders;
                assert(item.id == "episode-2");
            },
            [&](float x, float y, float width, float height, float scale, std::string_view value, int) {
                assert(x == 1478.0f);
                assert(width == 312.0f);
                if (text.empty()) {
                    assert(y == 214.0f);
                    assert(height == 34.0f);
                    assert(scale == 1.55f);
                }
                text.emplace_back(value);
            },
            [](const JellyfinItem& item) {
                return item.seriesName.empty() ? std::string{} : item.seriesName + " - S1E2";
            });
    }
};
} // namespace

int main() {
    JellyfinItem item;
    item.id = "episode-2";
    item.name = "The Next Episode";
    item.seriesName = "Series";

    Harness available;
    available.render(PlayerNextUpRenderState{.item = item, .remainingMs = 12'999});
    assert(available.modals == 1);
    assert(available.artwork == 1);
    assert(available.placeholders == 0);
    assert(available.text.size() == 3);
    assert(available.text[0] == "Up next  |  12s");
    assert(available.text[1] == "The Next Episode");
    assert(available.text[2] == "Series - S1E2");

    Harness fallback;
    fallback.artworkAvailable = false;
    fallback.render(PlayerNextUpRenderState{.item = item, .remainingMs = -1});
    assert(fallback.artwork == 1);
    assert(fallback.placeholders == 1);
    assert(fallback.text[0] == "Up next  |  0s");

    item.seriesName.clear();
    Harness noSecondary;
    noSecondary.render(PlayerNextUpRenderState{.item = item, .remainingMs = 5000});
    assert(noSecondary.text.size() == 2);

    return 0;
}
