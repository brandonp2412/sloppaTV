#include "artwork_coordinator.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {
DecodedImage decodeImage(const std::string& bytes, std::string& error) {
    if (bytes.empty()) {
        error = "empty";
        return {};
    }
    DecodedImage image;
    image.width = 2;
    image.height = 3;
    image.rgba.resize(24, 0xff);
    return image;
}

struct FakeTaskRunner {
    std::vector<std::function<void()>> queued;

    bool submit(std::function<void()> task) {
        queued.push_back(std::move(task));
        return true;
    }

    void runNext() {
        assert(!queued.empty());
        auto task = std::move(queued.front());
        queued.erase(queued.begin());
        task();
    }
};

struct FakeRenderer {
    uint64_t textureGeneration = 9;
    int createCalls = 0;
    std::vector<uint32_t> deleted;

    [[nodiscard]] uint64_t generation() const { return textureGeneration; }

    uint32_t createTexture(int width, int height, const uint8_t* pixels) {
        assert(width == 2);
        assert(height == 3);
        assert(pixels != nullptr);
        ++createCalls;
        return static_cast<uint32_t>(100 + createCalls);
    }

    void deleteTexture(uint32_t texture) {
        deleted.push_back(texture);
    }
};
}

int main() {
    FakeTaskRunner tasks;
    std::recursive_mutex stateMutex;
    ArtworkCoordinator<FakeTaskRunner, std::recursive_mutex> artwork(tasks, stateMutex);
    FakeRenderer renderer;

    int posterLoads = 0;
    const auto posterLoad = [&] {
        ++posterLoads;
        ArtworkLoadResult result;
        result.decoded = decodeImage("poster", result.error);
        return result;
    };
    assert(artwork.loadPoster("poster", renderer, posterLoad));
    assert(!artwork.loadPoster("poster", renderer, posterLoad));
    assert(tasks.queued.size() == 1);
    int missingRequests = 0;
    assert(artwork.posterTexture("poster", renderer, [&] { ++missingRequests; }) == nullptr);
    assert(missingRequests == 0);

    tasks.runNext();
    ArtworkEntry* poster = artwork.posterTexture("poster", renderer, [&] { ++missingRequests; });
    assert(poster && poster->texture == 101);
    assert(posterLoads == 1);
    assert(renderer.createCalls == 1);

    assert(artwork.loadProfile("profile", renderer, [] {
        ArtworkLoadResult result;
        result.decoded = decodeImage("profile", result.error);
        return result;
    }));
    tasks.runNext();
    ArtworkEntry* profile = artwork.profileTexture("profile", renderer, [&] { ++missingRequests; });
    assert(profile && profile->texture == 102);

    int homeObserved = 0;
    assert(artwork.loadHome(
        "home",
        renderer,
        [] {
            ArtworkLoadResult result;
            result.decoded = decodeImage("home", result.error);
            return result;
        },
        [&](const ArtworkLoadResult& loaded) {
            ++homeObserved;
            assert(loaded.ok());
            assert(loaded.source == ArtworkLoadSource::Network);
        }
    ));
    tasks.runNext();
    ArtworkEntry* home = artwork.homeTexture("home", renderer, [&] { ++missingRequests; });
    assert(home && home->texture == 103);
    assert(homeObserved == 1);

    artwork.clearSession(renderer);
    assert(renderer.deleted.size() == 2);
    assert(std::find(renderer.deleted.begin(), renderer.deleted.end(), 101) != renderer.deleted.end());
    assert(std::find(renderer.deleted.begin(), renderer.deleted.end(), 103) != renderer.deleted.end());
    assert(std::find(renderer.deleted.begin(), renderer.deleted.end(), 102) == renderer.deleted.end());

    assert(artwork.posterTexture("poster", renderer, [&] { ++missingRequests; }) == nullptr);
    assert(missingRequests == 1);
    profile = artwork.profileTexture("profile", renderer, [&] { ++missingRequests; });
    assert(profile && profile->texture == 102);
    assert(missingRequests == 1);

    artwork.eraseProfile("profile", renderer);
    assert(renderer.deleted.size() == 3);
    assert(renderer.deleted.back() == 102);
    assert(artwork.profileTexture("profile", renderer, [&] { ++missingRequests; }) == nullptr);
    assert(missingRequests == 2);
    return 0;
}
