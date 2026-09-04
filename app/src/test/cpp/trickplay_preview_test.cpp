#include "trickplay_preview.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    TrickplayPreviewState state;
    const auto now = TrickplayPreviewState::Clock::time_point{10s};

    assert(!state.visible(now, "item"));
    state.showAt(12'345, now);
    assert(state.positionMs() == 12'345);

    state.beginTile("item", 2);
    assert(state.matchesTile("item", 2));
    assert(!state.failed());
    assert(!state.visible(now, "item"));

    DecodedImage decoded;
    decoded.width = 2;
    decoded.height = 1;
    decoded.rgba.resize(8);
    state.applyDecoded(std::move(decoded));
    assert(state.visible(now + 3s, "item"));
    assert(!state.visible(now + 4s, "item"));
    assert(!state.visible(now + 1s, "other"));

    state.setTexture(7, 11);
    assert(state.texture() == 7);
    assert(state.textureGeneration() == 11);
    state.clearTexture();
    assert(state.texture() == 0);
    assert(state.textureGeneration() == 0);

    state.showAt(20'000, now);
    assert(state.matchesTile("item", 2));
    state.markFailed();
    assert(state.failed());
    assert(!state.visible(now, "item"));

    state.beginTile("item", 3);
    assert(state.matchesTile("item", 3));
    state.reset();
    assert(state.positionMs() == -1);
    assert(state.tileIndex() == -1);
    assert(!state.visible(now, "item"));

    return 0;
}
