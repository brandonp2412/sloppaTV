#pragma once
#include "remux_prefetch_status.hpp"

#include <algorithm>
#include <chrono>
#include <string>

enum class PlayerBufferPhase { Resolving, Connecting, Buffering, Stalled, Playing, Paused };

// Shows only verifiable libmpv buffered time. The server's downloaded torrent
// pieces are not necessarily contiguous playable media, so never label those
// bytes as available seconds ahead.
class PlayerBufferIndicator {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void reset() { *this = {}; }

    void observe(bool resolving, bool preparing, bool playing, bool paused, bool cachePaused, int positionMs,
                 int cacheAheadMs, bool remux, bool episode, TimePoint now = Clock::now()) {
        remux_ = remux;
        episode_ = episode;
        cacheAheadMs_ = std::max(0, cacheAheadMs);
        if (resolving) {
            phase_ = PlayerBufferPhase::Resolving;
            if (phaseStarted_ == TimePoint{}) phaseStarted_ = now;
            lastPositionMs_ = positionMs;
            return;
        }
        if (preparing) {
            if (phase_ != PlayerBufferPhase::Connecting) phaseStarted_ = now;
            phase_ = PlayerBufferPhase::Connecting;
            lastPositionMs_ = positionMs;
            return;
        }
        if (paused) {
            phase_ = PlayerBufferPhase::Paused;
            lastAdvancing_ = now;
            lastPositionMs_ = positionMs;
            return;
        }
        if (!playing) {
            reset();
            return;
        }
        if (lastAdvancing_ == TimePoint{} || positionMs > lastPositionMs_ + 125 || positionMs + 500 < lastPositionMs_) {
            lastAdvancing_ = now;
        }
        lastPositionMs_ = positionMs;
        const auto idle = now - lastAdvancing_;
        if (cachePaused || idle >= std::chrono::seconds(3)) {
            phase_ = idle >= std::chrono::seconds(15) ? PlayerBufferPhase::Stalled : PlayerBufferPhase::Buffering;
        } else {
            phase_ = PlayerBufferPhase::Playing;
        }
    }

    [[nodiscard]] PlayerBufferPhase phase() const { return phase_; }

    [[nodiscard]] int cacheAheadMs() const { return cacheAheadMs_; }

    [[nodiscard]] bool interrupted() const {
        return phase_ == PlayerBufferPhase::Buffering || phase_ == PlayerBufferPhase::Stalled;
    }

    [[nodiscard]] std::string statusText(TimePoint now = Clock::now()) const {
        const bool remoteProgressFresh = remux_ && remoteReceivedAt_ != TimePoint{} &&
                                         now - remoteReceivedAt_ <= std::chrono::seconds(20) &&
                                         remotePrefetch_.livePeers > 0 && remotePrefetch_.downloadMiBps > 0.0;
        switch (phase_) {
        case PlayerBufferPhase::Resolving:
            return episode_ ? (remux_ ? "Resolving Remux episode stream" : "Resolving episode stream")
                            : (remux_ ? "Resolving Remux stream" : "Resolving stream");
        case PlayerBufferPhase::Connecting:
            if (phaseStarted_ != TimePoint{} && now - phaseStarted_ >= std::chrono::seconds(12)) {
                if (remoteProgressFresh) return "Buffering—downloading slowly";
                return remux_ ? "Waiting for Remux stream data" : "Waiting for video data";
            }
            return remux_ ? "Opening Remux stream" : "Opening video stream";
        case PlayerBufferPhase::Buffering:
            if (remoteProgressFresh) return "Buffering—downloading slowly";
            return remux_ ? "Buffering from Remux" : "Buffering video";
        case PlayerBufferPhase::Stalled:
            if (remoteProgressFresh) return "Buffering—downloading slowly";
            return remux_ ? "Remux stream stalled - waiting for data" : "Stream stalled - waiting for data";
        case PlayerBufferPhase::Playing:
            return {};
        case PlayerBufferPhase::Paused:
            return "Paused";
        }
        return {};
    }

    void setRemotePrefetch(RemuxPrefetchStatus stats, TimePoint now = Clock::now()) {
        remotePrefetch_ = stats;
        remoteReceivedAt_ = now;
    }

    [[nodiscard]] std::string remotePrefetchText(TimePoint now = Clock::now()) const {
        if (!remux_ || remoteReceivedAt_ == TimePoint{} || now - remoteReceivedAt_ > std::chrono::seconds(20))
            return {};
        const auto percent = [](std::uint64_t cached, std::uint64_t total) {
            return total > 0 ? std::min<std::uint64_t>(100, cached * 100 / total) : 0;
        };
        std::string label =
            "Remux cached: current " +
            std::to_string(percent(remotePrefetch_.currentCachedBytes, remotePrefetch_.currentTotalBytes)) + "%";
        if (remotePrefetch_.nextTotalBytes > 0) {
            label += " | next " +
                     std::to_string(percent(remotePrefetch_.nextCachedBytes, remotePrefetch_.nextTotalBytes)) + "%";
        }
        label += " | " + std::to_string(remotePrefetch_.livePeers) + " peers";
        label += " | " +
                 std::to_string(static_cast<int>(remotePrefetch_.downloadMiBps * 10.0 + 0.5) / 10.0).substr(0, 4) +
                 " MiB/s";
        return label;
    }

private:
    PlayerBufferPhase phase_ = PlayerBufferPhase::Resolving;
    bool remux_ = false;
    bool episode_ = false;
    int cacheAheadMs_ = 0;
    int lastPositionMs_ = -1;
    TimePoint phaseStarted_{};
    TimePoint lastAdvancing_{};
    RemuxPrefetchStatus remotePrefetch_{};
    TimePoint remoteReceivedAt_{};
};
