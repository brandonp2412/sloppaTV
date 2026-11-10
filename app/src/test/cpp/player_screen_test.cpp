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

    PlayerScreenState nextEpisodeState;
    nextEpisodeState.showControls(now);
    assert(nextEpisodeState.preparationOverlayEnabled());
    nextEpisodeState.prepareForEpisodeAdvance(now + 1s);
    assert(!nextEpisodeState.controlsActive(now + 1s));
    assert(!nextEpisodeState.overlayVisible(now + 1s));
    assert(!nextEpisodeState.preparationOverlayEnabled());
    nextEpisodeState.beginPlayback(0, 50'000, false);
    assert(!nextEpisodeState.preparationOverlayEnabled());
    nextEpisodeState.beginPlayback(0, 50'000);
    assert(nextEpisodeState.preparationOverlayEnabled());

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
    assert(command.type == PlayerScreenCommandType::OpenAudioPicker);
    inputState.moveControl(1);
    command = inputState.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(command.type == PlayerScreenCommandType::OpenSubtitlePicker);
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

    // Startup controls are visible while preparing, even if the normal 10-second focus window
    // has expired or a playback transition reset it between remote key presses.
    PlayerScreenState startupInputState;
    startupInputState.beginPlayback(0, 50'000);
    assert(startupInputState.controlsVisible(now, true));
    assert(!startupInputState.controlsActive(now));
    command = startupInputState.handleInput(PlayerScreenInput::Up, now, true);
    assert(command.type == PlayerScreenCommandType::None);
    assert(startupInputState.controlsActive(now));
    startupInputState.beginPlayback(0, 50'000);
    command = startupInputState.handleInput(PlayerScreenInput::Right, now + 1s, true);
    assert(command.type == PlayerScreenCommandType::None);
    assert(startupInputState.controlSelection() == PlayerControl::NextEpisode);
    startupInputState.hideControls();
    command = startupInputState.handleInput(PlayerScreenInput::Right, now + 12s, true);
    assert(command.type == PlayerScreenCommandType::None);
    assert(startupInputState.controlSelection() == PlayerControl::NextEpisode);
    startupInputState.beginPlayback(0, 50'000, false);
    assert(!startupInputState.controlsVisible(now + 13s, true));
    command = startupInputState.handleInput(PlayerScreenInput::Right, now + 13s, true);
    assert(command.type == PlayerScreenCommandType::SeekForward);

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
    assert(!ambient.sampleDue(now + 500ms));
    assert(ambient.sampleDue(now + 1s));
    ambient.noteSampleAttempt(now + 1s);
    assert(!ambient.sampleDue(now + 1500ms));
    assert(ambient.sampleDue(now + 2s));
    assert(ambient.sampleCount() == 1);
    const auto ambientTarget = ambient.targetColor();
    assert(ambientTarget.r > 0.0f);
    assert(ambientTarget.r < 1.0f);
    const auto ambientStart = ambient.displayColor(now);
    const auto ambientLater = ambient.displayColor(now + 3s);
    assert(ambientLater.r > ambientStart.r);

    AmbientBarColorState fastAmbient;
    for (int i = 0; i < 6; ++i) {
        fastAmbient.addSample({0.25f, 0.0f, 0.0f}, now + std::chrono::seconds(i));
    }
    assert(fastAmbient.sampleCount() == 6);
    fastAmbient.addSample({0.0f, 0.0f, 0.25f}, now + 6s);
    assert(fastAmbient.sampleCount() == 6);
    assert(std::abs(fastAmbient.targetColor().r - 0.25f * 5.0f / 6.0f) < 0.001f);
    for (int i = 7; i <= 11; ++i) {
        fastAmbient.addSample({0.0f, 0.0f, 0.25f}, now + std::chrono::seconds(i));
    }
    assert(fastAmbient.targetColor().r < 0.001f);
    assert(std::abs(fastAmbient.targetColor().b - 0.25f) < 0.001f);
    assert(std::abs(fastAmbient.displayColor(now + 12s).b - 0.25f) < 0.001f);

    AmbientBarColorState easedAmbient;
    easedAmbient.addSample({0.25f, 0.0f, 0.0f}, now);
    assert(easedAmbient.displayColor(now).r == 0.0f);
    assert(std::abs(easedAmbient.displayColor(now + 250ms).r - 0.25f * 0.15625f) < 0.001f);
    const auto midpoint = easedAmbient.displayColor(now + 500ms);
    assert(std::abs(midpoint.r - 0.125f) < 0.001f);
    easedAmbient.addSample({0.0f, 0.0f, 0.25f}, now + 500ms);
    assert(std::abs(easedAmbient.displayColor(now + 500ms).r - midpoint.r) < 0.001f);
    assert(std::abs(easedAmbient.displayColor(now + 1s).b - 0.0625f) < 0.001f);
    assert(std::abs(easedAmbient.displayColor(now + 1500ms).b - 0.125f) < 0.001f);
    easedAmbient.reset();
    assert(easedAmbient.displayColor(now + 2s).b == 0.0f);
    AmbientFramePixels letterboxedPixels{};
    for (int y = 14; y < kAmbientSampleHeight - 14; ++y) {
        for (int x = 0; x < kAmbientSampleWidth; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * kAmbientSampleWidth + x) * 4;
            letterboxedPixels[offset] = 140;
            letterboxedPixels[offset + 1] = 80;
            letterboxedPixels[offset + 2] = 50;
            letterboxedPixels[offset + 3] = 255;
        }
    }
    const auto detectedInsets = detectEmbeddedLetterboxBars(letterboxedPixels);
    assert(detectedInsets[0] > 0.12f && detectedInsets[0] < 0.14f);
    assert(detectedInsets[1] > 0.12f && detectedInsets[1] < 0.14f);
    AmbientBarColorState encodedAmbient;
    encodedAmbient.observeEmbeddedInsets(detectedInsets);
    assert(encodedAmbient.embeddedInsets()[0] == 0.0f);
    encodedAmbient.observeEmbeddedInsets(detectedInsets);
    assert(encodedAmbient.embeddedInsets()[0] > 0.12f);
    for (int i = 0; i < 5; ++i) encodedAmbient.observeEmbeddedInsets({});
    assert(encodedAmbient.embeddedInsets()[0] > 0.12f);
    encodedAmbient.observeEmbeddedInsets({});
    assert(encodedAmbient.embeddedInsets()[0] == 0.0f);
    encodedAmbient.observeEmbeddedInsets(detectedInsets);
    encodedAmbient.observeEmbeddedInsets(detectedInsets);
    encodedAmbient.reset();
    assert(encodedAmbient.embeddedInsets()[0] == 0.0f);
    assert(detectEmbeddedLetterboxBars(AmbientFramePixels{})[0] == 0.0f);
    for (int y = 6; y < 14; ++y) {
        for (int x = 0; x < kAmbientSampleWidth; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * kAmbientSampleWidth + x) * 4;
            letterboxedPixels[offset] = 140;
            letterboxedPixels[offset + 1] = 80;
            letterboxedPixels[offset + 2] = 50;
        }
    }
    assert(detectEmbeddedLetterboxBars(letterboxedPixels)[0] == 0.0f);

    state.openTrackPicker(PlayerTrackPicker::Subtitle, {{-1, "Off", false}, {6, "English", true}, {9, "French", false}},
                          now);
    assert(state.trackPicker() == PlayerTrackPicker::Subtitle);
    assert(state.trackPickerSelection() == 1);
    assert(state.handleInput(PlayerScreenInput::Down, now + 1s).type == PlayerScreenCommandType::None);
    assert(state.trackPickerSelection() == 2);
    auto chosen = state.handleInput(PlayerScreenInput::Activate, now + 2s);
    assert(chosen.type == PlayerScreenCommandType::SelectSubtitleTrack);
    assert(chosen.trackIndex == 9);
    assert(state.trackPicker() == PlayerTrackPicker::None);
    state.openTrackPicker(PlayerTrackPicker::Audio, {{2, "English", true}}, now);
    assert(state.handleInput(PlayerScreenInput::Back, now + 1s).type == PlayerScreenCommandType::None);
    assert(state.trackPicker() == PlayerTrackPicker::None);

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

    PlayerScreenState seekEnd;
    seekEnd.beginPlayback(0, 2'505'632);
    assert(seekEnd.prematureSeekEndTarget(2'505'632, now) == -1);
    seekEnd.beginSeek(1'260'000, now);
    // Telemetry may already have cleared pendingSeekTarget after a failed
    // HTTP seek; the most recent explicit target must still be recoverable.
    seekEnd.applyObservedPosition(2'505'632, now + 20s);
    assert(seekEnd.prematureSeekEndTarget(2'505'632, now + 20s) == 1'260'000);
    assert(seekEnd.prematureSeekEndTarget(2'505'632, now + 91s) == -1);
    seekEnd.beginSeek(2'500'000, now);
    assert(seekEnd.prematureSeekEndTarget(2'505'632, now + 1s) == -1);

    return 0;
}
