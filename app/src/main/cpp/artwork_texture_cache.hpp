#pragma once

#include "artwork_cache.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class ArtworkTextureState {
    Missing,
    Loading,
    Failed,
    InvalidDecoded,
    Unavailable,
    Ready,
};

struct ArtworkTextureResult {
    ArtworkTextureState state = ArtworkTextureState::Missing;
    ArtworkEntry* entry = nullptr;
};

class ArtworkTextureCache {
public:
    explicit ArtworkTextureCache(size_t maxEntries = 0) : cache_(maxEntries) {}

    template <typename DeleteTexture>
    bool beginLoad(const std::string& key, uint64_t textureGeneration, DeleteTexture&& deleteTexture,
                   ArtworkCache::TimePoint now = ArtworkCache::Clock::now()) {
        return cache_.beginLoad(
            key, [&](ArtworkEntry& entry) { releaseTexture(entry, textureGeneration, deleteTexture); }, now);
    }

    void markFailed(const std::string& key, ArtworkCache::TimePoint now = ArtworkCache::Clock::now()) {
        cache_.markFailed(key, now);
    }

    bool markReady(const std::string& key, DecodedImage decoded) { return cache_.markReady(key, std::move(decoded)); }

    template <typename CreateTexture>
    ArtworkTextureResult prepare(const std::string& key, uint64_t textureGeneration, CreateTexture&& createTexture) {
        ArtworkEntry* entry = cache_.find(key);
        if (!entry) return {ArtworkTextureState::Missing, nullptr};
        if (entry->state == ArtworkState::Failed) return {ArtworkTextureState::Failed, entry};
        if (entry->state != ArtworkState::Ready) return {ArtworkTextureState::Loading, entry};

        if (entry->textureGeneration != textureGeneration || entry->texture == 0) {
            if (!entry->decoded.valid()) return {ArtworkTextureState::InvalidDecoded, entry};
            entry->sourceWidth = entry->decoded.width;
            entry->sourceHeight = entry->decoded.height;
            entry->texture = createTexture(entry->decoded.width, entry->decoded.height, entry->decoded.rgba.data());
            entry->textureGeneration = textureGeneration;
            if (entry->texture != 0) std::vector<uint8_t>().swap(entry->decoded.rgba);
        }

        if (entry->texture == 0) return {ArtworkTextureState::Unavailable, entry};
        return {ArtworkTextureState::Ready, entry};
    }

    template <typename DeleteTexture>
    void erase(const std::string& key, uint64_t textureGeneration, DeleteTexture&& deleteTexture) {
        cache_.erase(key, [&](ArtworkEntry& entry) { releaseTexture(entry, textureGeneration, deleteTexture); });
    }

    template <typename DeleteTexture> void clear(uint64_t textureGeneration, DeleteTexture&& deleteTexture) {
        cache_.clear([&](ArtworkEntry& entry) { releaseTexture(entry, textureGeneration, deleteTexture); });
    }

    [[nodiscard]] size_t size() const { return cache_.size(); }

    [[nodiscard]] const ArtworkEntry* peek(const std::string& key) const { return cache_.peek(key); }

private:
    template <typename DeleteTexture>
    static void releaseTexture(ArtworkEntry& entry, uint64_t textureGeneration, DeleteTexture& deleteTexture) {
        if (entry.texture != 0 && entry.textureGeneration == textureGeneration) {
            deleteTexture(entry.texture);
        }
        entry.texture = 0;
        entry.textureGeneration = 0;
    }

    ArtworkCache cache_;
};
