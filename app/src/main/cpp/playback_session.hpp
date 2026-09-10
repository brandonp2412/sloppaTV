#pragma once

#include "app_settings.hpp"
#include "jellyfin_types.hpp"

#include <chrono>
#include <utility>
#include <vector>

class PlaybackSessionState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto kMediaSegmentsRetryDelay = std::chrono::seconds(5);

    void reset() { begin(VideoZoomMode::Fit); }

    void begin(VideoZoomMode zoomMode) {
        mediaSegments_.clear();
        mediaSegmentsRequested_ = false;
        mediaSegmentsRetryAfter_ = {};
        fallbackAttempted_ = false;
        zoomMode_ = zoomMode;
    }

    [[nodiscard]] bool mediaSegmentsRequested() const { return mediaSegmentsRequested_; }
    bool beginMediaSegmentsRequest(TimePoint now = Clock::now()) {
        if (mediaSegmentsRequested_ || (mediaSegmentsRetryAfter_ != TimePoint{} && now < mediaSegmentsRetryAfter_)) {
            return false;
        }
        mediaSegmentsRequested_ = true;
        mediaSegmentsRetryAfter_ = {};
        return true;
    }
    void mediaSegmentsRequestFailed(TimePoint now = Clock::now()) {
        mediaSegmentsRequested_ = false;
        mediaSegmentsRetryAfter_ = now + kMediaSegmentsRetryDelay;
    }
    void resetMediaSegments() {
        mediaSegmentsRequested_ = false;
        mediaSegmentsRetryAfter_ = {};
        mediaSegments_.clear();
    }
    void setMediaSegments(std::vector<JellyfinMediaSegment> segments) {
        mediaSegments_ = std::move(segments);
        mediaSegmentsRetryAfter_ = {};
    }
    [[nodiscard]] const std::vector<JellyfinMediaSegment>& mediaSegments() const { return mediaSegments_; }

    [[nodiscard]] const JellyfinMediaSegment* activeSkippableSegment(int64_t positionTicks) const {
        for (const auto& segment : mediaSegments_) {
            if (segment.endTicks - segment.startTicks < 30000000) continue;
            if (positionTicks >= segment.startTicks && positionTicks < segment.endTicks - 5000000) return &segment;
        }
        return nullptr;
    }

    [[nodiscard]] bool fallbackAttempted() const { return fallbackAttempted_; }
    void markFallbackAttempted() { fallbackAttempted_ = true; }
    void resetFallbackAttempted() { fallbackAttempted_ = false; }

    [[nodiscard]] VideoZoomMode zoomMode() const { return zoomMode_; }
    void setZoomMode(VideoZoomMode mode) { zoomMode_ = mode; }

private:
    std::vector<JellyfinMediaSegment> mediaSegments_;
    bool mediaSegmentsRequested_ = false;
    TimePoint mediaSegmentsRetryAfter_{};
    bool fallbackAttempted_ = false;
    VideoZoomMode zoomMode_ = VideoZoomMode::Fit;
};
