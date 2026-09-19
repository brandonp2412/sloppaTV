#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "account_async_executor.hpp"
#include "account_flow.hpp"
#include "app_screen.hpp"
#include "app_settings.hpp"
#include "app_screen_presentation.hpp"
#include "artwork_provider.hpp"
#include "async_completion_queue.hpp"
#include "audio_policy.hpp"
#include "browse_async_executor.hpp"
#include "browse_completion_controller.hpp"
#include "browse_navigation_controller.hpp"
#include "browse_renderer.hpp"
#include "browse_screen.hpp"
#include "brand_mark.hpp"
#include "cast_renderer.hpp"
#include "content_screen_presentation.hpp"
#include "content_mutation_flow.hpp"
#include "details_completion_controller.hpp"
#include "details_flow.hpp"
#include "details_renderer.hpp"
#include "details_screen.hpp"
#include "details_navigation_controller.hpp"
#include "details_async_executor.hpp"
#include "deep_link.hpp"
#include "diagnostics_screen.hpp"
#include "diagnostics_renderer.hpp"
#include "discovery.hpp"
#include "display_mode.hpp"
#include "external_playback_executor.hpp"
#include "external_playback_state.hpp"
#include "external_player.hpp"
#include "home_async_executor.hpp"
#include "home_completion_controller.hpp"
#include "home_completion_flow.hpp"
#include "home_navigation_controller.hpp"
#include "home_renderer.hpp"
#include "home_row_renderer.hpp"
#include "home_screen.hpp"
#include "home_visibility.hpp"
#include "input_navigation.hpp"
#include "image_decoder.hpp"
#include "item_menu_renderer.hpp"
#include "item_mutation_controller.hpp"
#include "item_mutation_executor.hpp"
#include "jellyfin.hpp"
#include "keyboard_renderer.hpp"
#include "jellyfin_search_executor.hpp"
#include "jni_env.hpp"
#include "launch_intent.hpp"
#include "login_renderer.hpp"
#include "media_card_renderer.hpp"
#include "media_display_text.hpp"
#include "media_grid_renderer.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "media_session.hpp"
#include "navigation_stack.hpp"
#include "playback_continuation.hpp"
#include "playback_continuation_executor.hpp"
#include "playback_completion_controller.hpp"
#include "playback_completion_flow.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_resolution_executor.hpp"
#include "playback_request_flow.hpp"
#include "playback_release_flow.hpp"
#include "playback_lifecycle_flow.hpp"
#include "playback_runtime_controller.hpp"
#include "playback_resolver.hpp"
#include "playback_session.hpp"
#include "playback_stream_executor.hpp"
#include "playback_telemetry.hpp"
#include "playback_telemetry_executor.hpp"
#include "playback_track_selection.hpp"
#include "playback_transition.hpp"
#include "player_completion_controller.hpp"
#include "player_controls_renderer.hpp"
#include "player_header_renderer.hpp"
#include "player_next_up_renderer.hpp"
#include "player_presentation.hpp"
#include "player_progress_renderer.hpp"
#include "player_screen.hpp"
#include "primary_screen_presentation.hpp"
#include "player_seek_feedback_renderer.hpp"
#include "player_skip_button_renderer.hpp"
#include "player_status_renderer.hpp"
#include "player_subtitle_renderer.hpp"
#include "player_trickplay_renderer.hpp"
#include "player_tracks.hpp"
#include "player_video_renderer.hpp"
#include "profiles_renderer.hpp"
#include "queue_overlay_renderer.hpp"
#include "queue_navigation_controller.hpp"
#include "queue_overlay_screen.hpp"
#include "quick_connect_executor.hpp"
#include "request_epoch.hpp"
#include "screensaver_policy.hpp"
#include "screensaver_renderer.hpp"
#include "screen_presentation.hpp"
#include "screen_chrome_renderer.hpp"
#include "search_completion_controller.hpp"
#include "search_flow.hpp"
#include "search_navigation_controller.hpp"
#include "search_renderer.hpp"
#include "search_screen.hpp"
#include "seerr.hpp"
#include "series_playback_executor.hpp"
#include "seerr_async_executor.hpp"
#include "seerr_connection_coordinator.hpp"
#include "seerr_completion_flow.hpp"
#include "seerr_domain.hpp"
#include "seerr_drive_picker_renderer.hpp"
#include "seerr_drive_navigation_controller.hpp"
#include "seerr_drive_picker_screen.hpp"
#include "seerr_home_projection.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "seerr_refresh_coordinator.hpp"
#include "seerr_request_coordinator.hpp"
#include "seerr_search_coordinator.hpp"
#include "server_info_completion_controller.hpp"
#include "server_info_executor.hpp"
#include "session_registry.hpp"
#include "session_store.hpp"
#include "settings_flow.hpp"
#include "settings_screen.hpp"
#include "similar_prefetch_controller.hpp"
#include "settings_renderer.hpp"
#include "status_overlay_renderer.hpp"
#include "subtitle_load_executor.hpp"
#include "system_text_input.hpp"
#include "system_text_input_controller.hpp"
#include "text_layout.hpp"
#include "ui_theme.hpp"
#include "ui_components.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"
#include "ui_presentation.hpp"
#include "unicode_text.hpp"
#include "renderer.hpp"
#include "task_runner.hpp"
#include "trickplay_policy.hpp"
#include "trickplay_preview.hpp"
#include "trickplay_tile_executor.hpp"
#include "video_surface.hpp"
#include "version_policy.hpp"
#include "virtual_keyboard.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <limits>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

using namespace std::chrono_literals;

#ifndef SLOPPATV_VERSION_NAME
#define SLOPPATV_VERSION_NAME "dev"
#endif

namespace {
constexpr const char* kTag = "sloppaTV";

void logPlaybackReportFailure(const char* stage, const std::string& itemId, const ApiResult& result) {
    if (result.ok) return;
    __android_log_print(ANDROID_LOG_WARN, kTag, "Playback %s report failed for %s: %s", stage, itemId.c_str(),
                        result.error.c_str());
}

struct PendingTickWork {
    std::optional<ExternalPlayerResult> externalResult;
    std::optional<ExternalPlaybackLaunch> completedExternalPlayback;
    std::optional<ExternalPlaybackLaunch> externalLaunch;
    std::optional<PendingPlaybackTransition> playbackTransition;
};

using QueuedPlaybackCompletion = AppQueuedPlaybackCompletion;

using AsyncCompletion =
    std::variant<SystemTextInputEvent, SeerrDeleteCompletion, SeerrRequestCompletion, SeerrStorageRefreshCompletion,
                 SeerrPendingRefreshCompletion, SeerrSearchCompletion, SeerrConnectCompletion, JellyfinSearchCompletion,
                 ItemMenuDetailCompletion, PersonItemsCompletion, DiagnosticsCompletion, SeasonsCompletion,
                 EpisodesCompletion, BrowsePageCompletion, ServerInfoNoticeCompletion, FavoriteCompletion,
                 PlayedCompletion, MetadataRefreshCompletion, DeleteItemCompletion, DiscoveryCompletion,
                 LoginCompletion, DetailsItemCompletion, DetailsSimilarCompletion, SimilarPrefetchCompletion,
                 EpisodeSeriesContextRequestCompletion, EpisodeSeriesContextCompletion, QuickConnectStartedCompletion,
                 QuickConnectFailedCompletion, QuickConnectAuthenticatedCompletion, QuickConnectTimedOutCompletion,
                 HomeCoreCompletion, HomeSecondaryCompletion, ExternalPlaybackCompletion, SubtitleLoadCompletion,
                 TrickplayTileCompletion, ArtworkLoadCompletion, MediaSegmentsCompletion, NextEpisodeCompletion,
                 PlaybackAdjacentCompletion, PlaybackReportCompletion, QueuedPlaybackCompletion,
                 PlayerItemPlaybackCompletion, AutoplayPlaybackCompletion, StreamRestartCompletion,
                 FallbackPlaybackCompletion, BeginPlaybackCompletion, SeriesPlayAllCompletion>;

class SloppaApp;
SloppaApp* gActiveApp = nullptr;
std::mutex gActiveAppMutex;

class SloppaApp {
public:
    explicit SloppaApp(android_app* app)
        : app_(app), renderer_(app->activity->vm, app->activity->clazz), api_(app->activity->vm, app->activity->clazz),
          similarPrefetchApi_(app->activity->vm, app->activity->clazz), seerr_(app->activity->vm, app->activity->clazz),
          seerrSearch_(app->activity->vm, app->activity->clazz),
          player_(app->activity->vm, app->activity->clazz, app->activity->internalDataPath),
          mediaSession_(app->activity->vm, app->activity->clazz),
          externalPlayer_(app->activity->vm, app->activity->clazz), imageDecoder_(app->activity->vm),
          videoSurface_(app->activity->vm),
          tasks_(
              4,
              [app] {
                  if (app && app->looper) ALooper_wake(app->looper);
              },
              [](const std::string& error) {
                  __android_log_print(ANDROID_LOG_ERROR, kTag, "Background task exception: %s", error.c_str());
              }),
          similarPrefetchTasks_(
              1,
              [app] {
                  if (app && app->looper) ALooper_wake(app->looper);
              },
              [](const std::string& error) {
                  __android_log_print(ANDROID_LOG_ERROR, kTag, "Similar prefetch exception: %s", error.c_str());
              }),
          accountAsync_(api_, tasks_, asyncCompletions_), quickConnectAsync_(api_, tasks_, asyncCompletions_),
          detailsAsync_(api_, tasks_, asyncCompletions_), browseAsync_(api_, tasks_, asyncCompletions_),
          jellyfinSearchAsync_(api_, tasks_, asyncCompletions_), serverInfoAsync_(api_, tasks_, asyncCompletions_),
          itemMutationAsync_(api_, tasks_, asyncCompletions_), homeAsync_(api_, tasks_, asyncCompletions_),
          externalPlaybackAsync_(
              api_, tasks_, asyncCompletions_, requestEpochs_.playback,
              [](const ExternalPlaybackDiagnostic& diagnostic) {
                  if (diagnostic.kind == ExternalPlaybackDiagnosticKind::StopReport) {
                      __android_log_print(ANDROID_LOG_WARN, kTag, "External playback stop report failed: %s",
                                          diagnostic.error.c_str());
                      return;
                  }
                  __android_log_print(ANDROID_LOG_WARN, kTag, "External playback media segments unavailable: %s",
                                      diagnostic.error.c_str());
              }),
          playbackTelemetryAsync_(api_, tasks_, asyncCompletions_),
          playbackContinuationAsync_(api_, tasks_, asyncCompletions_),
          playbackResolutionAsync_(api_, tasks_, asyncCompletions_, requestEpochs_.playback),
          playbackStreamAsync_(api_, tasks_, asyncCompletions_, requestEpochs_.playback, logPlaybackReportFailure),
          seriesPlaybackAsync_(
              api_, tasks_, asyncCompletions_, requestEpochs_.playback,
              [](const SeriesEpisodeSlot& slot) {
                  __android_log_print(
                      ANDROID_LOG_WARN, kTag,
                      "No available source found for duplicate S%02dE%02d slot; retaining first server result",
                      slot.season, slot.episode);
              }),
          subtitleLoadAsync_(api_, tasks_, asyncCompletions_, requestEpochs_.playback,
                             [](const SubtitleLoadDiagnostic& diagnostic) {
                                 if (diagnostic.kind == SubtitleLoadDiagnosticKind::Fallback) {
                                     __android_log_print(ANDROID_LOG_INFO, kTag,
                                                         "Subtitle stream %d unavailable; using fallback stream %d",
                                                         diagnostic.requestedSubtitleIndex, diagnostic.subtitleIndex);
                                     return;
                                 }
                                 __android_log_print(ANDROID_LOG_WARN, kTag,
                                                     "Subtitle load failed item=%s stream=%d codec=%s reason=%s",
                                                     diagnostic.itemId.c_str(), diagnostic.subtitleIndex,
                                                     diagnostic.codec.c_str(), diagnostic.reason.c_str());
                             }),
          trickplayTileAsync_(api_, imageDecoder_, tasks_, asyncCompletions_),
          seerrAsync_(seerr_, seerrSearch_, api_, tasks_, asyncCompletions_),
          artwork_(api_, seerr_, imageDecoder_, tasks_, asyncCompletions_,
                   [](const HomeArtworkRequest& request, const ArtworkLoadResult& loaded) {
                       if (loaded.ok()) return;
                       __android_log_print(ANDROID_LOG_WARN, kTag,
                                           loaded.failure == ArtworkLoadFailure::Download
                                               ? "Home artwork download failed item=%s type=%s reason=%s"
                                               : "Home artwork decode failed item=%s type=%s reason=%s",
                                           request.itemId.c_str(), request.itemType.c_str(), loaded.error.c_str());
                   }),
          uiPresentation_(renderer_, artwork_), settingsFlow_(settings_), seerrConnection_(seerrDomain_, seerrAsync_),
          seerrRefresh_(seerrDomain_, seerrAsync_), seerrRequest_(seerrDomain_, seerrAsync_),
          searchFlow_(seerrDomain_, seerrAsync_, jellyfinSearchAsync_, requestEpochs_.search,
                      requestEpochs_.seerrSearch),
          homeVisibility_(session_, hiddenHomeItems_),
          homeCompletionFlow_(homeVisibility_, home_, homeState_, homeLoading_, homeRetryAt_, homeRetryAttempt_),
          seerrCompletionFlow_(seerrDomain_, contentMutationFlow_, searchFlow_.state(), detailsFlow_, settings_,
                               session_, seerrConnection_, seerrRequest_, seerrRefresh_),
          playbackCompletionFlow_(playbackCoordinator_, queueState_, detailsFlow_, playerScreenState_),
          playbackRequestFlow_(queueState_, playbackCoordinator_, playerScreenState_, detailsFlow_, session_, settings_,
                               requestEpochs_.playback, screen_, loading_, error_),
          playbackRuntime_(playbackCoordinator_, playerScreenState_, player_, videoSurface_, session_, settings_,
                           requestEpochs_.playback, loading_, error_, dataPath_),
          playbackReleaseFlow_(requestEpochs_.playback, player_, videoSurface_, displayMode_, mediaSession_,
                               playbackCoordinator_, playerScreenState_, session_, home_, homeState_, browseState_,
                               searchFlow_.state(), detailsFlow_, queueState_, playbackTelemetryAsync_,
                               hiddenHomeItems_, renderer_, trickplayState_),
          playbackLifecycleFlow_(renderer_, player_, mediaSession_, videoSurface_, displayMode_, playbackCoordinator_,
                                 playerScreenState_, playbackRuntime_, screen_, session_, settings_, browseState_,
                                 loading_, error_, lastInteraction_, screensaverActive_) {
        __android_log_print(ANDROID_LOG_INFO, kTag, "Startup init: platform bridges ready");
        dataPath_ = app->activity->internalDataPath ? app->activity->internalDataPath : "";
        artwork_.setDataPath(dataPath_);
        brandMark_.load(app_ && app_->activity ? app_->activity->assetManager : nullptr, imageDecoder_);
        const LaunchRequest launchRequest = readLaunchRequest(app_);
        pendingDeepLinkItemId_ = launchRequest.itemId;
        pendingSearchQuery_ = launchRequest.searchQuery;
        loadSession();
        if (!settings_.externalPlayerComponent.empty())
            settingsFlow_.refreshExternalPlayers(externalPlayer_.availablePlayers());
        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "Startup init: session loaded valid=%d seerrConfigured=%d seerrSession=%d drivePicker=%d",
                            session_.valid() ? 1 : 0, !settings_.seerrServer.empty() ? 1 : 0,
                            !settings_.seerrSessionCookie.empty() ? 1 : 0, settings_.seerrSelectDrive ? 1 : 0);
        if (session_.valid()) {
            resetNavigation(Screen::Home);
            loadHomeAsync();
            if (!settings_.seerrServer.empty() && !settings_.seerrSessionCookie.empty()) {
                connectSeerrAsync(false);
            }
        } else {
            resetNavigation(Screen::Login);
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Startup init: constructor complete");
    }

    void resetNavigation(Screen screen) {
        navigation_.reset(screen);
        screen_ = screen;
    }

    void pushScreen(Screen screen) {
        navigation_.push(screen);
        screen_ = navigation_.current();
    }

    void replaceScreen(Screen screen) {
        navigation_.replace(screen);
        screen_ = navigation_.current();
    }

    void popScreen(Screen fallback = Screen::Home) {
        screen_ = navigation_.popOr(fallback);
        if (screen_ == Screen::Browse && session_.valid() && !loading_ && !browseState_.activeContainer().id.empty()) {
            loadBrowsePageAsync(false);
        }
    }

    ~SloppaApp() {
        api_.cancelPendingRequests();
        similarPrefetchApi_.cancelPendingRequests();
        seerr_.cancelPendingRequests();
        seerrSearch_.cancelPendingRequests();
        requestEpochs_.invalidateAll();
        similarPrefetchTasks_.shutdown();
        tasks_.shutdown();
        stopPlayback();
        brandMark_.release(renderer_);
        renderer_.shutdown();
    }

    void warmDeviceCapabilitiesAsync() {
        tasks_.submit([this] { api_.warmDeviceCodecSupport(); });
    }

    static void handleAppCommand(android_app* app, int32_t command) {
        auto* self = static_cast<SloppaApp*>(app->userData);
        if (self) self->onAppCommand(command);
    }

    static int32_t handleInput(android_app* app, AInputEvent* event) {
        auto* self = static_cast<SloppaApp*>(app->userData);
        return self ? self->onInput(event) : 0;
    }

    void run() {
        while (!app_->destroyRequested) {
            int timeoutMs = -1;
            const auto pollNow = std::chrono::steady_clock::now();
            bool burstActive = pollNow < renderBurstUntil_;
            {
                std::scoped_lock lock(stateMutex_);
                if (screen_ == Screen::Player || burstActive)
                    timeoutMs = 0;
                else if (screensaverActive_)
                    timeoutMs = 30000;
                else if (loading_ || homeLoading_ || contentMutationFlow_.loading() || searchFlow_.state().loading() ||
                         seerrDomain_.searchLoading() || accountFlow_.state().quickConnectActive())
                    timeoutMs = 100;
                else {
                    const int64_t delayMs = screensaverDelayMs(settings_.screensaverMinutes);
                    if (delayMs > 0) {
                        const int64_t idleMs =
                            std::chrono::duration_cast<std::chrono::milliseconds>(pollNow - lastInteraction_).count();
                        timeoutMs = static_cast<int>(std::clamp<int64_t>(delayMs - idleMs, 0, delayMs));
                    }
                }
                if (searchFlow_.state().debouncePending()) {
                    const int64_t searchDelayMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                      searchFlow_.state().debounceDeadline() - pollNow)
                                                      .count();
                    const int searchTimeoutMs = static_cast<int>(std::max<int64_t>(0, searchDelayMs));
                    timeoutMs = timeoutMs < 0 ? searchTimeoutMs : std::min(timeoutMs, searchTimeoutMs);
                }
                if (seerrDomain_.searchDebouncePending()) {
                    const int64_t seerrDelayMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     seerrDomain_.searchDebounceDeadline() - pollNow)
                                                     .count();
                    const int seerrTimeoutMs = static_cast<int>(std::max<int64_t>(0, seerrDelayMs));
                    timeoutMs = timeoutMs < 0 ? seerrTimeoutMs : std::min(timeoutMs, seerrTimeoutMs);
                }
                auto tightenTimeoutUntil = [&](std::chrono::steady_clock::time_point deadline) {
                    if (deadline == std::chrono::steady_clock::time_point{}) return;
                    if (pollNow >= deadline) {
                        timeoutMs = 0;
                        return;
                    }
                    const auto remaining =
                        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - pollNow).count();
                    const int statusTimeoutMs = static_cast<int>(std::max<int64_t>(1, remaining));
                    timeoutMs = timeoutMs < 0 ? statusTimeoutMs : std::min(timeoutMs, statusTimeoutMs);
                };
                tightenTimeoutUntil(statusOverlayState_.wakeDeadline());
                tightenTimeoutUntil(homeRetryAt_);
                tightenTimeoutUntil(similarPrefetch_.dueDeadline());
                if (!seerrDomain_.pendingRequestsLoading() &&
                    (screen_ == Screen::Home || (screen_ == Screen::ItemMenu && isSeerrItem(detailsFlow_.item())))) {
                    tightenTimeoutUntil(seerrDomain_.pendingRequestsRefreshDeadline());
                }
                if (screen_ == Screen::Search && !searchFlow_.state().keyboard() &&
                    !searchFlow_.state().results().empty()) {
                    // Search cards can contain a slow pixel-based marquee. Redraw near display
                    // cadence without busy-spinning the looper when the search screen is idle.
                    timeoutMs = timeoutMs < 0 ? 16 : std::min(timeoutMs, 16);
                }
            }

            int events = 0;
            android_poll_source* source = nullptr;
            int pollResult = ALooper_pollOnce(timeoutMs, nullptr, &events, reinterpret_cast<void**>(&source));
            bool shouldRender =
                pollResult == ALOOPER_POLL_WAKE || (pollResult == ALOOPER_POLL_TIMEOUT && timeoutMs >= 0);

            while (pollResult >= 0) {
                if (source) {
                    source->process(app_, source);
                    shouldRender = true;
                }
                if (app_->destroyRequested) break;
                source = nullptr;
                pollResult = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source));
                if (pollResult == ALOOPER_POLL_WAKE) shouldRender = true;
            }

            runDueLiveSearch();
            tick();
            bool playerScreen = false;
            bool screensaver = false;
            {
                std::scoped_lock lock(stateMutex_);
                playerScreen = screen_ == Screen::Player;
                if (playerScreen) {
                    screensaverActive_ = false;
                } else if (!screensaverActive_) {
                    const auto now = std::chrono::steady_clock::now();
                    const int64_t idleMs =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInteraction_).count();
                    screensaverActive_ = shouldActivateScreensaver(
                        settings_.screensaverMinutes, idleMs, false,
                        loading_ || homeLoading_ || contentMutationFlow_.loading() || searchFlow_.state().loading() ||
                            seerrDomain_.searchLoading() || accountFlow_.state().quickConnectActive());
                }
                screensaver = screensaverActive_;
            }
            burstActive = std::chrono::steady_clock::now() < renderBurstUntil_;
            if (renderer_.ready() && (playerScreen || screensaver || shouldRender || burstActive)) render();
        }
    }

    void onSystemTextInputChanged(int mode, const std::string& value) {
        queueSystemTextInputEvent(SystemTextInputPhase::Changed, mode, value);
    }

    void onSystemTextInputCancelled(int mode, const std::string& value) {
        queueSystemTextInputEvent(SystemTextInputPhase::Cancelled, mode, value);
    }

    void onSystemTextInputDone(int mode, const std::string& value) {
        queueSystemTextInputEvent(SystemTextInputPhase::Done, mode, value);
    }

    void onNewLaunchIntent(const std::string& action, const std::string& data, const std::string& query) {
        LaunchRequest request = launchRequestFromIntentParts(action, data, query);
        if (request.itemId.empty() && request.searchQuery.empty()) return;
        {
            std::scoped_lock lock(stateMutex_);
            pendingRuntimeLaunchRequest_ = std::move(request);
        }
        renderBurstUntil_ = std::chrono::steady_clock::now() + 500ms;
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void onSystemBackPressed() {
        const auto now = std::chrono::steady_clock::now();
        {
            std::scoped_lock lock(stateMutex_);
            if (systemTextInputController_.active()) return;
            renderBurstUntil_ = now + 150ms;
            lastInteraction_ = now;
            if (screensaverActive_) {
                screensaverActive_ = false;
            } else if (queueState_.overlayActive()) {
                handleQueueOverlayKey(AKEYCODE_BACK);
            } else if (screen_ == Screen::Player) {
                handlePlayerKey(AKEYCODE_BACK);
            } else {
                dispatchScreenKey(AKEYCODE_BACK);
            }
        }
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

private:
    void queueSystemTextInputEvent(SystemTextInputPhase phase, int mode, std::string value) {
        asyncCompletions_.push(systemTextInputEvent(phase, mode, std::move(value)));
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void applySystemTextInputEffects(const SystemTextInputEffects& effects) {
        if (effects.scheduleSearch) scheduleLiveSearch();
        if (effects.cancelSeerrSearch) searchFlow_.cancelSeerrSearch();
        if (effects.submitSearch) searchAsync();
        if (effects.invalidateSeerrStorage) seerrDomain_.invalidateStorageTargets();
        if (effects.saveSession) saveSession(session_);

        switch (effects.notice) {
        case SystemTextInputNotice::None:
            break;
        case SystemTextInputNotice::SeerrDisconnected:
            showNotice("SEERR DISCONNECTED", 3s);
            break;
        case SystemTextInputNotice::SeerrServerSaved:
            showNotice("SEERR SERVER SAVED", 3s);
            break;
        case SystemTextInputNotice::SeerrApiKeyCleared:
            showNotice("SEERR API KEY CLEARED", 3s);
            break;
        case SystemTextInputNotice::SeerrApiKeySaved:
            showNotice("SEERR API KEY SAVED", 3s);
            break;
        }

        if (effects.refreshSeerr) {
            refreshSeerrPendingAsync();
            refreshSeerrStorageAsync(true);
        }
        if (effects.renderBurst.count() > 0) renderBurstUntil_ = std::chrono::steady_clock::now() + effects.renderBurst;
    }

    void onAppCommand(int32_t command) {
        std::scoped_lock lock(stateMutex_);
        switch (command) {
        case APP_CMD_INIT_WINDOW:
            (void)playbackLifecycleFlow_.initializeWindow(app_->window);
            break;
        case APP_CMD_TERM_WINDOW:
            playbackLifecycleFlow_.terminateWindow(playbackTelemetryAsync_);
            break;
        case APP_CMD_GAINED_FOCUS: {
            const PlaybackLifecycleEffects effects = playbackLifecycleFlow_.gainedFocus();
            if (effects.reloadBrowse) loadBrowsePageAsync(false);
            break;
        }
        case APP_CMD_LOST_FOCUS:
            playbackLifecycleFlow_.lostFocus();
            break;
        default:
            break;
        }
    }

    void dispatchScreenKey(int32_t key) {
        switch (screen_) {
        case Screen::Login:
            handleLoginKey(key);
            break;
        case Screen::Profiles:
            handleProfilesKey(key);
            break;
        case Screen::Home:
            handleHomeKey(key);
            break;
        case Screen::Browse:
            handleBrowseKey(key);
            break;
        case Screen::Search:
            handleSearchKey(key);
            break;
        case Screen::Settings:
            handleSettingsKey(key);
            break;
        case Screen::Diagnostics:
            handleDiagnosticsKey(key);
            break;
        case Screen::Details:
            handleDetailsKey(key);
            break;
        case Screen::Cast:
            handleCastKey(key);
            break;
        case Screen::PersonItems:
            handlePersonItemsKey(key);
            break;
        case Screen::ItemMenu:
            handleItemMenuKey(key);
            break;
        case Screen::Seasons:
            handleSeasonsKey(key);
            break;
        case Screen::Episodes:
            handleEpisodesKey(key);
            break;
        case Screen::SeerrDrivePicker:
            handleSeerrDrivePickerKey(key);
            break;
        case Screen::Player:
            break;
        }
    }

    int32_t onInput(AInputEvent* event) {
        if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY) return 0;
        const int32_t action = AKeyEvent_getAction(event);
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) return 0;

        const int32_t rawKey = AKeyEvent_getKeyCode(event);
        const int32_t key = rawKey == AKEYCODE_ESCAPE ? AKEYCODE_BACK : rawKey;
        const int32_t meta = AKeyEvent_getMetaState(event);
        const int repeatCount = AKeyEvent_getRepeatCount(event);
        const auto inputNow = std::chrono::steady_clock::now();
        renderBurstUntil_ = inputNow + 150ms;
        std::scoped_lock lock(stateMutex_);
        if (systemTextInputController_.active()) return 0;

        if (action == AKEY_EVENT_ACTION_UP) {
            // NativeActivity may apply its own BACK handling if the release is left
            // unconsumed, even when we already handled BACK on key-down. Consume both
            // halves so in-app BACK navigation cannot also finish the activity.
            if (key == AKEYCODE_BACK) return 1;
            if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && homeState_.centerPending()) {
                const bool activate = homeState_.consumeCenterRelease(screen_ == Screen::Home);
                if (activate) handleHomeKey(key);
                return 1;
            }
            return 0;
        }
        if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && homeState_.centerPending() &&
            homeState_.centerLongPressed()) {
            return 1;
        }

        lastInteraction_ = inputNow;
        if (screensaverActive_) {
            screensaverActive_ = false;
            return 1;
        }

        if (queueState_.overlayActive()) {
            handleQueueOverlayKey(key);
            return 1;
        }

        if (screen_ == Screen::Player) {
            handlePlayerKey(key, repeatCount);
            return 1;
        }

        if (screen_ == Screen::Home && homeState_.row() >= 0 &&
            (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)) {
            if (repeatCount == 0) {
                homeState_.beginCenterPress();
                return 1;
            }
            if (homeState_.centerPending() && !homeState_.centerLongPressed() &&
                homeState_.row() < static_cast<int>(home_.rows.size())) {
                auto& section = home_.rows[static_cast<size_t>(homeState_.row())];
                if (section.title != "My Media" && !section.items.empty()) {
                    const int selection =
                        homeState_.selection(homeState_.row(), static_cast<int>(section.items.size()));
                    openItemMenuForItem(section.items[static_cast<size_t>(selection)]);
                    homeState_.markCenterLongPressed();
                }
                return 1;
            }
        }

        const TextEntryResult textEntry =
            handlePhysicalTextInput(screen_, accountFlow_.state(), searchFlow_.state(), key, meta);
        if (textEntry.searchChanged) scheduleLiveSearch();
        if (textEntry.handled) return 1;

        dispatchScreenKey(key);
        return 1;
    }

    void scheduleLiveSearch() {
        const SearchScheduleEffects effects = searchFlow_.scheduleLive(
            std::chrono::steady_clock::now(), SeerrClient::configured(settings_.seerrServer, seerrAuth()));
        if (effects.clearError) error_.clear();
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void runDueLiveSearch() {
        std::scoped_lock lock(stateMutex_);
        applySearchDispatchEffects(
            searchFlow_.runDue(screen_ == Screen::Search, session_, seerrEndpoint(), std::chrono::steady_clock::now()));
    }

    bool showSystemTextInput(const std::string& initial, const std::string& hint, int mode, bool password = false) {
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return false;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return false;
        jobject activity = app_->activity->clazz;
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID method = activityClass ? env->GetMethodID(activityClass, "showTextInput",
                                                            "(Ljava/lang/String;Ljava/lang/String;IZ)Z")
                                         : nullptr;
        jstring jInitial = env->NewStringUTF(initial.c_str());
        jstring jHint = env->NewStringUTF(hint.c_str());
        jboolean shown = JNI_FALSE;
        if (method && jInitial && jHint) {
            shown = env->CallBooleanMethod(activity, method, jInitial, jHint, static_cast<jint>(mode),
                                           password ? JNI_TRUE : JNI_FALSE);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            shown = JNI_FALSE;
        }
        if (jHint) env->DeleteLocalRef(jHint);
        if (jInitial) env->DeleteLocalRef(jInitial);
        if (activityClass) env->DeleteLocalRef(activityClass);
        if (shown == JNI_TRUE) systemTextInputController_.begin(mode, initial);
        return shown == JNI_TRUE;
    }

    void hideSystemTextInput() {
        systemTextInputController_.hide();
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return;
        jobject activity = app_->activity->clazz;
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID method = activityClass ? env->GetMethodID(activityClass, "hideTextInput", "()V") : nullptr;
        if (method) env->CallVoidMethod(activity, method);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (activityClass) env->DeleteLocalRef(activityClass);
    }

    void activateKeyboardKey(bool forSearch) {
        const VirtualKeyboardEffects effects =
            virtualKeyboard_.activate(forSearch, searchFlow_.state(), accountFlow_.state());
        if (effects.searchChanged) scheduleLiveSearch();
        if (effects.submitSearch) searchAsync();
    }

    void handleLoginKey(int32_t key) {
        const AccountNavigationAction navigation = accountFlow_.handleLogin(screenNavigationKeyForAndroidKey(key));
        switch (navigation.type) {
        case AccountNavigationActionType::None:
            return;
        case AccountNavigationActionType::FinishActivity:
            ANativeActivity_finish(app_->activity);
            return;
        case AccountNavigationActionType::CancelQuickConnect:
            api_.cancelPendingRequests();
            requestEpochs_.auth.invalidate();
            loading_ = false;
            error_.clear();
            return;
        case AccountNavigationActionType::MoveKeyboard:
            virtualKeyboard_.move(navigation.dx, navigation.dy);
            return;
        case AccountNavigationActionType::ActivateKeyboard:
            activateKeyboardKey(false);
            return;
        case AccountNavigationActionType::EditField: {
            const int field = navigation.index;
            const int mode = kTextInputLoginServer + field;
            static constexpr std::array<const char*, 3> hints{"Jellyfin server URL", "Jellyfin username",
                                                              "Jellyfin password"};
            accountFlow_.state().setKeyboardActive(!showSystemTextInput(accountFlow_.state().field(field),
                                                                        hints[static_cast<size_t>(field)], mode,
                                                                        field == AccountScreenState::kPasswordField));
            if (accountFlow_.state().keyboardActive()) virtualKeyboard_.reset();
            return;
        }
        case AccountNavigationActionType::Login:
            loginAsync();
            return;
        case AccountNavigationActionType::QuickConnect:
            quickConnectAsync();
            return;
        case AccountNavigationActionType::Discover:
            discoverServersAsync();
            return;
        case AccountNavigationActionType::OpenProfiles:
            openProfiles();
            return;
        case AccountNavigationActionType::ProfilesBack:
        case AccountNavigationActionType::AddAccount:
        case AccountNavigationActionType::SwitchSession:
        case AccountNavigationActionType::ForgetSession:
            return;
        }
    }

    void handleProfilesKey(int32_t key) {
        AccountProfileCommand command = accountFlow_.handleProfiles(screenNavigationKeyForAndroidKey(key), session_);
        switch (command.action) {
        case AccountProfileAction::None:
            return;
        case AccountProfileAction::Back:
            popScreen(Screen::Login);
            return;
        case AccountProfileAction::AddAccount:
            startAddAccount();
            return;
        case AccountProfileAction::SwitchSession:
            if (!command.session) return;
            clearCurrentSessionUi();
            session_ = std::move(*command.session);
            accountFlow_.activateSession(session_);
            resetNavigation(Screen::Home);
            homeState_.setRow(0);
            homeState_.setFirstVisibleRow(0);
            saveSession(session_);
            loadHomeAsync();
            return;
        case AccountProfileAction::ForgetSession:
            if (!command.removedSession) return;
            artwork_.eraseProfile(*command.removedSession, renderer_);
            if (command.removedCurrent) {
                clearCurrentSessionUi();
                resetNavigation(Screen::Profiles);
            }
            saveSession(session_);
            if (command.sessionsEmpty) startAddAccount();
            return;
        }
    }

    bool supportsItemContextMenu(const JellyfinItem& item) const {
        return item.type == "Movie" || item.type == "Series" || item.type == "Episode" || item.type == "BoxSet";
    }

    void openItemMenuForItem(const JellyfinItem& item) {
        if (item.id.empty() || !supportsItemContextMenu(item)) return;
        detailsFlow_.item() = item;
        pushScreen(Screen::ItemMenu);
        detailsFlow_.state().beginItemMenu();
        error_.clear();
        if (isSeerrItem(item)) return;

        const JellyfinSession session = session_;
        const std::string itemId = item.id;
        detailsAsync_.loadItemMenuDetail(session, itemId);
    }

    void beginHomeRowSlide(int fromFirst, int toFirst) {
        if (fromFirst == toFirst) return;
        const auto now = std::chrono::steady_clock::now();
        homeSlideFromFirst_ = fromFirst;
        homeSlideToFirst_ = toFirst;
        homeSlideStarted_ = now;
        renderBurstUntil_ = std::max(renderBurstUntil_, now + 240ms);
    }

    void handleHomeKey(int32_t key) {
        const HomeNavigationAction navigation = HomeNavigationController::handle(
            homeState_, screenNavigationKeyForAndroidKey(key), home_.rows, seerrDomain_.pendingRequests());
        switch (navigation.type) {
        case HomeNavigationActionType::None:
            return;
        case HomeNavigationActionType::FinishActivity:
            ANativeActivity_finish(app_->activity);
            return;
        case HomeNavigationActionType::OpenProfiles:
            openProfiles();
            return;
        case HomeNavigationActionType::OpenSearch:
            openSearch();
            return;
        case HomeNavigationActionType::OpenSettings:
            openSettings();
            return;
        case HomeNavigationActionType::OpenContext:
            if (navigation.item) openItemMenuForItem(*navigation.item);
            return;
        case HomeNavigationActionType::OpenLibrary:
            if (navigation.item) openLibrary(*navigation.item);
            return;
        case HomeNavigationActionType::OpenDetails:
            if (navigation.item) openDetails(*navigation.item);
            return;
        case HomeNavigationActionType::FinalizeNavigation:
            beginHomeRowSlide(navigation.previousFirstVisibleRow, navigation.currentFirstVisibleRow);
            if (navigation.prefetchRow >= 0) {
                uiPresentation_.prefetchHomeWindow(session_, home_, navigation.prefetchRow,
                                                   navigation.prefetchSelection);
                if (navigation.prefetchRow < static_cast<int>(home_.rows.size())) {
                    const auto& items = home_.rows[static_cast<size_t>(navigation.prefetchRow)].items;
                    if (navigation.prefetchSelection >= 0 &&
                        navigation.prefetchSelection < static_cast<int>(items.size())) {
                        scheduleSimilarPrefetch(items[static_cast<size_t>(navigation.prefetchSelection)]);
                    }
                }
            }
            return;
        }
    }

    void cancelContentLoadForNavigation() {
        if (!loading_) return;
        requestEpochs_.content.invalidate();
        loading_ = false;
    }

    void handleBrowseKey(int32_t key) {
        const ScreenNavigationKey navigationKey = screenNavigationKeyForAndroidKey(key);
        if (navigationKey == ScreenNavigationKey::Back) cancelContentLoadForNavigation();

        constexpr int columns = mediaGridColumns();
        const BrowseNavigationAction navigation =
            BrowseNavigationController::handle(browseState_, navigationKey, columns, loading_);
        switch (navigation.type) {
        case BrowseNavigationActionType::None:
            return;
        case BrowseNavigationActionType::ReloadPage:
            loadBrowsePageAsync(false);
            return;
        case BrowseNavigationActionType::LocalPage:
            loading_ = false;
            error_.clear();
            return;
        case BrowseNavigationActionType::Exit:
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) homeState_.focusToolbar(1);
            return;
        case BrowseNavigationActionType::ApplyFilter:
            applyBrowseFilter(navigation.filterSelection);
            return;
        case BrowseNavigationActionType::OpenContext:
            if (navigation.item) openItemMenuForItem(*navigation.item);
            return;
        case BrowseNavigationActionType::OpenContainer:
            if (navigation.item) openBrowseContainer(*navigation.item, true);
            return;
        case BrowseNavigationActionType::OpenDetails:
            if (navigation.item) openDetails(*navigation.item);
            return;
        case BrowseNavigationActionType::SelectionChanged: {
            uiPresentation_.prefetchBrowseArtworkAhead(session_, browseState_);
            const auto& items = browseState_.items();
            if (browseState_.selection() >= 0 && browseState_.selection() < static_cast<int>(items.size())) {
                scheduleSimilarPrefetch(items[static_cast<size_t>(browseState_.selection())]);
            }
            if (navigation.loadMore) loadMoreBrowseAsync();
            return;
        }
        }
    }

    void handleSearchKey(int32_t key) {
        constexpr int columns = mediaGridColumns();
        const SearchNavigationAction navigation =
            searchFlow_.handleNavigation(screenNavigationKeyForAndroidKey(key), columns);
        if (const JellyfinItem* selected = searchFlow_.selectedResult()) scheduleSimilarPrefetch(*selected);
        switch (navigation.type) {
        case SearchNavigationActionType::None:
            return;
        case SearchNavigationActionType::Exit:
            hideSystemTextInput();
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) {
                homeState_.setRow(0);
                homeState_.updateViewport(static_cast<int>(home_.rows.size()));
            }
            return;
        case SearchNavigationActionType::SubmitSearch:
            searchAsync();
            return;
        case SearchNavigationActionType::MoveKeyboard:
            virtualKeyboard_.move(navigation.dx, navigation.dy);
            return;
        case SearchNavigationActionType::ActivateKeyboard:
            activateKeyboardKey(true);
            return;
        case SearchNavigationActionType::OpenTextInput:
            searchFlow_.state().setKeyboard(
                !showSystemTextInput(searchFlow_.state().query(), "Search Jellyfin & Seerr", kTextInputSearch));
            if (searchFlow_.state().keyboard()) virtualKeyboard_.reset();
            return;
        case SearchNavigationActionType::OpenContext:
            if (navigation.item) openItemMenuForItem(*navigation.item);
            return;
        case SearchNavigationActionType::OpenDetails:
            if (navigation.item) openDetails(*navigation.item);
            return;
        case SearchNavigationActionType::RequestSeerr:
            if (navigation.seerrItem) requestSeerrMediaAsync(*navigation.seerrItem);
            return;
        }
    }

    std::vector<std::string> detailActions() const {
        return detailsFlow_.detailActions(playbackCoordinator_.continuation().stillWatchingPrompt());
    }

    void applySettingsActionEffects(const SettingsActionEffects& effects) {
        if (hasSettingEffect(effects.settingEffects, SettingChangeEffect::ApplyVideoZoom))
            playbackCoordinator_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
        if (hasSettingEffect(effects.settingEffects, SettingChangeEffect::RestoreDisplayMode)) displayMode_.restore();
        if (hasSettingEffect(effects.settingEffects, SettingChangeEffect::ResetScreensaver)) {
            lastInteraction_ = std::chrono::steady_clock::now();
            screensaverActive_ = false;
        }
        if (effects.saveSession || hasSettingEffect(effects.settingEffects, SettingChangeEffect::Save))
            saveSession(session_);
        if (effects.refreshSeerrStorage) refreshSeerrStorageAsync(true);

        switch (effects.hostAction) {
        case SettingsHostAction::None:
            return;
        case SettingsHostAction::Exit:
            hideSystemTextInput();
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) homeState_.focusToolbar(3);
            return;
        case SettingsHostAction::EditSearch:
            showSystemTextInput(settingsFlow_.state().searchQuery(), "Search settings", kTextInputSettingsSearch);
            return;
        case SettingsHostAction::OpenDiagnostics:
            openDiagnostics();
            return;
        case SettingsHostAction::SwitchUser:
            openProfiles();
            return;
        case SettingsHostAction::EditSeerrServer:
            showSystemTextInput(settings_.seerrServer, "Seerr server URL", kTextInputSeerrServer);
            return;
        case SettingsHostAction::ConnectSeerr:
            connectSeerrAsync();
            return;
        case SettingsHostAction::EditSeerrApiKey:
            showSystemTextInput(settings_.seerrApiKey, "Seerr API key", kTextInputSeerrApiKey, true);
            return;
        }
    }

    void handleSettingsKey(int32_t key) {
        applySettingsActionEffects(settingsFlow_.handle(screenNavigationKeyForAndroidKey(key)));
    }

    void handleDiagnosticsKey(int32_t key) {
        DiagnosticsScreenInput input = DiagnosticsScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = DiagnosticsScreenInput::Back;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = DiagnosticsScreenInput::Activate;

        if (handleDiagnosticsScreenInput(input).type != DiagnosticsScreenCommandType::Exit) return;

        // Diagnostics owns the current content request. Cancel it before
        // returning so an in-flight server-info response cannot leave the
        // app's global loading state stuck after this screen is gone.
        requestEpochs_.content.invalidate();
        loading_ = false;
        popScreen(Screen::Settings);
    }

    void applyDetailsFlowCommand(DetailsFlowCommand command) {
        if (command.cancelContentLoad) cancelContentLoadForNavigation();
        if (command.resetContinuationPrompt) playbackCoordinator_.resetContinuationPrompt();
        if (command.closeItemMenu) {
            popScreen(Screen::Details);
            if ((command.action == DetailsFlowAction::BeginSeriesPlayAll ||
                 command.action == DetailsFlowAction::PlayExternal) &&
                screen_ != Screen::Details) {
                pushScreen(Screen::Details);
            }
        }

        switch (command.action) {
        case DetailsFlowAction::None:
            return;
        case DetailsFlowAction::ExitDetails:
            popScreen(Screen::Home);
            return;
        case DetailsFlowAction::OpenItemMenu:
            openItemMenu();
            return;
        case DetailsFlowAction::OpenDetails:
            if (command.item) openDetails(*command.item, command.replaceCurrentDetails);
            return;
        case DetailsFlowAction::OpenEpisodes:
            if (command.item) openEpisodes(*command.item);
            return;
        case DetailsFlowAction::BeginPlayback:
            beginPlayback();
            return;
        case DetailsFlowAction::OpenSeasons:
            openSeasons();
            return;
        case DetailsFlowAction::BeginSeriesPlayAll:
            beginSeriesPlayAll();
            return;
        case DetailsFlowAction::ToggleFavorite:
            toggleFavoriteAsync();
            return;
        case DetailsFlowAction::TogglePlayed:
            togglePlayedAsync();
            return;
        case DetailsFlowAction::OpenCast:
            openCast();
            return;
        case DetailsFlowAction::BackToDetails:
            popScreen(Screen::Details);
            return;
        case DetailsFlowAction::OpenPersonItems:
            if (command.person) openPersonItems(*command.person);
            return;
        case DetailsFlowAction::BackToCast:
            popScreen(Screen::Cast);
            return;
        case DetailsFlowAction::OpenItemContext:
            if (command.item) openItemMenuForItem(*command.item);
            return;
        case DetailsFlowAction::ConfirmDeleteMedia:
            deleteCurrentItemAsync();
            return;
        case DetailsFlowAction::ConfirmDeleteRequest:
            deleteSeerrRequestAsync();
            return;
        case DetailsFlowAction::PlayExternal:
            launchExternalPlaybackAsync();
            return;
        case DetailsFlowAction::ViewQueue:
            openQueueOverlay();
            return;
        case DetailsFlowAction::ToggleHomeVisibility:
            toggleHiddenFromHome();
            return;
        case DetailsFlowAction::RefreshMetadata:
            refreshCurrentItemMetadataAsync();
            return;
        case DetailsFlowAction::BackToSeasons:
            popScreen(Screen::Seasons);
            return;
        }
    }

    void handleDetailsKey(int32_t key) {
        const DetailsFlowCommand command = detailsFlow_.handleDetails(detailsNavigationKeyForAndroidKey(key));
        if (detailsFlow_.state().similarFocused()) {
            if (const auto* selected = detailsFlow_.state().selectedSimilar()) scheduleSimilarPrefetch(*selected);
        }
        applyDetailsFlowCommand(command);
    }

    void openCast() {
        if (!detailsFlow_.beginCast()) return;
        pushScreen(Screen::Cast);
        error_.clear();
    }

    void handleCastKey(int32_t key) {
        applyDetailsFlowCommand(detailsFlow_.handleCast(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
    }

    void openPersonItems(const JellyfinPerson& person) {
        if (!session_.valid() || !detailsFlow_.beginPerson(person)) return;
        pushScreen(Screen::PersonItems);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string personId = person.id;
        const uint64_t generation = requestEpochs_.content.begin();
        detailsAsync_.loadPersonItems(session, personId, generation, 60);
    }

    void handlePersonItemsKey(int32_t key) {
        applyDetailsFlowCommand(
            detailsFlow_.handlePersonItems(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
    }

    std::vector<std::string> itemMenuActions() const {
        return detailsFlow_.itemMenuActions(isSeerrItem(detailsFlow_.item()),
                                            settingsFlow_.selectedExternalPlayer().has_value(), !queueState_.empty(),
                                            homeVisibility_.isHidden(detailsFlow_.item()));
    }

    void openItemMenu() {
        if (!detailsFlow_.beginItemMenu()) return;
        pushScreen(Screen::ItemMenu);
        error_.clear();
    }

    void handleItemMenuKey(int32_t key) {
        applyDetailsFlowCommand(
            detailsFlow_.handleItemMenu(detailsNavigationKeyForAndroidKey(key), isSeerrItem(detailsFlow_.item()),
                                        settingsFlow_.selectedExternalPlayer().has_value(), !queueState_.empty()));
    }

    void launchExternalPlaybackAsync() {
        if (loading_ || !session_.valid() || detailsFlow_.item().id.empty()) return;
        const auto player = settingsFlow_.selectedExternalPlayer();
        if (!player) {
            error_ = "EXTERNAL PLAYER IS NOT CONFIGURED";
            return;
        }

        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const PlaybackLanguagePreferences preferences = playbackCoordinator_.languagePreferences();
        const PlaybackTrackSelectionPolicy trackPolicy = playbackRuntime_.trackSelectionPolicy();
        const uint64_t generation = requestEpochs_.playback.begin();
        if (!externalPlaybackAsync_.prepare(session, ExternalPlaybackRequest{
                                                         .generation = generation,
                                                         .selectedItemId = detailsFlow_.item().id,
                                                         .selectedItemType = detailsFlow_.item().type,
                                                         .seriesId = detailsFlow_.item().seriesId,
                                                         .container = detailsFlow_.item().container,
                                                         .mediaSourceId = detailsFlow_.item().mediaSourceId,
                                                         .audios = detailsFlow_.item().audios,
                                                         .subtitles = detailsFlow_.item().subtitles,
                                                         .player = *player,
                                                         .subtitlePreference = preferences.subtitle,
                                                         .trackPolicy = trackPolicy,
                                                     })) {
            loading_ = false;
            error_ = "EXTERNAL PLAYER COULD NOT BE STARTED";
        }
    }

    void handleSeasonsKey(int32_t key) {
        applyDetailsFlowCommand(detailsFlow_.handleSeasons(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
    }

    void handleEpisodesKey(int32_t key) {
        applyDetailsFlowCommand(
            detailsFlow_.handleEpisodes(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
    }

    void loadSubtitleAsync(const JellyfinSubtitleStream& subtitle, const std::string& deliveryUrl = {}) {
        if (!playbackRuntime_.loadSubtitle(subtitleLoadAsync_, subtitle, deliveryUrl))
            showNotice("SUBTITLES COULD NOT BE STARTED");
    }

    void clearTrickplayPreview() {
        if (trickplayState_.texture() != 0 && trickplayState_.textureGeneration() == renderer_.generation()) {
            renderer_.deleteTexture(trickplayState_.texture());
        }
        trickplayState_.reset();
    }

    void requestTrickplayPreview(int positionMs) {
        const auto& info = playbackCoordinator_.session().activeItem().trickplay;
        if (!session_.valid() || playbackCoordinator_.session().activeItem().id.empty() || !info.valid()) return;
        const TrickplayFrame frame = trickplayFrameForPosition(positionMs, info.intervalMs, info.thumbnailCount,
                                                               info.tileWidth, info.tileHeight);
        if (!frame.valid()) return;

        trickplayState_.showAt(positionMs, std::chrono::steady_clock::now());
        if (trickplayState_.matchesTile(playbackCoordinator_.session().activeItem().id, frame.tileIndex) &&
            !trickplayState_.failed())
            return;

        if (trickplayState_.texture() != 0 && trickplayState_.textureGeneration() == renderer_.generation()) {
            renderer_.deleteTexture(trickplayState_.texture());
        }
        trickplayState_.beginTile(playbackCoordinator_.session().activeItem().id, frame.tileIndex);
        const JellyfinSession session = session_;
        if (!trickplayTileAsync_.load(session, TrickplayTileRequest{
                                                   .itemId = playbackCoordinator_.session().activeItem().id,
                                                   .trickplay = info,
                                                   .tileIndex = frame.tileIndex,
                                               })) {
            trickplayState_.markFailed();
        }
    }

    void handleSeerrDrivePickerKey(int32_t key) {
        SeerrDriveNavigationAction navigation =
            SeerrDriveNavigationController::handle(seerrDomain_.storage(), screenNavigationKeyForAndroidKey(key));
        if (navigation.type == SeerrDriveNavigationActionType::Back) {
            popScreen(Screen::Search);
            return;
        }
        if (navigation.type != SeerrDriveNavigationActionType::Selected || !navigation.selection) return;

        auto selected = std::move(*navigation.selection);
        __android_log_print(ANDROID_LOG_INFO, kTag, "Seerr storage selected media=%s server=%d path=%s",
                            selected.item.mediaType.c_str(), selected.target.serverId, selected.target.path.c_str());
        popScreen(Screen::Search);
        requestSeerrMediaAsync(selected.item, &selected.target, true);
    }

    void applyPlaybackHostEffect(PlaybackHostEffect effect) {
        switch (effect.type) {
        case PlaybackHostEffectType::None:
            return;
        case PlaybackHostEffectType::StopPlayback:
            stopPlayback(effect.completed);
            if (effect.resetAutoplayChain) playbackCoordinator_.resetAutoplayChain();
            return;
        case PlaybackHostEffectType::OpenQueue:
            openQueueOverlay();
            return;
        case PlaybackHostEffectType::PlayAdjacentEpisode:
            playAdjacentEpisode(effect.episodeDirection);
            return;
        case PlaybackHostEffectType::SeekWithTrickplay:
            requestTrickplayPreview(effect.positionMs);
            playbackRuntime_.seekTo(effect.positionMs);
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
            return;
        case PlaybackHostEffectType::PlayQueueIndex:
            playQueuedIndexAsync(effect.queueIndex, effect.restartCurrent, effect.replacingCompleted);
            return;
        case PlaybackHostEffectType::QueueAutoplayNext:
            if (effect.item) queueAutoplayNext(*effect.item);
            return;
        case PlaybackHostEffectType::ShowStillWatching:
            if (effect.item) showStillWatching(*effect.item);
            return;
        case PlaybackHostEffectType::SubtitleStartFailed:
            showNotice("SUBTITLES COULD NOT BE STARTED");
            return;
        }
    }

    void handlePlayerKey(int32_t key, int repeatCount = 0) {
        applyPlaybackHostEffect(playbackRuntime_.handlePlayerInput(playerScreenInputForAndroidKey(key), repeatCount,
                                                                   subtitleLoadAsync_, playbackStreamAsync_,
                                                                   playbackTelemetryAsync_, screen_ == Screen::Player));
    }

    void handleMediaSessionCommand(const MediaSessionCommand& command) {
        if (screen_ != Screen::Player) return;
        applyPlaybackHostEffect(
            playbackRuntime_.handleMediaSessionCommand(command, queueState_, playbackTelemetryAsync_));
    }

    void showNotice(std::string message, std::chrono::seconds duration = 6s, bool persistent = false) {
        const auto now = std::chrono::steady_clock::now();
        statusOverlayState_.showNotice(std::move(message), duration, persistent, now);
        renderBurstUntil_ = std::max(renderBurstUntil_, now + 350ms);
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void requestServerInfoNoticeAsync() {
        if (!session_.valid() || serverInfoLoading_ || !serverInfo_.version.empty()) return;
        const JellyfinSession session = session_;
        serverInfoLoading_ = true;
        serverInfoAsync_.loadNotice(session);
    }

    void openSettings() {
        settingsFlow_.refreshExternalPlayers(externalPlayer_.availablePlayers());
        pushScreen(Screen::Settings);
        settingsFlow_.reset();
        error_.clear();
    }

    void openDiagnostics() {
        if (!session_.valid()) return;
        pushScreen(Screen::Diagnostics);
        loading_ = true;
        error_.clear();
        serverInfo_ = {};
        const JellyfinSession session = session_;
        const uint64_t generation = requestEpochs_.content.begin();
        serverInfoAsync_.loadDiagnostics(session, generation);
    }

    void clearCurrentSessionUi() {
        const bool hadAuthenticatedSession = session_.valid();
        api_.cancelPendingRequests();
        requestEpochs_.invalidateAll();
        if (screen_ == Screen::Player || player_.status() != PlayerStatus::Idle ||
            playbackCoordinator_.activeItemAvailable()) {
            releaseActivePlayback(true);
        }

        queueState_.reset();
        playbackCoordinator_.resetSession();
        externalPlaybackState_.reset();
        playerScreenState_.resetSession();
        clearTrickplayPreview();

        if (hadAuthenticatedSession) {
            pendingDeepLinkItemId_.clear();
            pendingSearchQuery_.clear();
            pendingRuntimeLaunchRequest_.reset();
        }
        session_ = {};
        settings_.seerrSessionCookie.clear();
        seerrDomain_.resetStorageForSessionClear();
        serverInfo_ = {};
        serverInfoLoading_ = false;
        home_ = {};
        homeState_.reset();
        browseState_.clear();
        searchFlow_.reset();
        detailsFlow_.item() = {};
        detailsFlow_.state().reset();

        artwork_.clearSession(renderer_);
        accountFlow_.state().clearSessionUi();
        loading_ = false;
        homeLoading_ = false;
        homeRetryAt_ = {};
        homeRetryAttempt_ = 0;
        contentMutationFlow_.reset();
        error_.clear();
        statusOverlayState_.clearNotice();
        screensaverActive_ = false;
        lastInteraction_ = std::chrono::steady_clock::now();
    }

    void openProfiles() {
        if (!accountFlow_.beginProfiles()) {
            startAddAccount();
            return;
        }
        pushScreen(Screen::Profiles);
        error_.clear();
    }

    void startAddAccount() {
        const std::string existingServer = session_.server;
        clearCurrentSessionUi();
        accountFlow_.beginAddAccount(existingServer);
        resetNavigation(Screen::Login);
        saveSession(session_);
    }

    static constexpr int kBrowsePageSize = 60;

    void openLibrary(const JellyfinItem& library) {
        if (loading_ || library.id.empty()) return;
        browseState_.resetForLibrary(library);
        pushScreen(Screen::Browse);
        loadBrowsePageAsync(false);
    }

    void applyBrowseFilter(int selection) {
        if (loading_) return;
        if (browseState_.applyFilter(selection)) {
            loadBrowsePageAsync(false);
        } else {
            loading_ = false;
            error_.clear();
        }
    }

    void openBrowseContainer(const JellyfinItem& container, bool pushCurrent) {
        if (loading_ || container.id.empty()) return;
        browseState_.openContainer(container, pushCurrent);
        pushScreen(Screen::Browse);
        loadBrowsePageAsync(false);
    }

    void loadMoreBrowseAsync() {
        if (loading_ || !browseState_.hasMore() || browseState_.activeContainer().id.empty()) return;
        loadBrowsePageAsync(true);
    }

    void loadBrowsePageAsync(bool append) {
        if (loading_ || browseState_.activeContainer().id.empty()) return;
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem container = browseState_.activeContainer();
        const int startIndex = append ? browseState_.nextIndex() : 0;
        const uint64_t generation = requestEpochs_.content.begin();
        const BrowseContentMode mode = browseState_.mode();
        const std::string genre = browseState_.genre();
        const std::string letter = browseState_.letter();
        const bool nested = browseState_.nested();
        browseAsync_.load(BrowsePageRequest{
            .session = session,
            .container = container,
            .startIndex = startIndex,
            .append = append,
            .generation = generation,
            .mode = mode,
            .genre = genre,
            .letter = letter,
            .nested = nested,
            .pageSize = kBrowsePageSize,
        });
    }

    void openSeasons() {
        if (loading_ || !detailsFlow_.beginSeries()) return;
        pushScreen(Screen::Seasons);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string seriesId = detailsFlow_.state().seriesDetail().id;
        const uint64_t generation = requestEpochs_.content.begin();
        detailsAsync_.loadSeasons(session, seriesId, generation);
    }

    void openEpisodes(const JellyfinItem& season) {
        if (loading_ || !detailsFlow_.beginSeason(season)) return;
        pushScreen(Screen::Episodes);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string seriesId = detailsFlow_.state().seriesDetail().id;
        const std::string seasonId = season.id;
        const uint64_t generation = requestEpochs_.content.begin();
        detailsAsync_.loadEpisodes(session, seriesId, seasonId, generation);
    }

    void toggleHiddenFromHome() {
        const auto hiding = homeVisibility_.toggle(detailsFlow_.item(), home_, homeState_);
        if (!hiding) return;
        saveSession(session_);
        showNotice(*hiding ? "HIDDEN FROM HOME" : "HOME VISIBILITY RESTORED", 2s);
    }

    void restoreHomeVisibilityForPlayback(const JellyfinItem& item) {
        if (homeVisibility_.restoreForPlayback(item)) saveSession(session_);
    }

    void toggleFavoriteAsync() {
        if (loading_ || contentMutationFlow_.loading() || detailsFlow_.item().id.empty()) return;
        const bool desired = !detailsFlow_.item().favorite;
        const JellyfinSession session = session_;
        const JellyfinItem item = detailsFlow_.item();
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        contentMutationFlow_.begin();
        error_.clear();
        itemMutationAsync_.setFavorite(session, item, desired, sessionEpoch);
    }

    void togglePlayedAsync() {
        if (loading_ || contentMutationFlow_.loading() || detailsFlow_.item().id.empty()) return;
        const JellyfinSession session = session_;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        const bool hiddenFromHome = homeVisibility_.isHidden(detailsFlow_.item());
        PlayedMutationPreparation preparation = contentMutationFlow_.preparePlayedToggle(
            home_, homeState_, browseState_, searchFlow_.state(), detailsFlow_.state(), queueState_,
            detailsFlow_.item(), hiddenFromHome, sessionEpoch);
        contentMutationFlow_.begin();
        error_.clear();
        itemMutationAsync_.setPlayed(session, std::move(preparation.item), preparation.desired, sessionEpoch,
                                     preparation.nextUpReplacementIndex);
    }

    void refreshCurrentItemMetadataAsync() {
        if (loading_ || contentMutationFlow_.loading() || detailsFlow_.item().id.empty()) return;
        const JellyfinSession session = session_;
        const std::string itemId = detailsFlow_.item().id;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        contentMutationFlow_.begin();
        error_.clear();
        itemMutationAsync_.refreshMetadata(session, itemId, sessionEpoch);
    }

    void deleteSeerrRequestAsync() {
        const auto deleteRequest = seerrDeleteRequestFromJellyfinItem(detailsFlow_.item());
        if (loading_ || contentMutationFlow_.loading() || !deleteRequest) {
            if (isSeerrItem(detailsFlow_.item()) && detailsFlow_.item().externalRequestId <= 0) {
                error_ = "SEERR REQUEST ID IS NOT AVAILABLE YET";
                detailsFlow_.state().setDeleteConfirmation(false);
            }
            return;
        }
        const SeerrEndpoint endpoint = seerrEndpoint();
        if (!endpoint.configured()) {
            error_ = "SEERR IS NOT CONNECTED";
            detailsFlow_.state().setDeleteConfirmation(false);
            return;
        }
        const SeerrDeleteRequest request = *deleteRequest;
        contentMutationFlow_.begin();
        error_.clear();
        seerrAsync_.deleteRequest(endpoint, request);
    }

    void deleteCurrentItemAsync() {
        if (loading_ || contentMutationFlow_.loading() || detailsFlow_.item().id.empty() ||
            !detailsFlow_.item().canDelete)
            return;
        const JellyfinSession session = session_;
        const std::string itemId = detailsFlow_.item().id;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        contentMutationFlow_.begin();
        error_.clear();
        itemMutationAsync_.deleteItem(session, itemId, sessionEpoch);
    }

    void openSearch() {
        pushScreen(Screen::Search);
        searchFlow_.state().setKeyboard(
            !showSystemTextInput(searchFlow_.state().query(), "Search Jellyfin & Seerr", kTextInputSearch));
        virtualKeyboard_.reset();
        error_.clear();
    }

    void discoverServersAsync() {
        if (!accountFlow_.beginDiscovery(loading_)) return;
        loading_ = true;
        error_.clear();
        accountAsync_.discover(requestEpochs_.auth.begin(), 1600);
    }

    void loginAsync() {
        auto fields = accountFlow_.beginLogin(loading_);
        if (!fields) return;
        loading_ = true;
        error_.clear();
        accountAsync_.login(std::move(*fields), accountFlow_.deviceId(), requestEpochs_.auth.begin());
    }

    void quickConnectAsync() {
        AccountQuickConnectPlan plan = accountFlow_.beginQuickConnect(loading_);
        if (plan.error) {
            error_ = std::move(*plan.error);
            return;
        }
        if (!plan.ready()) return;

        loading_ = true;
        error_.clear();
        quickConnectAsync_.connect(std::move(plan.server), accountFlow_.deviceId(), requestEpochs_.auth.beginToken());
    }

    void loadHomeAsync() {
        const JellyfinSession session = session_;
        if (!session.valid()) return;
        requestServerInfoNoticeAsync();

        HomeSelectionSnapshot homeSnapshot;
        {
            std::scoped_lock lock(stateMutex_);
            homeSnapshot = homeState_.snapshot(home_.rows);
            homeLoading_ = true;
            homeRetryAt_ = {};
        }

        const uint64_t generation = requestEpochs_.home.begin();
        const auto homeLoadStarted = std::chrono::steady_clock::now();
        homeAsync_.loadCore(session, generation, std::move(homeSnapshot), homeLoadStarted);
    }

    [[nodiscard]] SeerrEndpoint seerrEndpoint() const {
        return SeerrEndpoint{
            .server = settings_.seerrServer,
            .auth =
                {
                    .sessionCookie = settings_.seerrSessionCookie,
                    .apiKey = settings_.seerrApiKey,
                },
        };
    }

    [[nodiscard]] SeerrAuth seerrAuth() const { return seerrEndpoint().auth; }

    void connectSeerrAsync(bool announce = true) {
        auto plan = seerrConnection_.prepare(settings_.seerrServer, session_);
        switch (plan.action) {
        case SeerrDomainState::ConnectAction::AlreadyConnecting:
            return;
        case SeerrDomainState::ConnectAction::MissingServer:
            if (announce) showNotice("SET THE SEERR SERVER FIRST", 4s);
            return;
        case SeerrDomainState::ConnectAction::MissingJellyfin:
            if (announce) showNotice("JELLYFIN LOGIN REQUIRED", 4s);
            return;
        case SeerrDomainState::ConnectAction::Submit:
            break;
        }
        if (announce) {
            error_.clear();
            showNotice("CONNECTING SEERR WITH JELLYFIN…", 30s);
        }
        seerrConnection_.submit(std::move(plan), announce);
    }

    void refreshSeerrStorageAsync(bool force = false) {
        seerrRefresh_.refreshStorage(seerrEndpoint(), force, std::chrono::steady_clock::now());
    }

    void openSeerrDrivePicker(const SeerrMediaItem& item) {
        const auto status = seerrDomain_.prepareStoragePicker(item);
        if (status == SeerrStorageState::PickerStatus::Loading) {
            showNotice("LOADING SEERR STORAGE…", 4s);
            return;
        }
        if (status == SeerrStorageState::PickerStatus::Unavailable) {
            showNotice(seerrDomain_.storageError().empty() ? "NO SEERR STORAGE TARGETS ARE AVAILABLE"
                                                           : "SEERR STORAGE: " + seerrDomain_.storageError(),
                       5s);
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Opening Seerr storage picker media=%s choices=%zu",
                            item.mediaType.c_str(), seerrDomain_.storageDriveChoices().size());
        if (screen_ != Screen::SeerrDrivePicker) pushScreen(Screen::SeerrDrivePicker);
    }

    void syncSeerrHomeRowLocked() {
        const HomeSelectionSnapshot snapshot = homeState_.snapshot(home_.rows);
        projectSeerrHomeRow(home_.rows, seerrDomain_.pendingRequests());
        HomeRestorePlan restore = HomeScreenState::restorePlan(snapshot, home_.rows);
        homeState_.setSelections(std::move(restore.selections));
        homeState_.setRow(restore.focusedRow);
        homeState_.updateViewport(static_cast<int>(home_.rows.size()));
    }

    void refreshSeerrPendingAsync() {
        const auto action = seerrRefresh_.refreshPending(seerrEndpoint());
        if (action == SeerrDomainState::RefreshStartAction::Reset) syncSeerrHomeRowLocked();
    }

    void requestSeerrMediaAsync(const SeerrMediaItem& item, const SeerrStorageTarget* selectedTarget = nullptr,
                                bool skipDrivePrompt = false) {
        auto plan =
            seerrRequest_.prepare(item, seerrEndpoint(), settings_.seerrSelectDrive, skipDrivePrompt, selectedTarget);
        switch (plan.action) {
        case SeerrDomainState::RequestAction::Invalid:
            return;
        case SeerrDomainState::RequestAction::AlreadyRequested:
            showNotice(item.status.empty() ? "ALREADY REQUESTED IN SEERR" : item.status, 5s);
            return;
        case SeerrDomainState::RequestAction::NotConfigured:
            showNotice("CONNECT SEERR IN SETTINGS FIRST", 5s);
            return;
        case SeerrDomainState::RequestAction::DeferredForConnection:
            showNotice("REFRESHING SEERR SESSION…", 4s);
            return;
        case SeerrDomainState::RequestAction::ChooseStorage:
            // A deliberate request should not be blocked by a cached empty result.
            // Force discovery when there are no known targets and let the pending
            // item automatically open the picker when discovery completes.
            refreshSeerrStorageAsync(plan.refreshStorage);
            openSeerrDrivePicker(item);
            return;
        case SeerrDomainState::RequestAction::Submit:
            break;
        }

        contentMutationFlow_.begin();
        seerrRequest_.submit(std::move(plan));
    }

    void applySearchDispatchEffects(const SearchDispatchEffects& effects) {
        if (effects.clearError) error_.clear();
        if (effects.refreshSeerrStorage) refreshSeerrStorageAsync();
    }

    void searchAsync(bool includeSeerrImmediately = true) {
        applySearchDispatchEffects(
            searchFlow_.search(session_, seerrEndpoint(), includeSeerrImmediately, std::chrono::steady_clock::now()));
    }

    void scheduleSimilarPrefetch(const JellyfinItem& item) {
        if (similarPrefetch_.schedule(session_, item) && app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void runDueSimilarPrefetch() {
        const bool eligible = screen_ == Screen::Home || screen_ == Screen::Browse || screen_ == Screen::Search ||
                              screen_ == Screen::Details;
        auto work = similarPrefetch_.takeDue(session_, eligible);
        if (!work) return;

        SimilarPrefetchWork request = std::move(*work);
        const std::string key = request.key;
        if (!similarPrefetchTasks_.submit([this, request = std::move(request)]() mutable {
                auto result = similarPrefetchApi_.getSimilar(request.session, request.itemId, 18);
                asyncCompletions_.push(SimilarPrefetchCompletion{
                    .session = std::move(request.session),
                    .itemId = std::move(request.itemId),
                    .key = std::move(request.key),
                    .result = std::move(result),
                });
            })) {
            similarPrefetch_.submissionFailed(key);
        }
    }

    void openDetails(const JellyfinItem& item, bool replaceCurrent = false) {
        if (replaceCurrent)
            replaceScreen(Screen::Details);
        else
            pushScreen(Screen::Details);
        playbackCoordinator_.dismissStillWatchingPrompt();
        detailsFlow_.beginDetails(item);
        const JellyfinSession session = session_;
        const std::string id = item.id;
        const auto prefetchedSimilar = similarPrefetch_.cached(session, id);
        if (prefetchedSimilar) detailsFlow_.state().setSimilar(*prefetchedSimilar);
        similarPrefetch_.clearPending();
        loading_ = true;
        error_.clear();
        const RequestEpoch::Token requestToken = requestEpochs_.content.beginToken();
        detailsAsync_.load(session, id, requestToken, !prefetchedSimilar.has_value());
    }

    void shuffleRemainingQueue() { playbackRequestFlow_.shuffleRemaining(); }

    void openQueueOverlay() { playbackRequestFlow_.openQueue(); }

    void playQueuedIndexAsync(int index, bool restartCurrent = false, bool replacingCompleted = false) {
        playbackRequestFlow_.playQueued(
            index, restartCurrent, replacingCompleted,
            [this](bool reportStop, bool completed) { releaseActivePlayback(reportStop, completed); }, playbackRuntime_,
            playbackResolutionAsync_);
    }

    void playPlayerItemAsync(JellyfinItem selected) {
        playbackRequestFlow_.playPlayerItem(
            std::move(selected),
            [this](bool reportStop, bool completed) { releaseActivePlayback(reportStop, completed); }, playbackRuntime_,
            playbackResolutionAsync_);
    }

    void playAdjacentEpisode(int direction) {
        playbackRequestFlow_.playAdjacent(
            direction, [this](bool reportStop, bool completed) { releaseActivePlayback(reportStop, completed); },
            playbackRuntime_, playbackResolutionAsync_, playbackContinuationAsync_);
    }

    void handleQueueOverlayKey(int32_t key) {
        playbackRequestFlow_.handleQueue(
            screenNavigationKeyForAndroidKey(key),
            [this](bool reportStop, bool completed) { releaseActivePlayback(reportStop, completed); }, playbackRuntime_,
            playbackResolutionAsync_);
    }

    void beginSeriesPlayAll() { playbackRequestFlow_.beginSeriesPlayAll(seriesPlaybackAsync_); }

    void beginPlayback() {
        if (loading_ || detailsFlow_.item().id.empty()) return;
        if (settingsFlow_.selectedExternalPlayer()) {
            launchExternalPlaybackAsync();
            return;
        }
        playbackRequestFlow_.beginPlayback(playbackRuntime_, playbackResolutionAsync_);
    }

    void releaseActivePlayback(bool reportStop, bool completed = false) {
        playbackReleaseFlow_.release(reportStop, completed, playbackRuntime_);
    }

    void queueAutoplayNext(JellyfinItem nextItem) {
        const int queuedNextIndex = queueState_.autoplayAdvanceIndex(nextItem);
        releaseActivePlayback(true, true);
        playbackCoordinator_.beginAutoplayResolution();
        loading_ = true;
        detailsFlow_.item() = nextItem;
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        const JellyfinSession session = session_;
        PlaybackResolutionOptions resolutionOptions = playbackRuntime_.resolutionOptions();
        const uint64_t generation = requestEpochs_.playback.begin();
        playbackResolutionAsync_.resolveAutoplay(session, std::move(nextItem), std::move(resolutionOptions), generation,
                                                 queuedNextIndex);
    }

    void showStillWatching(JellyfinItem nextItem) {
        releaseActivePlayback(true, true);
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        detailsFlow_.item() = std::move(nextItem);
        detailsFlow_.state().beginDetails();
        popScreen(Screen::Details);
        playbackCoordinator_.showStillWatchingPrompt();
        error_.clear();
    }

    void applyRuntimeLaunchRequest(const LaunchRequest& request) {
        std::scoped_lock lock(stateMutex_);
        if (!session_.valid()) {
            if (!request.itemId.empty()) pendingDeepLinkItemId_ = request.itemId;
            if (!request.searchQuery.empty()) pendingSearchQuery_ = request.searchQuery;
            return;
        }

        api_.cancelPendingRequests();
        requestEpochs_.invalidateTransient();
        loading_ = false;
        homeLoading_ = false;
        accountFlow_.state().endQuickConnect();
        hideSystemTextInput();
        if (screen_ == Screen::Player || player_.status() != PlayerStatus::Idle) {
            releaseActivePlayback(true);
            playbackCoordinator_.finishStop();
        }
        resetNavigation(Screen::Home);
        queueState_.closeOverlay();
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        error_.clear();

        if (!request.itemId.empty()) {
            JellyfinItem linked;
            linked.id = request.itemId;
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening runtime ACTION_VIEW Jellyfin item %s",
                                linked.id.c_str());
            openDetails(linked);
        } else if (!request.searchQuery.empty()) {
            searchFlow_.state().setQuery(request.searchQuery);
            searchFlow_.state().setKeyboard(false);
            pushScreen(Screen::Search);
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening runtime ACTION_SEARCH query");
            searchAsync();
        }
    }

    void applyPendingRuntimeLaunchRequest() {
        std::optional<LaunchRequest> request;
        {
            std::scoped_lock lock(stateMutex_);
            if (pendingRuntimeLaunchRequest_) {
                request = std::move(pendingRuntimeLaunchRequest_);
                pendingRuntimeLaunchRequest_.reset();
            }
        }
        if (request) applyRuntimeLaunchRequest(*request);
    }

    PendingTickWork collectPendingTickWork() {
        PendingTickWork work;
        work.externalResult = externalPlayer_.takeResult();
        std::scoped_lock lock(stateMutex_);
        if (work.externalResult && externalPlaybackState_.hasActive()) {
            work.completedExternalPlayback = externalPlaybackState_.takeActive();
        }
        if (externalPlaybackState_.hasPending()) {
            work.externalLaunch = externalPlaybackState_.takePending();
        }
        if (!app_->window) return work;

        work.playbackTransition = playbackCoordinator_.takePendingTransition();
        if (!work.playbackTransition) return work;
        auto& transition = *work.playbackTransition;
        auto& target = transition.target;
        auto& item = transition.item;
        const bool streamRestart = transition.streamRestart;
        const PlaybackTransitionPlan playbackPlan = playbackCoordinator_.activateTransition(
            item, target, streamRestart, transition.restartPaused, transition.audioStreamIndex,
            static_cast<VideoZoomMode>(settings_.zoomMode), std::chrono::steady_clock::now());
        playerScreenState_.beginPlayback(playbackPlan.startPositionMs, playbackPlan.durationMs);
        if (playbackPlan.resetContinuation) playbackCoordinator_.syncQueueContinuation(queueState_);
        if (const auto* selectedSubtitle = playbackCoordinator_.selectedSubtitleStream()) {
            const SubtitleStrategy strategy = subtitleStrategy(selectedSubtitle->codec);
            if (useNativeSubtitleRenderer(strategy, true)) {
                loadSubtitleAsync(*selectedSubtitle,
                                  strategy == SubtitleStrategy::ClientText ? target.subtitleUrl : std::string{});
            }
        }
        if (!streamRestart) {
            if (screen_ == Screen::Player)
                replaceScreen(Screen::Player);
            else
                pushScreen(Screen::Player);
        } else {
            replaceScreen(Screen::Player);
        }
        return work;
    }

    void finishExternalPlayback(const PendingTickWork& work) {
        if (!work.externalResult || !work.completedExternalPlayback) return;
        const auto& completed = *work.completedExternalPlayback;
        const ExternalPlaybackFinishPlan plan = planExternalPlaybackFinish(completed, *work.externalResult);
        if (plan.failed) {
            std::scoped_lock lock(stateMutex_);
            error_ = "EXTERNAL PLAYER REPORTED PLAYBACK FAILURE";
            return;
        }

        if (plan.updatedItem) {
            std::scoped_lock lock(stateMutex_);
            ItemMutationController::updateCachedUserData(home_, homeState_, browseState_, searchFlow_.state(),
                                                         detailsFlow_.state(), queueState_, *plan.updatedItem,
                                                         homeVisibility_.isHidden(*plan.updatedItem));
            if (detailsFlow_.item().id == completed.item.id) {
                detailsFlow_.item().played = plan.updatedItem->played;
                detailsFlow_.item().positionTicks = plan.updatedItem->positionTicks;
            }
        }
        externalPlaybackAsync_.reportStopped(session_, ExternalPlaybackReportRequest{
                                                           .itemId = completed.item.id,
                                                           .mediaSourceId = completed.item.mediaSourceId,
                                                           .positionTicks = plan.positionTicks,
                                                       });
        std::scoped_lock lock(stateMutex_);
        error_.clear();
    }

    bool launchPendingExternalPlayback(PendingTickWork& work) {
        if (!work.externalLaunch) return false;
        auto launch = std::move(*work.externalLaunch);
        std::string launchError;
        const std::string title =
            launch.item.seriesName.empty() ? launch.item.name : launch.item.seriesName + " - " + launch.item.name;
        const int positionMs = playbackPositionMsFromTicks(launch.item.positionTicks);
        if (!externalPlayer_.launch(launch.player, launch.url, title, positionMs, launch.subtitleUrl,
                                    launch.skipSegmentsJson, launchError)) {
            std::scoped_lock lock(stateMutex_);
            error_ = launchError.empty() ? "EXTERNAL PLAYER COULD NOT BE LAUNCHED" : launchError;
        } else {
            std::scoped_lock lock(stateMutex_);
            error_.clear();
            playbackCoordinator_.recordExternalPlayback(launch.player.label);
            externalPlaybackState_.beginActive(std::move(launch));
        }
        return true;
    }

    bool startPendingPlaybackTransition(const PendingTickWork& work) {
        if (!work.playbackTransition) return false;
        const auto& transition = *work.playbackTransition;
        const auto& target = transition.target;
        const auto& item = transition.item;
        // A playback transition always creates a fresh SurfaceTexture. Stop libmpv first:
        // destroying the old Surface while MediaCodec is still bound to it can race
        // mediacodec_embed and abort when its wid becomes invalid.
        player_.stop();
        videoSurface_.release();
        std::string surfaceError;
        if (!renderer_.ready() || !videoSurface_.create(surfaceError)) {
            std::scoped_lock lock(stateMutex_);
            error_ = surfaceError.empty() ? "VIDEO SURFACE IS NOT AVAILABLE" : surfaceError;
            popScreen(Screen::Details);
            playbackCoordinator_.clearActivePlayback();
            return true;
        }
        if (settings_.refreshRateSwitching && item.videoFrameRate > 0.0f) {
            displayMode_.matchVideo(app_->window, item.videoFrameRate);
        }
        mediaSession_.updateMetadata(item.name, episodeLabel(item), playbackPositionMsFromTicks(item.runtimeTicks));
        mediaSession_.updateState(MediaSessionState::Buffering, playbackPositionMsFromTicks(target.startTicks));
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 5s);
        playbackRuntime_.startResolvedTarget();
        return true;
    }

    void tickActivePlayer() {
        if (screen_ != Screen::Player) return;
        applyPlaybackHostEffect(playbackRuntime_.tickActivePlayer(playbackStreamAsync_, playbackTelemetryAsync_,
                                                                  playbackContinuationAsync_, mediaSession_,
                                                                  queueState_, renderer_.ready(), stateMutex_));
    }

    void applyAsyncCompletion(SystemTextInputEvent& event) {
        applySystemTextInputEffects(systemTextInputController_.apply(event, searchFlow_.state(), settingsFlow_.state(),
                                                                     settings_, accountFlow_.state()));
    }

    void applySeerrCompletionHostEffects(SeerrCompletionHostEffects effects) {
        if (effects.clearNotice) statusOverlayState_.clearNotice();
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        if (effects.log) {
            const int priority =
                effects.log->level == SeerrCompletionLogLevel::Warning ? ANDROID_LOG_WARN : ANDROID_LOG_INFO;
            __android_log_print(priority, kTag, "%s", effects.log->message.c_str());
        }
        if (effects.syncHome) syncSeerrHomeRowLocked();
        if (effects.closeItem) {
            popScreen(Screen::Home);
            if (screen_ != Screen::Home) resetNavigation(Screen::Home);
        }
        if (effects.notice) showNotice(std::move(*effects.notice), std::chrono::seconds(effects.noticeSeconds));
        if (effects.reconnect) connectSeerrAsync(false);
        if (effects.openDrivePicker) openSeerrDrivePicker(*effects.openDrivePicker);
        if (effects.saveSession) saveSession(session_);
        if (effects.refreshPending) refreshSeerrPendingAsync();
        if (effects.refreshStorage) refreshSeerrStorageAsync(true);
        if (effects.deferredRequest) requestSeerrMediaAsync(*effects.deferredRequest);
        if (effects.retrySearch && screen_ == Screen::Search && !searchFlow_.state().query().empty()) {
            const auto now = std::chrono::steady_clock::now();
            (void)searchFlow_.prepareReconnectRetry(now);
            applySearchDispatchEffects(searchFlow_.searchSeerr(seerrEndpoint(), true, now));
        }
    }

    void applyAsyncCompletion(const SeerrDeleteCompletion& completion) {
        applySeerrCompletionHostEffects(
            seerrCompletionFlow_.complete(completion, seerrEndpoint(), screen_ == Screen::ItemMenu));
    }

    void applyAsyncCompletion(const SeerrRequestCompletion& completion) {
        applySeerrCompletionHostEffects(
            seerrCompletionFlow_.complete(completion, seerrEndpoint(), std::chrono::steady_clock::now()));
    }

    void applyAsyncCompletion(SeerrStorageRefreshCompletion& completion) {
        applySeerrCompletionHostEffects(seerrCompletionFlow_.complete(
            completion, seerrEndpoint(), settings_.seerrSelectDrive, std::chrono::steady_clock::now()));
    }

    void applyAsyncCompletion(SeerrPendingRefreshCompletion& completion) {
        applySeerrCompletionHostEffects(seerrCompletionFlow_.complete(
            completion, seerrEndpoint(), screen_ == Screen::ItemMenu, std::chrono::steady_clock::now()));
    }

    void applySearchCompletionEffects(SearchCompletionEffects effects) {
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        if (effects.reconnectSeerr) connectSeerrAsync(false);
        if (effects.syncSeerrHome) syncSeerrHomeRowLocked();
    }

    void applyAsyncCompletion(SeerrSearchCompletion& completion) {
        applySearchCompletionEffects(searchFlow_.complete(completion, screen_ == Screen::Search,
                                                          !settings_.seerrSessionCookie.empty(),
                                                          std::chrono::steady_clock::now()));
    }

    void applyAsyncCompletion(JellyfinSearchCompletion& completion) {
        applySearchCompletionEffects(searchFlow_.complete(completion, screen_ == Screen::Search));
    }

    void applyDetailsCompletionEffects(DetailsCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
    }

    void applyAsyncCompletion(ItemMenuDetailCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, screen_ == Screen::ItemMenu, detailsFlow_.item()));
    }

    void applyAsyncCompletion(PersonItemsCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, requestEpochs_.content.active(completion.generation),
                                               screen_ == Screen::PersonItems, detailsFlow_.state()));
    }

    void applyServerInfoCompletionEffects(ServerInfoCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.finishNoticeLoading) serverInfoLoading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.notice)
            showNotice(std::move(effects.notice->message), effects.notice->duration, effects.notice->persistent);
    }

    void applyAsyncCompletion(DiagnosticsCompletion& completion) {
        applyServerInfoCompletionEffects(ServerInfoCompletionController::applyDiagnostics(
            completion, requestEpochs_.content.active(completion.generation), screen_ == Screen::Diagnostics,
            serverInfo_));
    }

    void applyAsyncCompletion(SeasonsCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, requestEpochs_.content.active(completion.generation),
                                               screen_ == Screen::Seasons, detailsFlow_.state()));
    }

    void applyAsyncCompletion(EpisodesCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, requestEpochs_.content.active(completion.generation),
                                               screen_ == Screen::Episodes, detailsFlow_.state()));
    }

    void applyBrowseCompletionEffects(BrowseCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        if (effects.prefetchArtwork) uiPresentation_.prefetchBrowseArtworkAhead(session_, browseState_);
    }

    void applyAsyncCompletion(BrowsePageCompletion& completion) {
        applyBrowseCompletionEffects(BrowseCompletionController::apply(
            completion, requestEpochs_.content.active(completion.generation), screen_ == Screen::Browse,
            browseState_.activeContainer().id, browseState_, kBrowsePageSize));
    }

    void applyAsyncCompletion(ServerInfoNoticeCompletion& completion) {
        const bool activeSession =
            session_.valid() && session_.server == completion.server && session_.userId == completion.userId;
        applyServerInfoCompletionEffects(
            ServerInfoCompletionController::applyNotice(completion, activeSession, serverInfo_));
    }

    void applyContentMutationHostEffects(ContentMutationHostEffects effects) {
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.renderAnimationStarted) {
            renderBurstUntil_ =
                std::max(renderBurstUntil_, contentMutationFlow_.nextUpReplacementFadeStarted() + 320ms);
            if (app_ && app_->looper) ALooper_wake(app_->looper);
        }
        if (effects.closeDeletedItem) {
            popScreen(Screen::Home);
            if (screen_ == Screen::Details) popScreen(Screen::Home);
        }
        if (effects.notice) showNotice(std::move(*effects.notice));
    }

    void applyAsyncCompletion(FavoriteCompletion& completion) {
        applyContentMutationHostEffects(contentMutationFlow_.complete(
            completion, requestEpochs_.session.active(completion.sessionEpoch),
            screen_ == Screen::Details || screen_ == Screen::ItemMenu, homeVisibility_.isHidden(completion.item), home_,
            homeState_, browseState_, searchFlow_.state(), detailsFlow_.state(), queueState_, detailsFlow_.item()));
    }

    void applyAsyncCompletion(PlayedCompletion& completion) {
        const bool replacementHidden =
            completion.nextUpReplacement && homeVisibility_.isHidden(*completion.nextUpReplacement);
        applyContentMutationHostEffects(
            contentMutationFlow_.complete(completion, requestEpochs_.session.active(completion.sessionEpoch),
                                          screen_ == Screen::Details || screen_ == Screen::ItemMenu, replacementHidden,
                                          homeVisibility_.isHidden(completion.item), home_, homeState_, browseState_,
                                          searchFlow_.state(), detailsFlow_.state(), queueState_, detailsFlow_.item()));
    }

    void applyAsyncCompletion(MetadataRefreshCompletion& completion) {
        applyContentMutationHostEffects(
            contentMutationFlow_.complete(completion, requestEpochs_.session.active(completion.sessionEpoch)));
    }

    void applyAsyncCompletion(DeleteItemCompletion& completion) {
        applyContentMutationHostEffects(contentMutationFlow_.complete(
            completion, requestEpochs_.session.active(completion.sessionEpoch), screen_ == Screen::ItemMenu, home_,
            homeState_, browseState_, searchFlow_.state(), detailsFlow_.state(), queueState_, detailsFlow_.item()));
    }

    void applyAccountCompletionEffects(AccountCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        if (!effects.authenticatedSession) return;

        requestEpochs_.session.invalidate();
        session_ = std::move(*effects.authenticatedSession);
        resetNavigation(Screen::Home);
        homeState_.setRow(0);
        homeState_.setFirstVisibleRow(0);
        saveSession(session_);
        loadHomeAsync();
    }

    void applyAsyncCompletion(DiscoveryCompletion& completion) {
        applyAccountCompletionEffects(
            accountFlow_.complete(completion, requestEpochs_.auth.active(completion.generation)));
    }

    void applyAsyncCompletion(LoginCompletion& completion) {
        applyAccountCompletionEffects(
            accountFlow_.complete(completion, requestEpochs_.auth.active(completion.generation)));
    }

    void applyAsyncCompletion(DetailsItemCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, requestEpochs_.content.active(completion.generation),
                                               screen_ == Screen::Details, detailsFlow_.item()));
    }

    void applyAsyncCompletion(DetailsSimilarCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, requestEpochs_.content.active(completion.generation),
                                               screen_ == Screen::Details, detailsFlow_.item(), detailsFlow_.state()));
    }

    void applyAsyncCompletion(SimilarPrefetchCompletion& completion) {
        auto items = similarPrefetch_.complete(completion);
        if (!items || screen_ != Screen::Details || detailsFlow_.item().id != completion.itemId ||
            !detailsFlow_.state().similar().empty())
            return;
        detailsFlow_.state().setSimilar(std::move(*items));
    }

    void applyAsyncCompletion(EpisodeSeriesContextRequestCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        if (screen_ != Screen::Details || !completion.request.matches(detailsFlow_.item())) return;

        const RequestEpoch::Token requestToken = requestEpochs_.content.token(completion.generation);
        detailsAsync_.loadSeriesContext(std::move(completion.session), std::move(completion.request), requestToken);
    }

    void applyAsyncCompletion(EpisodeSeriesContextCompletion& completion) {
        applyDetailsCompletionEffects(
            DetailsCompletionController::apply(completion, requestEpochs_.content.active(completion.generation),
                                               screen_ == Screen::Details, detailsFlow_.item(), detailsFlow_.state()));
    }

    void applyAsyncCompletion(QuickConnectStartedCompletion& completion) {
        applyAccountCompletionEffects(
            accountFlow_.complete(completion, requestEpochs_.auth.active(completion.generation)));
    }

    void applyAsyncCompletion(QuickConnectFailedCompletion& completion) {
        applyAccountCompletionEffects(
            accountFlow_.complete(completion, requestEpochs_.auth.active(completion.generation)));
    }

    void applyAsyncCompletion(QuickConnectAuthenticatedCompletion& completion) {
        applyAccountCompletionEffects(
            accountFlow_.complete(completion, requestEpochs_.auth.active(completion.generation)));
    }

    void applyAsyncCompletion(QuickConnectTimedOutCompletion& completion) {
        applyAccountCompletionEffects(
            accountFlow_.complete(completion, requestEpochs_.auth.active(completion.generation)));
    }

    void applyAsyncCompletion(HomeCoreCompletion& completion) {
        HomeCoreFlowEffects effects =
            homeCompletionFlow_.complete(completion, requestEpochs_.home.active(completion.generation),
                                         screen_ == Screen::Home, std::chrono::steady_clock::now());
        if (!effects.active) return;
        if (effects.retryDelaySeconds) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Home load failed transiently; retrying in %d seconds: %s",
                                *effects.retryDelaySeconds, completion.result.error.c_str());
        }

        if (effects.sessionExpired) {
            const JellyfinSession expired = session_;
            artwork_.eraseProfile(expired, renderer_);
            (void)accountFlow_.removeIdentity(expired);
            requestEpochs_.invalidateAll();
            loading_ = false;
            contentMutationFlow_.reset();
            searchFlow_.state().setLoading(false);
            session_.token.clear();
            session_.userId.clear();
            settings_.seerrSessionCookie.clear();
            seerrDomain_.invalidateStorageTargets();
            resetNavigation(Screen::Login);
            error_ = "SESSION EXPIRED - LOG IN AGAIN";
            saveSession(session_);
            return;
        }
        if (effects.visibleError) error_ = std::move(*effects.visibleError);
        if (!effects.loaded) return;

        syncSeerrHomeRowLocked();
        if (effects.prefetch)
            uiPresentation_.prefetchHomeWindow(session_, home_, effects.prefetch->row, effects.prefetch->selection);
        const auto coreMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                  completion.startedAt)
                                .count();
        __android_log_print(ANDROID_LOG_INFO, kTag, "Home primary rows ready in %lld ms",
                            static_cast<long long>(coreMs));
        if (!pendingDeepLinkItemId_.empty()) {
            JellyfinItem linked;
            linked.id = std::move(pendingDeepLinkItemId_);
            pendingDeepLinkItemId_.clear();
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening ACTION_VIEW Jellyfin item %s", linked.id.c_str());
            openDetails(linked);
            return;
        }
        if (!pendingSearchQuery_.empty()) {
            searchFlow_.state().setQuery(std::move(pendingSearchQuery_));
            pendingSearchQuery_.clear();
            searchFlow_.state().setKeyboard(false);
            pushScreen(Screen::Search);
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening ACTION_SEARCH query");
            searchAsync();
            return;
        }

        refreshSeerrPendingAsync();
        homeAsync_.loadSecondary(session_, completion.generation, std::move(effects.secondaryViews),
                                 std::move(completion.snapshot), effects.coreRestoredRow, completion.startedAt);
    }

    void applyAsyncCompletion(HomeSecondaryCompletion& completion) {
        HomeSecondaryFlowEffects effects = homeCompletionFlow_.complete(
            completion, requestEpochs_.home.active(completion.generation), screen_ == Screen::Home);
        if (!effects.active) return;
        if (effects.visibleError) error_ = std::move(*effects.visibleError);
        if (!effects.loaded) return;
        if (effects.prefetch)
            uiPresentation_.prefetchHomeWindow(session_, home_, effects.prefetch->row, effects.prefetch->selection);

        const auto fullMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                  completion.startedAt)
                                .count();
        __android_log_print(ANDROID_LOG_INFO, kTag, "Home enrichment completed in %lld ms",
                            static_cast<long long>(fullMs));
    }

    void applyAsyncCompletion(ExternalPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Details || detailsFlow_.item().id != completion.selectedItemId) return;
        if (!completion.error.empty()) {
            error_ = std::move(completion.error);
            return;
        }
        if (!completion.launch) return;
        JellyfinItem selected;
        selected.id = completion.selectedItemId;
        selected.seriesId = completion.selectedSeriesId;
        restoreHomeVisibilityForPlayback(selected);
        restoreHomeVisibilityForPlayback(completion.launch->item);
        externalPlaybackState_.stage(std::move(*completion.launch));
    }

    void applyPlaybackCompletionHostEffects(PlaybackCompletionHostEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.popToDetails) popScreen(Screen::Details);
        if (effects.error) error_ = std::move(*effects.error);

        for (const auto& restore : effects.restoreHomeVisibility) {
            JellyfinItem item;
            item.id = restore.itemId;
            item.seriesId = restore.seriesId;
            restoreHomeVisibilityForPlayback(item);
        }

        if (effects.stopPlayback) stopPlayback();
        if (effects.diagnostic) {
            const auto& diagnostic = *effects.diagnostic;
            switch (diagnostic.kind) {
            case PlayerCompletionDiagnosticKind::None:
                break;
            case PlayerCompletionDiagnosticKind::SubtitleLoaded:
                __android_log_print(ANDROID_LOG_INFO, kTag, "Subtitle loaded item=%s stream=%d codec=%s cues=%zu",
                                    diagnostic.itemId.c_str(), diagnostic.streamIndex, diagnostic.codec.c_str(),
                                    diagnostic.count);
                break;
            case PlayerCompletionDiagnosticKind::MediaSegmentsUnavailable:
                __android_log_print(ANDROID_LOG_WARN, kTag, "Media segments unavailable: %s", diagnostic.error.c_str());
                break;
            case PlayerCompletionDiagnosticKind::MediaSegmentsLoaded:
                __android_log_print(ANDROID_LOG_INFO, kTag, "Loaded %zu media segments", diagnostic.count);
                break;
            case PlayerCompletionDiagnosticKind::NextEpisodeUnavailable:
                __android_log_print(ANDROID_LOG_WARN, kTag, "Next episode lookup failed: %s", diagnostic.error.c_str());
                break;
            }
        }
        if (effects.notice) showNotice(std::move(effects.notice->text), effects.notice->duration);
        if (effects.reportProgress)
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
        if (effects.playItem) playPlayerItemAsync(std::move(*effects.playItem));
    }

    void applyAsyncCompletion(SubtitleLoadCompletion& completion) {
        applyPlaybackCompletionHostEffects(
            playbackCompletionFlow_.complete(completion, requestEpochs_.playback.active(completion.generation)));
    }

    void applyAsyncCompletion(TrickplayTileCompletion& completion) {
        if (!trickplayState_.matchesTile(completion.itemId, completion.tileIndex)) return;
        if (!completion.decoded.valid()) {
            trickplayState_.markFailed();
            __android_log_print(ANDROID_LOG_WARN, kTag, "Trickplay tile %d unavailable: %s", completion.tileIndex,
                                completion.error.c_str());
            return;
        }
        trickplayState_.applyDecoded(std::move(completion.decoded));
    }

    void applyAsyncCompletion(ArtworkLoadCompletion& completion) { artwork_.applyCompletion(std::move(completion)); }

    void applyAsyncCompletion(MediaSegmentsCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(completion, screen_ == Screen::Player));
    }

    void applyAsyncCompletion(NextEpisodeCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(completion, screen_ == Screen::Player));
    }

    void applyAsyncCompletion(PlaybackAdjacentCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(completion, screen_ == Screen::Player));
    }

    void applyAsyncCompletion(const PlaybackReportCompletion& completion) {
        logPlaybackReportFailure(PlayerCompletionController::reportStage(completion.kind), completion.itemId,
                                 completion.result);
        PlayerCompletionController::apply(completion, session_.server, session_.userId, screen_ == Screen::Player,
                                          playbackCoordinator_);
    }

    void applyAsyncCompletion(QueuedPlaybackCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(
            completion, requestEpochs_.playback.active(completion.generation), screen_));
    }

    void applyAsyncCompletion(PlayerItemPlaybackCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(
            completion, requestEpochs_.playback.active(completion.generation), screen_ == Screen::Player));
    }

    void applyAsyncCompletion(AutoplayPlaybackCompletion& completion) {
        applyPlaybackCompletionHostEffects(
            playbackCompletionFlow_.complete(completion, requestEpochs_.playback.active(completion.generation)));
    }

    void applyAsyncCompletion(StreamRestartCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(
            completion, requestEpochs_.playback.active(completion.generation), screen_ == Screen::Player));
    }

    void applyAsyncCompletion(FallbackPlaybackCompletion& completion) {
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(
            completion, requestEpochs_.playback.active(completion.generation), screen_ == Screen::Player));
    }

    void applyAsyncCompletion(BeginPlaybackCompletion& completion) {
        const bool activeDetailsSelection =
            screen_ == Screen::Details && detailsFlow_.item().id == completion.selected.id;
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(
            completion, requestEpochs_.playback.active(completion.generation), activeDetailsSelection));
    }

    void applyAsyncCompletion(SeriesPlayAllCompletion& completion) {
        const bool activeDetailsSelection =
            screen_ == Screen::Details && detailsFlow_.item().id == completion.series.id;
        applyPlaybackCompletionHostEffects(playbackCompletionFlow_.complete(
            completion, requestEpochs_.playback.active(completion.generation), activeDetailsSelection));
    }

    void applyAsyncCompletion(SeerrConnectCompletion& completion) {
        applySeerrCompletionHostEffects(seerrCompletionFlow_.complete(completion));
    }

    void applyAsyncCompletions() {
        auto completions = asyncCompletions_.takeAll();
        if (completions.empty()) return;
        std::scoped_lock lock(stateMutex_);
        for (auto& completion : completions) {
            std::visit([this](auto& value) { applyAsyncCompletion(value); }, completion);
        }
    }

    void tick() {
        applyAsyncCompletions();
        runDueSimilarPrefetch();
        const auto mediaSessionCommand = mediaSession_.takeCommand();
        if (mediaSessionCommand) handleMediaSessionCommand(*mediaSessionCommand);
        applyPendingRuntimeLaunchRequest();

        bool retryHome = false;
        bool refreshHomeAfterPlaybackStop = false;
        bool refreshSeerr = false;
        {
            std::scoped_lock lock(stateMutex_);
            const auto now = std::chrono::steady_clock::now();
            if (!homeLoading_ && homeRetryAt_ != std::chrono::steady_clock::time_point{} && now >= homeRetryAt_ &&
                session_.valid()) {
                homeRetryAt_ = {};
                retryHome = true;
            }
            if (!homeLoading_ && session_.valid() && screen_ != Screen::Player &&
                playbackCoordinator_.consumeHomeRefreshRequest()) {
                refreshHomeAfterPlaybackStop = true;
            }
            const bool seerrRefreshEligible =
                SeerrClient::configured(settings_.seerrServer, seerrAuth()) &&
                (screen_ == Screen::Home || (screen_ == Screen::ItemMenu && isSeerrItem(detailsFlow_.item())));
            if (seerrDomain_.consumePendingRefreshDue(now, seerrRefreshEligible)) refreshSeerr = true;
        }
        if (retryHome || refreshHomeAfterPlaybackStop) loadHomeAsync();
        if (refreshSeerr) refreshSeerrPendingAsync();

        auto work = collectPendingTickWork();
        finishExternalPlayback(work);
        if (launchPendingExternalPlayback(work)) return;
        if (startPendingPlaybackTransition(work)) return;
        tickActivePlayer();
    }

    void stopPlayback(bool completed = false) {
        if (screen_ != Screen::Player && player_.status() == PlayerStatus::Idle) return;
        releaseActivePlayback(true, completed);
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        playbackCoordinator_.finishStop();
        if (screen_ == Screen::Player) popScreen(Screen::Details);
        if (app_->window && !renderer_.ready()) renderer_.init(app_->window);
        loadHomeAsync();
    }

    void render() {
        std::scoped_lock lock(stateMutex_);
        renderer_.beginFrame();

        DeviceCodecSupport codecSupport;
        if (screen_ == Screen::Settings || screen_ == Screen::Diagnostics) {
            codecSupport = api_.deviceCodecSupport();
        }

        renderAppScreenFrame(AppScreenPresentationFrame<decltype(uiPresentation_), decltype(artwork_)>{
            .screen = screen_,
            .backgroundScreen = navigation_.previousOr(Screen::Details),
            .screensaverActive = screensaverActive_,
            .loading = loading_,
            .homeLoading = homeLoading_,
            .mutationLoading = contentMutationFlow_.loading(),
            .seerrConfigured = SeerrClient::configured(settings_.seerrServer, seerrAuth()),
            .systemSearchInputActive = systemTextInputController_.mode() == kTextInputSearch,
            .systemSettingsSearchActive = systemTextInputController_.mode() == kTextInputSettingsSearch,
            .systemSeerrApiKeyActive = systemTextInputController_.mode() == kTextInputSeerrApiKey,
            .itemHiddenFromHome = homeVisibility_.isHidden(detailsFlow_.item()),
            .keyboardRow = virtualKeyboard_.row(),
            .keyboardCol = virtualKeyboard_.column(),
            .homeSlideFromFirst = homeSlideFromFirst_,
            .homeSlideToFirst = homeSlideToFirst_,
            .homeSlideStarted = homeSlideStarted_,
            .nextUpReplacementFadeIndex = contentMutationFlow_.nextUpReplacementFadeIndex(),
            .nextUpReplacementFadeItemId = contentMutationFlow_.nextUpReplacementFadeItemId(),
            .nextUpReplacementFadeStarted = contentMutationFlow_.nextUpReplacementFadeStarted(),
            .lastInteraction = lastInteraction_,
            .appVersion = SLOPPATV_VERSION_NAME,
            .error = error_,
            .renderer = renderer_,
            .ui = uiPresentation_,
            .artwork = artwork_,
            .brandMark = brandMark_,
            .accountFlow = accountFlow_,
            .home = home_,
            .homeState = homeState_,
            .session = session_,
            .settings = settings_,
            .browseState = browseState_,
            .searchState = searchFlow_.state(),
            .seerrDomain = seerrDomain_,
            .settingsScreen = settingsFlow_.state(),
            .externalPlayers = settingsFlow_.externalPlayers(),
            .deviceCodecSupport = codecSupport,
            .serverInfo = serverInfo_,
            .detailsFlow = detailsFlow_,
            .player = player_,
            .videoSurface = videoSurface_,
            .playbackCoordinator = playbackCoordinator_,
            .playerScreenState = playerScreenState_,
            .trickplayState = trickplayState_,
            .queueState = queueState_,
            .statusOverlayState = statusOverlayState_,
        });

        renderer_.endFrame();
    }

    void loadSession() {
        AccountPersistedState restored = accountFlow_.restore(dataPath_);
        if (!restored.warning.empty()) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to read session: %s", restored.warning.c_str());
        }
        session_ = std::move(restored.session);
        hiddenHomeItems_ = std::move(restored.hiddenHomeItems);
        settings_ = std::move(restored.settings);
        if (session_.valid()) artwork_.eraseProfile(session_, renderer_);
        playbackCoordinator_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
    }

    void saveSession(const JellyfinSession& session) {
        if (session.valid()) artwork_.eraseProfile(session, renderer_);
        std::string warning;
        if (!accountFlow_.persist(dataPath_, session, hiddenHomeItems_, settings_, warning) && !warning.empty()) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to save session: %s", warning.c_str());
        }
    }

    android_app* app_ = nullptr;
    Renderer renderer_;
    JellyfinClient api_;
    JellyfinClient similarPrefetchApi_;
    SeerrClient seerr_;
    SeerrClient seerrSearch_;
    DisplayModeController displayMode_;
    NativeMediaPlayer player_;
    NativeMediaSession mediaSession_;
    NativeExternalPlayer externalPlayer_;
    JniImageDecoder imageDecoder_;
    VideoSurface videoSurface_;
    TaskRunner tasks_;
    TaskRunner similarPrefetchTasks_;
    AsyncCompletionQueue<AsyncCompletion> asyncCompletions_;
    RequestEpochs requestEpochs_;
    AccountAsyncExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> accountAsync_;
    QuickConnectExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> quickConnectAsync_;
    DetailsAsyncExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> detailsAsync_;
    BrowseAsyncExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> browseAsync_;
    JellyfinSearchExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> jellyfinSearchAsync_;
    ServerInfoExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> serverInfoAsync_;
    ItemMutationExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> itemMutationAsync_;
    HomeAsyncExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> homeAsync_;
    ExternalPlaybackExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>, RequestEpoch>
        externalPlaybackAsync_;
    PlaybackTelemetryExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>
        playbackTelemetryAsync_;
    PlaybackContinuationExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>
        playbackContinuationAsync_;
    PlaybackResolutionExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>, RequestEpoch>
        playbackResolutionAsync_;
    PlaybackStreamExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>, RequestEpoch>
        playbackStreamAsync_;
    SeriesPlaybackExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>, RequestEpoch>
        seriesPlaybackAsync_;
    SubtitleLoadExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>, RequestEpoch>
        subtitleLoadAsync_;
    TrickplayTileExecutor<JellyfinClient, JniImageDecoder, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>
        trickplayTileAsync_;
    SeerrAsyncExecutor<SeerrClient, SeerrClient, JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>
        seerrAsync_;

    mutable std::recursive_mutex stateMutex_;
    ArtworkProvider<JellyfinClient, SeerrClient, JniImageDecoder, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>
        artwork_;
    UiPresentation<decltype(artwork_)> uiPresentation_;
    std::string dataPath_;

    Screen screen_ = Screen::Login;
    NavigationStack<Screen> navigation_{Screen::Login};
    bool loading_ = false;
    bool homeLoading_ = false;
    std::chrono::steady_clock::time_point homeRetryAt_{};
    int homeRetryAttempt_ = 0;
    ContentMutationFlow contentMutationFlow_;
    AppSettings settings_;
    SettingsFlow settingsFlow_;
    std::string error_;
    StatusOverlayState statusOverlayState_;
    int homeSlideFromFirst_ = 0;
    int homeSlideToFirst_ = 0;
    std::chrono::steady_clock::time_point homeSlideStarted_{};

    JellyfinSession session_;
    std::string pendingDeepLinkItemId_;
    std::string pendingSearchQuery_;
    std::optional<LaunchRequest> pendingRuntimeLaunchRequest_;
    AccountFlow accountFlow_;
    JellyfinServerInfo serverInfo_;
    bool serverInfoLoading_ = false;
    JellyfinHomeData home_;
    SeerrDomainState seerrDomain_;
    SeerrConnectionCoordinator<
        SeerrAsyncExecutor<SeerrClient, SeerrClient, JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>>
        seerrConnection_;
    SeerrRefreshCoordinator<
        SeerrAsyncExecutor<SeerrClient, SeerrClient, JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>>
        seerrRefresh_;
    SeerrRequestCoordinator<
        SeerrAsyncExecutor<SeerrClient, SeerrClient, JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>>
        seerrRequest_;
    SearchFlow<
        SeerrAsyncExecutor<SeerrClient, SeerrClient, JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>,
        JellyfinSearchExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>>
        searchFlow_;
    BrandMark brandMark_;
    HomeScreenState homeState_;
    std::unordered_set<std::string> hiddenHomeItems_;
    HomeVisibility homeVisibility_;
    HomeCompletionFlow homeCompletionFlow_;
    BrowseScreenState browseState_;

    SystemTextInputController systemTextInputController_;

    SimilarPrefetchController similarPrefetch_;

    VirtualKeyboardState virtualKeyboard_;

    DetailsFlow detailsFlow_;
    SeerrCompletionFlow<decltype(seerrConnection_), decltype(seerrRequest_), decltype(seerrRefresh_)>
        seerrCompletionFlow_;

    PlaybackQueueState queueState_;

    ExternalPlaybackState externalPlaybackState_;
    PlaybackCoordinator playbackCoordinator_;
    PlayerScreenState playerScreenState_;
    PlaybackCompletionFlow playbackCompletionFlow_;
    PlaybackRequestFlow playbackRequestFlow_;
    TrickplayPreviewState trickplayState_;
    PlaybackRuntimeController playbackRuntime_;
    PlaybackReleaseFlow<decltype(playbackTelemetryAsync_)> playbackReleaseFlow_;
    PlaybackLifecycleFlow playbackLifecycleFlow_;
    std::chrono::steady_clock::time_point renderBurstUntil_{};
    std::chrono::steady_clock::time_point lastInteraction_ = std::chrono::steady_clock::now();
    bool screensaverActive_ = false;
};
} // namespace

extern "C" JNIEXPORT void JNICALL Java_app_sloppatv_SloppaNativeActivity_nativeOnNewIntent(JNIEnv* env, jclass,
                                                                                           jstring action, jstring data,
                                                                                           jstring query) {
    const std::string actionValue = jniString(env, action);
    const std::string dataValue = jniString(env, data);
    const std::string queryValue = jniString(env, query);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onNewLaunchIntent(actionValue, dataValue, queryValue);
}

extern "C" JNIEXPORT void JNICALL Java_app_sloppatv_SloppaNativeActivity_nativeOnBackPressed(JNIEnv*, jclass) {
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onSystemBackPressed();
}

extern "C" JNIEXPORT void JNICALL Java_app_sloppatv_SloppaNativeActivity_nativeOnSystemTextInputChanged(JNIEnv* env,
                                                                                                        jclass,
                                                                                                        jint mode,
                                                                                                        jstring text) {
    const std::string value = jniString(env, text);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onSystemTextInputChanged(static_cast<int>(mode), value);
}

extern "C" JNIEXPORT void JNICALL Java_app_sloppatv_SloppaNativeActivity_nativeOnSystemTextInputDone(JNIEnv* env,
                                                                                                     jclass, jint mode,
                                                                                                     jstring text) {
    const std::string value = jniString(env, text);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onSystemTextInputDone(static_cast<int>(mode), value);
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaNativeActivity_nativeOnSystemTextInputCancelled(JNIEnv* env, jclass, jint mode, jstring text) {
    const std::string value = jniString(env, text);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onSystemTextInputCancelled(static_cast<int>(mode), value);
}

void android_main(android_app* app) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "Starting sloppaTV native activity");
    SloppaApp sloppa(app);
    {
        std::scoped_lock lock(gActiveAppMutex);
        gActiveApp = &sloppa;
    }
    app->userData = &sloppa;
    app->onAppCmd = SloppaApp::handleAppCommand;
    app->onInputEvent = SloppaApp::handleInput;
    sloppa.warmDeviceCapabilitiesAsync();
    sloppa.run();
    {
        std::scoped_lock lock(gActiveAppMutex);
        if (gActiveApp == &sloppa) gActiveApp = nullptr;
    }
}
