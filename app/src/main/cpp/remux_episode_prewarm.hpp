#pragma once

#include "jellyfin_types.hpp"
#include "playback_queue.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

// Playlist/Play All bypasses the next-episode API lookup. Only speculate on
// the next queued episode from the same series, never unrelated queue entries.
[[nodiscard]] inline const JellyfinItem* remuxQueuedNextEpisode(const JellyfinItem& current,
                                                                const PlaybackQueueState& queue) {
    if (current.type != "Episode" || current.seriesId.empty()) return nullptr;
    const JellyfinItem* next = queue.itemAt(queue.nextIndex(false));
    if (!next || next->type != "Episode" || next->seriesId != current.seriesId || next->id == current.id)
        return nullptr;
    return next;
}

// Remux keeps a torrent lease briefly after a stream closes. Fetch a small
// initial byte range shortly before the next episode is needed, not at the
// start of a potentially hour-long episode when the lease would expire.
class RemuxEpisodePrewarmState {
public:
    static constexpr int64_t kPrewarmWindowMs = 30'000;

    void remember(std::string currentItemId, JellyfinItem nextItem) {
        if (currentItemId.empty() || nextItem.id.empty() || currentItemId == nextItem.id) return;
        if (currentItemId_ == currentItemId && nextItem_.id == nextItem.id) return;
        currentItemId_ = std::move(currentItemId);
        nextItem_ = std::move(nextItem);
        submitted_ = false;
    }

    void clear() {
        currentItemId_.clear();
        nextItem_ = {};
        submitted_ = false;
    }

    [[nodiscard]] std::optional<JellyfinItem> takeDue(const std::string& currentItemId, int64_t positionMs,
                                                      int64_t durationMs, bool playing, bool isRemux) {
        if (!currentItemId_.empty() && currentItemId != currentItemId_) clear();
        if (currentItemId_.empty() || submitted_ || !isRemux || !playing || positionMs < 0 || durationMs <= 0 ||
            positionMs >= durationMs || durationMs - positionMs > kPrewarmWindowMs)
            return std::nullopt;
        submitted_ = true;
        return nextItem_;
    }

private:
    std::string currentItemId_;
    JellyfinItem nextItem_;
    bool submitted_ = false;
};
