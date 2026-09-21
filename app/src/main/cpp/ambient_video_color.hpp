#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

struct AmbientVideoColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

inline AmbientVideoColor ambientBarTarget(AmbientVideoColor color) {
    color.r = std::clamp(color.r, 0.0f, 1.0f);
    color.g = std::clamp(color.g, 0.0f, 1.0f);
    color.b = std::clamp(color.b, 0.0f, 1.0f);

    // Keep the bars comfortably darker than the picture while preserving its hue.
    constexpr float kMaximumLuma = 0.26f;
    const float luma = color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
    if (luma > kMaximumLuma) {
        const float scale = kMaximumLuma / luma;
        color.r *= scale;
        color.g *= scale;
        color.b *= scale;
    }
    return color;
}

class AmbientVideoColorState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr auto kSampleInterval = std::chrono::seconds(2);

    [[nodiscard]] bool sampleDue(TimePoint now) const {
        return lastSampleAttempt_ == TimePoint{} || now - lastSampleAttempt_ >= kSampleInterval;
    }

    void noteSampleAttempt(TimePoint now) { lastSampleAttempt_ = now; }

    void submitSample(AmbientVideoColor sampled, TimePoint now) {
        noteSampleAttempt(now);
        sampled = ambientBarTarget(sampled);
        if (!hasTarget_) {
            target_ = sampled;
            hasTarget_ = true;
        } else {
            constexpr float kTargetBlend = 0.18f;
            target_ = blend(target_, sampled, kTargetBlend);
        }
        if (lastUpdate_ == TimePoint{}) lastUpdate_ = now;
    }

    [[nodiscard]] AmbientVideoColor color(TimePoint now) {
        if (!hasTarget_) return {};
        if (lastUpdate_ == TimePoint{}) lastUpdate_ = now;
        const float seconds = std::clamp(std::chrono::duration<float>(now - lastUpdate_).count(), 0.0f, 1.0f);
        constexpr float kTransitionSeconds = 4.0f;
        const float amount = 1.0f - std::exp(-seconds / kTransitionSeconds);
        current_ = blend(current_, target_, amount);
        lastUpdate_ = now;
        return current_;
    }

    void reset() {
        lastSampleAttempt_ = {};
        lastUpdate_ = {};
        current_ = {};
        target_ = {};
        hasTarget_ = false;
    }

private:
    static AmbientVideoColor blend(AmbientVideoColor from, AmbientVideoColor to, float amount) {
        const float t = std::clamp(amount, 0.0f, 1.0f);
        return {
            .r = from.r + (to.r - from.r) * t,
            .g = from.g + (to.g - from.g) * t,
            .b = from.b + (to.b - from.b) * t,
        };
    }

    TimePoint lastSampleAttempt_{};
    TimePoint lastUpdate_{};
    AmbientVideoColor current_{};
    AmbientVideoColor target_{};
    bool hasTarget_ = false;
};
