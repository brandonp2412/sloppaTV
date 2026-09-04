#include "artwork_cache.hpp"

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
    ArtworkCache cache(2);
    std::vector<uint32_t> released;
    const auto release = [&](ArtworkEntry& entry) {
        if (entry.texture != 0) released.push_back(entry.texture);
        entry.texture = 0;
    };

    assert(cache.beginLoad("a", release));
    assert(!cache.beginLoad("a", release));
    assert(cache.beginLoad("b", release));
    assert(!cache.beginLoad("c", release));

    assert(cache.markReady("a", image(2, 3)));
    auto* a = cache.find("a");
    assert(a && a->state == ArtworkState::Ready);
    assert(a->sourceWidth == 2 && a->sourceHeight == 3);
    a->texture = 11;

    cache.markFailed("b");
    assert(cache.peek("b") && cache.peek("b")->state == ArtworkState::Failed);
    assert(cache.beginLoad("c", release));
    assert(cache.size() == 2);
    assert(cache.peek("a"));
    assert(!cache.peek("b"));
    assert(cache.peek("c"));
    assert(released.empty());

    assert(cache.markReady("c", image(1, 1)));
    auto* c = cache.find("c");
    assert(c);
    c->texture = 22;
    assert(cache.find("a"));
    assert(cache.beginLoad("d", release));
    assert(!cache.peek("c"));
    assert(released.size() == 1 && released.front() == 22);

    cache.erase("a", release);
    assert(!cache.peek("a"));
    assert(released.size() == 2 && released.back() == 11);
    cache.clear(release);
    assert(cache.size() == 0);

    ArtworkCache retryCache(1);
    const auto failedAt = ArtworkCache::Clock::now();
    assert(retryCache.beginLoad("retry", release, failedAt));
    retryCache.markFailed("retry", failedAt);
    assert(retryCache.peek("retry") && retryCache.peek("retry")->state == ArtworkState::Failed);
    assert(!retryCache.beginLoad("retry", release, failedAt + std::chrono::seconds(29)));
    assert(retryCache.beginLoad("retry", release, failedAt + std::chrono::seconds(30)));
    assert(retryCache.peek("retry") && retryCache.peek("retry")->state == ArtworkState::Loading);

    ArtworkCache unbounded;
    for (int index = 0; index < 50; ++index) {
        assert(unbounded.beginLoad(std::to_string(index), release));
    }
    assert(unbounded.size() == 50);
    return 0;
}
