#pragma once

#include "media_player_policy.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>

class PlayerScreenState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr std::size_t controlCount() { return 3; }

    void resetSession() {
        controlsActive_ = false;
        controlSelection_ = 0;
        overlayUntil_ = {};
        resetPosition();
    }

    void resetPosition() {
        positionMs_ = 0;
        durationMs_ = 0;
        pendingSeekTargetMs_ = -1;
        lastSeekIssued_ = {};
    }

    void beginPlayback(int positionMs, int durationMs) {
        controlsActive_ = false;
        controlSelection_ = 0;
        positionMs_ = std::max(0, positionMs);
        durationMs_ = std::max(0, durationMs);
        pendingSeekTargetMs_ = -1;
        lastSeekIssued_ = {};
    }

    [[nodiscard]] bool controlsActive() const { return controlsActive_; }
    [[nodiscard]] int controlSelection() const { return controlSelection_; }

    void showControls(TimePoint now) {
        controlsActive_ = true;
        controlSelection_ = 0;
        showOverlayFor(now, std::chrono::seconds(10));
    }

    void hideControls() { controlsActive_ = false; }

    void moveControl(int delta) {
        controlSelection_ = std::clamp(
            controlSelection_ + delta,
            0,
            static_cast<int>(controlCount()) - 1
        );
    }

    [[nodiscard]] bool overlayVisible(TimePoint now) const { return now < overlayUntil_; }

    template <typename Duration>
    void showOverlayFor(TimePoint now, Duration duration) {
        overlayUntil_ = now + std::chrono::duration_cast<Clock::duration>(duration);
    }

    void dismissOverlay(TimePoint now) {
        controlsActive_ = false;
        overlayUntil_ = now;
    }

    [[nodiscard]] bool shouldDismissOnBack(TimePoint now) const {
        return controlsActive_ || overlayVisible(now);
    }

    [[nodiscard]] int positionMs() const { return positionMs_; }
    [[nodiscard]] int durationMs() const { return durationMs_; }
    void setPositionMs(int value) { positionMs_ = std::max(0, value); }
    void setDurationMs(int value) { durationMs_ = std::max(0, value); }

    void beginSeek(int targetMs, TimePoint now) {
        const int target = std::max(0, targetMs);
        positionMs_ = target;
        pendingSeekTargetMs_ = target;
        lastSeekIssued_ = now;
        showOverlayFor(now, std::chrono::seconds(3));
    }

    void applyObservedPosition(int observedPositionMs, TimePoint now) {
        const int observed = std::max(0, observedPositionMs);
        if (pendingSeekTargetMs_ < 0) {
            positionMs_ = observed;
            return;
        }
        const int64_t elapsedSinceSeekMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastSeekIssued_
        ).count();
        if (shouldAcceptPostSeekTelemetry(observed, pendingSeekTargetMs_, elapsedSinceSeekMs)) {
            positionMs_ = observed;
            pendingSeekTargetMs_ = -1;
            lastSeekIssued_ = {};
        } else {
            positionMs_ = pendingSeekTargetMs_;
        }
    }

    [[nodiscard]] int pendingSeekTargetMs() const { return pendingSeekTargetMs_; }

private:
    bool controlsActive_ = false;
    int controlSelection_ = 0;
    TimePoint overlayUntil_{};
    int positionMs_ = 0;
    int durationMs_ = 0;
    int pendingSeekTargetMs_ = -1;
    TimePoint lastSeekIssued_{};
};
