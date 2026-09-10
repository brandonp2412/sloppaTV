#pragma once

#include "app_settings.hpp"
#include "jellyfin_types.hpp"

#include <chrono>
#include <utility>
#include <vector>

class PlaybackSessionState {
public:
    void reset() { begin(VideoZoomMode::Fit); }

    void begin(VideoZoomMode zoomMode) {
        mediaSegments_.clear();
        mediaSegmentsRequested_ = false;
        mediaSegmentsRetryAt_ = {};
        fallbackAttempted_ = false;
        zoomMode_ = zoomMode;
    }

    [[nodiscard]] bool mediaSegmentsRequested() const { return mediaSegmentsRequested_; }
    bool beginMediaSegmentsRequest(std::chrono::steady_clock::time_point now) {
        if (mediaSegmentsRequested_ || (mediaSegmentsRetryAt_ != std::chrono::steady_clock::time_point{} && now < mediaSegmentsRetryAt_)) {
            return false;
        }
        mediaSegmentsRequested_ = true;
        mediaSegmentsRetryAt_ = {};
        return true;
    }
    void failMediaSegmentsRequest(
        std::chrono::steady_clock::time_point now,
        std::chrono::seconds retryDelay = std::chrono::seconds(10)
    ) {
        mediaSegmentsRequested_ = false;
        mediaSegmentsRetryAt_ = now + retryDelay;
    }
    void resetMediaSegments() {
        mediaSegmentsRequested_ = false;
        mediaSegmentsRetryAt_ = {};
        mediaSegments_.clear();
    }
    void setMediaSegments(std::vector<JellyfinMediaSegment> segments) {
        mediaSegmentsRetryAt_ = {};
        mediaSegments_ = std::move(segments);
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
    std::chrono::steady_clock::time_point mediaSegmentsRetryAt_{};
    bool fallbackAttempted_ = false;
    VideoZoomMode zoomMode_ = VideoZoomMode::Fit;
};
