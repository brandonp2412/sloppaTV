#include "artwork_pipeline.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {
DecodedImage image(int width = 2, int height = 3) {
    DecodedImage result;
    result.width = width;
    result.height = height;
    result.rgba.resize(static_cast<size_t>(width * height * 4), 0xff);
    return result;
}

ArtworkLoadResult success(int width = 2, int height = 3) {
    ArtworkLoadResult result;
    result.decoded = image(width, height);
    return result;
}

ArtworkLoadResult failure() {
    ArtworkLoadResult result;
    result.failure = ArtworkLoadFailure::Download;
    result.error = "offline";
    return result;
}

struct FakeRenderer {
    uint64_t textureGeneration = 7;
    int createCalls = 0;
    std::vector<uint32_t> deleted;

    [[nodiscard]] uint64_t generation() const { return textureGeneration; }

    uint32_t createTexture(int width, int height, const uint8_t* pixels) {
        assert(width > 0);
        assert(height > 0);
        assert(pixels != nullptr);
        ++createCalls;
        return static_cast<uint32_t>(100 + createCalls);
    }

    void deleteTexture(uint32_t texture) { deleted.push_back(texture); }
};
} // namespace

int main() {
    ArtworkPipeline pipeline(2);
    FakeRenderer renderer;
    int requests = 0;

    assert(pipeline.readyTexture("missing", renderer, [&] { ++requests; }) == nullptr);
    assert(requests == 1);

    assert(pipeline.beginLoad("poster", renderer));
    pipeline.completeLoad("poster", success());
    ArtworkEntry* ready = pipeline.readyTexture("poster", renderer, [&] { ++requests; });
    assert(ready);
    assert(ready->texture == 101);
    assert(ready->textureGeneration == 7);
    assert(ready->sourceWidth == 2);
    assert(ready->sourceHeight == 3);
    assert(ready->decoded.rgba.empty());
    assert(renderer.createCalls == 1);

    ready = pipeline.readyTexture("poster", renderer, [&] { ++requests; });
    assert(ready && ready->texture == 101);
    assert(renderer.createCalls == 1);
    assert(requests == 1);

    renderer.textureGeneration = 8;
    assert(pipeline.readyTexture("poster", renderer, [&] { ++requests; }) == nullptr);
    assert(pipeline.peek("poster") == nullptr);
    assert(renderer.deleted.empty());
    assert(requests == 2);

    assert(pipeline.beginLoad("failed", renderer));
    pipeline.completeLoad("failed", failure());
    const ArtworkEntry* failed = pipeline.peek("failed");
    assert(failed && failed->state == ArtworkState::Failed);
    assert(pipeline.readyTexture("failed", renderer, [&] { ++requests; }) == nullptr);
    assert(requests == 3);

    pipeline.erase("failed", renderer);
    assert(!pipeline.peek("failed"));

    assert(pipeline.beginLoad("logo", renderer));
    pipeline.completeLoad("logo", success(4, 1));
    ready = pipeline.readyTexture("logo", renderer, [&] { ++requests; });
    assert(ready && ready->texture == 102);
    pipeline.clear(renderer);
    assert(pipeline.size() == 0);
    assert(renderer.deleted.size() == 1);
    assert(renderer.deleted.front() == 102);
    return 0;
}
