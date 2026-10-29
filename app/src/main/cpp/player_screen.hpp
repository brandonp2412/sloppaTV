#pragma once

#include "media_player_policy.hpp"
#include "player_buffer_indicator.hpp"
#include "player_ambient_bars.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class PlayerScreenInput {
    None,
    Back,
    Up,
    Down,
    Left,
    Right,
    Activate,
    Play,
    Pause,
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
    Play,
    Pause,
    TogglePause,
    OpenAudioPicker,
    OpenSubtitlePicker,
    SelectAudioTrack,
    SelectSubtitleTrack,
    SeekBackward,
    SeekForward,
};

struct PlayerScreenCommand {
    PlayerScreenCommandType type = PlayerScreenCommandType::None;
    int trackIndex = -1;
};

enum class PlayerTrackPicker { None, Audio, Subtitle };

struct PlayerTrackOption {
    int index = -1;
    std::string label;
    bool active = false;
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
        picker_ = PlayerTrackPicker::None;
        pickerOptions_.clear();
        pickerSelected_ = 0;
        overlayUntil_ = {};
        preparationOverlayEnabled_ = true;
        seekFeedbackSeconds_ = 0;
        seekFeedbackStarted_ = {};
        seekFeedbackUntil_ = {};
        windowRestorePending_ = false;
        resumeOnFocus_ = false;
        resetSkipButtonInteraction();
        ambientBars_.reset();
        resetPosition();
    }

    void resetPosition() {
        bufferIndicator_.reset();
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

    void beginPlayback(int positionMs, int durationMs, bool showPreparationOverlay = true) {
        closeTrackPicker();
        preparationOverlayEnabled_ = showPreparationOverlay;
        controlsActive_ = false;
        controlSelection_ = PlayerControl::PlayPause;
        controlsUntil_ = {};
        positionMs_ = std::max(0, positionMs);
        durationMs_ = std::max(0, durationMs);
        pendingSeekTargetMs_ = -1;
        lastSeekTargetMs_ = -1;
        lastSeekIssued_ = {};
        pendingSeekRecoveryEnabled_ = false;
        resetSkipButtonInteraction();
        ambientBars_.reset();
    }

    void prepareForEpisodeAdvance(TimePoint now) {
        preparationOverlayEnabled_ = false;
        dismissOverlay(now);
    }

    [[nodiscard]] bool preparationOverlayEnabled() const { return preparationOverlayEnabled_; }

    [[nodiscard]] bool controlsActive(TimePoint now) const { return controlsActive_ && now < controlsUntil_; }

    [[nodiscard]] PlayerControl controlSelection() const { return controlSelection_; }

    [[nodiscard]] bool controlSelected(std::size_t index) const {
        return index == static_cast<std::size_t>(controlSelection_);
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

    [[nodiscard]] PlayerTrackPicker trackPicker() const { return picker_; }

    [[nodiscard]] const std::vector<PlayerTrackOption>& trackOptions() const { return pickerOptions_; }

    [[nodiscard]] std::size_t trackPickerSelection() const { return pickerSelected_; }

    void openTrackPicker(PlayerTrackPicker kind, std::vector<PlayerTrackOption> options, TimePoint now) {
        if (options.empty()) return;
        picker_ = kind;
        pickerOptions_ = std::move(options);
        pickerSelected_ = 0;
        for (std::size_t i = 0; i < pickerOptions_.size(); ++i) {
            if (pickerOptions_[i].active) {
                pickerSelected_ = i;
                break;
            }
        }
        controlsActive_ = true;
        controlsUntil_ = now + std::chrono::minutes(5);
        showOverlayFor(now, std::chrono::minutes(5));
    }

    void closeTrackPicker() {
        picker_ = PlayerTrackPicker::None;
        pickerOptions_.clear();
        pickerSelected_ = 0;
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
        if (picker_ != PlayerTrackPicker::None) {
            refreshControls(now);
            if (input == PlayerScreenInput::Back) {
                closeTrackPicker();
            } else if (input == PlayerScreenInput::Up && pickerSelected_ > 0) {
                --pickerSelected_;
            } else if (input == PlayerScreenInput::Down && pickerSelected_ + 1 < pickerOptions_.size()) {
                ++pickerSelected_;
            } else if (input == PlayerScreenInput::Activate) {
                const auto type = picker_ == PlayerTrackPicker::Audio ? PlayerScreenCommandType::SelectAudioTrack
                                                                      : PlayerScreenCommandType::SelectSubtitleTrack;
                const int index = pickerOptions_[pickerSelected_].index;
                closeTrackPicker();
                return {.type = type, .trackIndex = index};
            }
            return {};
        }
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
        case PlayerScreenInput::Play:
            return {.type = PlayerScreenCommandType::Play};
        case PlayerScreenInput::Pause:
            return {.type = PlayerScreenCommandType::Pause};
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

    [[nodiscard]] int prematureSeekEndTarget(int durationMs, TimePoint now) const {
        if (lastSeekIssued_ == TimePoint{}) return -1;
        const int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSeekIssued_).count();
        return prematureSeekEnd(lastSeekTargetMs_, durationMs, elapsedMs) ? lastSeekTargetMs_ : -1;
    }

    void beginWindowRestore(bool resumePlayback) {
        windowRestorePending_ = true;
        if (resumePlayback) resumeOnFocus_ = true;
    }

    [[nodiscard]] bool windowRestorePending() const { return windowRestorePending_; }

    [[nodiscard]] PlayerBufferIndicator& bufferIndicator() { return bufferIndicator_; }

    [[nodiscard]] const PlayerBufferIndicator& bufferIndicator() const { return bufferIndicator_; }

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

    void beginSkipButtonPress(std::string seriesId, std::string seriesName, bool enabling) {
        skipButtonPressPending_ = true;
        skipButtonLongPressed_ = false;
        skipButtonPressEnabling_ = enabling;
        skipButtonPressSeriesId_ = std::move(seriesId);
        skipButtonPressSeriesName_ = std::move(seriesName);
    }

    [[nodiscard]] bool skipButtonPressPending() const { return skipButtonPressPending_; }

    [[nodiscard]] bool skipButtonLongPressed() const { return skipButtonLongPressed_; }

    void openSkipDisablePrompt() {
        if (!skipButtonPressPending_) return;
        skipButtonLongPressed_ = true;
        skipDisablePromptVisible_ = true;
        skipDisableSelected_ = false;
        skipDisableEnabling_ = skipButtonPressEnabling_;
        skipDisableSeriesId_ = skipButtonPressSeriesId_;
        skipDisableSeriesName_ = skipButtonPressSeriesName_;
    }

    [[nodiscard]] bool consumeSkipButtonRelease() {
        if (!skipButtonPressPending_) return false;
        const bool activate = !skipButtonLongPressed_;
        skipButtonPressPending_ = false;
        skipButtonLongPressed_ = false;
        skipButtonPressEnabling_ = false;
        skipButtonPressSeriesId_.clear();
        skipButtonPressSeriesName_.clear();
        return activate;
    }

    void cancelSkipButtonPress() {
        skipButtonPressPending_ = false;
        skipButtonLongPressed_ = false;
        skipButtonPressEnabling_ = false;
        skipButtonPressSeriesId_.clear();
        skipButtonPressSeriesName_.clear();
    }

    [[nodiscard]] bool skipDisablePromptVisible() const { return skipDisablePromptVisible_; }

    [[nodiscard]] bool skipDisableSelected() const { return skipDisableSelected_; }

    [[nodiscard]] bool skipDisableEnabling() const { return skipDisableEnabling_; }

    [[nodiscard]] const std::string& skipDisableSeriesId() const { return skipDisableSeriesId_; }

    [[nodiscard]] const std::string& skipDisableSeriesName() const { return skipDisableSeriesName_; }

    void selectSkipDisable(bool disable) { skipDisableSelected_ = disable; }

    void closeSkipDisablePrompt() {
        skipDisablePromptVisible_ = false;
        skipDisableSelected_ = false;
        skipDisableEnabling_ = false;
        skipDisableSeriesId_.clear();
        skipDisableSeriesName_.clear();
        cancelSkipButtonPress();
    }

    AmbientBarColorState& ambientBars() { return ambientBars_; }

    const AmbientBarColorState& ambientBars() const { return ambientBars_; }

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
            return {.type = PlayerScreenCommandType::OpenAudioPicker};
        case PlayerControl::SubtitleTrack:
            return {.type = PlayerScreenCommandType::OpenSubtitlePicker};
        case PlayerControl::Count:
            return {};
        }
        return {};
    }

    PlayerTrackPicker picker_ = PlayerTrackPicker::None;
    std::vector<PlayerTrackOption> pickerOptions_;
    std::size_t pickerSelected_ = 0;
    bool controlsActive_ = false;
    PlayerControl controlSelection_ = PlayerControl::PlayPause;
    TimePoint controlsUntil_{};
    TimePoint overlayUntil_{};
    bool preparationOverlayEnabled_ = true;
    int seekFeedbackSeconds_ = 0;
    TimePoint seekFeedbackStarted_{};
    TimePoint seekFeedbackUntil_{};
    int positionMs_ = 0;
    int durationMs_ = 0;
    int pendingSeekTargetMs_ = -1;
    int lastSeekTargetMs_ = -1;
    TimePoint lastSeekIssued_{};
    bool pendingSeekRecoveryEnabled_ = false;

    void resetSkipButtonInteraction() {
        skipButtonPressPending_ = false;
        skipButtonLongPressed_ = false;
        skipButtonPressEnabling_ = false;
        skipButtonPressSeriesId_.clear();
        skipButtonPressSeriesName_.clear();
        skipDisablePromptVisible_ = false;
        skipDisableSelected_ = false;
        skipDisableEnabling_ = false;
        skipDisableSeriesId_.clear();
        skipDisableSeriesName_.clear();
    }

    bool windowRestorePending_ = false;
    bool resumeOnFocus_ = false;
    bool skipButtonPressPending_ = false;
    bool skipButtonLongPressed_ = false;
    bool skipButtonPressEnabling_ = false;
    std::string skipButtonPressSeriesId_;
    std::string skipButtonPressSeriesName_;
    bool skipDisablePromptVisible_ = false;
    bool skipDisableSelected_ = false;
    bool skipDisableEnabling_ = false;
    std::string skipDisableSeriesId_;
    std::string skipDisableSeriesName_;
    PlayerBufferIndicator bufferIndicator_;
    AmbientBarColorState ambientBars_;
};
