#pragma once
#include "app_screen.hpp"
#include "app_settings.hpp"
#include "browse_screen.hpp"
#include "display_mode.hpp"
#include "media_player.hpp"
#include "media_session.hpp"
#include "playback_coordinator.hpp"
#include "playback_runtime_controller.hpp"
#include "player_screen.hpp"
#include "renderer.hpp"
#include "video_surface.hpp"

#include <android/log.h>

#include <chrono>
#include <string>

enum class PlaybackLifecycleCommand { Other, InitializeWindow, TerminateWindow, GainedFocus, LostFocus };
enum class PlaybackLifecycleDiagnostic { None, RestoredPreservedContext, RecreatedPlaybackSurface, ResumedAfterFocus };

struct PlaybackLifecycleEffects {
    bool reloadBrowse = false;
    PlaybackLifecycleDiagnostic diagnostic = PlaybackLifecycleDiagnostic::None;
};

class PlaybackLifecycleFlow {
public:
    PlaybackLifecycleFlow(Renderer& renderer, NativeMediaPlayer& player, NativeMediaSession& mediaSession,
                          VideoSurface& videoSurface, DisplayModeController& displayMode,
                          PlaybackCoordinator& coordinator, PlayerScreenState& playerScreen,
                          PlaybackRuntimeController& runtime, Screen& screen, JellyfinSession& session,
                          AppSettings& settings, BrowseScreenState& browseState, bool& loading, std::string& error,
                          std::chrono::steady_clock::time_point& lastInteraction, bool& screensaverActive)
        : renderer_(renderer), player_(player), mediaSession_(mediaSession), videoSurface_(videoSurface),
          displayMode_(displayMode), coordinator_(coordinator), playerScreen_(playerScreen), runtime_(runtime),
          screen_(screen), session_(session), settings_(settings), browseState_(browseState), loading_(loading),
          error_(error), lastInteraction_(lastInteraction), screensaverActive_(screensaverActive) {}

    PlaybackLifecycleDiagnostic initializeWindow(ANativeWindow* window) {
        bool reusedRendererContext = false;
        if (window) {
            if (renderer_.contextReady() && !renderer_.ready()) reusedRendererContext = renderer_.attachWindow(window);
            if (!renderer_.ready()) renderer_.init(window);
        }
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        const bool restoreCandidate = screen_ == Screen::Player && playerScreen_.windowRestorePending() &&
                                      renderer_.ready() && coordinator_.activeTargetAvailable();
        const PlayerStatus restoreStatus = restoreCandidate ? player_.status() : PlayerStatus::Idle;
        const PlaybackWindowRestorePlan plan = coordinator_.windowRestorePlan(
            screen_ == Screen::Player, playerScreen_.windowRestorePending(), renderer_.ready(), reusedRendererContext,
            videoSurface_.ready(),
            restoreStatus != PlayerStatus::Idle && restoreStatus != PlayerStatus::Ended &&
                restoreStatus != PlayerStatus::Error,
            playerScreen_.resumeOnFocusRequested());
        if (!plan.restore) return PlaybackLifecycleDiagnostic::None;
        PlaybackLifecycleDiagnostic diagnostic = PlaybackLifecycleDiagnostic::None;
        if (settings_.refreshRateSwitching && plan.videoFrameRate > 0.0f && window)
            displayMode_.matchVideo(window, plan.videoFrameRate);
        if (plan.preservePlayer) {
            if (plan.resumePlayback) player_.play();
            mediaSession_.updateState(plan.resumePlayback ? MediaSessionState::Playing : MediaSessionState::Paused,
                                      playerScreen_.positionMs());
            diagnostic = PlaybackLifecycleDiagnostic::RestoredPreservedContext;
            __android_log_print(ANDROID_LOG_INFO, "sloppaTV",
                                "Restored playback with preserved libmpv and GLES context");
        } else {
            player_.stop();
            videoSurface_.release();
            std::string surfaceError;
            if (!videoSurface_.create(surfaceError)) {
                error_ = surfaceError.empty() ? "VIDEO SURFACE COULD NOT BE RESTORED" : surfaceError;
                return PlaybackLifecycleDiagnostic::None;
            }
            const PlaybackPlayerStartContext start =
                coordinator_.playerStartContext(PlaybackPlayerStartMode::WindowRestore);
            player_.startAsync(start.url, videoSurface_.surface(), playerScreen_.positionMs(),
                               settings_.playbackBufferPreset, start.audioOrdinal, start.subtitleStreamIndex,
                               start.subtitleOrdinal, start.externalSubtitleUrl, runtime_.activeDecodeMode());
            coordinator_.setPauseAfterRestart(plan.pauseAfterRestart);
            mediaSession_.updateState(MediaSessionState::Buffering, playerScreen_.positionMs());
            diagnostic = PlaybackLifecycleDiagnostic::RecreatedPlaybackSurface;
            __android_log_print(ANDROID_LOG_WARN, "sloppaTV",
                                "GLES context was not reusable during window restore; recreated playback surface");
        }
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(4));
        playerScreen_.completeWindowRestore();
        return diagnostic;
    }

    template <typename TelemetryExecutor> void terminateWindow(TelemetryExecutor& telemetry) {
        const bool candidate = screen_ == Screen::Player && coordinator_.activeTargetAvailable();
        const PlayerStatus status = candidate ? player_.status() : PlayerStatus::Idle;
        const PlaybackWindowSuspendPlan plan = coordinator_.windowSuspendPlan(
            screen_ == Screen::Player, status == PlayerStatus::Playing || status == PlayerStatus::Preparing);
        if (plan.suspend) {
            runtime_.refreshTelemetry(true);
            playerScreen_.beginWindowRestore(plan.resumePlayback);
            if (plan.resumePlayback) player_.pause();
            runtime_.reportProgress(telemetry, screen_ == Screen::Player, true);
            displayMode_.restore();
            mediaSession_.updateState(MediaSessionState::Paused, playerScreen_.positionMs());
        }
        if (!renderer_.detachWindow()) renderer_.shutdown();
    }

    PlaybackLifecycleEffects gainedFocus() {
        PlaybackLifecycleEffects effects;
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        if (screen_ == Screen::Player && playerScreen_.takeResumeOnFocus()) {
            player_.play();
            mediaSession_.updateState(MediaSessionState::Playing, playerScreen_.positionMs());
            effects.diagnostic = PlaybackLifecycleDiagnostic::ResumedAfterFocus;
            __android_log_print(ANDROID_LOG_INFO, "sloppaTV", "Resumed playback after focus restoration");
            return effects;
        }
        effects.reloadBrowse =
            screen_ == Screen::Browse && session_.valid() && !loading_ && !browseState_.activeContainer().id.empty();
        return effects;
    }

    void lostFocus() {
        screensaverActive_ = false;
        const bool playerScreenActive = screen_ == Screen::Player;
        const PlayerStatus status = playerScreenActive ? player_.status() : PlayerStatus::Idle;
        if (!shouldPausePlaybackForFocusLoss(playerScreenActive,
                                             status == PlayerStatus::Playing || status == PlayerStatus::Preparing))
            return;
        playerScreen_.requestResumeOnFocus();
        player_.pause();
        mediaSession_.updateState(MediaSessionState::Paused, playerScreen_.positionMs());
    }

private:
    Renderer& renderer_;
    NativeMediaPlayer& player_;
    NativeMediaSession& mediaSession_;
    VideoSurface& videoSurface_;
    DisplayModeController& displayMode_;
    PlaybackCoordinator& coordinator_;
    PlayerScreenState& playerScreen_;
    PlaybackRuntimeController& runtime_;
    Screen& screen_;
    JellyfinSession& session_;
    AppSettings& settings_;
    BrowseScreenState& browseState_;
    bool& loading_;
    std::string& error_;
    std::chrono::steady_clock::time_point& lastInteraction_;
    bool& screensaverActive_;
};
