#pragma once

#include "media_player_policy.hpp"

#include <algorithm>
#include <chrono>
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

enum class PlayerSkipPreferenceCommand {
    None,
    DisableForShow,
    EnableForShow,
    Dismiss,
};

struct PlayerScreenCommand {
    PlayerScreenCommandType type = PlayerScreenCommandType::None;
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
        skipPreferencePressPending_ = false;
        skipPreferenceLongPressed_ = false;
        skipPreferencePressEnabling_ = false;
        skipPreferenceSheetActive_ = false;
        skipPreferenceSheetEnabling_ = false;
        skipPreferenceSheetSelection_ = 1;
        resetPosition();
    }

    void resetPosition() {
        positionMs_ = 0;
        durationMs_ = 0;
        pendingSeekTargetMs_ = -1;
        lastSeekTargetMs_ = -1;
        lastSeekIssued_ = {};
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
    }

    [[nodiscard]] bool controlsActive(TimePoint now) const { return controlsActive_ && now < controlsUntil_; }

    [[nodiscard]] PlayerControl controlSelection() const { return controlSelection_; }

    [[nodiscard]] bool controlSelected(std::size_t index) const {
        return index == static_cast<std::size_t>(controlSelection_);
    }

    void beginSkipPreferencePress(bool currentlyDisabled) {
        skipPreferencePressPending_ = true;
        skipPreferenceLongPressed_ = false;
        skipPreferencePressEnabling_ = currentlyDisabled;
    }

    [[nodiscard]] bool skipPreferencePressPending() const { return skipPreferencePressPending_; }

    void holdSkipPreferencePress() {
        if (!skipPreferencePressPending_ || skipPreferenceLongPressed_) return;
        skipPreferenceLongPressed_ = true;
        skipPreferenceSheetActive_ = true;
        skipPreferenceSheetEnabling_ = skipPreferencePressEnabling_;
        skipPreferenceSheetSelection_ = 1;
    }

    [[nodiscard]] bool consumeSkipPreferenceRelease() {
        const bool activateNormalAction = skipPreferencePressPending_ && !skipPreferenceLongPressed_;
        skipPreferencePressPending_ = false;
        skipPreferenceLongPressed_ = false;
        skipPreferencePressEnabling_ = false;
        return activateNormalAction;
    }

    [[nodiscard]] bool skipPreferenceSheetActive() const { return skipPreferenceSheetActive_; }

    [[nodiscard]] bool skipPreferenceSheetEnabling() const { return skipPreferenceSheetEnabling_; }

    [[nodiscard]] int skipPreferenceSheetSelection() const { return skipPreferenceSheetSelection_; }

    PlayerSkipPreferenceCommand handleSkipPreferenceSheetInput(PlayerScreenInput input) {
        if (!skipPreferenceSheetActive_) return PlayerSkipPreferenceCommand::None;
        switch (input) {
        case PlayerScreenInput::Left:
            skipPreferenceSheetSelection_ = 0;
            return PlayerSkipPreferenceCommand::None;
        case PlayerScreenInput::Right:
            skipPreferenceSheetSelection_ = 1;
            return PlayerSkipPreferenceCommand::None;
        case PlayerScreenInput::Activate:
        case PlayerScreenInput::PlayPause: {
            const bool confirm = skipPreferenceSheetSelection_ == 0;
            const bool enabling = skipPreferenceSheetEnabling_;
            skipPreferenceSheetActive_ = false;
            if (!confirm) return PlayerSkipPreferenceCommand::Dismiss;
            return enabling ? PlayerSkipPreferenceCommand::EnableForShow
                            : PlayerSkipPreferenceCommand::DisableForShow;
        }
        case PlayerScreenInput::Back:
            skipPreferenceSheetActive_ = false;
            return PlayerSkipPreferenceCommand::Dismiss;
        default:
            return PlayerSkipPreferenceCommand::None;
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

    void beginSeek(int targetMs, TimePoint now) {
        const int target = std::max(0, targetMs);
        positionMs_ = target;
        pendingSeekTargetMs_ = target;
        lastSeekTargetMs_ = target;
        lastSeekIssued_ = now;
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
        if (pendingSeekTargetMs_ < 0 || lastSeekIssued_ == TimePoint{}) return false;
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
    bool windowRestorePending_ = false;
    bool resumeOnFocus_ = false;
    bool skipPreferencePressPending_ = false;
    bool skipPreferenceLongPressed_ = false;
    bool skipPreferencePressEnabling_ = false;
    bool skipPreferenceSheetActive_ = false;
    bool skipPreferenceSheetEnabling_ = false;
    int skipPreferenceSheetSelection_ = 1;
};
