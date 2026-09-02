#include "trickplay_policy.hpp"

#include <cassert>

int main() {
    assert(!trickplayFrameForPosition(0, 0, 100, 10, 10).valid());
    assert(!trickplayFrameForPosition(0, 10000, 0, 10, 10).valid());

    const auto first = trickplayFrameForPosition(0, 10000, 250, 10, 10);
    assert(first.valid());
    assert(first.thumbnailIndex == 0);
    assert(first.tileIndex == 0);
    assert(first.cellX == 0);
    assert(first.cellY == 0);

    const auto middle = trickplayFrameForPosition(990000, 10000, 250, 10, 10);
    assert(middle.thumbnailIndex == 99);
    assert(middle.tileIndex == 0);
    assert(middle.cellX == 9);
    assert(middle.cellY == 9);

    const auto nextTile = trickplayFrameForPosition(1000000, 10000, 250, 10, 10);
    assert(nextTile.thumbnailIndex == 100);
    assert(nextTile.tileIndex == 1);
    assert(nextTile.cellX == 0);
    assert(nextTile.cellY == 0);

    const auto clamped = trickplayFrameForPosition(9999999, 10000, 250, 10, 10);
    assert(clamped.thumbnailIndex == 249);
    assert(clamped.tileIndex == 2);
    assert(clamped.cellX == 9);
    assert(clamped.cellY == 4);
    return 0;
}
