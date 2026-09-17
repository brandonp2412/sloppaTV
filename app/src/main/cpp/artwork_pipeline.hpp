#pragma once

#include "artwork_image_loader.hpp"
#include "artwork_texture_cache.hpp"

#include <cstdint>
#include <string>
#include <utility>

class ArtworkPipeline {
public:
    explicit ArtworkPipeline(size_t maxEntries = 0) : cache_(maxEntries) {}

    template <typename RendererLike> bool beginLoad(const std::string& key, RendererLike& renderer) {
        return cache_.beginLoad(key, renderer.generation(), [&](uint32_t texture) { renderer.deleteTexture(texture); });
    }

    void completeLoad(const std::string& key, ArtworkLoadResult loaded) {
        if (!loaded.ok()) {
            cache_.markFailed(key);
            return;
        }
        cache_.markReady(key, std::move(loaded.decoded));
    }

    template <typename RendererLike, typename Request>
    ArtworkEntry* readyTexture(const std::string& key, RendererLike& renderer, Request&& request) {
        const ArtworkTextureResult prepared =
            cache_.prepare(key, renderer.generation(), [&](int width, int height, const uint8_t* pixels) {
                return renderer.createTexture(width, height, pixels);
            });
        if (prepared.state == ArtworkTextureState::Missing || prepared.state == ArtworkTextureState::Failed) {
            request();
            return nullptr;
        }
        if (prepared.state == ArtworkTextureState::InvalidDecoded) {
            erase(key, renderer);
            request();
            return nullptr;
        }
        return prepared.state == ArtworkTextureState::Ready ? prepared.entry : nullptr;
    }

    template <typename RendererLike> void erase(const std::string& key, RendererLike& renderer) {
        cache_.erase(key, renderer.generation(), [&](uint32_t texture) { renderer.deleteTexture(texture); });
    }

    template <typename RendererLike> void clear(RendererLike& renderer) {
        cache_.clear(renderer.generation(), [&](uint32_t texture) { renderer.deleteTexture(texture); });
    }

    [[nodiscard]] size_t size() const { return cache_.size(); }

    [[nodiscard]] const ArtworkEntry* peek(const std::string& key) const { return cache_.peek(key); }

private:
    ArtworkTextureCache cache_;
};
