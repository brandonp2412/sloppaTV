#pragma once

#include "decoded_image.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

enum class TrickplayPreviewLoadState {
    Idle,
    Loading,
    Ready,
    Failed,
};

class TrickplayPreviewState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void reset() {
        itemId_.clear();
        tileIndex_ = -1;
        state_ = TrickplayPreviewLoadState::Idle;
        decoded_ = {};
        texture_ = 0;
        textureGeneration_ = 0;
        positionMs_ = -1;
        visibleUntil_ = {};
    }

    void showAt(int positionMs, TimePoint now) {
        positionMs_ = std::max(0, positionMs);
        visibleUntil_ = now + std::chrono::seconds(4);
    }

    [[nodiscard]] bool matchesTile(std::string_view itemId, int tileIndex) const {
        return itemId_ == itemId && tileIndex_ == tileIndex;
    }

    [[nodiscard]] bool failed() const { return state_ == TrickplayPreviewLoadState::Failed; }

    void beginTile(std::string itemId, int tileIndex) {
        itemId_ = std::move(itemId);
        tileIndex_ = tileIndex;
        state_ = TrickplayPreviewLoadState::Loading;
        decoded_ = {};
        texture_ = 0;
        textureGeneration_ = 0;
    }

    void applyDecoded(DecodedImage decoded) {
        decoded_ = std::move(decoded);
        state_ = decoded_.valid() ? TrickplayPreviewLoadState::Ready : TrickplayPreviewLoadState::Failed;
    }

    void markFailed() {
        state_ = TrickplayPreviewLoadState::Failed;
        decoded_ = {};
        texture_ = 0;
        textureGeneration_ = 0;
    }

    [[nodiscard]] bool visible(TimePoint now, std::string_view itemId) const {
        return now < visibleUntil_
            && positionMs_ >= 0
            && state_ == TrickplayPreviewLoadState::Ready
            && itemId_ == itemId
            && decoded_.valid();
    }

    [[nodiscard]] int positionMs() const { return positionMs_; }
    [[nodiscard]] int tileIndex() const { return tileIndex_; }
    [[nodiscard]] const DecodedImage& decoded() const { return decoded_; }
    [[nodiscard]] uint32_t texture() const { return texture_; }
    [[nodiscard]] uint64_t textureGeneration() const { return textureGeneration_; }

    void setTexture(uint32_t texture, uint64_t generation) {
        texture_ = texture;
        textureGeneration_ = generation;
    }

    void clearTexture() {
        texture_ = 0;
        textureGeneration_ = 0;
    }

private:
    std::string itemId_;
    int tileIndex_ = -1;
    TrickplayPreviewLoadState state_ = TrickplayPreviewLoadState::Idle;
    DecodedImage decoded_;
    uint32_t texture_ = 0;
    uint64_t textureGeneration_ = 0;
    int positionMs_ = -1;
    TimePoint visibleUntil_{};
};
