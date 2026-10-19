#pragma once
#include "remux_prefetch_status.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>

enum class PlayerBufferPhase { Resolving, Connecting, Buffering, Stalled, Playing, Paused };

// Shows only verifiable libmpv buffered time. The server's downloaded torrent
// pieces are not necessarily contiguous playable media, so never label those
// bytes as available seconds ahead.
class PlayerBufferIndicator {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void reset() {
        const auto resolve = resolveAverageSeconds_;
        const auto connect = connectAverageSeconds_;
        const auto resolveCount = resolveSamples_;
        const auto connectCount = connectSamples_;
        *this = {};
        resolveAverageSeconds_ = resolve;
        connectAverageSeconds_ = connect;
        resolveSamples_ = resolveCount;
        connectSamples_ = connectCount;
    }

    void clearRemotePrefetch() {
        remotePrefetch_ = {};
        remoteReceivedAt_ = {};
        observedReadRate_ = 0.0;
        positiveReadSamples_ = 0;
        lastReadProgressAt_ = {};
    }

    void observe(bool resolving, bool preparing, bool playing, bool paused, bool cachePaused, int positionMs,
                 int cacheAheadMs, bool remux, bool episode, TimePoint now = Clock::now()) {
        remux_ = remux;
        episode_ = episode;
        cacheAheadMs_ = std::max(0, cacheAheadMs);
        if (resolving) {
            if (phase_ != PlayerBufferPhase::Resolving || phaseStarted_ == TimePoint{}) {
                phaseStarted_ = now;
                lastAdvancing_ = {};
            }
            phase_ = PlayerBufferPhase::Resolving;
            lastPositionMs_ = positionMs;
            return;
        }
        if (preparing) {
            if (phase_ != PlayerBufferPhase::Connecting) {
                if (phase_ == PlayerBufferPhase::Resolving && phaseStarted_ != TimePoint{})
                    learnDuration(now - phaseStarted_, resolveAverageSeconds_, resolveSamples_);
                phaseStarted_ = now;
                lastAdvancing_ = {};
            }
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
        if (phase_ == PlayerBufferPhase::Connecting && phaseStarted_ != TimePoint{})
            learnDuration(now - phaseStarted_, connectAverageSeconds_, connectSamples_);
        if (lastAdvancing_ == TimePoint{} || positionMs > lastPositionMs_ + 125 || positionMs + 500 < lastPositionMs_) {
            lastAdvancing_ = now;
            // Compare against the last confirmed advance, not the previous
            // render frame: normal 16-50 ms increments must accumulate past
            // the 125 ms jitter threshold.
            lastPositionMs_ = positionMs;
        }
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
        // Aggregate torrent throughput does not prove the selected startup
        // range is growing. Only claim slow downloading when those bytes advance.
        const bool remoteProgressFresh = remux_ && remoteReceivedAt_ != TimePoint{} &&
                                         now - remoteReceivedAt_ <= std::chrono::seconds(20) &&
                                         remotePrefetch_.livePeers > 0 && remotePrefetch_.downloadMiBps > 0.0 &&
                                         remotePrefetch_.startupRequiredBytes > 0 && !remotePrefetch_.startupReady &&
                                         lastReadProgressAt_ != TimePoint{} &&
                                         now - lastReadProgressAt_ <= std::chrono::seconds(20);
        switch (phase_) {
        case PlayerBufferPhase::Resolving: {
            const std::string label = episode_
                                          ? (remux_ ? "Resolving Remux episode stream" : "Resolving episode stream")
                                          : (remux_ ? "Resolving Remux stream" : "Resolving stream");
            return label + historicalEta(now, resolveAverageSeconds_, resolveSamples_);
        }
        case PlayerBufferPhase::Connecting: {
            const std::string estimate =
                readinessEta(now).value_or(historicalEta(now, connectAverageSeconds_, connectSamples_));
            if (phaseStarted_ != TimePoint{} && now - phaseStarted_ >= std::chrono::seconds(12)) {
                if (remoteProgressFresh) return "Buffering—downloading slowly" + estimate;
                return std::string(remux_ ? "Waiting for Remux stream data" : "Waiting for video data") + estimate;
            }
            return std::string(remux_ ? "Opening Remux stream" : "Opening video stream") + estimate;
        }
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
        if (stats.startupRequiredBytes == 0 || stats.startupReady ||
            stats.startupRequiredBytes != remotePrefetch_.startupRequiredBytes ||
            stats.startupReadableBytes < remotePrefetch_.startupReadableBytes) {
            observedReadRate_ = 0.0;
            positiveReadSamples_ = 0;
            lastReadProgressAt_ = {};
        } else if (stats.startupReadableBytes > remotePrefetch_.startupReadableBytes &&
                   remoteReceivedAt_ != TimePoint{} && now > remoteReceivedAt_) {
            const double interval = std::chrono::duration<double>(now - remoteReceivedAt_).count();
            const double rate =
                static_cast<double>(stats.startupReadableBytes - remotePrefetch_.startupReadableBytes) / interval;
            if (std::isfinite(rate) && rate > 0.0) {
                observedReadRate_ = positiveReadSamples_ ? 0.35 * rate + 0.65 * observedReadRate_ : rate;
                ++positiveReadSamples_;
                lastReadProgressAt_ = now;
            }
        }
        remotePrefetch_ = stats;
        remoteReceivedAt_ = now;
    }

    [[nodiscard]] std::optional<float> startupProgress(TimePoint now = Clock::now()) const {
        if (!remux_ || remoteReceivedAt_ == TimePoint{} || now - remoteReceivedAt_ > std::chrono::seconds(8) ||
            remotePrefetch_.startupReady || remotePrefetch_.startupRequiredBytes == 0)
            return std::nullopt;
        return static_cast<float>(std::min(1.0, static_cast<double>(remotePrefetch_.startupReadableBytes) /
                                                    static_cast<double>(remotePrefetch_.startupRequiredBytes)));
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
    static void learnDuration(Clock::duration duration, double& average, int& samples) {
        const double seconds = std::chrono::duration<double>(duration).count();
        if (seconds < 0.2 || seconds > 180.0) return;
        average = samples == 0 ? seconds : average * 0.75 + seconds * 0.25;
        ++samples;
    }

    [[nodiscard]] std::string historicalEta(TimePoint now, double average, int samples) const {
        if (!remux_ || samples < 3 || phaseStarted_ == TimePoint{}) return {};
        const double remaining = average - std::chrono::duration<double>(now - phaseStarted_).count();
        if (remaining < 3.0 || remaining > 180.0) return {};
        return " · ~" + std::to_string(static_cast<int>(std::ceil(remaining / 5.0)) * 5) + "s remaining";
    }

    [[nodiscard]] std::optional<std::string> readinessEta(TimePoint now) const {
        if (!remux_ || remotePrefetch_.startupReady || remotePrefetch_.startupRequiredBytes == 0 ||
            remotePrefetch_.startupReadableBytes >= remotePrefetch_.startupRequiredBytes || positiveReadSamples_ < 2 ||
            observedReadRate_ <= 0.0 || lastReadProgressAt_ == TimePoint{} ||
            now - lastReadProgressAt_ > std::chrono::seconds(8))
            return std::nullopt;
        const double remaining =
            static_cast<double>(remotePrefetch_.startupRequiredBytes - remotePrefetch_.startupReadableBytes) /
            observedReadRate_;
        if (!std::isfinite(remaining) || remaining < 1.0 || remaining > 300.0) return std::nullopt;
        return " · ~" + std::to_string(static_cast<int>(std::ceil(remaining / 5.0)) * 5) + "s remaining";
    }

    PlayerBufferPhase phase_ = PlayerBufferPhase::Resolving;
    bool remux_ = false;
    bool episode_ = false;
    int cacheAheadMs_ = 0;
    int lastPositionMs_ = -1;
    TimePoint phaseStarted_{};
    TimePoint lastAdvancing_{};
    RemuxPrefetchStatus remotePrefetch_{};
    TimePoint remoteReceivedAt_{};
    TimePoint lastReadProgressAt_{};
    double observedReadRate_ = 0.0;
    int positiveReadSamples_ = 0;
    double resolveAverageSeconds_ = 0.0;
    double connectAverageSeconds_ = 0.0;
    int resolveSamples_ = 0;
    int connectSamples_ = 0;
};
