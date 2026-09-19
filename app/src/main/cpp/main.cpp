#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "account_async_executor.hpp"
#include "account_completion_controller.hpp"
#include "account_navigation_controller.hpp"
#include "account_screen.hpp"
#include "app_screen.hpp"
#include "app_settings.hpp"
#include "artwork_provider.hpp"
#include "async_completion_queue.hpp"
#include "audio_policy.hpp"
#include "browse_async_executor.hpp"
#include "browse_completion_controller.hpp"
#include "browse_navigation_controller.hpp"
#include "browse_renderer.hpp"
#include "browse_screen.hpp"
#include "cast_renderer.hpp"
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
#include "home_navigation_controller.hpp"
#include "home_renderer.hpp"
#include "home_row_renderer.hpp"
#include "home_screen.hpp"
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
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_resolution_executor.hpp"
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
#include "search_navigation_controller.hpp"
#include "search_renderer.hpp"
#include "search_screen.hpp"
#include "seerr.hpp"
#include "series_playback_executor.hpp"
#include "seerr_async_executor.hpp"
#include "seerr_connection_coordinator.hpp"
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
#include "settings_action_controller.hpp"
#include "settings_navigation_controller.hpp"
#include "settings_screen.hpp"
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
#include <unordered_map>
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

constexpr Color kPanel = material_tv::surface;
constexpr Color kPanelAlt = material_tv::surfaceContainer;
constexpr Color kPanelElevated = material_tv::surfaceContainerHigh;
constexpr Color kText = material_tv::onSurface;
constexpr Color kSecondaryText = material_tv::onSurfaceSecondary;
constexpr Color kMuted = material_tv::onSurfaceVariant;
constexpr Color kTertiary = material_tv::onSurfaceDisabled;
constexpr Color kFocus = material_tv::primary;
constexpr Color kFocusSoft = material_tv::primaryContainer;
constexpr Color kOutline = material_tv::outline;
constexpr Color kTrack = material_tv::track;
constexpr Color kError = material_tv::error;

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

struct SimilarPrefetchEntry {
    std::vector<JellyfinItem> items;
    std::chrono::steady_clock::time_point loadedAt{};
};

struct SimilarPrefetchCompletion {
    JellyfinSession session;
    std::string itemId;
    std::string key;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

using QueuedPlaybackCompletion = QueuedPlaybackResolutionCompletion<Screen>;

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
          uiPresentation_(renderer_, artwork_), seerrConnection_(seerrDomain_, seerrAsync_),
          seerrRefresh_(seerrDomain_, seerrAsync_), seerrRequest_(seerrDomain_, seerrAsync_),
          seerrSearchCoordinator_(seerrDomain_, seerrAsync_), searchState_(seerrDomain_.searchResults()),
          playbackRuntime_(playbackCoordinator_, playerScreenState_, player_, videoSurface_, session_, settings_,
                           requestEpochs_.playback, loading_, error_, dataPath_) {
        __android_log_print(ANDROID_LOG_INFO, kTag, "Startup init: platform bridges ready");
        dataPath_ = app->activity->internalDataPath ? app->activity->internalDataPath : "";
        artwork_.setDataPath(dataPath_);
        loadBundledBrandMark();
        const LaunchRequest launchRequest = readLaunchRequest(app_);
        pendingDeepLinkItemId_ = launchRequest.itemId;
        pendingSearchQuery_ = launchRequest.searchQuery;
        loadSession();
        if (!settings_.externalPlayerComponent.empty()) refreshExternalPlayers();
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
        if (brandMarkTexture_ != 0 && renderer_.ready()) renderer_.deleteTexture(brandMarkTexture_);
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
                else if (loading_ || homeLoading_ || mutationLoading_ || searchState_.loading() ||
                         seerrDomain_.searchLoading() || accountState_.quickConnectActive())
                    timeoutMs = 100;
                else {
                    const int64_t delayMs = screensaverDelayMs(settings_.screensaverMinutes);
                    if (delayMs > 0) {
                        const int64_t idleMs =
                            std::chrono::duration_cast<std::chrono::milliseconds>(pollNow - lastInteraction_).count();
                        timeoutMs = static_cast<int>(std::clamp<int64_t>(delayMs - idleMs, 0, delayMs));
                    }
                }
                if (searchState_.debouncePending()) {
                    const int64_t searchDelayMs =
                        std::chrono::duration_cast<std::chrono::milliseconds>(searchState_.debounceDeadline() - pollNow)
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
                tightenTimeoutUntil(similarPrefetchDue_);
                if (!seerrDomain_.pendingRequestsLoading() &&
                    (screen_ == Screen::Home || (screen_ == Screen::ItemMenu && isSeerrItem(detailsFlow_.item())))) {
                    tightenTimeoutUntil(seerrDomain_.pendingRequestsRefreshDeadline());
                }
                if (screen_ == Screen::Search && !searchState_.keyboard() && !searchState_.results().empty()) {
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
                        loading_ || homeLoading_ || mutationLoading_ || searchState_.loading() ||
                            seerrDomain_.searchLoading() || accountState_.quickConnectActive());
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
        if (effects.cancelSeerrSearch) seerrSearchCoordinator_.cancel();
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
        case APP_CMD_INIT_WINDOW: {
            bool reusedRendererContext = false;
            if (app_->window) {
                if (renderer_.contextReady() && !renderer_.ready()) {
                    reusedRendererContext = renderer_.attachWindow(app_->window);
                }
                if (!renderer_.ready()) renderer_.init(app_->window);
            }
            lastInteraction_ = std::chrono::steady_clock::now();
            screensaverActive_ = false;
            const bool restoreCandidate = screen_ == Screen::Player && playerScreenState_.windowRestorePending() &&
                                          renderer_.ready() && playbackCoordinator_.activeTargetAvailable();
            const PlayerStatus restoreStatus = restoreCandidate ? player_.status() : PlayerStatus::Idle;
            const PlaybackWindowRestorePlan restorePlan = playbackCoordinator_.windowRestorePlan(
                screen_ == Screen::Player, playerScreenState_.windowRestorePending(), renderer_.ready(),
                reusedRendererContext, videoSurface_.ready(),
                restoreStatus != PlayerStatus::Idle && restoreStatus != PlayerStatus::Ended &&
                    restoreStatus != PlayerStatus::Error,
                playerScreenState_.resumeOnFocusRequested());
            if (restorePlan.restore) {
                if (settings_.refreshRateSwitching && restorePlan.videoFrameRate > 0.0f) {
                    displayMode_.matchVideo(app_->window, restorePlan.videoFrameRate);
                }
                if (restorePlan.preservePlayer) {
                    if (restorePlan.resumePlayback) player_.play();
                    mediaSession_.updateState(restorePlan.resumePlayback ? MediaSessionState::Playing
                                                                         : MediaSessionState::Paused,
                                              playerScreenState_.positionMs());
                    __android_log_print(ANDROID_LOG_INFO, kTag,
                                        "Restored playback with preserved libmpv and GLES context");
                } else {
                    // libmpv must release MediaCodec before its Android Surface is destroyed.
                    // Releasing VideoSurface first can make mediacodec_embed observe wid=0 while
                    // the decoder is still active and abort inside vo_mediacodec_embed.
                    player_.stop();
                    videoSurface_.release();
                    std::string surfaceError;
                    if (!videoSurface_.create(surfaceError)) {
                        error_ = surfaceError.empty() ? "VIDEO SURFACE COULD NOT BE RESTORED" : surfaceError;
                        break;
                    }
                    const PlaybackPlayerStartContext start =
                        playbackCoordinator_.playerStartContext(PlaybackPlayerStartMode::WindowRestore);
                    player_.startAsync(start.url, videoSurface_.surface(), playerScreenState_.positionMs(),
                                       settings_.playbackBufferPreset, start.audioOrdinal, start.subtitleStreamIndex,
                                       start.subtitleOrdinal, start.externalSubtitleUrl);
                    playbackCoordinator_.setPauseAfterRestart(restorePlan.pauseAfterRestart);
                    mediaSession_.updateState(MediaSessionState::Buffering, playerScreenState_.positionMs());
                    __android_log_print(
                        ANDROID_LOG_WARN, kTag,
                        "GLES context was not reusable during window restore; recreated playback surface");
                }
                playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
                playerScreenState_.completeWindowRestore();
            }
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            const bool suspendCandidate = screen_ == Screen::Player && playbackCoordinator_.activeTargetAvailable();
            const PlayerStatus status = suspendCandidate ? player_.status() : PlayerStatus::Idle;
            const PlaybackWindowSuspendPlan suspendPlan = playbackCoordinator_.windowSuspendPlan(
                screen_ == Screen::Player, status == PlayerStatus::Playing || status == PlayerStatus::Preparing);
            if (suspendPlan.suspend) {
                playbackRuntime_.refreshTelemetry(true);
                playerScreenState_.beginWindowRestore(suspendPlan.resumePlayback);
                if (suspendPlan.resumePlayback) player_.pause();
                playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, true);
                displayMode_.restore();
                mediaSession_.updateState(MediaSessionState::Paused, playerScreenState_.positionMs());
            }
            if (!renderer_.detachWindow()) renderer_.shutdown();
            break;
        }
        case APP_CMD_GAINED_FOCUS:
            lastInteraction_ = std::chrono::steady_clock::now();
            screensaverActive_ = false;
            if (screen_ == Screen::Player && playerScreenState_.takeResumeOnFocus()) {
                player_.play();
                mediaSession_.updateState(MediaSessionState::Playing, playerScreenState_.positionMs());
                __android_log_print(ANDROID_LOG_INFO, kTag, "Resumed playback after focus restoration");
            } else if (screen_ == Screen::Browse && session_.valid() && !loading_ &&
                       !browseState_.activeContainer().id.empty()) {
                loadBrowsePageAsync(false);
            }
            break;
        case APP_CMD_LOST_FOCUS: {
            screensaverActive_ = false;
            const bool playerScreenActive = screen_ == Screen::Player;
            const PlayerStatus status = playerScreenActive ? player_.status() : PlayerStatus::Idle;
            if (shouldPausePlaybackForFocusLoss(playerScreenActive,
                                                status == PlayerStatus::Playing || status == PlayerStatus::Preparing)) {
                playerScreenState_.requestResumeOnFocus();
                player_.pause();
                mediaSession_.updateState(MediaSessionState::Paused, playerScreenState_.positionMs());
            }
            break;
        }
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

        const TextEntryResult textEntry = handlePhysicalTextInput(screen_, accountState_, searchState_, key, meta);
        if (textEntry.searchChanged) scheduleLiveSearch();
        if (textEntry.handled) return 1;

        dispatchScreenKey(key);
        return 1;
    }

    void scheduleLiveSearch() {
        const auto now = std::chrono::steady_clock::now();
        requestEpochs_.search.invalidate();
        const bool seerrConfigured = SeerrClient::configured(settings_.seerrServer, seerrAuth());
        const auto seerrPlan = seerrSearchCoordinator_.schedule(searchState_.query(), now, seerrConfigured);
        if (seerrPlan.resultsChanged) searchState_.refreshSeerrResults();
        if (seerrPlan.invalidateRequest) requestEpochs_.seerrSearch.invalidate();
        if (!searchState_.scheduleDebounce(now)) error_.clear();
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void runDueLiveSearch() {
        std::scoped_lock lock(stateMutex_);
        if (screen_ != Screen::Search) {
            searchState_.cancelPending();
            seerrSearchCoordinator_.cancel();
            requestEpochs_.search.invalidate();
            requestEpochs_.seerrSearch.invalidate();
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (searchState_.debounceDue(now)) searchAsync(false);
        if (seerrSearchCoordinator_.debounceDue(now)) searchSeerrAsync(false);
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

    void moveKeyboard(int dx, int dy) {
        const auto& rows = keyboardRows();
        if (dy != 0) {
            keyboardRow_ = std::clamp(keyboardRow_ + dy, 0, static_cast<int>(rows.size()) - 1);
            keyboardCol_ =
                std::clamp(keyboardCol_, 0, static_cast<int>(rows[static_cast<size_t>(keyboardRow_)].size()) - 1);
        }
        if (dx != 0) {
            const int columns = static_cast<int>(rows[static_cast<size_t>(keyboardRow_)].size());
            keyboardCol_ = wrappedIndex(keyboardCol_, dx, columns);
        }
    }

    void activateKeyboardKey(bool forSearch) {
        const auto& rows = keyboardRows();
        const auto& key = rows[static_cast<size_t>(keyboardRow_)][static_cast<size_t>(keyboardCol_)];
        if (forSearch) {
            switch (key.action) {
            case KeyAction::Insert:
                for (char value : key.value) searchState_.append(value);
                scheduleLiveSearch();
                break;
            case KeyAction::Backspace:
                if (searchState_.backspace()) scheduleLiveSearch();
                break;
            case KeyAction::Done:
                searchState_.setKeyboard(false);
                searchAsync();
                break;
            }
            return;
        }
        if (accountState_.loginFocus() < 0 || accountState_.loginFocus() >= 3) return;
        switch (key.action) {
        case KeyAction::Insert:
            for (char value : key.value) accountState_.appendToFocusedField(value);
            break;
        case KeyAction::Backspace:
            accountState_.backspaceFocusedField();
            break;
        case KeyAction::Done:
            accountState_.setKeyboardActive(false);
            break;
        }
    }

    void handleLoginKey(int32_t key) {
        const AccountNavigationAction navigation = AccountNavigationController::handleLogin(
            accountState_, screenNavigationKeyForAndroidKey(key), !sessionRegistry_.empty());
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
            moveKeyboard(navigation.dx, navigation.dy);
            return;
        case AccountNavigationActionType::ActivateKeyboard:
            activateKeyboardKey(false);
            return;
        case AccountNavigationActionType::EditField: {
            const int field = navigation.index;
            const int mode = kTextInputLoginServer + field;
            static constexpr std::array<const char*, 3> hints{"Jellyfin server URL", "Jellyfin username",
                                                              "Jellyfin password"};
            accountState_.setKeyboardActive(!showSystemTextInput(accountState_.field(field),
                                                                 hints[static_cast<size_t>(field)], mode,
                                                                 field == AccountScreenState::kPasswordField));
            if (accountState_.keyboardActive()) keyboardRow_ = keyboardCol_ = 0;
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
        const AccountNavigationAction navigation = AccountNavigationController::handleProfiles(
            accountState_, screenNavigationKeyForAndroidKey(key), static_cast<int>(sessionRegistry_.size()));
        switch (navigation.type) {
        case AccountNavigationActionType::ProfilesBack:
            popScreen(Screen::Login);
            return;
        case AccountNavigationActionType::AddAccount:
            startAddAccount();
            return;
        case AccountNavigationActionType::SwitchSession:
            switchSavedSession(static_cast<size_t>(navigation.index));
            return;
        case AccountNavigationActionType::ForgetSession:
            forgetSavedSession(static_cast<size_t>(navigation.index));
            return;
        default:
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
            SearchNavigationController::handle(searchState_, screenNavigationKeyForAndroidKey(key), columns);
        const auto& results = searchState_.results();
        if (!searchState_.keyboard() && searchState_.selection() >= 0 &&
            searchState_.selection() < static_cast<int>(results.size())) {
            scheduleSimilarPrefetch(results[static_cast<size_t>(searchState_.selection())]);
        }
        switch (navigation.type) {
        case SearchNavigationActionType::None:
            return;
        case SearchNavigationActionType::Exit:
            seerrSearchCoordinator_.cancel();
            requestEpochs_.search.invalidate();
            requestEpochs_.seerrSearch.invalidate();
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
            moveKeyboard(navigation.dx, navigation.dy);
            return;
        case SearchNavigationActionType::ActivateKeyboard:
            activateKeyboardKey(true);
            return;
        case SearchNavigationActionType::OpenTextInput:
            searchState_.setKeyboard(
                !showSystemTextInput(searchState_.query(), "Search Jellyfin & Seerr", kTextInputSearch));
            if (searchState_.keyboard()) keyboardRow_ = keyboardCol_ = 0;
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

    void refreshExternalPlayers() {
        externalPlayers_ = externalPlayer_.availablePlayers();
        SettingsActionController::reconcileExternalPlayer(settings_, externalPlayers_);
    }

    std::string externalPlayerLabel() const {
        return SettingsActionController::externalPlayerLabel(settings_, externalPlayers_);
    }

    std::optional<ExternalPlayerApp> selectedExternalPlayer() const {
        return SettingsActionController::selectedExternalPlayer(settings_, externalPlayers_);
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
            showSystemTextInput(settingsScreen_.searchQuery(), "Search settings", kTextInputSettingsSearch);
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
        const SettingsNavigationAction navigation =
            SettingsNavigationController::handle(settingsScreen_, settings_, screenNavigationKeyForAndroidKey(key));
        applySettingsActionEffects(
            SettingsActionController::apply(navigation, settingsScreen_, settings_, externalPlayers_));
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
        return detailsFlow_.itemMenuActions(isSeerrItem(detailsFlow_.item()), selectedExternalPlayer().has_value(),
                                            !queueState_.empty(), isHiddenFromHome(detailsFlow_.item()));
    }

    void openItemMenu() {
        if (!detailsFlow_.beginItemMenu()) return;
        pushScreen(Screen::ItemMenu);
        error_.clear();
    }

    void handleItemMenuKey(int32_t key) {
        applyDetailsFlowCommand(
            detailsFlow_.handleItemMenu(detailsNavigationKeyForAndroidKey(key), isSeerrItem(detailsFlow_.item()),
                                        selectedExternalPlayer().has_value(), !queueState_.empty()));
    }

    void launchExternalPlaybackAsync() {
        if (loading_ || !session_.valid() || detailsFlow_.item().id.empty()) return;
        const auto player = selectedExternalPlayer();
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

    void cycleAudioTrack() {
        playbackRuntime_.cycleAudioTrack(playbackStreamAsync_, playbackTelemetryAsync_, screen_ == Screen::Player);
    }

    void loadSubtitleAsync(const JellyfinSubtitleStream& subtitle, const std::string& deliveryUrl = {}) {
        if (!playbackRuntime_.loadSubtitle(subtitleLoadAsync_, subtitle, deliveryUrl))
            showNotice("SUBTITLES COULD NOT BE STARTED");
    }

    void cycleSubtitleTrack() {
        if (playbackRuntime_.cycleSubtitleTrack(subtitleLoadAsync_, playbackStreamAsync_, playbackTelemetryAsync_,
                                                screen_ == Screen::Player) ==
            PlaybackRuntimeNotice::SubtitleStartFailed)
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

    void handlePlayerKey(int32_t key, int repeatCount = 0) {
        PlayerScreenInput input = PlayerScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = PlayerScreenInput::Back;
        else if (key == AKEYCODE_DPAD_UP)
            input = PlayerScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = PlayerScreenInput::Down;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = PlayerScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = PlayerScreenInput::Right;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = PlayerScreenInput::Activate;
        else if (key == AKEYCODE_MEDIA_PLAY_PAUSE)
            input = PlayerScreenInput::PlayPause;
        else if (key == AKEYCODE_MEDIA_PREVIOUS)
            input = PlayerScreenInput::Previous;
        else if (key == AKEYCODE_MEDIA_NEXT)
            input = PlayerScreenInput::Next;
        else if (key == AKEYCODE_MEDIA_REWIND)
            input = PlayerScreenInput::Rewind;
        else if (key == AKEYCODE_MEDIA_FAST_FORWARD)
            input = PlayerScreenInput::FastForward;

        const auto now = std::chrono::steady_clock::now();
        const PlayerScreenCommand command = playerScreenState_.handleInput(input, now);
        switch (command.type) {
        case PlayerScreenCommandType::None:
            return;
        case PlayerScreenCommandType::StopPlayback:
            stopPlayback();
            return;
        case PlayerScreenCommandType::OpenQueue:
            openQueueOverlay();
            return;
        case PlayerScreenCommandType::PreviousEpisode:
            playAdjacentEpisode(-1);
            return;
        case PlayerScreenCommandType::NextEpisode:
            playAdjacentEpisode(1);
            return;
        case PlayerScreenCommandType::ActivatePlayback:
            if (playbackRuntime_.skipActiveMediaSegment(playbackTelemetryAsync_, screen_ == Screen::Player)) return;
            [[fallthrough]];
        case PlayerScreenCommandType::TogglePause:
            player_.togglePause();
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, true);
            return;
        case PlayerScreenCommandType::CycleAudioTrack:
            cycleAudioTrack();
            return;
        case PlayerScreenCommandType::CycleSubtitleTrack:
            cycleSubtitleTrack();
            return;
        case PlayerScreenCommandType::SeekBackward:
        case PlayerScreenCommandType::SeekForward: {
            const bool forward = command.type == PlayerScreenCommandType::SeekForward;
            const int64_t deltaMs =
                heldSeekDeltaMs(forward ? settings_.seekForwardSeconds : settings_.seekBackSeconds, repeatCount);
            const int targetMs = relativeSeekPositionMs(playerScreenState_.positionMs(), forward ? deltaMs : -deltaMs,
                                                        playerScreenState_.durationMs());
            playerScreenState_.showSeekFeedback(static_cast<int>((forward ? deltaMs : -deltaMs) / 1000), now);
            requestTrickplayPreview(targetMs);
            playbackRuntime_.seekTo(targetMs);
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
            return;
        }
        }
    }

    void handleMediaSessionCommand(const MediaSessionCommand& command) {
        if (screen_ != Screen::Player) return;
        switch (command.type) {
        case MediaSessionCommandType::Play:
            player_.play();
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, true);
            break;
        case MediaSessionCommandType::Pause:
            player_.pause();
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, true);
            break;
        case MediaSessionCommandType::Stop:
            stopPlayback();
            break;
        case MediaSessionCommandType::SeekTo: {
            const int64_t maxPosition = playerScreenState_.durationMs() > 0
                                            ? playerScreenState_.durationMs()
                                            : static_cast<int64_t>(std::numeric_limits<int>::max());
            const int positionMs = static_cast<int>(std::clamp<int64_t>(command.positionMs, 0, maxPosition));
            requestTrickplayPreview(positionMs);
            playbackRuntime_.seekTo(positionMs);
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
            break;
        }
        case MediaSessionCommandType::Next:
            if (queueState_.currentIndex() >= 0) {
                const int next = queueState_.nextIndex(true);
                if (next >= 0) playQueuedIndexAsync(next);
            }
            break;
        case MediaSessionCommandType::Previous:
            if (queueState_.currentIndex() > 0) {
                playQueuedIndexAsync(queueState_.currentIndex() - 1);
            } else {
                requestTrickplayPreview(0);
                playbackRuntime_.seekTo(0);
                playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
            }
            break;
        }
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
        refreshExternalPlayers();
        pushScreen(Screen::Settings);
        settingsScreen_.reset();
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
        seerrSearchCoordinator_.reset();
        searchState_.reset();
        detailsFlow_.item() = {};
        detailsFlow_.state().reset();

        artwork_.clearSession(renderer_);
        accountState_.clearSessionUi();
        loading_ = false;
        homeLoading_ = false;
        homeRetryAt_ = {};
        homeRetryAttempt_ = 0;
        mutationLoading_ = false;
        playedRollback_.reset();
        error_.clear();
        statusOverlayState_.clearNotice();
        screensaverActive_ = false;
        lastInteraction_ = std::chrono::steady_clock::now();
    }

    void openProfiles() {
        if (sessionRegistry_.empty()) {
            startAddAccount();
            return;
        }
        pushScreen(Screen::Profiles);
        accountState_.beginProfiles(static_cast<int>(sessionRegistry_.size()));
        error_.clear();
    }

    void startAddAccount() {
        const std::string existingServer = session_.server;
        clearCurrentSessionUi();
        accountState_.beginAddAccount(existingServer);
        resetNavigation(Screen::Login);
        saveSession(session_);
    }

    void switchSavedSession(size_t index) {
        const JellyfinSession* saved = sessionRegistry_.at(index);
        if (!saved) return;
        clearCurrentSessionUi();
        session_ = *saved;
        session_.deviceId = deviceId_;
        accountState_.setAuthenticatedAccount(session_.server, session_.username);
        resetNavigation(Screen::Home);
        homeState_.setRow(0);
        homeState_.setFirstVisibleRow(0);
        saveSession(session_);
        loadHomeAsync();
    }

    void forgetSavedSession(size_t index) {
        const JellyfinSession* saved = sessionRegistry_.at(index);
        if (!saved) return;
        const JellyfinSession removed = *saved;
        const bool removedCurrent = SessionRegistry::sameIdentity(session_, removed);
        artwork_.eraseProfile(removed, renderer_);
        sessionRegistry_.eraseAt(index);
        if (removedCurrent) {
            clearCurrentSessionUi();
            resetNavigation(Screen::Profiles);
        }
        accountState_.beginProfiles(static_cast<int>(sessionRegistry_.size()));
        saveSession(session_);
        if (sessionRegistry_.empty()) startAddAccount();
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

    std::string hiddenHomeKey(const std::string& itemId) const {
        return session_.server + "\n" + session_.userId + "\n" + itemId;
    }

    bool isHiddenFromHome(const JellyfinItem& item) const {
        return !item.id.empty() && hiddenHomeItems_.contains(hiddenHomeKey(item.id));
    }

    void filterHiddenHomeItems(JellyfinHomeData& data) const {
        for (auto& row : data.rows) {
            if (row.title == "My Media") continue;
            std::erase_if(row.items, [&](const JellyfinItem& item) { return isHiddenFromHome(item); });
        }
    }

    void clampHomeSelections() {
        std::vector<int> itemCounts;
        itemCounts.reserve(home_.rows.size());
        for (const auto& row : home_.rows) itemCounts.push_back(static_cast<int>(row.items.size()));
        homeState_.clampSelections(itemCounts);
    }

    void toggleHiddenFromHome() {
        if (detailsFlow_.item().id.empty()) return;
        const std::string key = hiddenHomeKey(detailsFlow_.item().id);
        const bool hiding = !hiddenHomeItems_.contains(key);
        if (hiding)
            hiddenHomeItems_.insert(key);
        else
            hiddenHomeItems_.erase(key);
        filterHiddenHomeItems(home_);
        clampHomeSelections();
        saveSession(session_);
        showNotice(hiding ? "HIDDEN FROM HOME" : "HOME VISIBILITY RESTORED", 2s);
    }

    void restoreHomeVisibilityForPlayback(const JellyfinItem& item) {
        bool changed = false;
        auto restore = [&](const std::string& itemId) {
            if (!itemId.empty()) changed = hiddenHomeItems_.erase(hiddenHomeKey(itemId)) > 0 || changed;
        };
        restore(item.id);
        restore(item.seriesId);
        if (changed) saveSession(session_);
    }

    void toggleFavoriteAsync() {
        if (loading_ || mutationLoading_ || detailsFlow_.item().id.empty()) return;
        const bool desired = !detailsFlow_.item().favorite;
        const JellyfinSession session = session_;
        const JellyfinItem item = detailsFlow_.item();
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        itemMutationAsync_.setFavorite(session, item, desired, sessionEpoch);
    }

    void togglePlayedAsync() {
        if (loading_ || mutationLoading_ || detailsFlow_.item().id.empty()) return;
        const JellyfinSession session = session_;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        const bool hiddenFromHome = isHiddenFromHome(detailsFlow_.item());
        PlayedMutationPreparation preparation = ItemMutationController::preparePlayedToggle(
            home_, homeState_, browseState_, searchState_, detailsFlow_.state(), queueState_, detailsFlow_.item(),
            hiddenFromHome, sessionEpoch);
        mutationLoading_ = true;
        error_.clear();
        playedRollback_ = std::move(preparation.rollback);
        itemMutationAsync_.setPlayed(session, std::move(preparation.item), preparation.desired, sessionEpoch,
                                     preparation.nextUpReplacementIndex);
    }

    void refreshCurrentItemMetadataAsync() {
        if (loading_ || mutationLoading_ || detailsFlow_.item().id.empty()) return;
        const JellyfinSession session = session_;
        const std::string itemId = detailsFlow_.item().id;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        itemMutationAsync_.refreshMetadata(session, itemId, sessionEpoch);
    }

    void deleteSeerrRequestAsync() {
        const auto deleteRequest = seerrDeleteRequestFromJellyfinItem(detailsFlow_.item());
        if (loading_ || mutationLoading_ || !deleteRequest) {
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
        mutationLoading_ = true;
        error_.clear();
        seerrAsync_.deleteRequest(endpoint, request);
    }

    void deleteCurrentItemAsync() {
        if (loading_ || mutationLoading_ || detailsFlow_.item().id.empty() || !detailsFlow_.item().canDelete) return;
        const JellyfinSession session = session_;
        const std::string itemId = detailsFlow_.item().id;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        itemMutationAsync_.deleteItem(session, itemId, sessionEpoch);
    }

    void openSearch() {
        pushScreen(Screen::Search);
        searchState_.setKeyboard(
            !showSystemTextInput(searchState_.query(), "Search Jellyfin & Seerr", kTextInputSearch));
        keyboardRow_ = keyboardCol_ = 0;
        error_.clear();
    }

    void discoverServersAsync() {
        if (loading_) return;
        loading_ = true;
        error_.clear();
        accountState_.setDiscoveryStatus("SEARCHING LOCAL NETWORK...");
        const uint64_t generation = requestEpochs_.auth.begin();
        accountAsync_.discover(generation, 1600);
    }

    void loginAsync() {
        if (loading_) return;
        loading_ = true;
        error_.clear();
        const auto fields = accountState_.fields();
        const std::string deviceId = deviceId_;
        const uint64_t generation = requestEpochs_.auth.begin();
        accountAsync_.login(fields, deviceId, generation);
    }

    void quickConnectAsync() {
        if (loading_ || accountState_.quickConnectActive()) return;
        if (accountState_.field(AccountScreenState::kServerField).empty()) {
            error_ = "ENTER THE JELLYFIN SERVER ADDRESS FIRST";
            accountState_.setLoginFocus(AccountScreenState::kServerField);
            return;
        }

        const std::string server = accountState_.field(AccountScreenState::kServerField);
        const std::string deviceId = deviceId_;
        loading_ = true;
        accountState_.beginQuickConnect();
        error_.clear();
        const RequestEpoch::Token requestToken = requestEpochs_.auth.beginToken();

        quickConnectAsync_.connect(server, deviceId, requestToken);
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

        mutationLoading_ = true;
        seerrRequest_.submit(std::move(plan));
    }

    void searchSeerrAsync(bool immediate) {
        auto plan = immediate ? seerrSearchCoordinator_.prepareImmediate(seerrEndpoint(), searchState_.query())
                              : seerrSearchCoordinator_.prepareDue(seerrEndpoint(), searchState_.query(),
                                                                   std::chrono::steady_clock::now());
        if (plan.resultsChanged) searchState_.refreshSeerrResults();
        if (!plan.ready()) return;

        refreshSeerrStorageAsync();
        const uint64_t generation = requestEpochs_.seerrSearch.begin();
        seerrSearchCoordinator_.submit(std::move(plan), generation);
    }

    void searchAsync(bool includeSeerrImmediately = true) {
        if (searchState_.query().empty()) {
            seerrSearchCoordinator_.reset();
            searchState_.refreshSeerrResults();
        }
        if (!session_.valid() || !searchState_.beginSearch()) return;
        const JellyfinSession session = session_;
        const std::string query = searchState_.query();
        error_.clear();
        const uint64_t generation = requestEpochs_.search.begin();
        jellyfinSearchAsync_.search(session, query, generation);
        if (includeSeerrImmediately) searchSeerrAsync(true);
    }

    static std::string similarPrefetchKey(const JellyfinSession& session, const std::string& itemId) {
        return session.server + "\n" + session.userId + "\n" + itemId;
    }

    bool supportsSimilarPrefetch(const JellyfinItem& item) const {
        if (!session_.valid() || item.id.empty() || isSeerrItem(item)) return false;
        return item.type != "Folder" && item.type != "BoxSet" && item.type != "CollectionFolder" &&
               item.type != "Genre" && item.type != "Letter" && item.type != "Person";
    }

    std::optional<std::vector<JellyfinItem>> cachedSimilarPrefetch(const JellyfinSession& session,
                                                                   const std::string& itemId) {
        std::scoped_lock lock(stateMutex_);
        const std::string key = similarPrefetchKey(session, itemId);
        const auto found = similarPrefetchCache_.find(key);
        if (found == similarPrefetchCache_.end()) return std::nullopt;
        if (std::chrono::steady_clock::now() - found->second.loadedAt > 5min) {
            similarPrefetchCache_.erase(found);
            return std::nullopt;
        }
        return found->second.items;
    }

    void storeSimilarPrefetch(const JellyfinSession& session, const std::string& itemId,
                              std::vector<JellyfinItem> items) {
        std::scoped_lock lock(stateMutex_);
        const std::string key = similarPrefetchKey(session, itemId);
        if (!similarPrefetchCache_.contains(key) && similarPrefetchCache_.size() >= 8) {
            similarPrefetchCache_.erase(similarPrefetchCache_.begin());
        }
        SimilarPrefetchEntry entry{
            .items = std::move(items),
            .loadedAt = std::chrono::steady_clock::now(),
        };
        if (screen_ == Screen::Details && detailsFlow_.item().id == itemId && detailsFlow_.state().similar().empty()) {
            detailsFlow_.state().setSimilar(entry.items);
        }
        similarPrefetchCache_.insert_or_assign(key, std::move(entry));
    }

    void scheduleSimilarPrefetch(const JellyfinItem& item) {
        std::scoped_lock lock(stateMutex_);
        if (!supportsSimilarPrefetch(item)) {
            similarPrefetchCandidateId_.clear();
            similarPrefetchCandidateKey_.clear();
            similarPrefetchDue_ = {};
            return;
        }
        const std::string key = similarPrefetchKey(session_, item.id);
        const auto cached = similarPrefetchCache_.find(key);
        if (cached != similarPrefetchCache_.end() &&
            std::chrono::steady_clock::now() - cached->second.loadedAt <= 5min) {
            similarPrefetchCandidateId_.clear();
            similarPrefetchCandidateKey_.clear();
            similarPrefetchDue_ = {};
            return;
        }
        if (similarPrefetchInFlight_.contains(key)) return;
        similarPrefetchCandidateId_ = item.id;
        similarPrefetchCandidateKey_ = key;
        similarPrefetchDue_ = std::chrono::steady_clock::now() + 350ms;
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void runDueSimilarPrefetch() {
        JellyfinSession session;
        std::string itemId;
        std::string key;
        {
            std::scoped_lock lock(stateMutex_);
            const auto now = std::chrono::steady_clock::now();
            if (similarPrefetchDue_ == std::chrono::steady_clock::time_point{} || now < similarPrefetchDue_) return;
            if (screen_ != Screen::Home && screen_ != Screen::Browse && screen_ != Screen::Search &&
                screen_ != Screen::Details) {
                similarPrefetchCandidateId_.clear();
                similarPrefetchCandidateKey_.clear();
                similarPrefetchDue_ = {};
                return;
            }
            session = session_;
            itemId = similarPrefetchCandidateId_;
            key = similarPrefetchCandidateKey_;
            similarPrefetchCandidateId_.clear();
            similarPrefetchCandidateKey_.clear();
            similarPrefetchDue_ = {};
            if (!session.valid() || itemId.empty() || key != similarPrefetchKey(session, itemId) ||
                similarPrefetchInFlight_.contains(key)) {
                return;
            }
            const auto cached = similarPrefetchCache_.find(key);
            if (cached != similarPrefetchCache_.end() && now - cached->second.loadedAt <= 5min) return;
            similarPrefetchInFlight_.insert(key);
        }

        if (!similarPrefetchTasks_.submit([this, session, itemId, key] {
                auto result = similarPrefetchApi_.getSimilar(session, itemId, 18);
                asyncCompletions_.push(SimilarPrefetchCompletion{
                    .session = session,
                    .itemId = itemId,
                    .key = key,
                    .result = std::move(result),
                });
            })) {
            std::scoped_lock lock(stateMutex_);
            similarPrefetchInFlight_.erase(key);
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
        const auto prefetchedSimilar = cachedSimilarPrefetch(session, id);
        if (prefetchedSimilar) detailsFlow_.state().setSimilar(*prefetchedSimilar);
        similarPrefetchCandidateId_.clear();
        similarPrefetchCandidateKey_.clear();
        similarPrefetchDue_ = {};
        loading_ = true;
        error_.clear();
        const RequestEpoch::Token requestToken = requestEpochs_.content.beginToken();
        detailsAsync_.load(session, id, requestToken, !prefetchedSimilar.has_value());
    }

    void shuffleRemainingQueue() {
        static thread_local std::mt19937 generator(std::random_device{}());
        if (!queueState_.shuffleRemaining(generator)) return;
        playbackCoordinator_.syncQueueContinuation(queueState_);
    }

    void openQueueOverlay() {
        if (!queueState_.openOverlay()) {
            error_.clear();
            return;
        }
        error_.clear();
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
    }

    void playQueuedIndexAsync(int index, bool restartCurrent = false, bool replacingCompleted = false) {
        if (loading_ || index < 0 || index >= queueState_.size() || !session_.valid()) return;
        if (index == queueState_.currentIndex() && screen_ == Screen::Player && !restartCurrent) {
            queueState_.closeOverlay();
            return;
        }

        const Screen originScreen = screen_;
        const bool replacingPlayer = screen_ == Screen::Player && playbackCoordinator_.activeItemAvailable();
        if (replacingPlayer) releaseActivePlayback(true, replacingCompleted);
        const int previousQueueIndex = queueState_.currentIndex();
        queueState_.closeOverlay();
        loading_ = true;
        playbackCoordinator_.beginPlaybackResolution(replacingPlayer);
        error_.clear();
        const JellyfinSession session = session_;
        JellyfinItem queued = *queueState_.itemAt(index);
        if (restartCurrent) queued.positionTicks = 0;
        PlaybackResolutionOptions resolutionOptions = playbackRuntime_.resolutionOptions();
        const uint64_t generation = requestEpochs_.playback.begin();
        playbackResolutionAsync_.resolveQueued(session, std::move(queued), std::move(resolutionOptions), generation,
                                               originScreen, index, previousQueueIndex, replacingPlayer);
    }

    void playPlayerItemAsync(JellyfinItem selected) {
        if (loading_ || screen_ != Screen::Player || !session_.valid() || selected.id.empty()) return;
        const JellyfinSession session = session_;
        PlaybackResolutionOptions resolutionOptions = playbackRuntime_.resolutionOptions();

        queueState_.reset();
        releaseActivePlayback(true, false);
        loading_ = true;
        playbackCoordinator_.beginPlaybackResolution(true);
        error_.clear();
        const uint64_t generation = requestEpochs_.playback.begin();
        playbackResolutionAsync_.resolvePlayerItem(session, std::move(selected), std::move(resolutionOptions),
                                                   generation);
    }

    void playAdjacentEpisode(int direction) {
        if (direction == 0 || loading_ || screen_ != Screen::Player) return;
        const int currentQueueIndex = queueState_.currentIndex();
        if (currentQueueIndex >= 0) {
            const int targetIndex = direction > 0 ? queueState_.nextIndex(true) : currentQueueIndex - 1;
            if (targetIndex >= 0 && targetIndex < queueState_.size()) {
                playQueuedIndexAsync(targetIndex);
                return;
            }
        }
        if (!session_.valid()) return;
        PlaybackAdjacentEpisodePlan plan = playbackCoordinator_.beginAdjacentEpisodePlan(direction);
        if (plan.nextItem) {
            playPlayerItemAsync(std::move(*plan.nextItem));
            return;
        }
        if (!plan.lookup) return;
        const JellyfinSession session = session_;
        const std::string currentItemId = plan.lookup->currentItemId;
        const std::string seriesId = plan.lookup->seriesId;
        const int currentSeason = plan.lookup->currentSeason;
        const int currentEpisode = plan.lookup->currentEpisode;
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 5s);
        playbackContinuationAsync_.requestAdjacentEpisode(session, seriesId, currentItemId, currentSeason,
                                                          currentEpisode, direction);
    }

    void handleQueueOverlayKey(int32_t key) {
        const QueueNavigationAction navigation =
            QueueNavigationController::handle(queueState_, screenNavigationKeyForAndroidKey(key));
        switch (navigation.type) {
        case QueueNavigationActionType::None:
            return;
        case QueueNavigationActionType::PlayIndex:
            playQueuedIndexAsync(navigation.index);
            return;
        case QueueNavigationActionType::QueueChanged:
            playbackCoordinator_.syncQueueContinuation(queueState_);
            return;
        case QueueNavigationActionType::Shuffle:
            shuffleRemainingQueue();
            return;
        }
    }

    void beginSeriesPlayAll() {
        if (loading_ || detailsFlow_.item().type != "Series" || detailsFlow_.item().id.empty() || !session_.valid())
            return;
        loading_ = true;
        error_.clear();
        playbackCoordinator_.beginUserPlayback(false);
        const JellyfinSession session = session_;
        const JellyfinItem series = detailsFlow_.item();
        SeriesPlayAllOptions options{
            .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
            .maxAudioChannels = settings_.maxAudioChannels,
            .overrides = playbackOverridesFor(settings_),
        };
        const uint64_t generation = requestEpochs_.playback.begin();
        seriesPlaybackAsync_.playAll(session, series, std::move(options), generation);
    }

    void beginPlayback() {
        if (loading_ || detailsFlow_.item().id.empty()) return;
        if (selectedExternalPlayer()) {
            launchExternalPlaybackAsync();
            return;
        }
        const PlaybackUserSelectionPlan selectionPlan =
            playbackCoordinator_.beginUserPlaybackSelection(queueState_, detailsFlow_.item().id);
        if (selectionPlan.resetQueue) queueState_.reset();
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem selected = detailsFlow_.item();
        PlaybackResolutionOptions resolutionOptions = playbackRuntime_.resolutionOptions();
        const uint64_t generation = requestEpochs_.playback.begin();

        playbackResolutionAsync_.resolveSelection(session, selected, std::move(resolutionOptions), generation,
                                                  selectionPlan.queuedPlaybackIndex);
    }

    void releaseActivePlayback(bool reportStop, bool completed = false) {
        requestEpochs_.playback.invalidate();
        if (player_.status() == PlayerStatus::Playing || player_.status() == PlayerStatus::Paused) {
            playbackRuntime_.refreshTelemetry(true);
        }
        const auto session = session_;
        const PlaybackReleaseContext release = playbackCoordinator_.releaseContext(
            reportStop, completed, session.valid(), playerScreenState_.positionMs());
        const PlaybackReleasePlan releasePlan = release.plan;
        const auto& item = release.item;
        const auto& target = release.target;
        if (!item.id.empty()) {
            JellyfinItem updated = item;
            updated.positionTicks = releasePlan.cachedPositionTicks;
            if (releasePlan.markPlayed) updated.played = true;
            ItemMutationController::updateCachedUserData(home_, homeState_, browseState_, searchState_,
                                                         detailsFlow_.state(), queueState_, updated,
                                                         isHiddenFromHome(updated));
            if (detailsFlow_.item().id == item.id) {
                detailsFlow_.item().played = updated.played;
                detailsFlow_.item().positionTicks = updated.positionTicks;
            }
        }
        player_.stop();
        videoSurface_.release();
        displayMode_.restore();
        mediaSession_.clear();
        clearTrickplayPreview();
        playbackCoordinator_.finishRelease();
        playerScreenState_.resetPosition();
        if (releasePlan.reportStop) {
            playbackTelemetryAsync_.reportStop(session, item, target, releasePlan.reportTicks);
        }
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
        accountState_.endQuickConnect();
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
            searchState_.setQuery(request.searchQuery);
            searchState_.setKeyboard(false);
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
        const auto& result = *work.externalResult;
        const auto& completed = *work.completedExternalPlayback;
        if (!result.success) {
            std::scoped_lock lock(stateMutex_);
            error_ = "EXTERNAL PLAYER REPORTED PLAYBACK FAILURE";
            return;
        }

        const bool playbackCompleted = result.completionKnown && result.completed;
        std::optional<int64_t> positionTicks;
        if (result.positionMs >= 0) {
            positionTicks = static_cast<int64_t>(result.positionMs) * 10000;
        } else if (playbackCompleted && completed.item.runtimeTicks > 0) {
            positionTicks = completed.item.runtimeTicks;
        }
        if (positionTicks || playbackCompleted) {
            JellyfinItem updated = completed.item;
            if (positionTicks) {
                const int64_t boundedTicks = completed.item.runtimeTicks > 0
                                                 ? std::clamp<int64_t>(*positionTicks, 0, completed.item.runtimeTicks)
                                                 : std::max<int64_t>(0, *positionTicks);
                positionTicks = boundedTicks;
                updated.positionTicks = boundedTicks;
            }
            if (playbackCompleted) {
                updated.played = true;
                updated.positionTicks = 0;
            }
            std::scoped_lock lock(stateMutex_);
            ItemMutationController::updateCachedUserData(home_, homeState_, browseState_, searchState_,
                                                         detailsFlow_.state(), queueState_, updated,
                                                         isHiddenFromHome(updated));
            if (detailsFlow_.item().id == completed.item.id) {
                detailsFlow_.item().played = updated.played;
                detailsFlow_.item().positionTicks = updated.positionTicks;
            }
        }
        const JellyfinSession reportSession = session_;
        externalPlaybackAsync_.reportStopped(reportSession, ExternalPlaybackReportRequest{
                                                                .itemId = completed.item.id,
                                                                .mediaSourceId = completed.item.mediaSourceId,
                                                                .positionTicks = positionTicks,
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
        PlayerStatus status = player_.status();
        if (status == PlayerStatus::Preparing) {
            mediaSession_.updateState(MediaSessionState::Buffering, playerScreenState_.positionMs());
            const auto now = std::chrono::steady_clock::now();
            const PlaybackPreparePlan preparePlan = playbackCoordinator_.preparePlan(now);
            if (preparePlan.timedOut) {
                std::scoped_lock lock(stateMutex_);
                __android_log_print(ANDROID_LOG_WARN, kTag, "Playback prepare timed out after %lld ms (%s)",
                                    static_cast<long long>(preparePlan.elapsedMs),
                                    preparePlan.transcoding ? "transcode" : "direct");
                if (preparePlan.retryWithTranscodeFallback &&
                    playbackRuntime_.retryWithFallback(playbackStreamAsync_, renderer_.ready())) {
                    error_.clear();
                    return;
                }
                error_ = "PLAYBACK TOOK TOO LONG TO START";
                stopPlayback();
                return;
            }
        } else {
            playbackCoordinator_.finishPreparing();
        }
        if (playbackCoordinator_.consumePauseAfterRestart(status == PlayerStatus::Playing)) {
            player_.togglePause();
            status = player_.status();
        }
        if (status == PlayerStatus::Error) {
            std::scoped_lock lock(stateMutex_);
            const std::string playerError = player_.error();
            if (playbackRuntime_.retryWithoutSubtitle(playbackStreamAsync_)) {
                error_.clear();
                return;
            }
            if (playbackRuntime_.retryWithFallback(playbackStreamAsync_, renderer_.ready())) {
                error_.clear();
                return;
            }
            error_ = playerError;
            stopPlayback();
            return;
        }
        const bool playbackEnded = status == PlayerStatus::Ended;
        if (!playbackEnded && status != PlayerStatus::Playing && status != PlayerStatus::Paused) return;
        if (playbackEnded && playerScreenState_.durationMs() > 0) {
            playerScreenState_.setPositionMs(playerScreenState_.durationMs());
        }

        if (!playbackEnded && playbackCoordinator_.activeTargetUsesDirectPlay()) {
            const int pendingSeekTargetMs = playerScreenState_.pendingSeekTargetMs();
            const int recoveryTargetMs =
                pendingSeekTargetMs >= 0 ? pendingSeekTargetMs : playerScreenState_.recentSeekTargetMs();
            if (recoveryTargetMs >= 0) {
                const auto now = std::chrono::steady_clock::now();
                const int observedPositionMs = player_.positionMs();
                const bool mediaSeekable = player_.seekable();
                const bool seekFailureMatured =
                    pendingSeekTargetMs >= 0 ? playerScreenState_.pendingSeekAppearsFailed(observedPositionMs, now)
                                             : playerScreenState_.recentSeekAppearsFailed(observedPositionMs, now);
                const bool failedSeek = shouldFallbackAfterUnseekableSeek(mediaSeekable, observedPositionMs,
                                                                          recoveryTargetMs, seekFailureMatured);
                if (failedSeek) {
                    std::scoped_lock lock(stateMutex_);
                    playerScreenState_.setPositionMs(recoveryTargetMs);
                    __android_log_print(
                        ANDROID_LOG_WARN, kTag,
                        "Direct-play seek failed target=%d observed=%d seekable=%d; using Jellyfin stream fallback",
                        recoveryTargetMs, observedPositionMs, mediaSeekable);
                    if (playbackRuntime_.retryWithFallback(playbackStreamAsync_, renderer_.ready(), true)) {
                        error_.clear();
                        return;
                    }
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const PlaybackTickPlan plan = playbackCoordinator_.consumeTickPlan(
            playbackEnded, status == PlayerStatus::Playing, playerScreenState_.positionMs(), now);
        if (plan.refreshTelemetry) {
            playbackRuntime_.refreshTelemetry();
            mediaSession_.updateState(status == PlayerStatus::Playing ? MediaSessionState::Playing
                                                                      : MediaSessionState::Paused,
                                      playerScreenState_.positionMs());
        }
        if (plan.requestMediaSegments) playbackRuntime_.requestMediaSegments(playbackContinuationAsync_);
        if (plan.reportPlaybackStart) {
            const PlaybackStartContext start =
                playbackCoordinator_.playbackStartContext(playerScreenState_.positionMs());
            playbackTelemetryAsync_.reportStart(session_, start.item, start.target, start.ticks);
        }
        if (plan.reportProgress)
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
        if (plan.requestNextEpisode) playbackRuntime_.requestNextEpisode(playbackContinuationAsync_, queueState_);

        const PlaybackContinuationPlan continuationPlan = playbackCoordinator_.continuationPlan(
            playbackEnded, playerScreenState_.positionMs(), playerScreenState_.durationMs(), queueState_,
            settings_.autoplayNext, settings_.stillWatchingAfter);
        switch (continuationPlan.action) {
        case PlaybackContinuationAction::None:
            return;
        case PlaybackContinuationAction::PlayQueueIndex:
            playQueuedIndexAsync(continuationPlan.queueIndex, continuationPlan.repeatCurrentQueueItem, true);
            return;
        case PlaybackContinuationAction::AutoplayNext:
            if (continuationPlan.nextItem) queueAutoplayNext(*continuationPlan.nextItem);
            return;
        case PlaybackContinuationAction::ShowStillWatching:
            if (continuationPlan.nextItem) showStillWatching(*continuationPlan.nextItem);
            return;
        case PlaybackContinuationAction::Stop:
            stopPlayback(true);
            if (continuationPlan.resetAutoplayChain) playbackCoordinator_.resetAutoplayChain();
            return;
        }
    }

    void applyAsyncCompletion(SystemTextInputEvent& event) {
        applySystemTextInputEffects(
            systemTextInputController_.apply(event, searchState_, settingsScreen_, settings_, accountState_));
    }

    void applyAsyncCompletion(const SeerrDeleteCompletion& completion) {
        mutationLoading_ = false;
        if (screen_ != Screen::ItemMenu || detailsFlow_.item().id != completion.request.itemId) return;
        const auto outcome =
            seerrRequest_.completeDelete(completion.endpoint, seerrEndpoint(), completion.request.itemId,
                                         completion.request.requestId, completion.result.ok);
        if (outcome == SeerrDomainState::MutationOutcome::StaleEndpoint) return;
        if (outcome == SeerrDomainState::MutationOutcome::Failed) {
            error_ = "SEERR DELETE: " + completion.result.error;
            detailsFlow_.state().setDeleteConfirmation(false);
            return;
        }
        searchState_.refreshSeerrResults();
        syncSeerrHomeRowLocked();
        detailsFlow_.item() = {};
        detailsFlow_.state().setDeleteConfirmation(false);
        popScreen(Screen::Home);
        if (screen_ != Screen::Home) resetNavigation(Screen::Home);
        showNotice("SEERR REQUEST DELETED", 4s);
        error_.clear();
    }

    void applyAsyncCompletion(const SeerrRequestCompletion& completion) {
        mutationLoading_ = false;
        const auto domainCompletion =
            seerrRequest_.complete(completion.endpoint, seerrEndpoint(), completion.requestedItem,
                                   completion.result.value, completion.result.ok, std::chrono::steady_clock::now());
        if (domainCompletion.outcome == SeerrDomainState::MutationOutcome::StaleEndpoint) return;
        if (domainCompletion.outcome == SeerrDomainState::MutationOutcome::Failed) {
            error_ = "SEERR REQUEST: " + completion.result.error;
            return;
        }

        searchState_.refreshSeerrResults();
        syncSeerrHomeRowLocked();
        showNotice("REQUEST SENT TO SEERR", 4s);
        error_.clear();
        refreshSeerrStorageAsync(true);
    }

    void applyAsyncCompletion(SeerrStorageRefreshCompletion& completion) {
        auto& result = completion.result;
        const auto plan =
            seerrRefresh_.completeStorage(completion.endpoint, seerrEndpoint(), result.ok, std::move(result.value),
                                          result.error, settings_.seerrSelectDrive, std::chrono::steady_clock::now());
        if (plan.domain.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint) return;
        if (plan.domain.outcome == SeerrDomainState::RefreshOutcome::Failed) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Seerr storage refresh failed: %s", result.error.c_str());
            if (plan.domain.reconnect) {
                connectSeerrAsync(false);
                return;
            }
            if (plan.domain.clearedPendingRequest) showNotice("SEERR STORAGE: " + result.error, 5s);
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Seerr storage refresh found %zu targets", plan.targetCount);
        if (plan.pendingRequest) openSeerrDrivePicker(*plan.pendingRequest);
    }

    void applyAsyncCompletion(SeerrPendingRefreshCompletion& completion) {
        auto& result = completion.result;
        const auto plan = seerrRefresh_.completePending(completion.endpoint, seerrEndpoint(), result.ok,
                                                        std::move(result.value), std::chrono::steady_clock::now());
        if (plan.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint) return;
        if (plan.outcome == SeerrDomainState::RefreshOutcome::Failed) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Seerr pending requests unavailable: %s", result.error.c_str());
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Seerr pending refresh found %zu requests", plan.pendingCount);
        if (screen_ == Screen::ItemMenu && isSeerrItem(detailsFlow_.item())) {
            if (const SeerrMediaItem* current = seerrDomain_.findPendingRequest(detailsFlow_.item().id)) {
                detailsFlow_.item() = jellyfinItemFromSeerrMedia(*current);
            }
        }
        syncSeerrHomeRowLocked();
    }

    void applySearchCompletionEffects(SearchCompletionEffects effects) {
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        if (effects.reconnectSeerr) connectSeerrAsync(false);
        if (effects.syncSeerrHome) syncSeerrHomeRowLocked();
    }

    void applyAsyncCompletion(SeerrSearchCompletion& completion) {
        applySearchCompletionEffects(
            SearchCompletionController::apply(completion, requestEpochs_.seerrSearch.active(completion.generation),
                                              screen_ == Screen::Search, !settings_.seerrSessionCookie.empty(),
                                              searchState_, seerrSearchCoordinator_, std::chrono::steady_clock::now()));
    }

    void applyAsyncCompletion(JellyfinSearchCompletion& completion) {
        applySearchCompletionEffects(SearchCompletionController::apply(
            completion, requestEpochs_.search.active(completion.generation), screen_ == Screen::Search, searchState_));
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

    void applyItemMutationCompletionEffects(ItemMutationCompletionEffects effects) {
        if (effects.finishLoading) mutationLoading_ = false;
        if (effects.cacheUpdate) {
            ItemMutationController::updateCachedUserData(home_, homeState_, browseState_, searchState_,
                                                         detailsFlow_.state(), queueState_, *effects.cacheUpdate,
                                                         isHiddenFromHome(*effects.cacheUpdate));
        }
        ItemMutationController::restorePlayedRollback(effects, home_, homeState_);
        if (effects.removeCachedItemId) {
            ItemMutationController::removeCachedItem(home_, browseState_, searchState_, detailsFlow_.state(),
                                                     *effects.removeCachedItemId);
        }
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.nextUpAnimation) {
            nextUpReplacementFadeIndex_ = effects.nextUpAnimation->index;
            nextUpReplacementFadeItemId_ = std::move(effects.nextUpAnimation->itemId);
            nextUpReplacementFadeStarted_ = std::chrono::steady_clock::now();
            renderBurstUntil_ = std::max(renderBurstUntil_, nextUpReplacementFadeStarted_ + 320ms);
            if (app_ && app_->looper) ALooper_wake(app_->looper);
        }
        if (effects.closeDeletedItem) {
            popScreen(Screen::Home);
            if (screen_ == Screen::Details) popScreen(Screen::Home);
        }
        if (effects.notice) showNotice(std::move(*effects.notice));
    }

    void applyAsyncCompletion(FavoriteCompletion& completion) {
        applyItemMutationCompletionEffects(ItemMutationController::apply(
            completion, requestEpochs_.session.active(completion.sessionEpoch),
            screen_ == Screen::Details || screen_ == Screen::ItemMenu, detailsFlow_.item()));
    }

    void applyAsyncCompletion(PlayedCompletion& completion) {
        const bool replacementHidden = completion.nextUpReplacement && isHiddenFromHome(*completion.nextUpReplacement);
        applyItemMutationCompletionEffects(
            ItemMutationController::apply(completion, requestEpochs_.session.active(completion.sessionEpoch),
                                          screen_ == Screen::Details || screen_ == Screen::ItemMenu, replacementHidden,
                                          playedRollback_, home_, homeState_, detailsFlow_.item()));
    }

    void applyAsyncCompletion(MetadataRefreshCompletion& completion) {
        applyItemMutationCompletionEffects(
            ItemMutationController::apply(completion, requestEpochs_.session.active(completion.sessionEpoch)));
    }

    void applyAsyncCompletion(DeleteItemCompletion& completion) {
        applyItemMutationCompletionEffects(
            ItemMutationController::apply(completion, requestEpochs_.session.active(completion.sessionEpoch),
                                          screen_ == Screen::ItemMenu, detailsFlow_.item(), detailsFlow_.state()));
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
        applyAccountCompletionEffects(AccountCompletionController::apply(
            completion, requestEpochs_.auth.active(completion.generation), accountState_));
    }

    void applyAsyncCompletion(LoginCompletion& completion) {
        applyAccountCompletionEffects(AccountCompletionController::apply(
            completion, requestEpochs_.auth.active(completion.generation), accountState_));
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
        similarPrefetchInFlight_.erase(completion.key);
        if (!completion.result.ok) return;
        storeSimilarPrefetch(completion.session, completion.itemId, std::move(completion.result.value));
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
        applyAccountCompletionEffects(AccountCompletionController::apply(
            completion, requestEpochs_.auth.active(completion.generation), accountState_));
    }

    void applyAsyncCompletion(QuickConnectFailedCompletion& completion) {
        applyAccountCompletionEffects(AccountCompletionController::apply(
            completion, requestEpochs_.auth.active(completion.generation), accountState_));
    }

    void applyAsyncCompletion(QuickConnectAuthenticatedCompletion& completion) {
        applyAccountCompletionEffects(AccountCompletionController::apply(
            completion, requestEpochs_.auth.active(completion.generation), accountState_));
    }

    void applyAsyncCompletion(QuickConnectTimedOutCompletion& completion) {
        applyAccountCompletionEffects(AccountCompletionController::apply(
            completion, requestEpochs_.auth.active(completion.generation), accountState_));
    }

    void applyAsyncCompletion(HomeCoreCompletion& completion) {
        const bool activeGeneration = requestEpochs_.home.active(completion.generation);
        if (!activeGeneration) return;
        if (completion.result.ok) filterHiddenHomeItems(completion.result.value);

        HomeCoreCompletionEffects effects = HomeCompletionController::apply(
            completion, activeGeneration, screen_ == Screen::Home, homeRetryAttempt_, home_, homeState_);
        if (effects.finishLoading) homeLoading_ = false;
        homeRetryAttempt_ = effects.nextRetryAttempt;
        if (effects.resetRetry) homeRetryAt_ = {};
        if (effects.retryDelaySeconds) {
            homeRetryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(*effects.retryDelaySeconds);
            __android_log_print(ANDROID_LOG_WARN, kTag, "Home load failed transiently; retrying in %d seconds: %s",
                                *effects.retryDelaySeconds, completion.result.error.c_str());
        }

        if (effects.sessionExpired) {
            const JellyfinSession expired = session_;
            artwork_.eraseProfile(expired, renderer_);
            sessionRegistry_.removeIdentity(expired);
            requestEpochs_.invalidateAll();
            loading_ = false;
            mutationLoading_ = false;
            playedRollback_.reset();
            searchState_.setLoading(false);
            session_.token.clear();
            session_.userId.clear();
            settings_.seerrSessionCookie.clear();
            seerrDomain_.invalidateStorageTargets();
            resetNavigation(Screen::Login);
            error_ = "SESSION EXPIRED - LOG IN AGAIN";
            saveSession(session_);
            return;
        }
        if (effects.updateVisibleError) error_ = std::move(effects.visibleError);
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
            searchState_.setQuery(std::move(pendingSearchQuery_));
            pendingSearchQuery_.clear();
            searchState_.setKeyboard(false);
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
        const bool activeGeneration = requestEpochs_.home.active(completion.generation);
        if (!activeGeneration) return;
        if (completion.result.ok) filterHiddenHomeItems(completion.result.value);

        HomeSecondaryCompletionEffects effects =
            HomeCompletionController::apply(completion, activeGeneration, screen_ == Screen::Home, home_, homeState_);
        if (effects.updateVisibleError) error_ = std::move(effects.visibleError);
        if (!completion.result.ok) return;
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

    void applyPlayerCompletionEffects(PlayerCompletionEffects effects) {
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
        if (effects.showSubtitleOverlay) playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
        if (effects.reportProgress)
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
        if (effects.playItem) playPlayerItemAsync(std::move(*effects.playItem));
    }

    void applyAsyncCompletion(SubtitleLoadCompletion& completion) {
        applyPlayerCompletionEffects(PlayerCompletionController::apply(
            completion, requestEpochs_.playback.active(completion.generation), playbackCoordinator_));
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
        applyPlayerCompletionEffects(
            PlayerCompletionController::apply(completion, screen_ == Screen::Player, playbackCoordinator_));
    }

    void applyAsyncCompletion(NextEpisodeCompletion& completion) {
        applyPlayerCompletionEffects(
            PlayerCompletionController::apply(completion, screen_ == Screen::Player, playbackCoordinator_));
    }

    void applyAsyncCompletion(PlaybackAdjacentCompletion& completion) {
        applyPlayerCompletionEffects(
            PlayerCompletionController::apply(completion, screen_ == Screen::Player, playbackCoordinator_));
    }

    void applyAsyncCompletion(const PlaybackReportCompletion& completion) {
        logPlaybackReportFailure(PlayerCompletionController::reportStage(completion.kind), completion.itemId,
                                 completion.result);
        PlayerCompletionController::apply(completion, session_.server, session_.userId, screen_ == Screen::Player,
                                          playbackCoordinator_);
    }

    void applyPlaybackCompletionEffects(PlaybackCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.popToDetails) popScreen(Screen::Details);
        if (effects.detailUpdate) detailsFlow_.item() = std::move(*effects.detailUpdate);
        if (effects.error) error_ = std::move(*effects.error);

        for (const auto& restore : effects.restoreHomeVisibility) {
            JellyfinItem item;
            item.id = restore.itemId;
            item.seriesId = restore.seriesId;
            restoreHomeVisibilityForPlayback(item);
        }

        if (effects.stopPlayback) stopPlayback();
        if (!effects.transition) return;

        auto transition = std::move(*effects.transition);
        switch (transition.kind) {
        case PlaybackCompletionTransitionKind::Resolved:
            playbackCoordinator_.stageResolvedPlayback(std::move(transition.target), std::move(transition.item));
            break;
        case PlaybackCompletionTransitionKind::StreamRestart:
            playbackCoordinator_.stageStreamRestart(std::move(transition.target), std::move(transition.item),
                                                    transition.restartPaused, transition.audioStreamIndex);
            break;
        case PlaybackCompletionTransitionKind::ResolvedFallback:
            playbackCoordinator_.stageResolvedFallback(std::move(transition.target), std::move(transition.item),
                                                       transition.audioStreamIndex);
            break;
        }
    }

    void applyAsyncCompletion(QueuedPlaybackCompletion& completion) {
        const bool activeQueueContext = screen_ == completion.originScreen &&
                                        queueState_.currentIndex() == completion.previousQueueIndex &&
                                        queueState_.itemMatches(completion.index, completion.item.id);
        applyPlaybackCompletionEffects(PlaybackCompletionController::apply(
            completion, requestEpochs_.playback.active(completion.generation), activeQueueContext,
            screen_ == Screen::Player, queueState_, playbackCoordinator_));
    }

    void applyAsyncCompletion(PlayerItemPlaybackCompletion& completion) {
        applyPlaybackCompletionEffects(
            PlaybackCompletionController::apply(completion, requestEpochs_.playback.active(completion.generation),
                                                screen_ == Screen::Player, playbackCoordinator_));
    }

    void applyAsyncCompletion(AutoplayPlaybackCompletion& completion) {
        applyPlaybackCompletionEffects(PlaybackCompletionController::apply(
            completion, requestEpochs_.playback.active(completion.generation), queueState_, playbackCoordinator_));
    }

    void applyAsyncCompletion(StreamRestartCompletion& completion) {
        applyPlaybackCompletionEffects(
            PlaybackCompletionController::apply(completion, requestEpochs_.playback.active(completion.generation),
                                                screen_ == Screen::Player, playbackCoordinator_));
    }

    void applyAsyncCompletion(FallbackPlaybackCompletion& completion) {
        applyPlaybackCompletionEffects(
            PlaybackCompletionController::apply(completion, requestEpochs_.playback.active(completion.generation),
                                                screen_ == Screen::Player, playbackCoordinator_));
    }

    void applyAsyncCompletion(BeginPlaybackCompletion& completion) {
        const bool activeDetailsSelection =
            screen_ == Screen::Details && detailsFlow_.item().id == completion.selected.id;
        applyPlaybackCompletionEffects(PlaybackCompletionController::apply(
            completion, requestEpochs_.playback.active(completion.generation), activeDetailsSelection, queueState_));
    }

    void applyAsyncCompletion(SeriesPlayAllCompletion& completion) {
        const bool activeDetailsSelection =
            screen_ == Screen::Details && detailsFlow_.item().id == completion.series.id;
        applyPlaybackCompletionEffects(PlaybackCompletionController::apply(
            completion, requestEpochs_.playback.active(completion.generation), activeDetailsSelection, queueState_));
    }

    void applyAsyncCompletion(SeerrConnectCompletion& completion) {
        auto& result = completion.result;
        auto plan = seerrConnection_.complete(
            result.ok, result.failedStage == SeerrQuickConnectStage::AuthenticateSeerr, completion.server,
            completion.jellyfinUserId, settings_.seerrServer, session_.userId);
        const auto action = plan.action;
        if (action == SeerrDomainState::ConnectCompletionAction::PreAuthenticationFailed) {
            if (completion.announce) {
                statusOverlayState_.clearNotice();
                error_ = (result.failedStage == SeerrQuickConnectStage::AuthorizeJellyfin ? "JELLYFIN QUICK CONNECT: "
                                                                                          : "SEERR QUICK CONNECT: ") +
                         result.error;
            } else if (result.failedStage == SeerrQuickConnectStage::AuthorizeJellyfin) {
                __android_log_print(ANDROID_LOG_WARN, kTag, "Silent Jellyfin Quick Connect authorization failed: %s",
                                    result.error.c_str());
            } else {
                __android_log_print(ANDROID_LOG_WARN, kTag, "Silent Seerr reconnect failed: %s", result.error.c_str());
            }
            return;
        }
        if (completion.announce) statusOverlayState_.clearNotice();
        if (action == SeerrDomainState::ConnectCompletionAction::Stale) return;
        if (action == SeerrDomainState::ConnectCompletionAction::AuthenticationFailed) {
            if (completion.announce)
                error_ = "SEERR QUICK CONNECT: " + result.error;
            else
                __android_log_print(ANDROID_LOG_WARN, kTag, "Silent Seerr authentication failed: %s",
                                    result.error.c_str());
            return;
        }
        settings_.seerrSessionCookie = std::move(result.sessionCookie);
        saveSession(session_);
        if (completion.announce) {
            error_.clear();
            showNotice("SEERR CONNECTED WITH JELLYFIN", 4s);
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Seerr session refreshed");
        refreshSeerrPendingAsync();
        refreshSeerrStorageAsync(true);
        if (plan.deferred.request) requestSeerrMediaAsync(*plan.deferred.request);
        if (plan.deferred.retrySearch && screen_ == Screen::Search && !searchState_.query().empty()) {
            if (seerrSearchCoordinator_.prepareReconnectRetry(searchState_.query(), std::chrono::steady_clock::now())) {
                searchState_.refreshSeerrResults();
            }
            searchSeerrAsync(true);
        }
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
        if (screensaverActive_) {
            renderer_.setUiTransform(0.0f, 1.0f);
            renderScreensaver();
            renderer_.endFrame();
            return;
        }
        renderer_.setUiTransform(uiSafeAreaFraction(settings_.safeAreaPercent), uiTextScale(settings_.uiTextSize));
        uiPresentation_.beginFrame(session_, settings_, lastInteraction_,
                                   screen_ == Screen::Home && homeState_.centerPending());
        auto renderScreen = [&](Screen target) {
            switch (target) {
            case Screen::Login:
                renderLogin();
                break;
            case Screen::Profiles:
                renderProfiles();
                break;
            case Screen::Home:
                renderHome();
                break;
            case Screen::Browse:
                renderBrowse();
                break;
            case Screen::Search:
                renderSearch();
                break;
            case Screen::Settings:
                renderSettings();
                break;
            case Screen::Diagnostics:
                renderDiagnostics();
                break;
            case Screen::Details:
                renderDetails();
                break;
            case Screen::Cast:
                renderCast();
                break;
            case Screen::PersonItems:
                renderPersonItems();
                break;
            case Screen::ItemMenu:
                renderDetails();
                break;
            case Screen::Seasons:
                renderSeasons();
                break;
            case Screen::Episodes:
                renderEpisodes();
                break;
            case Screen::SeerrDrivePicker:
                renderSeerrDrivePicker();
                break;
            case Screen::Player:
                renderPlayer();
                break;
            }
        };
        if (screen_ == Screen::ItemMenu) {
            renderScreen(navigation_.previousOr(Screen::Details));
            renderItemMenu();
        } else {
            renderScreen(screen_);
        }
        if (queueState_.overlayActive()) renderQueueOverlay();
        renderStatus();
        renderer_.endFrame();
    }

    void renderLogin() {
        renderLoginPresentation(renderer_, uiPresentation_, accountState_, loading_,
                                static_cast<int>(sessionRegistry_.size()), Renderer::logicalWidth(),
                                Renderer::logicalHeight(), [this](float top) { renderKeyboard(top); });
    }

    void renderProfiles() {
        renderProfilesPresentation(renderer_, uiPresentation_, static_cast<int>(sessionRegistry_.size()),
                                   accountState_.profileSelection(), accountState_.profileAction(),
                                   [this](int index) { return sessionRegistry_.at(static_cast<size_t>(index)); });
    }

    void renderKeyboard(float top) {
        renderKeyboardPresentation(renderer_, uiPresentation_, keyboardRows(), keyboardRow_, keyboardCol_, top,
                                   Renderer::logicalHeight());
    }

    void loadBundledBrandMark() {
        AAssetManager* manager = app_ && app_->activity ? app_->activity->assetManager : nullptr;
        if (!manager) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Brand mark asset manager unavailable");
            return;
        }
        AAsset* asset = AAssetManager_open(manager, "sloppatv_brand_mark.png", AASSET_MODE_BUFFER);
        if (!asset) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Brand mark asset unavailable");
            return;
        }
        const off_t length = AAsset_getLength(asset);
        std::string encoded(length > 0 ? static_cast<size_t>(length) : 0, '\0');
        const int bytesRead = encoded.empty() ? 0 : AAsset_read(asset, encoded.data(), encoded.size());
        AAsset_close(asset);
        if (bytesRead <= 0) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Brand mark asset was empty");
            return;
        }
        encoded.resize(static_cast<size_t>(bytesRead));
        std::string decodeError;
        brandMarkDecoded_ = imageDecoder_.decode(encoded, decodeError);
        if (!brandMarkDecoded_.valid()) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Brand mark decode failed: %s", decodeError.c_str());
        }
    }

    bool drawBrandMark(float x, float y, float size) {
        if (!brandMarkDecoded_.valid() || !renderer_.ready()) return false;
        if (brandMarkTextureGeneration_ != renderer_.generation()) {
            brandMarkTexture_ = 0;
            brandMarkTextureGeneration_ = renderer_.generation();
        }
        if (brandMarkTexture_ == 0) {
            brandMarkTexture_ = renderer_.createTexture(brandMarkDecoded_.width, brandMarkDecoded_.height,
                                                        brandMarkDecoded_.rgba.data());
        }
        if (brandMarkTexture_ == 0) return false;
        renderer_.image(brandMarkTexture_, x, y, size, size);
        return true;
    }

    void renderHome() {
        renderHomePresentation(
            renderer_, uiPresentation_, home_.rows, homeState_, session_, settings_, homeLoading_, homeSlideFromFirst_,
            homeSlideToFirst_, homeSlideStarted_, Renderer::logicalWidth(), Renderer::logicalHeight(),
            [this](float x, float y, float size) { return drawBrandMark(x, y, size); },
            [this](std::string_view title, const std::vector<JellyfinItem>& items, int row, float top) {
                renderHomeRow(std::string(title), items, row, top);
            });
    }

    void renderHomeRow(const std::string& title, const std::vector<JellyfinItem>& items, int row, float top) {
        renderHomeRowContent(
            renderer_, title, items, row, top, homeState_, settings_.uiTextSize,
            HomeRowFadeState{
                .itemIndex = nextUpReplacementFadeIndex_,
                .itemId = nextUpReplacementFadeItemId_,
                .started = nextUpReplacementFadeStarted_,
                .now = std::chrono::steady_clock::now(),
            },
            HomeRowRenderStyle<Color>{
                .cornerSmall = material_tv::cornerSmall,
                .cardFocusScale = materialCardFocusScale(),
                .text = kText,
                .secondaryText = kSecondaryText,
                .muted = kMuted,
                .track = kTrack,
                .focus = kFocus,
            },
            [this](float x, float y, float width, float height, bool focused, float focusScale) {
                return uiPresentation_.focusedBounds(x, y, width, height, focused, focusScale);
            },
            [this](const JellyfinItem& item, float x, float y, float width, float height, float radius, float alpha) {
                return uiPresentation_.drawHomeArtwork(item, x, y, width, height, radius, alpha);
            },
            [this](const JellyfinItem& item, float x, float y, float width, float height, float radius, float alpha) {
                uiPresentation_.drawArtworkPlaceholder(item, x, y, width, height, radius, alpha);
            },
            [this](float x, float y, float width, float height, Color color, float radius) {
                uiPresentation_.drawFocusHalo(x, y, width, height, color, radius);
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return uiPresentation_.fitTextLines(value, scale, maxWidth, maxLines);
            },
            [this](float x, float y, const std::string& label, bool selected, float scale, float height,
                   float maxWidth) { return uiPresentation_.drawChip(x, y, label, selected, scale, height, maxWidth); },
            [](const JellyfinItem& item) { return isSeerrItem(item); },
            [](const JellyfinItem& item) { return episodeNumberLabel(item); });
    }

    MediaCardRenderStyle<Color> mediaCardStyle() const {
        return MediaCardRenderStyle<Color>{
            .canvasHeight = Renderer::logicalHeight(),
            .cornerSmall = material_tv::cornerSmall,
            .cornerMedium = material_tv::cornerMedium,
            .cardFocusScale = materialCardFocusScale(),
            .labelScale = material_tv::type::label,
            .panel = kPanel,
            .panelAlt = kPanelAlt,
            .panelElevated = kPanelElevated,
            .tertiary = kTertiary,
            .text = kText,
            .muted = kMuted,
            .track = kTrack,
            .focus = kFocus,
            .focusSoft = kFocusSoft,
            .outline = kOutline,
        };
    }

    void renderMediaArtworkCard(const JellyfinItem& item, float x, float y, float slotWidth, bool focused,
                                bool showState = true, bool preferSeriesCover = false, bool alignToPortraitBand = false,
                                int titleLineLimit = 0) {
        renderMediaArtworkCardContent(
            renderer_, item, x, y, slotWidth, focused,
            MediaArtworkCardRenderOptions{
                .showState = showState,
                .preferSeriesCover = preferSeriesCover,
                .alignToPortraitBand = alignToPortraitBand,
                .titleLineLimit = titleLineLimit,
                .uiTextSize = settings_.uiTextSize,
                .showWatchedIndicators = settings_.showWatchedIndicators,
            },
            mediaCardStyle(),
            [this](float imageX, float imageY, float imageWidth, float imageHeight, bool isFocused, float focusScale) {
                return uiPresentation_.focusedBounds(imageX, imageY, imageWidth, imageHeight, isFocused, focusScale);
            },
            [this](const JellyfinItem& source, bool seriesCoverForEpisode, bool landscape, float imageX, float imageY,
                   float imageWidth, float imageHeight, float radius) {
                JellyfinItem cover = source;
                if (seriesCoverForEpisode) {
                    cover.id = source.seriesId;
                    cover.imageTag = source.seriesPrimaryImageTag;
                    cover.type = "Series";
                }
                return landscape
                           ? uiPresentation_.drawHomeArtwork(cover, imageX, imageY, imageWidth, imageHeight, radius)
                           : uiPresentation_.drawArtwork(cover, imageX, imageY, imageWidth, imageHeight, 1.0f, radius);
            },
            [this](const JellyfinItem& source, float imageX, float imageY, float imageWidth, float imageHeight,
                   float radius) {
                uiPresentation_.drawArtworkPlaceholder(source, imageX, imageY, imageWidth, imageHeight, radius);
            },
            [this](float imageX, float imageY, float imageWidth, float imageHeight, Color color, float radius) {
                uiPresentation_.drawFocusHalo(imageX, imageY, imageWidth, imageHeight, color, radius);
            },
            [this](float centeredX, float centeredY, float centeredWidth, float centeredHeight, float scale,
                   std::string_view value, Color color, float horizontalPadding, float verticalPadding) {
                uiPresentation_.drawCenteredSingleLineFit(centeredX, centeredY, centeredWidth, centeredHeight, scale,
                                                          value, color, horizontalPadding, verticalPadding);
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return uiPresentation_.fitTextLines(value, scale, maxWidth, maxLines);
            },
            [](const JellyfinItem& source) {
                return isSeerrItem(source)
                           ? (source.externalRequested ? source.externalStatus : std::string("Press OK to request"))
                           : episodeLabel(source);
            });
    }

    void renderTextTile(const JellyfinItem& item, float x, float y, float width, float height, bool focused) {
        renderMediaTextTileContent(
            renderer_, item, x, y, width, height, focused, mediaCardStyle(),
            [this](float tileX, float tileY, float tileWidth, float tileHeight, bool isFocused, float focusScale) {
                return uiPresentation_.focusedBounds(tileX, tileY, tileWidth, tileHeight, isFocused, focusScale);
            },
            [this](float centeredX, float centeredY, float centeredWidth, float centeredHeight, float scale,
                   std::string_view value, Color color, float horizontalPadding, float verticalPadding) {
                uiPresentation_.drawCenteredSingleLineFit(centeredX, centeredY, centeredWidth, centeredHeight, scale,
                                                          value, color, horizontalPadding, verticalPadding);
            },
            [this](float tileX, float tileY, float tileWidth, float tileHeight, Color color, float radius) {
                uiPresentation_.drawFocusHalo(tileX, tileY, tileWidth, tileHeight, color, radius);
            });
    }

    void renderBrowse() {
        renderBrowsePresentation(
            renderer_, uiPresentation_, browseState_, loading_,
            [this](const JellyfinItem& item, float x, float y, float width, float height, bool focused) {
                renderTextTile(item, x, y, width, height, focused);
            },
            [this](const JellyfinItem& item, float x, float y, float width, bool focused, bool showState,
                   bool seriesCoverForEpisode, bool alignMixedHeights, int titleLineLimit) {
                renderMediaArtworkCard(item, x, y, width, focused, showState, seriesCoverForEpisode, alignMixedHeights,
                                       titleLineLimit);
            });
    }

    void renderSearch() {
        renderSearchPresentation(
            renderer_, uiPresentation_, searchState_, SeerrClient::configured(settings_.seerrServer, seerrAuth()),
            systemTextInputController_.mode() == kTextInputSearch, seerrDomain_.searchLoading(),
            seerrDomain_.searchError(), seerrDomain_.storageLoading(), seerrDomain_.storageTargets(),
            [this](float top) { renderKeyboard(top); },
            [this](float x, float y, float scale, std::string_view value, float maxWidth, Color color,
                   std::chrono::steady_clock::time_point now) {
                drawLingeringTitle(x, y, scale, value, maxWidth, color, now);
            },
            [this](const JellyfinItem& item, float x, float y, float width, bool focused, bool showState,
                   bool seriesCoverForEpisode, bool alignMixedHeights, int titleLineLimit) {
                renderMediaArtworkCard(item, x, y, width, focused, showState, seriesCoverForEpisode, alignMixedHeights,
                                       titleLineLimit);
            });
    }

    void renderSeerrDrivePicker() {
        const SeerrDrivePickerViewModel model =
            seerrDrivePickerViewModel(seerrDomain_.pendingStorageRequest(), seerrDomain_.storageDriveChoices(),
                                      seerrDomain_.storageDriveSelection());
        renderSeerrDrivePickerScreen(
            renderer_, model,
            SeerrDrivePickerRenderStyle<Color>{
                .headlineScale = material_tv::type::headline,
                .cornerMedium = material_tv::cornerMedium,
                .focusScale = materialWideListItemFocusScale(),
                .text = kText,
                .muted = kMuted,
                .secondaryText = kSecondaryText,
                .focus = kFocus,
                .focusSoft = kFocusSoft,
                .panelElevated = kPanelElevated,
                .error = kError,
            },
            [this](float x, float y, float width, float height, bool focused, float radius, float focusScale) {
                uiPresentation_.drawListItemSurface(x, y, width, height, focused, radius, focusScale);
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return uiPresentation_.fitTextLines(value, scale, maxWidth, maxLines);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                uiPresentation_.drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                                          verticalPadding);
            },
            [this](const std::string& title, const std::string& message) {
                uiPresentation_.renderEmptyState(title, message);
            });
    }

    void renderPlayer() {
        renderPlayerPresentation(renderer_, player_, videoSurface_, playbackCoordinator_, playerScreenState_,
                                 trickplayState_, settings_, session_, artwork_, lastInteraction_);
    }

    void renderQueueOverlay() { renderQueueOverlayPresentation(renderer_, uiPresentation_, queueState_); }

    void renderScreensaver() {
        renderScreensaverPresentation(renderer_, uiPresentation_, settings_.clock24Hour, Renderer::logicalWidth(),
                                      Renderer::logicalHeight());
    }

    void renderSettings() {
        const std::string externalPlayer = externalPlayerLabel();
        renderSettingsPresentation(renderer_, uiPresentation_, settingsScreen_, settings_,
                                   api_.deviceCodecSupport().maxAudioOutputChannels, externalPlayer, session_.username,
                                   systemTextInputController_.mode() == kTextInputSettingsSearch,
                                   systemTextInputController_.mode() == kTextInputSeerrApiKey);
    }

    void renderDiagnostics() {
        renderDiagnosticsPresentation(renderer_, uiPresentation_, api_.deviceCodecSupport(), SLOPPATV_VERSION_NAME,
                                      session_.server, serverInfo_.name, serverInfo_.version, loading_,
                                      playbackCoordinator_.session().lastPlaybackSummary());
    }

    void renderItemMenu() {
        std::vector<std::string> actions;
        if (!detailsFlow_.state().deleteConfirmation()) actions = itemMenuActions();
        renderItemMenuPresentation(renderer_, uiPresentation_, detailsFlow_.item(), detailsFlow_.state(), actions,
                                   Renderer::logicalWidth(), Renderer::logicalHeight());
    }

    void drawLingeringTitle(float x, float y, float scale, std::string_view value, float maxWidth, Color color,
                            std::chrono::steady_clock::time_point now) {
        renderLingeringTitle(renderer_, x, y, scale, value, maxWidth, color, lastInteraction_, now);
    }

    void renderMediaGrid(const std::string& title, const std::vector<JellyfinItem>& items, int selection) {
        renderMediaGridPresentation(title, items, loading_, selection, settings_.uiTextSize, uiPresentation_,
                                    [this](const JellyfinItem& item, const MediaGridCardPlacement& placement) {
                                        renderMediaArtworkCard(item, placement.x, placement.y, placement.slotWidth,
                                                               placement.focused, placement.showState,
                                                               placement.preferSeriesCover,
                                                               placement.alignToPortraitBand, placement.titleLineLimit);
                                    });
    }

    void renderCast() {
        renderCastPresentation(renderer_, uiPresentation_, detailsFlow_.item(), detailsFlow_.state(),
                               settings_.uiTextSize);
    }

    void renderPersonItems() {
        const auto& person = detailsFlow_.state().selectedPerson();
        const std::string heading = person.name.empty() ? "Person" : "Featuring " + person.name;
        renderMediaGrid(heading, detailsFlow_.state().personItems(), detailsFlow_.state().personItemSelection());
    }

    void renderSeasons() {
        const auto& series = detailsFlow_.state().seriesDetail();
        renderMediaGrid(series.name.empty() ? "Seasons" : series.name + " | Seasons", detailsFlow_.state().seasons(),
                        detailsFlow_.state().seasonSelection());
    }

    void renderEpisodes() {
        const auto& series = detailsFlow_.state().seriesDetail();
        const auto& season = detailsFlow_.state().selectedSeason();
        const std::string heading = season.name.empty() ? "Episodes" : series.name + " - " + season.name;
        renderMediaGrid(heading, detailsFlow_.state().episodes(), detailsFlow_.state().episodeSelection());
    }

    void renderDetails() {
        renderDetailsPresentation(renderer_, uiPresentation_, detailsFlow_.item(), detailsFlow_.state(),
                                  detailActions(), settings_, playbackCoordinator_.continuation().stillWatchingPrompt(),
                                  screen_ == Screen::ItemMenu, Renderer::logicalWidth(), Renderer::logicalHeight());
    }

    void renderStatus() {
        const StatusOverlayRenderState state =
            statusOverlayState_.renderState(error_, loading_ || homeLoading_ || mutationLoading_,
                                            screen_ == Screen::Player, std::chrono::steady_clock::now());
        renderStatusOverlay(renderer_, state,
                            StatusOverlayRenderStyle<Color>{
                                .cornerMedium = material_tv::cornerMedium,
                                .panelElevated = kPanelElevated,
                                .focus = kFocus,
                                .error = kError,
                                .errorOutline = Color{kError.r, kError.g, kError.b, 0.55f},
                                .text = kText,
                            },
                            [this](std::string_view value, float scale, float width, int lines) {
                                return uiPresentation_.fitTextLines(value, scale, width, lines);
                            });
    }

    void loadSession() {
        std::string warning;
        StoredSessionState stored = loadSessionState(dataPath_, generateDeviceId(), warning);
        if (!warning.empty()) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to read session: %s", warning.c_str());
        }
        deviceId_ = std::move(stored.deviceId);
        session_ = SessionRegistry::fromStored(stored.currentSession, deviceId_);
        sessionRegistry_.importStored(stored.savedSessions, deviceId_);
        hiddenHomeItems_ = std::move(stored.hiddenHomeItems);
        settings_ = std::move(stored.settings);
        if (session_.valid()) {
            artwork_.eraseProfile(session_, renderer_);
            sessionRegistry_.remember(session_, deviceId_);
        }
        playbackCoordinator_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
        accountState_.setAuthenticatedAccount(session_.server, session_.username);
    }

    void saveSession(const JellyfinSession& session) {
        if (session.valid()) {
            artwork_.eraseProfile(session, renderer_);
            sessionRegistry_.remember(session, deviceId_);
        }
        StoredSessionState stored;
        stored.deviceId = deviceId_;
        stored.currentSession = SessionRegistry::toStored(session);
        stored.savedSessions = sessionRegistry_.exportStored();
        stored.hiddenHomeItems = hiddenHomeItems_;
        stored.settings = settings_;
        std::string warning;
        if (!saveSessionState(dataPath_, stored, warning) && !warning.empty()) {
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
    std::string deviceId_;

    Screen screen_ = Screen::Login;
    NavigationStack<Screen> navigation_{Screen::Login};
    bool loading_ = false;
    bool homeLoading_ = false;
    std::chrono::steady_clock::time_point homeRetryAt_{};
    int homeRetryAttempt_ = 0;
    bool mutationLoading_ = false;
    std::optional<PlayedRollbackState> playedRollback_;
    AppSettings settings_;
    SettingsScreenState settingsScreen_;
    std::vector<ExternalPlayerApp> externalPlayers_;
    std::string error_;
    StatusOverlayState statusOverlayState_;
    int homeSlideFromFirst_ = 0;
    int homeSlideToFirst_ = 0;
    std::chrono::steady_clock::time_point homeSlideStarted_{};

    JellyfinSession session_;
    std::string pendingDeepLinkItemId_;
    std::string pendingSearchQuery_;
    std::optional<LaunchRequest> pendingRuntimeLaunchRequest_;
    SessionRegistry sessionRegistry_;
    AccountScreenState accountState_;
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
    SeerrSearchCoordinator<
        SeerrAsyncExecutor<SeerrClient, SeerrClient, JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>>>
        seerrSearchCoordinator_;
    DecodedImage brandMarkDecoded_;
    GLuint brandMarkTexture_ = 0;
    uint64_t brandMarkTextureGeneration_ = 0;
    HomeScreenState homeState_;
    std::unordered_set<std::string> hiddenHomeItems_;
    int nextUpReplacementFadeIndex_ = -1;
    std::string nextUpReplacementFadeItemId_;
    std::chrono::steady_clock::time_point nextUpReplacementFadeStarted_{};
    BrowseScreenState browseState_;

    SystemTextInputController systemTextInputController_;

    SearchScreenState searchState_;
    std::unordered_map<std::string, SimilarPrefetchEntry> similarPrefetchCache_;
    std::unordered_set<std::string> similarPrefetchInFlight_;
    std::string similarPrefetchCandidateId_;
    std::string similarPrefetchCandidateKey_;
    std::chrono::steady_clock::time_point similarPrefetchDue_{};

    int keyboardRow_ = 0;
    int keyboardCol_ = 0;

    DetailsFlow detailsFlow_;

    PlaybackQueueState queueState_;

    ExternalPlaybackState externalPlaybackState_;
    PlaybackCoordinator playbackCoordinator_;
    PlayerScreenState playerScreenState_;
    TrickplayPreviewState trickplayState_;
    PlaybackRuntimeController playbackRuntime_;
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
