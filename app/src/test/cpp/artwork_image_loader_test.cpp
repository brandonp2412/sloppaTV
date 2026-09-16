#include "artwork_image_loader.hpp"
#include "jellyfin_types.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <unordered_map>

namespace {
DecodedImage decodedImage(int width = 2, int height = 3) {
    DecodedImage image;
    image.width = width;
    image.height = height;
    image.rgba.resize(static_cast<size_t>(width * height * 4), 0xff);
    return image;
}

struct FakeDiskCache {
    std::unordered_map<std::string, std::string> values;
    int reads = 0;
    int writes = 0;
    int erases = 0;

    std::optional<std::string> read(const std::string& key) {
        ++reads;
        const auto found = values.find(key);
        if (found == values.end()) return std::nullopt;
        return found->second;
    }

    void write(const std::string& key, const std::string& value) {
        ++writes;
        values[key] = value;
    }

    void erase(const std::string& key) {
        ++erases;
        values.erase(key);
    }
};
}

int main() {
    int downloads = 0;
    int decodes = 0;
    const auto decode = [&](const std::string& bytes, std::string& error) {
        ++decodes;
        if (bytes == "invalid") {
            error = "decode failed";
            return DecodedImage{};
        }
        return decodedImage();
    };

    auto success = ArtworkImageLoader::load(
        [&] {
            ++downloads;
            return ApiValueResult<std::string>{{true, {}}, "network"};
        },
        decode
    );
    assert(success.ok());
    assert(success.source == ArtworkLoadSource::Network);
    assert(success.failure == ArtworkLoadFailure::None);
    assert(downloads == 1 && decodes == 1);

    auto downloadFailure = ArtworkImageLoader::load(
        [&] {
            ++downloads;
            return ApiValueResult<std::string>{{false, "offline"}, {}};
        },
        decode
    );
    assert(!downloadFailure.ok());
    assert(downloadFailure.failure == ArtworkLoadFailure::Download);
    assert(downloadFailure.error == "offline");
    assert(decodes == 1);

    auto decodeFailure = ArtworkImageLoader::load(
        [&] {
            ++downloads;
            return ApiValueResult<std::string>{{true, {}}, "invalid"};
        },
        decode
    );
    assert(!decodeFailure.ok());
    assert(decodeFailure.failure == ArtworkLoadFailure::Decode);
    assert(decodeFailure.error == "decode failed");
    assert(decodes == 2);

    FakeDiskCache cache;
    cache.values["cached"] = "disk";
    auto cached = ArtworkImageLoader::loadCached(
        cache,
        "cached",
        [&] {
            ++downloads;
            return ApiValueResult<std::string>{{false, "should not download"}, {}};
        },
        decode
    );
    assert(cached.ok());
    assert(cached.source == ArtworkLoadSource::DiskCache);
    assert(cache.reads == 1 && cache.writes == 0 && cache.erases == 0);

    cache.values["recover"] = "invalid";
    auto recovered = ArtworkImageLoader::loadCached(
        cache,
        "recover",
        [&] {
            ++downloads;
            return ApiValueResult<std::string>{{true, {}}, "fresh"};
        },
        decode
    );
    assert(recovered.ok());
    assert(recovered.source == ArtworkLoadSource::Network);
    assert(cache.erases == 1);
    assert(cache.writes == 1);
    assert(cache.values["recover"] == "fresh");

    cache.values["bad"] = "invalid";
    auto retryFailure = ArtworkImageLoader::loadCached(
        cache,
        "bad",
        [&] {
            ++downloads;
            return ApiValueResult<std::string>{{false, "network failed"}, {}};
        },
        decode
    );
    assert(!retryFailure.ok());
    assert(retryFailure.failure == ArtworkLoadFailure::Download);
    assert(retryFailure.error == "network failed");
    assert(cache.values.find("bad") == cache.values.end());
    assert(cache.erases == 2);
    assert(cache.writes == 1);
    return 0;
}
