#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

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

// A low-resolution copy of the rendered video, with pixels ordered bottom-to-top.
inline constexpr int kAmbientSampleWidth = 16;
inline constexpr int kAmbientSampleHeight = 108;
using AmbientFramePixels = std::array<uint8_t, kAmbientSampleWidth * kAmbientSampleHeight * 4>;

inline std::array<float, 2> detectEmbeddedLetterboxBars(const AmbientFramePixels& pixels) {
    auto isBlackRow = [&](int y) {
        int black = 0;
        for (int x = 0; x < kAmbientSampleWidth; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * kAmbientSampleWidth + x) * 4;
            if (std::max({pixels[offset], pixels[offset + 1], pixels[offset + 2]}) <= 10) ++black;
        }
        return black >= kAmbientSampleWidth - 2;
    };
    int bottomRows = 0;
    while (bottomRows < kAmbientSampleHeight / 3 && isBlackRow(bottomRows)) ++bottomRows;
    int topRows = 0;
    while (topRows < kAmbientSampleHeight / 3 && isBlackRow(kAmbientSampleHeight - 1 - topRows)) ++topRows;

    // Require substantial, roughly symmetrical stripes and an actual visible image
    // between them. This rules out most dark shots and fade-to-black transitions.
    if (topRows < 4 || bottomRows < 4 || std::abs(topRows - bottomRows) > 3) return {};
    bool visibleContent = false;
    for (int y = kAmbientSampleHeight / 3; y < kAmbientSampleHeight * 2 / 3 && !visibleContent; ++y) {
        for (int x = 0; x < kAmbientSampleWidth; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * kAmbientSampleWidth + x) * 4;
            if (std::max({pixels[offset], pixels[offset + 1], pixels[offset + 2]}) >= 32) {
                visibleContent = true;
                break;
            }
        }
    }
    if (!visibleContent) return {};
    return {(static_cast<float>(topRows) + 0.2f) / kAmbientSampleHeight,
            (static_cast<float>(bottomRows) + 0.2f) / kAmbientSampleHeight};
}

class AmbientBarColorState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    [[nodiscard]] bool sampleDue(TimePoint now) const {
        return lastSample_ == TimePoint{} || now - lastSample_ >= std::chrono::seconds(1);
    }

    void noteSampleAttempt(TimePoint now) { lastSample_ = now; }

    void addSample(AmbientBarColor sample, TimePoint now) {
        transitionStart_ = displayColor(now);
        transitionStarted_ = now;
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

    void observeEmbeddedInsets(std::array<float, 2> insets) {
        if (std::abs(insets[0] - candidateInsets_[0]) < 0.025f && std::abs(insets[1] - candidateInsets_[1]) < 0.025f) {
            ++candidateCount_;
        } else {
            candidateInsets_ = insets;
            candidateCount_ = 1;
        }
        // A black frame must not remove bars; wait for several consistent samples.
        const int required = insets[0] == 0.0f && insets[1] == 0.0f ? 6 : 2;
        if (candidateCount_ >= required) embeddedInsets_ = candidateInsets_;
    }

    [[nodiscard]] const std::array<float, 2>& embeddedInsets() const { return embeddedInsets_; }

    [[nodiscard]] AmbientBarColor displayColor(TimePoint now) {
        if (transitionStarted_ == TimePoint{}) return display_;
        const float elapsedSeconds =
            std::chrono::duration_cast<std::chrono::duration<float>>(now - transitionStarted_).count();
        const float progress = std::clamp(elapsedSeconds, 0.0f, 1.0f);
        const float blend = progress * progress * (3.0f - 2.0f * progress);
        display_.r = transitionStart_.r + (target_.r - transitionStart_.r) * blend;
        display_.g = transitionStart_.g + (target_.g - transitionStart_.g) * blend;
        display_.b = transitionStart_.b + (target_.b - transitionStart_.b) * blend;
        return display_;
    }

    void reset() {
        samples_.fill({});
        nextSample_ = 0;
        sampleCount_ = 0;
        lastSample_ = {};
        transitionStarted_ = {};
        transitionStart_ = {};
        target_ = {};
        display_ = {};
        embeddedInsets_ = {};
        candidateInsets_ = {};
        candidateCount_ = 0;
    }

    [[nodiscard]] std::size_t sampleCount() const { return sampleCount_; }

    [[nodiscard]] AmbientBarColor targetColor() const { return target_; }

private:
    std::array<AmbientBarColor, 6> samples_{};
    std::size_t nextSample_ = 0;
    std::size_t sampleCount_ = 0;
    TimePoint lastSample_{};
    TimePoint transitionStarted_{};
    AmbientBarColor transitionStart_{};
    AmbientBarColor target_{};
    AmbientBarColor display_{};
    std::array<float, 2> embeddedInsets_{};
    std::array<float, 2> candidateInsets_{};
    int candidateCount_ = 0;
};
