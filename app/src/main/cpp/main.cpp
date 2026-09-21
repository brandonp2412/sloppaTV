#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "account_async_executor.hpp"
#include "account_flow.hpp"
#include "account_screen_coordinator.hpp"
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
#include "browse_screen_coordinator.hpp"
#include "brand_mark.hpp"
#include "cast_renderer.hpp"
#include "content_screen_presentation.hpp"
#include "content_mutation_flow.hpp"
#include "content_completion_coordinator.hpp"
#include "details_completion_controller.hpp"
#include "details_flow.hpp"
#include "details_renderer.hpp"
#include "details_screen.hpp"
#include "details_screen_coordinator.hpp"
#include "details_navigation_controller.hpp"
#include "details_async_executor.hpp"
#include "deep_link.hpp"
#include "diagnostics_screen.hpp"
#include "diagnostics_renderer.hpp"
#include "discovery.hpp"
#include "display_mode.hpp"
#include "external_playback_executor.hpp"
#include "external_playback_state.hpp"
#include "external_playback_coordinator.hpp"
#include "external_player.hpp"
#include "home_async_executor.hpp"
#include "home_completion_application.hpp"
#include "home_completion_controller.hpp"
#include "home_completion_flow.hpp"
#include "home_navigation_controller.hpp"
#include "home_renderer.hpp"
#include "home_row_renderer.hpp"
#include "home_screen.hpp"
#include "home_screen_coordinator.hpp"
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
#include "runtime_launch_coordinator.hpp"
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
#include "playback_completion_application.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_resolution_executor.hpp"
#include "playback_request_coordinator.hpp"
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
#include "playback_transition_coordinator.hpp"
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
#include "search_screen_coordinator.hpp"
#include "seerr.hpp"
#include "series_playback_executor.hpp"
#include "seerr_async_executor.hpp"
#include "seerr_app_coordinator.hpp"
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
#include "server_info_screen_coordinator.hpp"
#include "session_registry.hpp"
#include "session_store.hpp"
#include "settings_flow.hpp"
#include "settings_screen.hpp"
#include "settings_screen_coordinator.hpp"
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
#include "trickplay_coordinator.hpp"
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

bool clearPendingJniException(JNIEnv* env) {
    if (!env || !env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass objectClassChecked(JNIEnv* env, jobject object) {
    if (!env || !object) return nullptr;
    jclass value = env->GetObjectClass(object);
    if (!clearPendingJniException(env)) return value;
    if (value) env->DeleteLocalRef(value);
    return nullptr;
}

jmethodID methodChecked(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jmethodID value = env->GetMethodID(clazz, name, signature);
    return clearPendingJniException(env) ? nullptr : value;
}

void logPlaybackReportFailure(const char* stage, const std::string& itemId, const ApiResult& result) {
    if (result.ok) return;
    __android_log_print(ANDROID_LOG_WARN, kTag, "Playback %s report failed for %s: %s", stage, itemId.c_str(),
                        result.error.c_str());
}

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
          uiPresentation_(renderer_, artwork_), settingsFlow_(settings_),
          serverInfoScreens_(session_, serverInfo_, serverInfoLoading_, loading_, error_, requestEpochs_.content,
                             serverInfoAsync_, navigation_, screen_),
          seerrConnection_(seerrDomain_, seerrAsync_), seerrRefresh_(seerrDomain_, seerrAsync_),
          seerrRequest_(seerrDomain_, seerrAsync_), searchFlow_(seerrDomain_, seerrAsync_, jellyfinSearchAsync_,
                                                                requestEpochs_.search, requestEpochs_.seerrSearch),
          homeVisibility_(session_, hiddenHomeItems_),
          homeCompletionFlow_(homeVisibility_, home_, homeState_, homeLoading_, homeRetryAt_, homeRetryAttempt_),
          homeScreens_(homeState_, home_, session_, seerrDomain_.pendingRequests(), homeLoading_, homeRetryAt_,
                       requestEpochs_.home, homeCompletionFlow_, screen_, error_, pendingDeepLinkItemId_,
                       pendingSearchQuery_, homeAsync_, uiPresentation_, stateMutex_),
          browseScreens_(browseState_, homeState_, session_, navigation_, screen_, loading_, error_,
                         requestEpochs_.content, browseAsync_, uiPresentation_),
          accountScreens_(accountFlow_, virtualKeyboard_, accountAsync_, quickConnectAsync_, requestEpochs_.auth,
                          loading_, error_),
          searchScreens_(searchFlow_, virtualKeyboard_, navigation_, screen_, homeState_, home_, session_, error_),
          seerrCompletionFlow_(seerrDomain_, contentMutationFlow_, searchFlow_.state(), detailsFlow_, settings_,
                               session_, seerrConnection_, seerrRequest_, seerrRefresh_),
          seerrApp_(seerrDomain_, contentMutationFlow_, detailsFlow_, settings_, session_, loading_, seerrAsync_,
                    seerrConnection_, seerrRequest_, seerrRefresh_, seerrCompletionFlow_),
          settingsScreens_(settingsFlow_, settings_, playbackCoordinator_, displayMode_, homeState_, navigation_,
                           screen_, lastInteraction_, screensaverActive_, error_),
          detailsScreens_(detailsFlow_, navigation_, screen_, session_, loading_, error_, requestEpochs_.content,
                          requestEpochs_.session, detailsAsync_, itemMutationAsync_, contentMutationFlow_, home_,
                          homeState_, browseState_, searchFlow_.state(), queueState_, homeVisibility_, settingsFlow_,
                          playbackCoordinator_, similarPrefetch_),
          runtimeLaunch_(api_, requestEpochs_, session_, accountFlow_, queueState_, searchFlow_.state(),
                         detailsScreens_, navigation_, screen_, loading_, homeLoading_, pendingDeepLinkItemId_,
                         pendingSearchQuery_, error_, lastInteraction_, screensaverActive_),
          contentCompletions_(requestEpochs_.content, requestEpochs_.session, screen_, loading_, error_, session_,
                              detailsFlow_, contentMutationFlow_, homeVisibility_, home_, homeState_, browseState_,
                              searchFlow_.state(), queueState_, detailsAsync_, similarPrefetch_),
          homeCompletions_(homeScreens_, detailsScreens_, session_, accountFlow_, requestEpochs_, contentMutationFlow_,
                           searchFlow_.state(), settings_, seerrDomain_, navigation_, screen_, loading_, error_),
          playbackCompletionFlow_(playbackCoordinator_, queueState_, detailsFlow_, playerScreenState_),
          trickplayCoordinator_(trickplayState_, playbackCoordinator_, session_, trickplayTileAsync_),
          playbackRuntime_(playbackCoordinator_, playerScreenState_, player_, videoSurface_, session_, settings_,
                           requestEpochs_.playback, loading_, error_, dataPath_),
          playbackCompletions_(playbackCompletionFlow_, requestEpochs_.playback, screen_, detailsFlow_, detailsScreens_,
                               session_, loading_, error_, playbackRuntime_, playbackTelemetryAsync_),
          externalPlaybackCoordinator_(
              externalPlayer_, externalPlaybackState_, externalPlaybackAsync_, playbackCoordinator_, session_, home_,
              homeState_, browseState_, searchFlow_.state(), detailsFlow_, queueState_, homeVisibility_,
              requestEpochs_.playback, screen_, loading_, playbackRuntime_, error_, stateMutex_),
          playbackTransitionCoordinator_(playbackCoordinator_, playerScreenState_, queueState_, settings_, navigation_,
                                         screen_, playbackRuntime_, subtitleLoadAsync_, player_, videoSurface_,
                                         renderer_, displayMode_, mediaSession_, error_, stateMutex_),
          playbackReleaseFlow_(requestEpochs_.playback, player_, videoSurface_, displayMode_, mediaSession_,
                               playbackCoordinator_, playerScreenState_, session_, home_, homeState_, browseState_,
                               searchFlow_.state(), detailsFlow_, queueState_, playbackTelemetryAsync_,
                               hiddenHomeItems_, renderer_, trickplayState_),
          playbackRequests_(queueState_, playbackCoordinator_, playerScreenState_, detailsFlow_, session_, settings_,
                            requestEpochs_.playback, screen_, loading_, error_, playbackReleaseFlow_, playbackRuntime_,
                            playbackResolutionAsync_, playbackContinuationAsync_, seriesPlaybackAsync_),
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
            browseScreens_.loadPage(false);
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
            publishAccessibilitySummary();
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
                playbackRequests_.handleQueue(screenNavigationKeyForAndroidKey(AKEYCODE_BACK));
            } else if (screen_ == Screen::Player) {
                handlePlayerKey(AKEYCODE_BACK);
            } else {
                dispatchScreenKey(AKEYCODE_BACK);
            }
        }
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void publishAccessibilitySummary() {
        std::string summary;
        {
            std::scoped_lock lock(stateMutex_);
            const char* screenName;
            switch (screen_) {
            case Screen::Login:
                screenName = "Sign in";
                break;
            case Screen::Profiles:
                screenName = "Profiles";
                break;
            case Screen::Home:
                screenName = "Home";
                break;
            case Screen::Browse:
                screenName = "Browse";
                break;
            case Screen::Search:
                screenName = "Search";
                break;
            case Screen::Settings:
                screenName = "Settings";
                break;
            case Screen::Diagnostics:
                screenName = "Diagnostics";
                break;
            case Screen::Details:
                screenName = "Details";
                break;
            case Screen::Cast:
                screenName = "Cast";
                break;
            case Screen::PersonItems:
                screenName = "Filmography";
                break;
            case Screen::ItemMenu:
                screenName = "Item options";
                break;
            case Screen::Seasons:
                screenName = "Seasons";
                break;
            case Screen::Episodes:
                screenName = "Episodes";
                break;
            case Screen::SeerrDrivePicker:
                screenName = "Storage location";
                break;
            case Screen::Player:
                screenName = "Player";
                break;
            }
            summary = std::string("sloppaTV, ") + screenName;
            if (!detailsFlow_.item().name.empty() &&
                (screen_ == Screen::Details || screen_ == Screen::Player || screen_ == Screen::ItemMenu)) {
                summary += ", " + detailsFlow_.item().name;
            }
            if (screen_ == Screen::Home && homeState_.row() >= 0 &&
                homeState_.row() < static_cast<int>(home_.rows.size())) {
                const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
                const auto& selections = homeState_.selections();
                const int selection = homeState_.row() < static_cast<int>(selections.size())
                                          ? selections[static_cast<size_t>(homeState_.row())]
                                          : 0;
                if (selection >= 0 && selection < static_cast<int>(row.items.size()))
                    summary += ", " + row.title + ", " + row.items[static_cast<size_t>(selection)].name;
            } else if (screen_ == Screen::Browse) {
                const auto& items = browseState_.items();
                const int selection = browseState_.selection();
                if (selection >= 0 && selection < static_cast<int>(items.size()))
                    summary += ", " + items[static_cast<size_t>(selection)].name;
            } else if (screen_ == Screen::Details) {
                const auto action = detailsFlow_.state().selectedAction(detailsFlow_.item());
                if (action) summary += ", " + detailsActionLabel(*action, detailsFlow_.item(), false);
            } else if (screen_ == Screen::Player) {
                switch (playerScreenState_.controlSelection()) {
                case PlayerControl::PreviousEpisode:
                    summary += ", Previous episode";
                    break;
                case PlayerControl::PlayPause:
                    summary += ", Play or pause";
                    break;
                case PlayerControl::NextEpisode:
                    summary += ", Next episode";
                    break;
                case PlayerControl::AudioTrack:
                    summary += ", Audio track";
                    break;
                case PlayerControl::SubtitleTrack:
                    summary += ", Subtitle track";
                    break;
                case PlayerControl::Count:
                    break;
                }
            }
            if (loading_) summary += ", loading";
        }
        if (summary == lastAccessibilitySummary_) return;
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return;
        jclass activityClass = objectClassChecked(env, app_->activity->clazz);
        jmethodID publish = methodChecked(env, activityClass, "setAccessibilitySummaryBridge", "(Ljava/lang/String;)V");
        bool published = false;
        if (publish) {
            jstring value = jniNewString(env, summary);
            const bool valueFailed = clearPendingJniException(env);
            if (value && !valueFailed) {
                env->CallVoidMethod(app_->activity->clazz, publish, value);
                published = !clearPendingJniException(env);
            }
            if (value) env->DeleteLocalRef(value);
        }
        if (activityClass) env->DeleteLocalRef(activityClass);
        if (published) lastAccessibilitySummary_ = std::move(summary);
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
            if (effects.reloadBrowse) browseScreens_.loadPage(false);
            break;
        }
        case APP_CMD_LOST_FOCUS:
            playbackLifecycleFlow_.lostFocus();
            break;
        default:
            break;
        }
    }

    void applyAccountScreenEffects(AccountScreenEffects effects) {
        switch (effects.action) {
        case AccountScreenHostAction::None:
            return;
        case AccountScreenHostAction::FinishActivity:
            ANativeActivity_finish(app_->activity);
            return;
        case AccountScreenHostAction::CancelPendingRequests:
            api_.cancelPendingRequests();
            return;
        case AccountScreenHostAction::ActivateKeyboard:
            activateKeyboardKey(false);
            return;
        case AccountScreenHostAction::EditField: {
            const int field = effects.field;
            const int mode = kTextInputLoginServer + field;
            static constexpr std::array<const char*, 3> hints{"Jellyfin server URL", "Jellyfin username",
                                                              "Jellyfin password"};
            accountFlow_.state().setKeyboardActive(!showSystemTextInput(accountFlow_.state().field(field),
                                                                        hints[static_cast<size_t>(field)], mode,
                                                                        field == AccountScreenState::kPasswordField));
            if (accountFlow_.state().keyboardActive()) virtualKeyboard_.reset();
            return;
        }
        case AccountScreenHostAction::OpenProfiles:
            openProfiles();
            return;
        }
    }

    void applySearchScreenEffects(SearchScreenEffects effects) {
        if (effects.hideTextInput) hideSystemTextInput();
        if (effects.openTextInput) {
            const bool shown =
                showSystemTextInput(searchFlow_.state().query(), "Search Jellyfin & Seerr", kTextInputSearch);
            searchScreens_.applyTextInputShown(shown);
        }
        if (effects.activateKeyboard) activateKeyboardKey(true);
        if (effects.refreshSeerrStorage) refreshSeerrStorageAsync();
        if (effects.reconnectSeerr) connectSeerrAsync(false);
        if (effects.syncSeerrHome) homeScreens_.syncSeerrHome();
        if (effects.prefetchSimilarItem) scheduleSimilarPrefetch(*effects.prefetchSimilarItem);
        if (effects.openContextItem) detailsScreens_.openItemMenuForItem(*effects.openContextItem);
        if (effects.openDetailsItem) detailsScreens_.openDetails(*effects.openDetailsItem);
        if (effects.requestSeerrItem) requestSeerrMediaAsync(*effects.requestSeerrItem);
    }

    void applyBrowseScreenEffects(BrowseScreenEffects effects) {
        if (effects.openContextItem) detailsScreens_.openItemMenuForItem(*effects.openContextItem);
        if (effects.openDetailsItem) detailsScreens_.openDetails(*effects.openDetailsItem);
        if (effects.prefetchSimilarItem) scheduleSimilarPrefetch(*effects.prefetchSimilarItem);
    }

    void applyDetailsScreenEffects(DetailsScreenEffects effects) {
        if (effects.reloadBrowse) browseScreens_.loadPage(false);
        if (effects.prefetchItem) scheduleSimilarPrefetch(*effects.prefetchItem);
        if (effects.persistSession) saveSession(session_);
        if (effects.notice) showNotice(std::move(effects.notice->message), effects.notice->duration);

        switch (effects.action) {
        case DetailsScreenHostAction::None:
            return;
        case DetailsScreenHostAction::BeginPlayback:
            beginPlayback();
            return;
        case DetailsScreenHostAction::BeginSeriesPlayAll:
            playbackRequests_.beginSeriesPlayAll();
            return;
        case DetailsScreenHostAction::DeleteSeerrRequest:
            deleteSeerrRequestAsync();
            return;
        case DetailsScreenHostAction::PlayExternal:
            externalPlaybackCoordinator_.prepare(settingsFlow_.selectedExternalPlayer());
            return;
        case DetailsScreenHostAction::ViewQueue:
            playbackRequests_.openQueue();
            return;
        }
    }

    void dispatchScreenKey(int32_t key) {
        switch (screen_) {
        case Screen::Login:
            applyAccountScreenEffects(accountScreens_.handleLogin(screenNavigationKeyForAndroidKey(key)));
            break;
        case Screen::Profiles:
            handleProfilesKey(key);
            break;
        case Screen::Home:
            handleHomeKey(key);
            break;
        case Screen::Browse:
            applyBrowseScreenEffects(browseScreens_.handle(screenNavigationKeyForAndroidKey(key)));
            break;
        case Screen::Search:
            applySearchScreenEffects(searchScreens_.handle(screenNavigationKeyForAndroidKey(key), seerrApp_.endpoint(),
                                                           std::chrono::steady_clock::now()));
            break;
        case Screen::Settings:
            handleSettingsKey(key);
            break;
        case Screen::Diagnostics:
            handleDiagnosticsKey(key);
            break;
        case Screen::Details:
            applyDetailsScreenEffects(detailsScreens_.handleDetails(detailsNavigationKeyForAndroidKey(key)));
            break;
        case Screen::Cast:
            applyDetailsScreenEffects(
                detailsScreens_.handleCast(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
            break;
        case Screen::PersonItems:
            applyDetailsScreenEffects(
                detailsScreens_.handlePersonItems(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
            break;
        case Screen::ItemMenu:
            applyDetailsScreenEffects(detailsScreens_.handleItemMenu(detailsNavigationKeyForAndroidKey(key)));
            break;
        case Screen::Seasons:
            applyDetailsScreenEffects(
                detailsScreens_.handleSeasons(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
            break;
        case Screen::Episodes:
            applyDetailsScreenEffects(
                detailsScreens_.handleEpisodes(detailsNavigationKeyForAndroidKey(key), mediaGridColumns()));
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
            if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && screen_ == Screen::Player &&
                playerScreenState_.skipButtonPressPending()) {
                const bool activate = playerScreenState_.consumeSkipButtonRelease();
                if (activate) handlePlayerKey(key);
                return 1;
            }
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
            playbackRequests_.handleQueue(screenNavigationKeyForAndroidKey(key));
            return 1;
        }

        if (screen_ == Screen::Player) {
            if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) &&
                !playerScreenState_.skipDisablePromptVisible()) {
                if (playerScreenState_.skipButtonPressPending()) {
                    if (repeatCount > 0 && !playerScreenState_.skipButtonLongPressed()) {
                        playerScreenState_.openSkipDisablePrompt();
                    }
                    return 1;
                }
                if (repeatCount == 0 && beginPlayerSkipButtonPress()) return 1;
            }
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
                    detailsScreens_.openItemMenuForItem(section.items[static_cast<size_t>(selection)]);
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
        const bool seerrConfigured = SeerrClient::configured(settings_.seerrServer, seerrApp_.auth());
        if (searchScreens_.scheduleLive(seerrConfigured, std::chrono::steady_clock::now()) && app_ && app_->looper)
            ALooper_wake(app_->looper);
    }

    void runDueLiveSearch() {
        std::scoped_lock lock(stateMutex_);
        applySearchScreenEffects(searchScreens_.runDue(seerrApp_.endpoint(), std::chrono::steady_clock::now()));
    }

    bool showSystemTextInput(const std::string& initial, const std::string& hint, int mode, bool password = false) {
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return false;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return false;
        jobject activity = app_->activity->clazz;
        jclass activityClass = objectClassChecked(env, activity);
        jmethodID method =
            methodChecked(env, activityClass, "showTextInput", "(Ljava/lang/String;Ljava/lang/String;IZ)Z");
        jstring jInitial = nullptr;
        jstring jHint = nullptr;
        jboolean shown = JNI_FALSE;
        if (method) {
            jInitial = jniNewString(env, initial);
            bool failed = clearPendingJniException(env) || !jInitial;
            if (!failed) {
                jHint = jniNewString(env, hint);
                failed = clearPendingJniException(env) || !jHint;
            }
            if (!failed) {
                shown = env->CallBooleanMethod(activity, method, jInitial, jHint, static_cast<jint>(mode),
                                               password ? JNI_TRUE : JNI_FALSE);
                if (clearPendingJniException(env)) shown = JNI_FALSE;
            }
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
        jclass activityClass = objectClassChecked(env, activity);
        jmethodID method = methodChecked(env, activityClass, "hideTextInput", "()V");
        if (method) {
            env->CallVoidMethod(activity, method);
            clearPendingJniException(env);
        }
        if (activityClass) env->DeleteLocalRef(activityClass);
    }

    void activateKeyboardKey(bool forSearch) {
        const VirtualKeyboardEffects effects =
            virtualKeyboard_.activate(forSearch, searchFlow_.state(), accountFlow_.state());
        if (effects.searchChanged) scheduleLiveSearch();
        if (effects.submitSearch) searchAsync();
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

    void handleHomeKey(int32_t key) {
        HomeScreenEffects effects = homeScreens_.handle(screenNavigationKeyForAndroidKey(key));
        if (effects.rowSlideStarted) renderBurstUntil_ = std::max(renderBurstUntil_, *effects.rowSlideStarted + 240ms);
        if (effects.prefetchSimilarItem) scheduleSimilarPrefetch(*effects.prefetchSimilarItem);
        if (effects.openContextItem) {
            detailsScreens_.openItemMenuForItem(*effects.openContextItem);
            return;
        }
        if (effects.openLibraryItem) {
            browseScreens_.openLibrary(*effects.openLibraryItem);
            return;
        }
        if (effects.openDetailsItem) {
            detailsScreens_.openDetails(*effects.openDetailsItem);
            return;
        }
        switch (effects.action) {
        case HomeScreenHostAction::None:
            return;
        case HomeScreenHostAction::FinishActivity:
            ANativeActivity_finish(app_->activity);
            return;
        case HomeScreenHostAction::OpenProfiles:
            openProfiles();
            return;
        case HomeScreenHostAction::OpenSearch:
            openSearch();
            return;
        case HomeScreenHostAction::OpenSettings:
            openSettings();
            return;
        }
    }

    void applySettingsScreenEffects(SettingsScreenEffects effects) {
        if (effects.persistSession) saveSession(session_);
        if (effects.refreshSeerrStorage) refreshSeerrStorageAsync(true);

        switch (effects.hostAction) {
        case SettingsHostAction::None:
        case SettingsHostAction::Exit:
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
        applySettingsScreenEffects(settingsScreens_.handle(screenNavigationKeyForAndroidKey(key)));
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

    void loadSubtitleAsync(const JellyfinSubtitleStream& subtitle, const std::string& deliveryUrl = {}) {
        if (!playbackRuntime_.loadSubtitle(subtitleLoadAsync_, subtitle, deliveryUrl))
            showNotice("SUBTITLES COULD NOT BE STARTED");
    }

    void clearTrickplayPreview() {
        if (const auto texture = trickplayCoordinator_.clear(renderer_.generation())) renderer_.deleteTexture(*texture);
    }

    void requestTrickplayPreview(int positionMs) {
        if (const auto texture = trickplayCoordinator_.request(positionMs, renderer_.generation()))
            renderer_.deleteTexture(*texture);
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
            playbackRequests_.openQueue();
            return;
        case PlaybackHostEffectType::PlayAdjacentEpisode:
            playbackRequests_.playAdjacent(effect.episodeDirection);
            return;
        case PlaybackHostEffectType::SeekWithTrickplay:
            requestTrickplayPreview(effect.positionMs);
            playbackRuntime_.seekTo(effect.positionMs);
            playbackRuntime_.reportProgress(playbackTelemetryAsync_, screen_ == Screen::Player, false);
            return;
        case PlaybackHostEffectType::PlayQueueIndex:
            playbackRequests_.playQueued(effect.queueIndex, effect.restartCurrent, effect.replacingCompleted);
            return;
        case PlaybackHostEffectType::QueueAutoplayNext:
            if (effect.item) applyPlaybackRequestHostEffects(playbackRequests_.queueAutoplay(*effect.item));
            return;
        case PlaybackHostEffectType::ShowStillWatching:
            if (effect.item) applyPlaybackRequestHostEffects(playbackRequests_.showStillWatching(*effect.item));
            return;
        case PlaybackHostEffectType::SubtitleStartFailed:
            showNotice("SUBTITLES COULD NOT BE STARTED");
            return;
        }
    }

    bool beginPlayerSkipButtonPress() {
        const auto now = std::chrono::steady_clock::now();
        const auto* segment = playbackCoordinator_.activeSkippableSegment(playerScreenState_.positionMs());
        if (!segment || !canDisableSkipForSegmentType(segment->type)) return false;
        const auto& item = playbackCoordinator_.session().activeItem();
        if (item.seriesId.empty()) return false;
        const bool disabled = skipSegmentsDisabledForSeries(settings_, item.seriesId);
        const bool controlsActive = playerScreenState_.controlsActive(now);
        if ((!disabled && controlsActive) ||
            (disabled && (!controlsActive || playerScreenState_.controlSelection() != PlayerControl::PlayPause))) {
            return false;
        }
        playerScreenState_.beginSkipButtonPress(item.seriesId, item.seriesName, disabled);
        return true;
    }

    bool handleSkipDisablePromptKey(int32_t key, int repeatCount) {
        if (!playerScreenState_.skipDisablePromptVisible()) return false;
        if (repeatCount > 0) return true;
        if (key == AKEYCODE_BACK) {
            playerScreenState_.closeSkipDisablePrompt();
            return true;
        }
        if (key == AKEYCODE_DPAD_LEFT) {
            playerScreenState_.selectSkipDisable(false);
            return true;
        }
        if (key == AKEYCODE_DPAD_RIGHT) {
            playerScreenState_.selectSkipDisable(true);
            return true;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (playerScreenState_.skipDisableSelected()) {
                const std::string seriesId = playerScreenState_.skipDisableSeriesId();
                const std::string seriesName = playerScreenState_.skipDisableSeriesName();
                const bool disabled = !playerScreenState_.skipDisableEnabling();
                if (setSkipSegmentsDisabledForSeries(settings_, seriesId, disabled)) {
                    saveSession(session_);
                    const std::string action = disabled ? "disabled" : "enabled";
                    showNotice(seriesName.empty() ? "Skip buttons " + action + " for this show"
                                                  : "Skip buttons " + action + " for " + seriesName,
                               4s);
                }
            }
            playerScreenState_.closeSkipDisablePrompt();
            return true;
        }
        return true;
    }

    void handlePlayerKey(int32_t key, int repeatCount = 0) {
        if (handleSkipDisablePromptKey(key, repeatCount)) return;
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

    void openSettings() { settingsScreens_.open(externalPlayer_.availablePlayers()); }

    void openDiagnostics() { serverInfoScreens_.openDiagnostics(); }

    void clearCurrentSessionUi() {
        const bool hadAuthenticatedSession = session_.valid();
        api_.cancelPendingRequests();
        requestEpochs_.invalidateAll();
        if (screen_ == Screen::Player || player_.status() != PlayerStatus::Idle ||
            playbackCoordinator_.activeItemAvailable()) {
            playbackRequests_.release(true);
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

    void deleteSeerrRequestAsync() { applySeerrCompletionHostEffects(seerrApp_.deleteCurrentRequest()); }

    void openSearch() {
        pushScreen(Screen::Search);
        const bool shown =
            showSystemTextInput(searchFlow_.state().query(), "Search Jellyfin & Seerr", kTextInputSearch);
        searchScreens_.applyTextInputShown(shown);
        virtualKeyboard_.reset();
        error_.clear();
    }

    void loadHomeAsync() {
        serverInfoScreens_.requestNotice();
        homeScreens_.load();
    }

    void connectSeerrAsync(bool announce = true) { applySeerrCompletionHostEffects(seerrApp_.connect(announce)); }

    void refreshSeerrStorageAsync(bool force = false) { seerrApp_.refreshStorage(force); }

    void openSeerrDrivePicker(const SeerrMediaItem& item) {
        applySeerrCompletionHostEffects(seerrApp_.openDrivePicker(item));
    }

    void refreshSeerrPendingAsync() { applySeerrCompletionHostEffects(seerrApp_.refreshPending()); }

    void requestSeerrMediaAsync(const SeerrMediaItem& item, const SeerrStorageTarget* selectedTarget = nullptr,
                                bool skipDrivePrompt = false) {
        applySeerrCompletionHostEffects(seerrApp_.requestMedia(item, selectedTarget, skipDrivePrompt));
    }

    void searchAsync(bool includeSeerrImmediately = true) {
        applySearchScreenEffects(
            searchScreens_.search(seerrApp_.endpoint(), includeSeerrImmediately, std::chrono::steady_clock::now()));
    }

    void scheduleSimilarPrefetch(const JellyfinItem& item) {
        if (similarPrefetch_.schedule(session_, item) && app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void scheduleFocusedHomePrefetch() {
        if (screen_ != Screen::Home) return;
        const int row = homeState_.row();
        if (row < 0 || row >= static_cast<int>(home_.rows.size())) return;
        const auto& items = home_.rows[static_cast<size_t>(row)].items;
        if (items.empty()) return;
        const int selection = homeState_.selection(row, static_cast<int>(items.size()));
        scheduleSimilarPrefetch(items[static_cast<size_t>(selection)]);
    }

    void runDueSimilarPrefetch() {
        const bool eligible = screen_ == Screen::Home || screen_ == Screen::Browse || screen_ == Screen::Search ||
                              screen_ == Screen::Details || screen_ == Screen::Player;
        auto work = similarPrefetch_.takeDue(session_, eligible);
        if (!work) return;

        SimilarPrefetchWork request = std::move(*work);
        const std::string key = request.key;
        if (!similarPrefetchTasks_.submit([this, request = std::move(request)]() mutable {
                ApiValueResult<JellyfinItem> detail;
                JellyfinItem item = request.item;
                if (item.type != "Season" || item.seriesId.empty()) {
                    if (const auto cached = similarPrefetch_.cachedDetail(request.session, request.itemId)) {
                        item = *cached;
                    } else {
                        detail = similarPrefetchApi_.getItem(request.session, request.itemId);
                        if (detail.ok) item = detail.value;
                    }
                }

                ApiValueResult<std::vector<JellyfinItem>> result;
                if (item.type != "Season" && !similarPrefetch_.cached(request.session, request.itemId)) {
                    result = similarPrefetchApi_.getSimilar(request.session, request.itemId, 18);
                }

                std::string seriesId;
                if (item.type == "Series")
                    seriesId = item.id;
                else if (item.type == "Episode" || item.type == "Season")
                    seriesId = item.seriesId;

                ApiValueResult<JellyfinItem> seriesDetail;
                ApiValueResult<std::vector<JellyfinItem>> seasons;
                std::vector<JellyfinItem> seasonItems;
                std::string seasonId;
                ApiValueResult<std::vector<JellyfinItem>> episodes;
                ApiValueResult<JellyfinItem> nextEpisodeDetail;

                if (!seriesId.empty()) {
                    if (seriesId != item.id && !similarPrefetch_.cachedSeriesDetail(request.session, seriesId)) {
                        seriesDetail = similarPrefetchApi_.getItem(request.session, seriesId);
                    }

                    if (const auto cached = similarPrefetch_.cachedSeasons(request.session, seriesId)) {
                        seasonItems = *cached;
                    } else {
                        seasons = similarPrefetchApi_.getSeasons(request.session, seriesId);
                        if (seasons.ok) seasonItems = seasons.value;
                    }

                    if (item.type == "Season") {
                        seasonId = item.id;
                    } else if (!seasonItems.empty()) {
                        int preferredSeason = item.type == "Episode" ? item.parentIndexNumber : -1;
                        JellyfinItem nextUp;
                        if (item.type == "Series") {
                            auto next = similarPrefetchApi_.getNextUpForSeries(request.session, seriesId);
                            if (next.ok) {
                                nextUp = next.value;
                                preferredSeason = nextUp.parentIndexNumber;
                            }
                        }

                        auto selected = seasonItems.end();
                        if (preferredSeason >= 0) {
                            selected =
                                std::find_if(seasonItems.begin(), seasonItems.end(), [&](const JellyfinItem& season) {
                                    return season.indexNumber == preferredSeason;
                                });
                        }
                        if (selected == seasonItems.end()) {
                            selected = std::find_if(seasonItems.begin(), seasonItems.end(),
                                                    [](const JellyfinItem& season) { return season.indexNumber > 0; });
                        }
                        if (selected == seasonItems.end()) selected = seasonItems.begin();
                        seasonId = selected->id;

                        if (!nextUp.id.empty() && !similarPrefetch_.cachedDetail(request.session, nextUp.id)) {
                            nextEpisodeDetail = similarPrefetchApi_.getItem(request.session, nextUp.id);
                        }
                    }

                    if (!seasonId.empty() && !similarPrefetch_.cachedEpisodes(request.session, seriesId, seasonId)) {
                        episodes = similarPrefetchApi_.getEpisodes(request.session, seriesId, seasonId);
                    }

                    if (item.type == "Episode") {
                        auto next =
                            similarPrefetchApi_.getFollowingEpisodeForSeries(request.session, seriesId, item.id);
                        if (next.ok && !similarPrefetch_.cachedDetail(request.session, next.value.id)) {
                            nextEpisodeDetail = similarPrefetchApi_.getItem(request.session, next.value.id);
                        }
                    }
                }

                asyncCompletions_.push(SimilarPrefetchCompletion{
                    .session = std::move(request.session),
                    .itemId = std::move(request.itemId),
                    .key = std::move(request.key),
                    .detail = std::move(detail),
                    .result = std::move(result),
                    .seriesId = std::move(seriesId),
                    .seriesDetail = std::move(seriesDetail),
                    .seasons = std::move(seasons),
                    .seasonId = std::move(seasonId),
                    .episodes = std::move(episodes),
                    .nextEpisodeDetail = std::move(nextEpisodeDetail),
                });
            })) {
            similarPrefetch_.submissionFailed(key);
        }
    }

    void beginPlayback() {
        if (loading_ || detailsFlow_.item().id.empty()) return;
        if (const auto externalPlayer = settingsFlow_.selectedExternalPlayer())
            externalPlaybackCoordinator_.prepare(*externalPlayer);
        else
            playbackRequests_.beginPlayback();
    }

    void applyPlaybackRequestHostEffects(PlaybackRequestHostEffects effects) {
        if (effects.resetIdle) {
            lastInteraction_ = std::chrono::steady_clock::now();
            screensaverActive_ = false;
        }
        if (effects.popToDetails) popScreen(Screen::Details);
    }

    void applyRuntimeLaunchRequest(const LaunchRequest& request) {
        std::scoped_lock lock(stateMutex_);
        const bool playbackActive = screen_ == Screen::Player || player_.status() != PlayerStatus::Idle;
        RuntimeLaunchEffects effects = runtimeLaunch_.apply(request, playbackActive);
        if (effects.hideTextInput) hideSystemTextInput();
        if (effects.releasePlayback) {
            playbackRequests_.release(true);
            playbackCoordinator_.finishStop();
        }
        if (effects.openedItemId)
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening runtime ACTION_VIEW Jellyfin item %s",
                                effects.openedItemId->c_str());
        if (effects.openedSearch) __android_log_print(ANDROID_LOG_INFO, kTag, "Opening runtime ACTION_SEARCH query");
        if (effects.triggerSearch) searchAsync();
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

    [[nodiscard]] bool processPendingPlaybackTickWork() {
        ExternalPlaybackTickWork externalWork = externalPlaybackCoordinator_.collect();
        PlaybackTransitionTickWork transitionWork = playbackTransitionCoordinator_.collect(app_->window != nullptr);
        if (transitionWork.subtitleLoadFailed) showNotice("SUBTITLES COULD NOT BE STARTED");

        externalPlaybackCoordinator_.finish(externalWork);
        if (externalPlaybackCoordinator_.launch(externalWork)) return true;

        const PlaybackTransitionStartEffects transitionEffects =
            playbackTransitionCoordinator_.start(transitionWork, app_->window);
        if (transitionEffects.surfaceFailed) {
            popScreen(Screen::Details);
            playbackCoordinator_.clearActivePlayback();
        }
        return transitionEffects.handled;
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
        if (effects.syncHome) homeScreens_.syncSeerrHome();
        if (effects.closeItem) {
            popScreen(Screen::Home);
            if (screen_ != Screen::Home) resetNavigation(Screen::Home);
        }
        if (effects.notice) showNotice(std::move(*effects.notice), std::chrono::seconds(effects.noticeSeconds));
        if (effects.reconnect) connectSeerrAsync(false);
        if (effects.openDrivePicker) {
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening Seerr storage picker media=%s choices=%zu",
                                effects.openDrivePicker->mediaType.c_str(), seerrDomain_.storageDriveChoices().size());
            if (screen_ != Screen::SeerrDrivePicker) pushScreen(Screen::SeerrDrivePicker);
        }
        if (effects.saveSession) saveSession(session_);
        if (effects.refreshPending) refreshSeerrPendingAsync();
        if (effects.refreshStorage) refreshSeerrStorageAsync(true);
        if (effects.deferredRequest) requestSeerrMediaAsync(*effects.deferredRequest);
        if (effects.retrySearch && screen_ == Screen::Search && !searchFlow_.state().query().empty()) {
            const auto now = std::chrono::steady_clock::now();
            (void)searchFlow_.prepareReconnectRetry(now);
            applySearchScreenEffects(searchScreens_.searchSeerr(seerrApp_.endpoint(), true, now));
        }
    }

    template <typename Completion>
        requires isSimpleSeerrCompletionV<Completion>
    void applyAsyncCompletion(Completion& completion) {
        applySeerrCompletionHostEffects(seerrApp_.complete(completion));
    }

    void applyAsyncCompletion(const SeerrDeleteCompletion& completion) {
        applySeerrCompletionHostEffects(seerrApp_.complete(completion, screen_ == Screen::ItemMenu));
    }

    void applyAsyncCompletion(SeerrPendingRefreshCompletion& completion) {
        applySeerrCompletionHostEffects(seerrApp_.complete(completion, screen_ == Screen::ItemMenu));
    }

    void applyAsyncCompletion(SeerrSearchCompletion& completion) {
        applySearchScreenEffects(searchScreens_.complete(completion, !settings_.seerrSessionCookie.empty(),
                                                         std::chrono::steady_clock::now()));
    }

    void applyAsyncCompletion(JellyfinSearchCompletion& completion) {
        applySearchScreenEffects(searchScreens_.complete(completion));
    }

    void applyContentCompletionHostEffects(ContentCompletionHostEffects effects) {
        if (effects.prefetchItem) scheduleSimilarPrefetch(*effects.prefetchItem);
        if (effects.renderAnimationStarted) {
            renderBurstUntil_ = std::max(renderBurstUntil_, *effects.renderAnimationStarted + 320ms);
            if (app_ && app_->looper) ALooper_wake(app_->looper);
        }
        if (effects.closeDeletedItem) {
            popScreen(Screen::Home);
            if (screen_ == Screen::Details) popScreen(Screen::Home);
        }
        if (effects.notice) showNotice(std::move(*effects.notice));
    }

    template <typename Completion>
        requires isContentCompletionV<Completion>
    void applyAsyncCompletion(Completion& completion) {
        applyContentCompletionHostEffects(contentCompletions_.complete(completion));
    }

    void applyAsyncCompletion(DiagnosticsCompletion& completion) {
        if (auto notice = serverInfoScreens_.complete(completion))
            showNotice(std::move(notice->message), notice->duration, notice->persistent);
    }

    void applyAsyncCompletion(BrowsePageCompletion& completion) {
        browseScreens_.complete(completion);
        if (screen_ != Screen::Browse) return;
        const auto& items = browseState_.items();
        const int selection = browseState_.selection();
        if (selection >= 0 && selection < static_cast<int>(items.size())) {
            scheduleSimilarPrefetch(items[static_cast<size_t>(selection)]);
        }
    }

    void applyAsyncCompletion(ServerInfoNoticeCompletion& completion) {
        if (auto notice = serverInfoScreens_.complete(completion))
            showNotice(std::move(notice->message), notice->duration, notice->persistent);
    }

    void applyAuthenticatedSession(std::optional<JellyfinSession> authenticatedSession) {
        if (!authenticatedSession) return;
        requestEpochs_.session.invalidate();
        session_ = std::move(*authenticatedSession);
        resetNavigation(Screen::Home);
        homeState_.setRow(0);
        homeState_.setFirstVisibleRow(0);
        saveSession(session_);
        loadHomeAsync();
    }

    template <typename Completion>
        requires isAccountScreenCompletionV<Completion>
    void applyAsyncCompletion(Completion& completion) {
        applyAuthenticatedSession(accountScreens_.complete(completion));
    }

    void applyAsyncCompletion(HomeCoreCompletion& completion) {
        const auto effects = homeCompletions_.complete(completion);
        if (effects.retryDelaySeconds)
            __android_log_print(ANDROID_LOG_WARN, kTag, "Home load failed transiently; retrying in %d seconds: %s",
                                *effects.retryDelaySeconds, completion.result.error.c_str());
        if (effects.sessionExpired) {
            if (effects.eraseProfile) artwork_.eraseProfile(*effects.eraseProfile, renderer_);
            saveSession(session_);
            return;
        }
        if (effects.primaryReadyMs)
            __android_log_print(ANDROID_LOG_INFO, kTag, "Home primary rows ready in %lld ms", *effects.primaryReadyMs);
        if (effects.openedItemId)
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening ACTION_VIEW Jellyfin item %s",
                                effects.openedItemId->c_str());
        if (effects.openedSearch) {
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening ACTION_SEARCH query");
            if (effects.triggerSearch) searchAsync();
        }
        if (effects.refreshSeerrPending) refreshSeerrPendingAsync();
        scheduleFocusedHomePrefetch();
    }

    void applyAsyncCompletion(HomeSecondaryCompletion& completion) {
        const auto effects = homeCompletions_.complete(completion);
        if (effects.secondaryReadyMs)
            __android_log_print(ANDROID_LOG_INFO, kTag, "Home enrichment completed in %lld ms",
                                *effects.secondaryReadyMs);
        scheduleFocusedHomePrefetch();
    }

    void applyAsyncCompletion(ExternalPlaybackCompletion& completion) {
        const int persistCount = externalPlaybackCoordinator_.complete(completion);
        for (int i = 0; i < persistCount; ++i) saveSession(session_);
    }

    void applyPlaybackCompletionApplicationEffects(PlaybackCompletionApplicationEffects effects) {
        if (effects.popToDetails) popScreen(Screen::Details);
        for (int i = 0; i < effects.saveSessionCount; ++i) saveSession(session_);
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
        if (effects.playItem) playbackRequests_.playItem(std::move(*effects.playItem));
    }

    void applyAsyncCompletion(NextEpisodeCompletion& completion) {
        if (completion.ok && !completion.item.id.empty()) {
            similarPrefetch_.rememberDetail(session_, completion.item);
            scheduleSimilarPrefetch(completion.item);
        }
        applyPlaybackCompletionApplicationEffects(playbackCompletions_.complete(completion));
    }

    template <typename Completion>
        requires isPlaybackApplicationCompletionV<Completion>
    void applyAsyncCompletion(Completion& completion) {
        applyPlaybackCompletionApplicationEffects(playbackCompletions_.complete(completion));
    }

    void applyAsyncCompletion(TrickplayTileCompletion& completion) {
        const TrickplayCompletionEffects effects = trickplayCoordinator_.complete(completion);
        if (effects.unavailable)
            __android_log_print(ANDROID_LOG_WARN, kTag, "Trickplay tile %d unavailable: %s", effects.tileIndex,
                                effects.error.c_str());
    }

    void applyAsyncCompletion(ArtworkLoadCompletion& completion) { artwork_.applyCompletion(std::move(completion)); }

    void applyAsyncCompletion(const PlaybackReportCompletion& completion) {
        logPlaybackReportFailure(PlayerCompletionController::reportStage(completion.kind), completion.itemId,
                                 completion.result);
        PlayerCompletionController::apply(completion, session_.server, session_.userId, screen_ == Screen::Player,
                                          playbackCoordinator_);
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
                SeerrClient::configured(settings_.seerrServer, seerrApp_.auth()) &&
                (screen_ == Screen::Home || (screen_ == Screen::ItemMenu && isSeerrItem(detailsFlow_.item())));
            if (seerrDomain_.consumePendingRefreshDue(now, seerrRefreshEligible)) refreshSeerr = true;
        }
        if (retryHome || refreshHomeAfterPlaybackStop) loadHomeAsync();
        if (refreshSeerr) refreshSeerrPendingAsync();

        if (processPendingPlaybackTickWork()) return;
        tickActivePlayer();
    }

    void stopPlayback(bool completed = false) {
        if (screen_ != Screen::Player && player_.status() == PlayerStatus::Idle) return;
        playbackRequests_.release(true, completed);
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
            .seerrConfigured = SeerrClient::configured(settings_.seerrServer, seerrApp_.auth()),
            .systemSearchInputActive = systemTextInputController_.mode() == kTextInputSearch,
            .systemSettingsSearchActive = systemTextInputController_.mode() == kTextInputSettingsSearch,
            .systemSeerrApiKeyActive = systemTextInputController_.mode() == kTextInputSeerrApiKey,
            .itemHiddenFromHome = homeVisibility_.isHidden(detailsFlow_.item()),
            .keyboardRow = virtualKeyboard_.row(),
            .keyboardCol = virtualKeyboard_.column(),
            .homeSlideFromFirst = homeScreens_.slideFromFirst(),
            .homeSlideToFirst = homeScreens_.slideToFirst(),
            .homeSlideStarted = homeScreens_.slideStarted(),
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
    JellyfinSession session_;
    std::string pendingDeepLinkItemId_;
    std::string pendingSearchQuery_;
    std::optional<LaunchRequest> pendingRuntimeLaunchRequest_;
    AccountFlow accountFlow_;
    JellyfinServerInfo serverInfo_;
    bool serverInfoLoading_ = false;
    ServerInfoScreenCoordinator<decltype(serverInfoAsync_)> serverInfoScreens_;
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
    HomeScreenCoordinator<decltype(homeAsync_), decltype(uiPresentation_)> homeScreens_;
    BrowseScreenState browseState_;
    BrowseScreenCoordinator<decltype(browseAsync_), decltype(uiPresentation_)> browseScreens_;

    SystemTextInputController systemTextInputController_;

    SimilarPrefetchController similarPrefetch_;

    VirtualKeyboardState virtualKeyboard_;
    AccountScreenCoordinator<decltype(accountAsync_), decltype(quickConnectAsync_)> accountScreens_;
    SearchScreenCoordinator<decltype(searchFlow_)> searchScreens_;

    DetailsFlow detailsFlow_;
    SeerrCompletionFlow<decltype(seerrConnection_), decltype(seerrRequest_), decltype(seerrRefresh_)>
        seerrCompletionFlow_;
    SeerrAppCoordinator<decltype(seerrAsync_), decltype(seerrConnection_), decltype(seerrRequest_),
                        decltype(seerrRefresh_), decltype(seerrCompletionFlow_)>
        seerrApp_;

    PlaybackQueueState queueState_;

    ExternalPlaybackState externalPlaybackState_;
    PlaybackCoordinator playbackCoordinator_;
    SettingsScreenCoordinator settingsScreens_;
    DetailsScreenCoordinator<decltype(detailsAsync_), decltype(itemMutationAsync_)> detailsScreens_;
    RuntimeLaunchCoordinator<JellyfinClient, decltype(detailsScreens_)> runtimeLaunch_;
    ContentCompletionCoordinator<decltype(detailsAsync_)> contentCompletions_;
    HomeCompletionApplication<decltype(homeScreens_), decltype(detailsScreens_)> homeCompletions_;
    PlayerScreenState playerScreenState_;
    PlaybackCompletionFlow playbackCompletionFlow_;
    TrickplayPreviewState trickplayState_;
    TrickplayCoordinator<decltype(trickplayTileAsync_)> trickplayCoordinator_;
    PlaybackRuntimeController playbackRuntime_;
    PlaybackCompletionApplication<decltype(detailsScreens_), decltype(playbackTelemetryAsync_)> playbackCompletions_;
    ExternalPlaybackCoordinator<decltype(externalPlaybackAsync_)> externalPlaybackCoordinator_;
    PlaybackTransitionCoordinator<decltype(subtitleLoadAsync_)> playbackTransitionCoordinator_;
    PlaybackReleaseFlow<decltype(playbackTelemetryAsync_)> playbackReleaseFlow_;
    PlaybackRequestCoordinator<decltype(playbackReleaseFlow_), decltype(playbackResolutionAsync_),
                               decltype(playbackContinuationAsync_), decltype(seriesPlaybackAsync_)>
        playbackRequests_;
    PlaybackLifecycleFlow playbackLifecycleFlow_;
    std::chrono::steady_clock::time_point renderBurstUntil_{};
    std::chrono::steady_clock::time_point lastInteraction_ = std::chrono::steady_clock::now();
    bool screensaverActive_ = false;
    std::string lastAccessibilitySummary_;
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
