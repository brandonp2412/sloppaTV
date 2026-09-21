#include "player_screen.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    PlayerScreenState state;
    const auto now = PlayerScreenState::Clock::time_point{10s};

    assert(PlayerScreenState::controlCount() == 5);
    assert(!state.controlsActive(now));
    assert(state.controlSelection() == PlayerControl::PlayPause);
    assert(!state.overlayVisible(now));

    state.showControls(now);
    assert(state.controlsActive(now + 9s));
    state.refreshControls(now + 9s);
    assert(state.controlsActive(now + 18s));
    assert(!state.controlsActive(now + 19s));
    state.showControls(now);
    assert(state.overlayVisible(now + 9s));
    assert(!state.controlsActive(now + 10s));
    assert(!state.overlayVisible(now + 10s));
    state.showOverlayFor(now + 11s, 5s);
    assert(!state.controlsActive(now + 11s));
    state.moveControl(1);
    state.moveControl(1);
    state.moveControl(1);
    state.moveControl(1);
    assert(state.controlSelection() == PlayerControl::SubtitleTrack);
    state.moveControl(-1);
    assert(state.controlSelection() == PlayerControl::AudioTrack);
    assert(!state.shouldDismissOnBack(now + 20s));
    state.showOverlayFor(now + 20s, 5s);
    assert(state.shouldDismissOnBack(now + 21s));
    state.dismissOverlay(now + 21s);
    assert(!state.controlsActive(now + 21s));
    assert(!state.shouldDismissOnBack(now + 21s));

    PlayerScreenState resumeState;
    resumeState.beginInitialPosition(60'000, now);
    assert(resumeState.positionMs() == 60'000);
    assert(resumeState.pendingSeekTargetMs() == 60'000);
    assert(!resumeState.pendingSeekAppearsFailed(0, now + 2s));
    resumeState.applyObservedPosition(60'050, now + 600ms);
    assert(resumeState.pendingSeekTargetMs() == -1);
    resumeState.beginSeek(90'000, now + 3s);
    assert(resumeState.pendingSeekAppearsFailed(0, now + 5s));

    PlayerScreenState inputState;
    auto command = inputState.handleInput(PlayerScreenInput::Up, now);
    assert(command.type == PlayerScreenCommandType::None);
    assert(inputState.controlsActive(now));
    command = inputState.handleInput(PlayerScreenInput::Right, now + 1s);
    assert(command.type == PlayerScreenCommandType::None);
    assert(inputState.controlSelection() == PlayerControl::NextEpisode);
    command = inputState.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(command.type == PlayerScreenCommandType::NextEpisode);
    inputState.moveControl(1);
    command = inputState.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(command.type == PlayerScreenCommandType::CycleAudioTrack);
    inputState.moveControl(1);
    command = inputState.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(command.type == PlayerScreenCommandType::CycleSubtitleTrack);
    inputState.moveControl(-3);
    command = inputState.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(command.type == PlayerScreenCommandType::TogglePause);
    inputState.moveControl(-1);
    command = inputState.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(command.type == PlayerScreenCommandType::PreviousEpisode);
    command = inputState.handleInput(PlayerScreenInput::Down, now + 3s);
    assert(command.type == PlayerScreenCommandType::None);
    assert(!inputState.controlsActive(now + 3s));
    command = inputState.handleInput(PlayerScreenInput::Down, now + 4s);
    assert(command.type == PlayerScreenCommandType::OpenQueue);
    assert(inputState.overlayVisible(now + 8s));
    command = inputState.handleInput(PlayerScreenInput::Back, now + 5s);
    assert(command.type == PlayerScreenCommandType::None);
    assert(!inputState.overlayVisible(now + 5s));
    command = inputState.handleInput(PlayerScreenInput::Back, now + 6s);
    assert(command.type == PlayerScreenCommandType::StopPlayback);

    PlayerScreenState playbackInputState;
    command = playbackInputState.handleInput(PlayerScreenInput::Previous, now);
    assert(command.type == PlayerScreenCommandType::PreviousEpisode);
    command = playbackInputState.handleInput(PlayerScreenInput::Next, now + 6s);
    assert(command.type == PlayerScreenCommandType::NextEpisode);
    command = playbackInputState.handleInput(PlayerScreenInput::Activate, now + 12s);
    assert(command.type == PlayerScreenCommandType::ActivatePlayback);
    command = playbackInputState.handleInput(PlayerScreenInput::PlayPause, now + 18s);
    assert(command.type == PlayerScreenCommandType::TogglePause);
    command = playbackInputState.handleInput(PlayerScreenInput::Left, now + 24s);
    assert(command.type == PlayerScreenCommandType::SeekBackward);
    command = playbackInputState.handleInput(PlayerScreenInput::Rewind, now + 30s);
    assert(command.type == PlayerScreenCommandType::SeekBackward);
    command = playbackInputState.handleInput(PlayerScreenInput::Right, now + 36s);
    assert(command.type == PlayerScreenCommandType::SeekForward);
    command = playbackInputState.handleInput(PlayerScreenInput::FastForward, now + 42s);
    assert(command.type == PlayerScreenCommandType::SeekForward);

    state.beginPlayback(12'000, 60'000);
    assert(state.positionMs() == 12'000);
    assert(state.durationMs() == 60'000);
    state.showSeekFeedback(30, now);
    assert(state.seekFeedbackVisible(now));
    assert(state.seekFeedbackSeconds() == 30);
    assert(state.seekFeedbackAlpha(now + 300ms) == 1.0f);
    assert(state.seekFeedbackAlpha(now + 575ms) > 0.0f);
    assert(!state.seekFeedbackVisible(now + 850ms));
    assert(state.seekFeedbackAlpha(now + 850ms) == 0.0f);

    state.beginSeek(30'000, now);
    assert(state.positionMs() == 30'000);
    assert(state.pendingSeekTargetMs() == 30'000);
    assert(state.overlayVisible(now + 2s));
    state.applyObservedPosition(12'000, now + 200ms);
    assert(state.positionMs() == 30'000);
    assert(state.pendingSeekTargetMs() == 30'000);
    state.applyObservedPosition(30'500, now + 400ms);
    assert(state.positionMs() == 30'500);
    assert(state.pendingSeekTargetMs() == 30'000);
    state.applyObservedPosition(30'800, now + 600ms);
    assert(state.positionMs() == 30'800);
    assert(state.pendingSeekTargetMs() == -1);
    assert(state.recentSeekTargetMs() == 30'000);
    assert(!state.recentSeekAppearsFailed(31'000, now + 700ms));
    assert(state.recentSeekAppearsFailed(0, now + 700ms));

    state.beginSeek(40'000, now);
    state.applyObservedPosition(15'000, now + 800ms);
    assert(state.positionMs() == 40'000);
    assert(state.pendingSeekTargetMs() == 40'000);
    assert(state.recentSeekAppearsFailed(15'000, now + 800ms));
    assert(!state.pendingSeekAppearsFailed(15'000, now + 1499ms));
    assert(state.pendingSeekAppearsFailed(15'000, now + 1500ms));
    state.applyObservedPosition(15'000, now + 5s);
    assert(state.positionMs() == 15'000);
    assert(state.pendingSeekTargetMs() == -1);

    state.beginSeek(0, now);
    assert(state.recentSeekTargetMs() == 0);
    assert(!state.recentSeekAppearsFailed(10'000, now + 499ms));
    assert(state.recentSeekAppearsFailed(10'000, now + 500ms));

    assert(!state.windowRestorePending());
    assert(!state.resumeOnFocusRequested());
    state.beginWindowRestore(true);
    assert(state.windowRestorePending());
    assert(state.resumeOnFocusRequested());
    assert(!state.takeResumeOnFocus());
    state.completeWindowRestore();
    assert(!state.windowRestorePending());
    assert(!state.resumeOnFocusRequested());
    state.requestResumeOnFocus();
    assert(state.takeResumeOnFocus());
    assert(!state.resumeOnFocusRequested());

    state.beginSkipButtonPress("series-1", "Example Show", false);
    assert(state.skipButtonPressPending());
    assert(!state.skipButtonLongPressed());
    assert(state.consumeSkipButtonRelease());

    state.beginSkipButtonPress("series-1", "Example Show", false);
    state.openSkipDisablePrompt();
    assert(state.skipButtonPressPending());
    assert(state.skipButtonLongPressed());
    assert(state.skipDisablePromptVisible());
    assert(!state.skipDisableSelected());
    assert(state.skipDisableSeriesId() == "series-1");
    assert(state.skipDisableSeriesName() == "Example Show");
    state.selectSkipDisable(true);
    assert(state.skipDisableSelected());
    assert(!state.consumeSkipButtonRelease());
    assert(state.skipDisablePromptVisible());
    state.closeSkipDisablePrompt();
    assert(!state.skipDisablePromptVisible());
    assert(!state.skipButtonPressPending());

    state.beginSkipButtonPress("series-1", "Example Show", true);
    state.openSkipDisablePrompt();
    assert(state.skipDisableEnabling());
    state.closeSkipDisablePrompt();

    auto& ambient = state.ambientBars();
    assert(ambient.sampleDue(now));
    ambient.addSample({1.0f, 0.5f, 0.25f}, now);
    assert(!ambient.sampleDue(now + 1s));
    assert(ambient.sampleDue(now + 2s));
    ambient.noteSampleAttempt(now + 2s);
    assert(!ambient.sampleDue(now + 3s));
    assert(ambient.sampleDue(now + 4s));
    assert(ambient.sampleCount() == 1);
    const auto ambientTarget = ambient.targetColor();
    assert(ambientTarget.r > 0.0f);
    assert(ambientTarget.r < 1.0f);
    const auto ambientStart = ambient.displayColor(now);
    const auto ambientLater = ambient.displayColor(now + 3s);
    assert(ambientLater.r > ambientStart.r);

    state.resetSession();
    assert(!state.controlsActive(now));
    assert(state.controlSelection() == PlayerControl::PlayPause);
    assert(state.positionMs() == 0);
    assert(state.durationMs() == 0);
    assert(state.pendingSeekTargetMs() == -1);
    assert(state.recentSeekTargetMs() == -1);
    assert(!state.seekFeedbackVisible(now));
    assert(state.seekFeedbackSeconds() == 0);
    assert(!state.windowRestorePending());
    assert(!state.resumeOnFocusRequested());

    return 0;
}
