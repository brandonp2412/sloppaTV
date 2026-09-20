#pragma once

#include "app_screen.hpp"
#include "app_settings.hpp"
#include "display_mode.hpp"
#include "media_display_text.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "media_session.hpp"
#include "navigation_stack.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_runtime_controller.hpp"
#include "player_screen.hpp"
#include "renderer.hpp"
#include "video_surface.hpp"

#include <android/native_window.h>

#include <chrono>
#include <mutex>
#include <optional>
#include <string>

struct PlaybackTransitionTickWork {
    std::optional<PendingPlaybackTransition> transition;
    bool subtitleLoadFailed = false;
};

struct PlaybackTransitionStartEffects {
    bool handled = false;
    bool surfaceFailed = false;
};

template <typename SubtitleLoadAsync> class PlaybackTransitionCoordinator {
public:
    PlaybackTransitionCoordinator(PlaybackCoordinator& playback, PlayerScreenState& playerScreen,
                                  PlaybackQueueState& queue, AppSettings& settings, NavigationStack<Screen>& navigation,
                                  Screen& screen, PlaybackRuntimeController& runtime,
                                  SubtitleLoadAsync& subtitleLoadAsync, NativeMediaPlayer& player,
                                  VideoSurface& videoSurface, Renderer& renderer, DisplayModeController& displayMode,
                                  NativeMediaSession& mediaSession, std::string& error,
                                  std::recursive_mutex& stateMutex)
        : playback_(playback), playerScreen_(playerScreen), queue_(queue), settings_(settings), navigation_(navigation),
          screen_(screen), runtime_(runtime), subtitleLoadAsync_(subtitleLoadAsync), player_(player),
          videoSurface_(videoSurface), renderer_(renderer), displayMode_(displayMode), mediaSession_(mediaSession),
          error_(error), stateMutex_(stateMutex) {}

    [[nodiscard]] PlaybackTransitionTickWork collect(bool windowAvailable) {
        PlaybackTransitionTickWork work;
        if (!windowAvailable) return work;

        std::scoped_lock lock(stateMutex_);
        work.transition = playback_.takePendingTransition();
        if (!work.transition) return work;

        auto& transition = *work.transition;
        auto& target = transition.target;
        auto& item = transition.item;
        const bool streamRestart = transition.streamRestart;
        const PlaybackTransitionPlan plan = playback_.activateTransition(
            item, target, streamRestart, transition.restartPaused, transition.audioStreamIndex,
            static_cast<VideoZoomMode>(settings_.zoomMode), std::chrono::steady_clock::now());
        playerScreen_.beginPlayback(plan.startPositionMs, plan.durationMs);
        if (plan.resetContinuation) playback_.syncQueueContinuation(queue_);

        if (const auto* selectedSubtitle = playback_.selectedSubtitleStream()) {
            const SubtitleStrategy strategy = subtitleStrategy(selectedSubtitle->codec);
            if (useNativeSubtitleRenderer(strategy, true)) {
                const std::string deliveryUrl =
                    strategy == SubtitleStrategy::ClientText ? target.subtitleUrl : std::string{};
                work.subtitleLoadFailed = !runtime_.loadSubtitle(subtitleLoadAsync_, *selectedSubtitle, deliveryUrl);
            }
        }

        if (!streamRestart) {
            if (screen_ == Screen::Player)
                navigation_.replace(Screen::Player);
            else
                navigation_.push(Screen::Player);
        } else {
            navigation_.replace(Screen::Player);
        }
        screen_ = navigation_.current();
        return work;
    }

    [[nodiscard]] PlaybackTransitionStartEffects start(const PlaybackTransitionTickWork& work, ANativeWindow* window) {
        if (!work.transition) return {};

        const auto& transition = *work.transition;
        const auto& target = transition.target;
        const auto& item = transition.item;

        player_.stop();
        videoSurface_.release();
        std::string surfaceError;
        if (!renderer_.ready() || !videoSurface_.create(surfaceError)) {
            std::scoped_lock lock(stateMutex_);
            error_ = surfaceError.empty() ? "VIDEO SURFACE IS NOT AVAILABLE" : surfaceError;
            return {.handled = true, .surfaceFailed = true};
        }

        if (settings_.refreshRateSwitching && item.videoFrameRate > 0.0f) {
            displayMode_.matchVideo(window, item.videoFrameRate);
        }
        mediaSession_.updateMetadata(item.name, episodeLabel(item), playbackPositionMsFromTicks(item.runtimeTicks));
        mediaSession_.updateState(MediaSessionState::Buffering, playbackPositionMsFromTicks(target.startTicks));
        playerScreen_.showOverlayFor(std::chrono::steady_clock::now(), std::chrono::seconds(5));
        runtime_.startResolvedTarget();
        return {.handled = true};
    }

private:
    PlaybackCoordinator& playback_;
    PlayerScreenState& playerScreen_;
    PlaybackQueueState& queue_;
    AppSettings& settings_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    PlaybackRuntimeController& runtime_;
    SubtitleLoadAsync& subtitleLoadAsync_;
    NativeMediaPlayer& player_;
    VideoSurface& videoSurface_;
    Renderer& renderer_;
    DisplayModeController& displayMode_;
    NativeMediaSession& mediaSession_;
    std::string& error_;
    std::recursive_mutex& stateMutex_;
};
