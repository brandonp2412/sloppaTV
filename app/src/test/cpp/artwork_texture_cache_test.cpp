#include "artwork_texture_cache.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <vector>

namespace {
DecodedImage image(int width, int height) {
    DecodedImage result;
    result.width = width;
    result.height = height;
    result.rgba.resize(static_cast<size_t>(width * height * 4), 0xff);
    return result;
}
}

int main() {
    ArtworkTextureCache cache(2);
    std::vector<uint32_t> deleted;
    int createCalls = 0;
    const auto create = [&](int width, int height, const uint8_t* pixels) {
        assert(width > 0);
        assert(height > 0);
        assert(pixels != nullptr);
        ++createCalls;
        return static_cast<uint32_t>(100 + createCalls);
    };
    const auto destroy = [&](uint32_t texture) {
        deleted.push_back(texture);
    };

    assert(cache.beginLoad("poster", 7, destroy));
    assert(cache.prepare("poster", 7, create).state == ArtworkTextureState::Loading);
    assert(cache.markReady("poster", image(2, 3)));

    ArtworkTextureResult ready = cache.prepare("poster", 7, create);
    assert(ready.state == ArtworkTextureState::Ready);
    assert(ready.entry);
    assert(ready.entry->texture == 101);
    assert(ready.entry->textureGeneration == 7);
    assert(ready.entry->sourceWidth == 2);
    assert(ready.entry->sourceHeight == 3);
    assert(ready.entry->decoded.rgba.empty());
    assert(createCalls == 1);

    ready = cache.prepare("poster", 7, create);
    assert(ready.state == ArtworkTextureState::Ready);
    assert(ready.entry && ready.entry->texture == 101);
    assert(createCalls == 1);

    DecodedImage regenerated = image(4, 5);
    assert(cache.markReady("poster", std::move(regenerated)));
    ready = cache.prepare("poster", 8, create);
    assert(ready.state == ArtworkTextureState::Ready);
    assert(ready.entry && ready.entry->texture == 102);
    assert(ready.entry->textureGeneration == 8);
    assert(deleted.empty());

    cache.erase("poster", 8, destroy);
    assert(deleted.size() == 1);
    assert(deleted.front() == 102);
    assert(!cache.peek("poster"));

    const auto failedAt = ArtworkCache::Clock::now();
    assert(cache.beginLoad("retry", 8, destroy, failedAt));
    cache.markFailed("retry", failedAt);
    assert(cache.prepare("retry", 8, create).state == ArtworkTextureState::Failed);
    assert(!cache.beginLoad("retry", 8, destroy, failedAt + std::chrono::seconds(29)));
    assert(cache.beginLoad("retry", 8, destroy, failedAt + std::chrono::seconds(30)));

    assert(cache.markReady("retry", DecodedImage{}));
    assert(cache.prepare("retry", 8, create).state == ArtworkTextureState::InvalidDecoded);

    assert(cache.beginLoad("other", 8, destroy));
    assert(cache.markReady("other", image(1, 1)));
    ready = cache.prepare("other", 8, create);
    assert(ready.state == ArtworkTextureState::Ready);
    assert(ready.entry && ready.entry->texture == 103);

    cache.clear(9, destroy);
    assert(deleted.size() == 1);
    assert(cache.size() == 0);
    return 0;
}
