#pragma once

#include "image_decoder.hpp"
#include "renderer.hpp"

#include <android/asset_manager.h>
#include <android/log.h>

#include <cstddef>
#include <cstdint>
#include <string>

class BrandMark {
public:
    template <typename Decoder> void load(AAssetManager* manager, Decoder& decoder) {
        if (!manager) {
            __android_log_print(ANDROID_LOG_WARN, "sloppaTV", "Brand mark asset manager unavailable");
            return;
        }
        AAsset* asset = AAssetManager_open(manager, "sloppatv_brand_mark.png", AASSET_MODE_BUFFER);
        if (!asset) {
            __android_log_print(ANDROID_LOG_WARN, "sloppaTV", "Brand mark asset unavailable");
            return;
        }
        const off_t length = AAsset_getLength(asset);
        std::string encoded(length > 0 ? static_cast<std::size_t>(length) : 0, '\0');
        const int bytesRead = encoded.empty() ? 0 : AAsset_read(asset, encoded.data(), encoded.size());
        AAsset_close(asset);
        if (bytesRead <= 0) {
            __android_log_print(ANDROID_LOG_WARN, "sloppaTV", "Brand mark asset was empty");
            return;
        }
        encoded.resize(static_cast<std::size_t>(bytesRead));
        std::string decodeError;
        decoded_ = decoder.decode(encoded, decodeError);
        if (!decoded_.valid()) {
            __android_log_print(ANDROID_LOG_WARN, "sloppaTV", "Brand mark decode failed: %s", decodeError.c_str());
        }
    }

    [[nodiscard]] bool draw(Renderer& renderer, float x, float y, float size) {
        if (!decoded_.valid() || !renderer.ready()) return false;
        if (textureGeneration_ != renderer.generation()) {
            texture_ = 0;
            textureGeneration_ = renderer.generation();
        }
        if (texture_ == 0) {
            texture_ = renderer.createTexture(decoded_.width, decoded_.height, decoded_.rgba.data());
        }
        if (texture_ == 0) return false;
        renderer.image(texture_, x, y, size, size);
        return true;
    }

    void release(Renderer& renderer) {
        if (texture_ != 0 && renderer.ready()) renderer.deleteTexture(texture_);
        texture_ = 0;
        textureGeneration_ = 0;
    }

private:
    DecodedImage decoded_;
    GLuint texture_ = 0;
    uint64_t textureGeneration_ = 0;
};
