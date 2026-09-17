#pragma once

#include "decoded_image.hpp"

#include <string>
#include <utility>

enum class ArtworkLoadSource {
    Network,
    DiskCache,
};

enum class ArtworkLoadFailure {
    None,
    Download,
    Decode,
};

struct ArtworkLoadResult {
    DecodedImage decoded;
    std::string error;
    ArtworkLoadSource source = ArtworkLoadSource::Network;
    ArtworkLoadFailure failure = ArtworkLoadFailure::None;

    [[nodiscard]] bool ok() const { return failure == ArtworkLoadFailure::None && decoded.valid(); }
};

class ArtworkImageLoader {
public:
    template <typename Download, typename Decode> static ArtworkLoadResult load(Download&& download, Decode&& decode) {
        ArtworkLoadResult result;
        auto bytes = download();
        if (!bytes.ok) {
            result.failure = ArtworkLoadFailure::Download;
            result.error = std::move(bytes.error);
            return result;
        }

        result.decoded = decode(bytes.value, result.error);
        if (!result.decoded.valid()) result.failure = ArtworkLoadFailure::Decode;
        return result;
    }

    template <typename DiskCache, typename Download, typename Decode>
    static ArtworkLoadResult loadCached(DiskCache& diskCache, const std::string& key, Download&& download,
                                        Decode&& decode) {
        if (auto cached = diskCache.read(key)) {
            ArtworkLoadResult result;
            result.source = ArtworkLoadSource::DiskCache;
            result.decoded = decode(*cached, result.error);
            if (result.decoded.valid()) return result;
            diskCache.erase(key);
        }

        ArtworkLoadResult result;
        auto bytes = download();
        if (!bytes.ok) {
            result.failure = ArtworkLoadFailure::Download;
            result.error = std::move(bytes.error);
            return result;
        }

        result.decoded = decode(bytes.value, result.error);
        if (!result.decoded.valid()) {
            result.failure = ArtworkLoadFailure::Decode;
            return result;
        }
        diskCache.write(key, bytes.value);
        return result;
    }
};
