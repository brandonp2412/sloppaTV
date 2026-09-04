#pragma once

#include <chrono>

class PlaybackTelemetryState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void reset() {
        playbackStartReported_ = false;
        lastProgressReport_ = {};
        resetReadIntervals();
    }

    void beginPlayback(TimePoint now) {
        playbackStartReported_ = false;
        lastProgressReport_ = now;
        resetReadIntervals();
    }

    [[nodiscard]] bool playbackStartReported() const { return playbackStartReported_; }
    bool markPlaybackStartReported() {
        if (playbackStartReported_) return false;
        playbackStartReported_ = true;
        return true;
    }
    void clearPlaybackStartReported() { playbackStartReported_ = false; }

    [[nodiscard]] bool shouldReadPlayback(TimePoint now, bool force) const {
        return force || lastPlaybackTelemetryRead_ == TimePoint{}
            || now - lastPlaybackTelemetryRead_ >= std::chrono::milliseconds(250);
    }
    void markPlaybackRead(TimePoint now) { lastPlaybackTelemetryRead_ = now; }

    [[nodiscard]] bool shouldProbeDuration(TimePoint now, bool force) const {
        return force || lastPlaybackDurationProbe_ == TimePoint{}
            || now - lastPlaybackDurationProbe_ >= std::chrono::seconds(2);
    }
    void markDurationProbe(TimePoint now) { lastPlaybackDurationProbe_ = now; }

    void resetReadIntervals() {
        lastPlaybackTelemetryRead_ = {};
        lastPlaybackDurationProbe_ = {};
    }

    [[nodiscard]] bool progressReportDue(TimePoint now) const {
        return lastProgressReport_ == TimePoint{} || now - lastProgressReport_ >= std::chrono::seconds(10);
    }
    void markProgressReport(TimePoint now) { lastProgressReport_ = now; }

private:
    bool playbackStartReported_ = false;
    TimePoint lastProgressReport_{};
    TimePoint lastPlaybackTelemetryRead_{};
    TimePoint lastPlaybackDurationProbe_{};
};
