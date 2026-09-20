#pragma once

#include "playback_coordinator.hpp"
#include "trickplay_policy.hpp"
#include "trickplay_preview.hpp"
#include "trickplay_tile_executor.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

struct TrickplayCompletionEffects {
    bool unavailable = false;
    int tileIndex = -1;
    std::string error;
};

template <typename Async> class TrickplayCoordinator {
public:
    TrickplayCoordinator(TrickplayPreviewState& state, PlaybackCoordinator& playback, JellyfinSession& session,
                         Async& async)
        : state_(state), playback_(playback), session_(session), async_(async) {}

    [[nodiscard]] std::optional<uint32_t> clear(uint64_t rendererGeneration) {
        std::optional<uint32_t> texture;
        if (state_.texture() != 0 && state_.textureGeneration() == rendererGeneration) texture = state_.texture();
        state_.reset();
        return texture;
    }

    [[nodiscard]] std::optional<uint32_t> request(int positionMs, uint64_t rendererGeneration) {
        const auto& active = playback_.session().activeItem();
        const auto& info = active.trickplay;
        if (!session_.valid() || active.id.empty() || !info.valid()) return std::nullopt;
        const TrickplayFrame frame = trickplayFrameForPosition(positionMs, info.intervalMs, info.thumbnailCount,
                                                               info.tileWidth, info.tileHeight);
        if (!frame.valid()) return std::nullopt;

        state_.showAt(positionMs, std::chrono::steady_clock::now());
        if (state_.matchesTile(active.id, frame.tileIndex) && !state_.failed()) return std::nullopt;

        std::optional<uint32_t> oldTexture;
        if (state_.texture() != 0 && state_.textureGeneration() == rendererGeneration) oldTexture = state_.texture();
        state_.beginTile(active.id, frame.tileIndex);
        if (!async_.load(session_,
                         TrickplayTileRequest{.itemId = active.id, .trickplay = info, .tileIndex = frame.tileIndex}))
            state_.markFailed();
        return oldTexture;
    }

    [[nodiscard]] TrickplayCompletionEffects complete(TrickplayTileCompletion& completion) {
        if (!state_.matchesTile(completion.itemId, completion.tileIndex)) return {};
        if (!completion.decoded.valid()) {
            state_.markFailed();
            return {.unavailable = true, .tileIndex = completion.tileIndex, .error = completion.error};
        }
        state_.applyDecoded(std::move(completion.decoded));
        return {};
    }

private:
    TrickplayPreviewState& state_;
    PlaybackCoordinator& playback_;
    JellyfinSession& session_;
    Async& async_;
};
