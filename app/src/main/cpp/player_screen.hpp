#pragma once

#include "media_player_policy.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>

enum class PlayerScreenInput {
    None,
    Back,
    Up,
    Down,
    Left,
    Right,
    Activate,
    PlayPause,
    Previous,
    Next,
    Rewind,
    FastForward,
};

enum class PlayerControl {
    PreviousEpisode,
    PlayPause,
    NextEpisode,
    AudioTrack,
    SubtitleTrack,
    Count,
};

enum class PlayerScreenCommandType {
    None,
    StopPlayback,
    OpenQueue,
    PreviousEpisode,
    NextEpisode,
    ActivatePlayback,
    TogglePause,
    CycleAudioTrack,
    CycleSubtitleTrack,
    SeekBackward,
    SeekForward,
};

enum class PlayerSkipSheetCommand {
    None,
    DisableForShow,
    Dismiss,
};

struct PlayerScreenCommand {
    PlayerScreenCommandType type = PlayerScreenCommandType::None;
};

struct PlayerAmbientColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

class PlayerScreenState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr std::size_t controlCount() { return static_cast<std::size_t>(PlayerControl::Count); }

    void resetSession() {
        controlsActive_ = false;
        controlSelection_ = PlayerControl::PlayPause;
        controlsUntil_ = {};
        overlayUntil_ = {};
        seekFeedbackSeconds_ = 0;
        seekFeedbackStarted_ = {};
        seekFeedbackUntil_ = {};
        windowRestorePending_ = false;
        resumeOnFocus_ = false;
        skipButtonPressPending_ = false;
        skipButtonLongPressed_ = false;
        skipDisableSheetActive_ = false;
        skipDisableSheetSelection_ = 1;
        resetAmbientColor();
        resetPosition();
    }

    void resetPosition() {
        positionMs_ = 0;
        durationMs_ = 0;
        pendingSeekTargetMs_ = -1;
        lastSeekTargetMs_ = -1;
        lastSeekIssued_ = {};
        pendingSeekRecoveryEnabled_ = false;
        seekFeedbackSeconds_ = 0;
        seekFeedbackStarted_ = {};
        seekFeedbackUntil_ = {};
    }

    void beginPlayback(int positionMs, int durationMs) {
        controlsActive_ = false;
        controlSelection_ = PlayerControl::PlayPause;
        controlsUntil_ = {};
        positionMs_ = std::max(0, positionMs);
        durationMs_ = std::max(0, durationMs);
        pendingSeekTargetMs_ = -1;
        lastSeekTargetMs_ = -1;
        lastSeekIssued_ = {};
        pendingSeekRecoveryEnabled_ = false;
        resetAmbientColor();
    }

    [[nodiscard]] bool controlsActive(TimePoint now) const { return controlsActive_ && now < controlsUntil_; }

    [[nodiscard]] PlayerControl controlSelection() const { return controlSelection_; }

    [[nodiscard]] bool controlSelected(std::size_t index) const {
        return index == static_cast<std::size_t>(controlSelection_);
    }

    void beginSkipButtonPress() {
        skipButtonPressPending_ = true;
        skipButtonLongPressed_ = false;
    }

    [[nodiscard]] bool skipButtonPressPending() const { return skipButtonPressPending_; }

    void holdSkipButtonPress() {
        if (!skipButtonPressPending_ || skipButtonLongPressed_) return;
        skipButtonLongPressed_ = true;
        skipDisableSheetActive_ = true;
        skipDisableSheetSelection_ = 1;
    }

    [[nodiscard]] bool consumeSkipButtonRelease() {
        const bool activateSkip = skipButtonPressPending_ && !skipButtonLongPressed_;
        skipButtonPressPending_ = false;
        skipButtonLongPressed_ = false;
        return activateSkip;
    }

    [[nodiscard]] bool ambientSampleDue(TimePoint now) const {
        return lastAmbientSample_ == TimePoint{} || now - lastAmbientSample_ >= std::chrono::seconds(3);
    }

    void applyAmbientSample(float r, float g, float b, TimePoint now) {
        const float sampleR = std::clamp(r, 0.0f, 1.0f);
        const float sampleG = std::clamp(g, 0.0f, 1.0f);
        const float sampleB = std::clamp(b, 0.0f, 1.0f);
        float alpha = 0.18f;
        if (lastAmbientSample_ != TimePoint{}) {
            const float elapsedSeconds =
                std::max(0.0f, std::chrono::duration<float>(now - lastAmbientSample_).count());
            alpha = 1.0f - std::exp(-elapsedSeconds / 60.0f);
        }
        ambientColor_.r += (sampleR - ambientColor_.r) * alpha;
        ambientColor_.g += (sampleG - ambientColor_.g) * alpha;
        ambientColor_.b += (sampleB - ambientColor_.b) * alpha;
        lastAmbientSample_ = now;
        ambientColorReady_ = true;
    }

    [[nodiscard]] bool ambientColorReady() const { return ambientColorReady_; }

    [[nodiscard]] PlayerAmbientColor ambientColor() const { return ambientColor_; }

    void resetAmbientColor() {
        ambientColor_ = {};
        lastAmbientSample_ = {};
        ambientColorReady_ = false;
    }

    [[nodiscard]] bool skipDisableSheetActive() const { return skipDisableSheetActive_; }

    [[nodiscard]] int skipDisableSheetSelection() const { return skipDisableSheetSelection_; }

    PlayerSkipSheetCommand handleSkipDisableSheetInput(PlayerScreenInput input) {
        if (!skipDisableSheetActive_) return PlayerSkipSheetCommand::None;
        switch (input) {
        case PlayerScreenInput::Left:
            skipDisableSheetSelection_ = 0;
            return PlayerSkipSheetCommand::None;
        case PlayerScreenInput::Right:
            skipDisableSheetSelection_ = 1;
            return PlayerSkipSheetCommand::None;
        case PlayerScreenInput::Activate:
        case PlayerScreenInput::PlayPause: {
            const bool disableForShow = skipDisableSheetSelection_ == 0;
            skipDisableSheetActive_ = false;
            return disableForShow ? PlayerSkipSheetCommand::DisableForShow : PlayerSkipSheetCommand::Dismiss;
        }
        case PlayerScreenInput::Back:
            skipDisableSheetActive_ = false;
            return PlayerSkipSheetCommand::Dismiss;
        default:
            return PlayerSkipSheetCommand::None;
        }
    }

    void showControls(TimePoint now) {
        controlsActive_ = true;
        controlSelection_ = PlayerControl::PlayPause;
        controlsUntil_ = now + std::chrono::seconds(10);
        showOverlayFor(now, std::chrono::seconds(10));
    }

    void refreshControls(TimePoint now) {
        if (!controlsActive(now)) return;
        controlsUntil_ = now + std::chrono::seconds(10);
        showOverlayFor(now, std::chrono::seconds(10));
    }

    void hideControls() {
        controlsActive_ = false;
        controlsUntil_ = {};
    }

    void moveControl(int delta) {
        const int selection =
            std::clamp(static_cast<int>(controlSelection_) + delta, 0, static_cast<int>(controlCount()) - 1);
        controlSelection_ = static_cast<PlayerControl>(selection);
    }

    [[nodiscard]] bool overlayVisible(TimePoint now) const { return now < overlayUntil_; }

    void showSeekFeedback(int seconds, TimePoint now) {
        seekFeedbackSeconds_ = seconds;
        seekFeedbackStarted_ = now;
        seekFeedbackUntil_ = now + std::chrono::milliseconds(850);
    }

    [[nodiscard]] bool seekFeedbackVisible(TimePoint now) const {
        return seekFeedbackSeconds_ != 0 && now < seekFeedbackUntil_;
    }

    [[nodiscard]] int seekFeedbackSeconds() const { return seekFeedbackSeconds_; }

    [[nodiscard]] float seekFeedbackAlpha(TimePoint now) const {
        if (!seekFeedbackVisible(now)) return 0.0f;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - seekFeedbackStarted_).count();
        if (elapsed <= 300) return 1.0f;
        const float fade = 1.0f - static_cast<float>(elapsed - 300) / 550.0f;
        return std::clamp(fade, 0.0f, 1.0f);
    }

    template <typename Duration> void showOverlayFor(TimePoint now, Duration duration) {
        overlayUntil_ = now + std::chrono::duration_cast<Clock::duration>(duration);
    }

    void dismissOverlay(TimePoint now) {
        hideControls();
        overlayUntil_ = now;
    }

    [[nodiscard]] bool shouldDismissOnBack(TimePoint now) const { return overlayVisible(now); }

    [[nodiscard]] PlayerScreenCommand handleInput(PlayerScreenInput input, TimePoint now) {
        if (input == PlayerScreenInput::Back) {
            if (shouldDismissOnBack(now)) {
                dismissOverlay(now);
                return {};
            }
            return {.type = PlayerScreenCommandType::StopPlayback};
        }
        if (input == PlayerScreenInput::Up && !controlsActive(now)) {
            showControls(now);
            return {};
        }

        if (controlsActive(now))
            refreshControls(now);
        else
            showOverlayFor(now, std::chrono::seconds(5));

        if (controlsActive(now)) {
            if (input == PlayerScreenInput::Down)
                hideControls();
            else if (input == PlayerScreenInput::Left)
                moveControl(-1);
            else if (input == PlayerScreenInput::Right)
                moveControl(1);
            else if (input == PlayerScreenInput::Activate)
                return selectedControlCommand();
            return {};
        }

        switch (input) {
        case PlayerScreenInput::Down:
            return {.type = PlayerScreenCommandType::OpenQueue};
        case PlayerScreenInput::Previous:
            return {.type = PlayerScreenCommandType::PreviousEpisode};
        case PlayerScreenInput::Next:
            return {.type = PlayerScreenCommandType::NextEpisode};
        case PlayerScreenInput::Activate:
            return {.type = PlayerScreenCommandType::ActivatePlayback};
        case PlayerScreenInput::PlayPause:
            return {.type = PlayerScreenCommandType::TogglePause};
        case PlayerScreenInput::Left:
        case PlayerScreenInput::Rewind:
            return {.type = PlayerScreenCommandType::SeekBackward};
        case PlayerScreenInput::Right:
        case PlayerScreenInput::FastForward:
            return {.type = PlayerScreenCommandType::SeekForward};
        default:
            return {};
        }
    }

    [[nodiscard]] int positionMs() const { return positionMs_; }

    [[nodiscard]] int durationMs() const { return durationMs_; }

    void setPositionMs(int value) { positionMs_ = std::max(0, value); }

    void setDurationMs(int value) { durationMs_ = std::max(0, value); }

    void beginInitialPosition(int targetMs, TimePoint now) {
        const int target = std::max(0, targetMs);
        positionMs_ = target;
        pendingSeekTargetMs_ = target;
        lastSeekTargetMs_ = -1;
        lastSeekIssued_ = now;
        pendingSeekRecoveryEnabled_ = false;
    }

    void beginSeek(int targetMs, TimePoint now) {
        const int target = std::max(0, targetMs);
        positionMs_ = target;
        pendingSeekTargetMs_ = target;
        lastSeekTargetMs_ = target;
        lastSeekIssued_ = now;
        pendingSeekRecoveryEnabled_ = true;
        showOverlayFor(now, std::chrono::seconds(3));
    }

    void applyObservedPosition(int observedPositionMs, TimePoint now) {
        const int observed = std::max(0, observedPositionMs);
        if (pendingSeekTargetMs_ < 0) {
            positionMs_ = observed;
            return;
        }
        const int64_t elapsedSinceSeekMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSeekIssued_).count();
        if (postSeekPositionMatchesTarget(observed, pendingSeekTargetMs_)) {
            positionMs_ = observed;
            if (elapsedSinceSeekMs >= 500) pendingSeekTargetMs_ = -1;
        } else if (shouldAcceptPostSeekTelemetry(observed, pendingSeekTargetMs_, elapsedSinceSeekMs)) {
            positionMs_ = observed;
            pendingSeekTargetMs_ = -1;
        } else {
            positionMs_ = pendingSeekTargetMs_;
        }
    }

    [[nodiscard]] int pendingSeekTargetMs() const { return pendingSeekTargetMs_; }

    [[nodiscard]] int recentSeekTargetMs() const { return lastSeekTargetMs_; }

    [[nodiscard]] bool pendingSeekAppearsFailed(int observedPositionMs, TimePoint now) const {
        if (!pendingSeekRecoveryEnabled_ || pendingSeekTargetMs_ < 0 || lastSeekIssued_ == TimePoint{}) return false;
        const int64_t elapsedSinceSeekMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSeekIssued_).count();
        return postSeekPositionFailed(observedPositionMs, pendingSeekTargetMs_, elapsedSinceSeekMs);
    }

    [[nodiscard]] bool recentSeekAppearsFailed(int observedPositionMs, TimePoint now) const {
        if (lastSeekTargetMs_ < 0 || lastSeekIssued_ == TimePoint{}) return false;
        const int64_t elapsedSinceSeekMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSeekIssued_).count();
        return elapsedSinceSeekMs >= 500 && elapsedSinceSeekMs <= 3000 &&
               !postSeekPositionMatchesTarget(observedPositionMs, lastSeekTargetMs_, 3000);
    }

    void beginWindowRestore(bool resumePlayback) {
        windowRestorePending_ = true;
        if (resumePlayback) resumeOnFocus_ = true;
    }

    [[nodiscard]] bool windowRestorePending() const { return windowRestorePending_; }

    [[nodiscard]] bool resumeOnFocusRequested() const { return resumeOnFocus_; }

    void completeWindowRestore() {
        windowRestorePending_ = false;
        resumeOnFocus_ = false;
    }

    void requestResumeOnFocus() { resumeOnFocus_ = true; }

    [[nodiscard]] bool takeResumeOnFocus() {
        if (windowRestorePending_ || !resumeOnFocus_) return false;
        resumeOnFocus_ = false;
        return true;
    }

private:
    [[nodiscard]] PlayerScreenCommand selectedControlCommand() const {
        switch (controlSelection_) {
        case PlayerControl::PreviousEpisode:
            return {.type = PlayerScreenCommandType::PreviousEpisode};
        case PlayerControl::PlayPause:
            return {.type = PlayerScreenCommandType::TogglePause};
        case PlayerControl::NextEpisode:
            return {.type = PlayerScreenCommandType::NextEpisode};
        case PlayerControl::AudioTrack:
            return {.type = PlayerScreenCommandType::CycleAudioTrack};
        case PlayerControl::SubtitleTrack:
            return {.type = PlayerScreenCommandType::CycleSubtitleTrack};
        case PlayerControl::Count:
            return {};
        }
        return {};
    }

    bool controlsActive_ = false;
    PlayerControl controlSelection_ = PlayerControl::PlayPause;
    TimePoint controlsUntil_{};
    TimePoint overlayUntil_{};
    int seekFeedbackSeconds_ = 0;
    TimePoint seekFeedbackStarted_{};
    TimePoint seekFeedbackUntil_{};
    int positionMs_ = 0;
    int durationMs_ = 0;
    int pendingSeekTargetMs_ = -1;
    int lastSeekTargetMs_ = -1;
    TimePoint lastSeekIssued_{};
    bool pendingSeekRecoveryEnabled_ = false;
    bool windowRestorePending_ = false;
    bool resumeOnFocus_ = false;
    bool skipButtonPressPending_ = false;
    bool skipButtonLongPressed_ = false;
    bool skipDisableSheetActive_ = false;
    int skipDisableSheetSelection_ = 1;
    PlayerAmbientColor ambientColor_{};
    TimePoint lastAmbientSample_{};
    bool ambientColorReady_ = false;
};
