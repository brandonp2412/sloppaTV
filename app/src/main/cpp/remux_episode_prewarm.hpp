#pragma once

#include "http_response.hpp"
#include "jellyfin_types.hpp"
#include "playback_queue.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// A broken upstream proxy can answer a byte-range probe with a 206 HTML/JSON
// error page. That is not evidence of playable buffered media.
[[nodiscard]] inline bool isUsableRemuxPrefetchRange(const HttpResponse& response) {
    if (response.status != 206 || response.body.empty() || response.body.size() > 65536) return false;
    std::string_view bytes(response.body);
    while (!bytes.empty() &&
           (bytes.front() == ' ' || bytes.front() == '\n' || bytes.front() == '\r' || bytes.front() == '\t'))
        bytes.remove_prefix(1);
    if (bytes.size() >= 3 && bytes.substr(0, 3) == "\xef\xbb\xbf") bytes.remove_prefix(3);
    while (!bytes.empty() &&
           (bytes.front() == ' ' || bytes.front() == '\n' || bytes.front() == '\r' || bytes.front() == '\t'))
        bytes.remove_prefix(1);
    return !bytes.empty() && bytes.front() != '<' && bytes.front() != '{' && bytes.front() != '[';
}

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

// Remux now selects complete current and next episode files and retains the
// torrent for two hours after playback. Start the next file early, rather
// than leaving its first download until the end of this episode.
class RemuxEpisodePrewarmState {
public:
    static constexpr int64_t kPrewarmStartMs = 5'000;

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
        if (currentItemId_.empty() || submitted_ || !isRemux || !playing || positionMs < kPrewarmStartMs ||
            (durationMs > 0 && positionMs >= durationMs))
            return std::nullopt;
        submitted_ = true;
        return nextItem_;
    }

private:
    std::string currentItemId_;
    JellyfinItem nextItem_;
    bool submitted_ = false;
};
