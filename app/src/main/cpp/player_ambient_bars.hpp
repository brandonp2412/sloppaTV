#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>

struct AmbientBarColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

inline AmbientBarColor normalizedAmbientBarColor(AmbientBarColor color) {
    color.r = std::clamp(color.r, 0.0f, 1.0f);
    color.g = std::clamp(color.g, 0.0f, 1.0f);
    color.b = std::clamp(color.b, 0.0f, 1.0f);
    const float luminance = color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
    constexpr float maxLuminance = 0.28f;
    if (luminance > maxLuminance && luminance > 0.0f) {
        const float scale = maxLuminance / luminance;
        color.r *= scale;
        color.g *= scale;
        color.b *= scale;
    }
    return color;
}

class AmbientBarColorState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    [[nodiscard]] bool sampleDue(TimePoint now) const {
        return lastSample_ == TimePoint{} || now - lastSample_ >= std::chrono::seconds(2);
    }

    void noteSampleAttempt(TimePoint now) { lastSample_ = now; }

    void addSample(AmbientBarColor sample, TimePoint now) {
        samples_[nextSample_] = normalizedAmbientBarColor(sample);
        nextSample_ = (nextSample_ + 1) % samples_.size();
        sampleCount_ = std::min(sampleCount_ + 1, samples_.size());
        lastSample_ = now;

        target_ = {};
        for (std::size_t i = 0; i < sampleCount_; ++i) {
            target_.r += samples_[i].r;
            target_.g += samples_[i].g;
            target_.b += samples_[i].b;
        }
        const float divisor = static_cast<float>(sampleCount_);
        target_.r /= divisor;
        target_.g /= divisor;
        target_.b /= divisor;
    }

    [[nodiscard]] AmbientBarColor displayColor(TimePoint now) {
        if (lastDisplayUpdate_ == TimePoint{}) {
            lastDisplayUpdate_ = now;
            return display_;
        }
        const float elapsedSeconds =
            std::chrono::duration_cast<std::chrono::duration<float>>(now - lastDisplayUpdate_).count();
        lastDisplayUpdate_ = now;
        const float blend = 1.0f - std::exp(-std::max(0.0f, elapsedSeconds) / 2.5f);
        display_.r += (target_.r - display_.r) * blend;
        display_.g += (target_.g - display_.g) * blend;
        display_.b += (target_.b - display_.b) * blend;
        return display_;
    }

    void reset() {
        samples_.fill({});
        nextSample_ = 0;
        sampleCount_ = 0;
        lastSample_ = {};
        lastDisplayUpdate_ = {};
        target_ = {};
        display_ = {};
    }

    [[nodiscard]] std::size_t sampleCount() const { return sampleCount_; }

    [[nodiscard]] AmbientBarColor targetColor() const { return target_; }

private:
    std::array<AmbientBarColor, 30> samples_{};
    std::size_t nextSample_ = 0;
    std::size_t sampleCount_ = 0;
    TimePoint lastSample_{};
    TimePoint lastDisplayUpdate_{};
    AmbientBarColor target_{};
    AmbientBarColor display_{};
};
