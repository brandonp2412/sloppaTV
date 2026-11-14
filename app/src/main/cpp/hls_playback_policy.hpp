#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

inline bool serverStreamSeekNeedsRestart(bool directPlay, std::string_view url, int targetMs, int baseMs) {
    // A server's rolling playlist may have dropped the requested segment while
    // paused. Negotiate the requested position instead of seeking stale media.
    return !directPlay && (std::max(0, targetMs) < baseMs || url.find(".m3u8") != std::string_view::npos);
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
