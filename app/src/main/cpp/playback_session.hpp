#pragma once

#include "app_settings.hpp"
#include "jellyfin_types.hpp"

#include <optional>
#include <utility>
#include <vector>

class PlaybackSessionState {
public:
    void reset() {
        mediaSegments_.clear();
        mediaSegmentsRequested_ = false;
        fallbackAttempted_ = false;
        zoomMode_ = VideoZoomMode::Fit;
    }

    void begin(VideoZoomMode zoomMode) {
        mediaSegments_.clear();
        mediaSegmentsRequested_ = false;
        fallbackAttempted_ = false;
        zoomMode_ = zoomMode;
    }

    [[nodiscard]] bool mediaSegmentsRequested() const { return mediaSegmentsRequested_; }
    bool beginMediaSegmentsRequest() {
        if (mediaSegmentsRequested_) return false;
        mediaSegmentsRequested_ = true;
        return true;
    }
    void resetMediaSegments() {
        mediaSegmentsRequested_ = false;
        mediaSegments_.clear();
    }
    void setMediaSegments(std::vector<JellyfinMediaSegment> segments) {
        mediaSegments_ = std::move(segments);
    }
    [[nodiscard]] const std::vector<JellyfinMediaSegment>& mediaSegments() const { return mediaSegments_; }

    [[nodiscard]] std::optional<JellyfinMediaSegment> activeSkippableSegment(int64_t positionTicks) const {
        for (const auto& segment : mediaSegments_) {
            if (segment.endTicks - segment.startTicks < 30000000) continue;
            if (positionTicks >= segment.startTicks && positionTicks < segment.endTicks - 5000000) return segment;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool fallbackAttempted() const { return fallbackAttempted_; }
    void markFallbackAttempted() { fallbackAttempted_ = true; }
    void resetFallbackAttempted() { fallbackAttempted_ = false; }

    [[nodiscard]] VideoZoomMode zoomMode() const { return zoomMode_; }
    void setZoomMode(VideoZoomMode mode) { zoomMode_ = mode; }

private:
    std::vector<JellyfinMediaSegment> mediaSegments_;
    bool mediaSegmentsRequested_ = false;
    bool fallbackAttempted_ = false;
    VideoZoomMode zoomMode_ = VideoZoomMode::Fit;
};
