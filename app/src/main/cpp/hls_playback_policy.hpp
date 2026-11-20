#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

inline bool serverStreamSeekNeedsRestart(bool directPlay, std::string_view url, int targetMs, int baseMs) {
    // A full-length Jellyfin HLS VOD playlist is seekable by its native
    // segment index. Restarting it reopens segment zero, making the displayed
    // timeline disagree with the video. Jellyfin also rejects StartTimeTicks
    // on dynamic HLS segment requests, so don't try URL-based HLS seeks.
    // Non-HLS server streams still require re-negotiation if seeking before
    // their known start offset.
    return !directPlay && url.find(".m3u8") == std::string_view::npos && std::max(0, targetMs) < baseMs;
}

inline std::optional<int> prematureHlsEndPosition(int observedMs, int recentSeekMs, int durationMs) {
    const int position = recentSeekMs >= 0 ? recentSeekMs : observedMs;
    if (position < 0 || durationMs <= 0 || static_cast<int64_t>(position) + 15000 >= durationMs) return std::nullopt;
    return position;
}

class HlsRecoveryBudget {
public:
    using Clock = std::chrono::steady_clock;

    bool consume(const std::string& item, const std::string& source, Clock::time_point now) {
        if (item_ != item || source_ != source) {
            item_ = item;
            source_ = source;
            attempted_ = false;
        }
        if (attempted_) return false;
        attempted_ = true;
        playingSince_.reset();
        (void)now;
        return true;
    }

    void observePlaying(bool playing, Clock::time_point now) {
        if (!playing) {
            playingSince_.reset();
        } else if (!playingSince_) {
            playingSince_ = now;
        } else if (now - *playingSince_ >= std::chrono::seconds(30)) {
            attempted_ = false;
        }
    }

    void reset() {
        attempted_ = false;
        playingSince_.reset();
    }

private:
    std::string item_;
    std::string source_;
    std::optional<Clock::time_point> playingSince_;
    bool attempted_ = false;
};
