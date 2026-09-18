#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "account_async_executor.hpp"
#include "account_screen.hpp"
#include "app_settings.hpp"
#include "artwork_provider.hpp"
#include "async_completion_queue.hpp"
#include "audio_policy.hpp"
#include "browse_async_executor.hpp"
#include "browse_renderer.hpp"
#include "browse_screen.hpp"
#include "cast_renderer.hpp"
#include "details_renderer.hpp"
#include "details_screen.hpp"
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
#include "home_renderer.hpp"
#include "home_screen.hpp"
#include "image_decoder.hpp"
#include "item_menu_renderer.hpp"
#include "item_mutation_executor.hpp"
#include "jellyfin.hpp"
#include "jellyfin_search_executor.hpp"
#include "jni_env.hpp"
#include "launch_intent.hpp"
#include "login_renderer.hpp"
#include "media_grid_renderer.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "media_session.hpp"
#include "navigation_stack.hpp"
#include "playback_continuation.hpp"
#include "playback_continuation_executor.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_resolution_executor.hpp"
#include "playback_resolver.hpp"
#include "playback_session.hpp"
#include "playback_stream_executor.hpp"
#include "playback_telemetry.hpp"
#include "playback_telemetry_executor.hpp"
#include "playback_track_selection.hpp"
#include "playback_transition.hpp"
#include "player_controls_renderer.hpp"
#include "player_header_renderer.hpp"
#include "player_next_up_renderer.hpp"
#include "player_progress_renderer.hpp"
#include "player_screen.hpp"
#include "player_seek_feedback_renderer.hpp"
#include "player_skip_button_renderer.hpp"
#include "player_subtitle_renderer.hpp"
#include "player_trickplay_renderer.hpp"
#include "player_tracks.hpp"
#include "player_video_renderer.hpp"
#include "profiles_renderer.hpp"
#include "queue_overlay_renderer.hpp"
#include "queue_overlay_screen.hpp"
#include "quick_connect_executor.hpp"
#include "request_epoch.hpp"
#include "screensaver_policy.hpp"
#include "screensaver_renderer.hpp"
#include "search_renderer.hpp"
#include "search_screen.hpp"
#include "seerr.hpp"
#include "series_playback_executor.hpp"
#include "seerr_async_executor.hpp"
#include "seerr_domain.hpp"
#include "seerr_drive_picker_renderer.hpp"
#include "seerr_drive_picker_screen.hpp"
#include "seerr_home_projection.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "server_info_executor.hpp"
#include "session_registry.hpp"
#include "session_store.hpp"
#include "settings_screen.hpp"
#include "settings_renderer.hpp"
#include "status_overlay_renderer.hpp"
#include "subtitle_load_executor.hpp"
#include "system_text_input.hpp"
#include "ui_theme.hpp"
#include "ui_components.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"
#include "unicode_text.hpp"
#include "renderer.hpp"
#include "task_runner.hpp"
#include "trickplay_policy.hpp"
#include "trickplay_preview.hpp"
#include "trickplay_tile_executor.hpp"
#include "video_surface.hpp"
#include "version_policy.hpp"

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

constexpr Color kBackground = material_tv::background;
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
constexpr Color kDivider = material_tv::outlineVariant;
constexpr Color kTrack = material_tv::track;
constexpr Color kScrim = material_tv::scrim;
constexpr Color kBrandGold = material_tv::tertiary;
constexpr Color kError = material_tv::error;

void logPlaybackReportFailure(const char* stage, const std::string& itemId, const ApiResult& result) {
    if (result.ok) return;
    __android_log_print(ANDROID_LOG_WARN, kTag, "Playback %s report failed for %s: %s", stage, itemId.c_str(),
                        result.error.c_str());
}

bool isTransientHomeLoadError(std::string_view error) {
    return error.find("Server hostname could not be resolved") != std::string_view::npos ||
           error.find("Server connection timed out") != std::string_view::npos ||
           error.find("Unable to connect to server") != std::string_view::npos ||
           error.find("HTTP 408") != std::string_view::npos || error.find("HTTP 425") != std::string_view::npos ||
           error.find("HTTP 429") != std::string_view::npos || error.find("HTTP 5") != std::string_view::npos;
}

std::string episodeNumberLabel(const JellyfinItem& item) {
    if (item.type != "Episode") return {};
    std::string result;
    if (item.parentIndexNumber >= 0) result += "S" + std::to_string(item.parentIndexNumber);
    if (item.indexNumber >= 0) result += "E" + std::to_string(item.indexNumber);
    return result;
}

std::string episodeLabel(const JellyfinItem& item) {
    std::string result = item.seriesName;
    const std::string number = episodeNumberLabel(item);
    if (!number.empty()) {
        if (!result.empty()) result += " - ";
        result += number;
    }
    return result;
}

std::string formatLocalClock(std::time_t instant, bool clock24Hour) {
    std::tm local{};
    localtime_r(&instant, &local);
    std::ostringstream out;
    out << std::put_time(&local, clock24Hour ? "%H:%M" : "%I:%M %p");
    std::string value = out.str();
    if (!clock24Hour && !value.empty() && value.front() == '0') value.erase(value.begin());
    return value;
}

std::string formatPlaybackTime(int milliseconds) {
    const int totalSeconds = std::max(0, milliseconds / 1000);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;
    std::ostringstream out;
    if (hours > 0)
        out << hours << ':' << std::setw(2) << std::setfill('0') << minutes;
    else
        out << minutes;
    out << ':' << std::setw(2) << std::setfill('0') << seconds;
    return out.str();
}

enum class Screen {
    Login,
    Profiles,
    Home,
    Browse,
    Search,
    Settings,
    Diagnostics,
    Details,
    Cast,
    PersonItems,
    ItemMenu,
    Seasons,
    Episodes,
    SeerrDrivePicker,
    Player,
};

enum class KeyAction {
    Insert,
    Backspace,
    Done,
};

struct VirtualKey {
    std::string label;
    std::string value;
    KeyAction action = KeyAction::Insert;
};

constexpr int kTextInputSearch = 1;
constexpr int kTextInputSettingsSearch = 2;
constexpr int kTextInputLoginServer = 10;
constexpr int kTextInputLoginPassword = 12;
constexpr int kTextInputSeerrServer = 20;
constexpr int kTextInputSeerrApiKey = 21;

const std::vector<std::vector<VirtualKey>>& keyboardRows() {
    static const std::vector<std::vector<VirtualKey>> rows = {
        {{"A", "A"},
         {"B", "B"},
         {"C", "C"},
         {"D", "D"},
         {"E", "E"},
         {"F", "F"},
         {"G", "G"},
         {"H", "H"},
         {"I", "I"},
         {"J", "J"}},
        {{"K", "K"},
         {"L", "L"},
         {"M", "M"},
         {"N", "N"},
         {"O", "O"},
         {"P", "P"},
         {"Q", "Q"},
         {"R", "R"},
         {"S", "S"},
         {"T", "T"}},
        {{"U", "U"},
         {"V", "V"},
         {"W", "W"},
         {"X", "X"},
         {"Y", "Y"},
         {"Z", "Z"},
         {"0", "0"},
         {"1", "1"},
         {"2", "2"},
         {"3", "3"}},
        {{"4", "4"},
         {"5", "5"},
         {"6", "6"},
         {"7", "7"},
         {"8", "8"},
         {"9", "9"},
         {".", "."},
         {"-", "-"},
         {"_", "_"},
         {"/", "/"}},
        {{":", ":"}, {"@", "@"}, {"SPACE", " "}, {"BACK", "", KeyAction::Backspace}, {"DONE", "", KeyAction::Done}},
    };
    return rows;
}

char keyCodeToChar(int32_t keyCode, int32_t metaState) {
    const bool shift = (metaState & AMETA_SHIFT_ON) != 0;
    if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z) {
        const char base = static_cast<char>('a' + (keyCode - AKEYCODE_A));
        return shift ? static_cast<char>(base - 'a' + 'A') : base;
    }
    if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9) {
        static constexpr char shifted[] = ")!@#$%^&*(";
        const int index = keyCode - AKEYCODE_0;
        return shift ? shifted[index] : static_cast<char>('0' + index);
    }
    switch (keyCode) {
    case AKEYCODE_SPACE:
        return ' ';
    case AKEYCODE_PERIOD:
        return shift ? '>' : '.';
    case AKEYCODE_COMMA:
        return shift ? '<' : ',';
    case AKEYCODE_SLASH:
        return shift ? '?' : '/';
    case AKEYCODE_BACKSLASH:
        return shift ? '|' : '\\';
    case AKEYCODE_MINUS:
        return shift ? '_' : '-';
    case AKEYCODE_EQUALS:
        return shift ? '+' : '=';
    case AKEYCODE_SEMICOLON:
        return shift ? ':' : ';';
    case AKEYCODE_APOSTROPHE:
        return shift ? '"' : '\'';
    case AKEYCODE_AT:
        return '@';
    default:
        return 0;
    }
}

struct PendingTickWork {
    std::optional<ExternalPlayerResult> externalResult;
    std::optional<ExternalPlaybackLaunch> completedExternalPlayback;
    std::optional<ExternalPlaybackLaunch> externalLaunch;
    std::optional<PendingPlaybackTransition> playbackTransition;
};

struct PlayedRollbackState {
    std::string itemId;
    uint64_t sessionEpoch = 0;
    JellyfinHomeData previousHome;
    HomeSelectionSnapshot previousHomeSelection;
};

using QueuedPlaybackCompletion = QueuedPlaybackResolutionCompletion<Screen>;

using AsyncCompletion = std::variant<SystemTextInputEvent, SeerrDeleteCompletion, SeerrRequestCompletion,
                                     SeerrStorageRefreshCompletion,
                                     SeerrPendingRefreshCompletion, SeerrSearchCompletion, SeerrConnectCompletion,
                                     JellyfinSearchCompletion, ItemMenuDetailCompletion, PersonItemsCompletion,
                                     DiagnosticsCompletion, SeasonsCompletion, EpisodesCompletion, BrowsePageCompletion,
                                     ServerInfoNoticeCompletion, FavoriteCompletion, PlayedCompletion,
                                     MetadataRefreshCompletion, DeleteItemCompletion, DiscoveryCompletion,
                                     LoginCompletion, DetailsItemCompletion, DetailsSimilarCompletion,
                                     EpisodeSeriesContextRequestCompletion, EpisodeSeriesContextCompletion,
                                     QuickConnectStartedCompletion,
                                     QuickConnectFailedCompletion, QuickConnectAuthenticatedCompletion,
                                     QuickConnectTimedOutCompletion, HomeCoreCompletion, HomeSecondaryCompletion,
                                     ExternalPlaybackCompletion, SubtitleLoadCompletion, TrickplayTileCompletion,
                                     MediaSegmentsCompletion, NextEpisodeCompletion, PlaybackAdjacentCompletion,
                                     PlaybackReportCompletion, QueuedPlaybackCompletion, PlayerItemPlaybackCompletion,
                                     AutoplayPlaybackCompletion, StreamRestartCompletion, FallbackPlaybackCompletion,
                                     BeginPlaybackCompletion, SeriesPlayAllCompletion>;

class SloppaApp;
SloppaApp* gActiveApp = nullptr;
std::mutex gActiveAppMutex;

class SloppaApp {
public:
    explicit SloppaApp(android_app* app)
        : app_(app), renderer_(app->activity->vm, app->activity->clazz), api_(app->activity->vm, app->activity->clazz),
          seerr_(app->activity->vm, app->activity->clazz), seerrSearch_(app->activity->vm, app->activity->clazz),
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
          accountAsync_(api_, tasks_, asyncCompletions_),
          quickConnectAsync_(api_, tasks_, asyncCompletions_),
          detailsAsync_(api_, tasks_, asyncCompletions_),
          browseAsync_(api_, tasks_, asyncCompletions_),
          jellyfinSearchAsync_(api_, tasks_, asyncCompletions_),
          serverInfoAsync_(api_, tasks_, asyncCompletions_),
          itemMutationAsync_(api_, tasks_, asyncCompletions_),
          homeAsync_(api_, tasks_, asyncCompletions_),
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
              api_, tasks_, asyncCompletions_, requestEpochs_.playback, [](const SeriesEpisodeSlot& slot) {
                  __android_log_print(
                      ANDROID_LOG_WARN, kTag,
                      "No available source found for duplicate S%02dE%02d slot; retaining first server result",
                      slot.season, slot.episode);
              }),
          subtitleLoadAsync_(
              api_, tasks_, asyncCompletions_, requestEpochs_.playback, [](const SubtitleLoadDiagnostic& diagnostic) {
                  if (diagnostic.kind == SubtitleLoadDiagnosticKind::Fallback) {
                      __android_log_print(ANDROID_LOG_INFO, kTag,
                                          "Subtitle stream %d unavailable; using fallback stream %d",
                                          diagnostic.requestedSubtitleIndex, diagnostic.subtitleIndex);
                      return;
                  }
                  __android_log_print(ANDROID_LOG_WARN, kTag,
                                      "Subtitle load failed item=%s stream=%d codec=%s reason=%s",
                                      diagnostic.itemId.c_str(), diagnostic.subtitleIndex, diagnostic.codec.c_str(),
                                      diagnostic.reason.c_str());
              }),
          trickplayTileAsync_(api_, imageDecoder_, tasks_, asyncCompletions_),
          seerrAsync_(seerr_, seerrSearch_, api_, tasks_, asyncCompletions_),
          artwork_(api_, seerr_, imageDecoder_, tasks_, stateMutex_,
                   [](const HomeArtworkRequest& request, const ArtworkLoadResult& loaded) {
                       if (loaded.ok()) return;
                       __android_log_print(ANDROID_LOG_WARN, kTag,
                                           loaded.failure == ArtworkLoadFailure::Download
                                               ? "Home artwork download failed item=%s type=%s reason=%s"
                                               : "Home artwork decode failed item=%s type=%s reason=%s",
                                           request.itemId.c_str(), request.itemType.c_str(), loaded.error.c_str());
                   }),
          searchState_(seerrDomain_.searchResults()) {
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
        seerr_.cancelPendingRequests();
        seerrSearch_.cancelPendingRequests();
        requestEpochs_.invalidateAll();
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
                if (!noticePersistent_ && !notice_.empty()) tightenTimeoutUntil(noticeUntil_);
                if (!presentedError_.empty()) tightenTimeoutUntil(errorUntil_);
                tightenTimeoutUntil(homeRetryAt_);
                if (!seerrDomain_.pendingRequestsLoading() &&
                    (screen_ == Screen::Home || (screen_ == Screen::ItemMenu && isSeerrItem(detail_)))) {
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
            if (systemTextInputMode_ >= 0) return;
            renderBurstUntil_ = now + 150ms;
            lastInteraction_ = now;
            if (screensaverActive_) {
                screensaverActive_ = false;
            } else if (queueState_.overlayActive()) {
                handleQueueOverlayKey(AKEYCODE_BACK);
            } else if (screen_ == Screen::Player) {
                handlePlayerKey(AKEYCODE_BACK);
            } else {
                switch (screen_) {
                case Screen::Login:
                    handleLoginKey(AKEYCODE_BACK);
                    break;
                case Screen::Profiles:
                    handleProfilesKey(AKEYCODE_BACK);
                    break;
                case Screen::Home:
                    handleHomeKey(AKEYCODE_BACK);
                    break;
                case Screen::Browse:
                    handleBrowseKey(AKEYCODE_BACK);
                    break;
                case Screen::Search:
                    handleSearchKey(AKEYCODE_BACK);
                    break;
                case Screen::Settings:
                    handleSettingsKey(AKEYCODE_BACK);
                    break;
                case Screen::Diagnostics:
                    handleDiagnosticsKey(AKEYCODE_BACK);
                    break;
                case Screen::Details:
                    handleDetailsKey(AKEYCODE_BACK);
                    break;
                case Screen::Cast:
                    handleCastKey(AKEYCODE_BACK);
                    break;
                case Screen::PersonItems:
                    handlePersonItemsKey(AKEYCODE_BACK);
                    break;
                case Screen::ItemMenu:
                    handleItemMenuKey(AKEYCODE_BACK);
                    break;
                case Screen::Seasons:
                    handleSeasonsKey(AKEYCODE_BACK);
                    break;
                case Screen::Episodes:
                    handleEpisodesKey(AKEYCODE_BACK);
                    break;
                case Screen::SeerrDrivePicker:
                    handleSeerrDrivePickerKey(AKEYCODE_BACK);
                    break;
                case Screen::Player:
                    break;
                }
            }
        }
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

private:
    void queueSystemTextInputEvent(SystemTextInputPhase phase, int mode, std::string value) {
        asyncCompletions_.push(systemTextInputEvent(phase, mode, std::move(value)));
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void applySystemTextInputChanged(int mode, const std::string& text) {
        if (mode == kTextInputSearch) {
            searchState_.setQuery(text);
            searchState_.setKeyboard(false);
            scheduleLiveSearch();
        } else if (mode == kTextInputSettingsSearch) {
            settingsScreen_.setSearchText(text);
        } else if (mode == kTextInputSeerrServer) {
            settings_.seerrServer = text;
        } else if (mode == kTextInputSeerrApiKey) {
            settings_.seerrApiKey = text;
        } else if (mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword) {
            accountState_.setField(mode - kTextInputLoginServer, text);
        }
        systemTextInputMode_ = mode;
        renderBurstUntil_ = std::chrono::steady_clock::now() + 300ms;
    }

    void applySystemTextInputCancelled(int mode, const std::string& text) {
        systemTextInputMode_ = -1;
        if (mode == kTextInputSearch) {
            searchState_.setQuery(text);
            searchState_.setKeyboard(false);
            scheduleLiveSearch();
        } else if (mode == kTextInputSettingsSearch) {
            settingsScreen_.setSearchText(text);
        } else if (mode == kTextInputSeerrServer) {
            settings_.seerrServer = systemTextInputOriginal_;
            systemTextInputOriginal_.clear();
        } else if (mode == kTextInputSeerrApiKey) {
            settings_.seerrApiKey = systemTextInputOriginal_;
            systemTextInputOriginal_.clear();
        } else if (mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword) {
            accountState_.setField(mode - kTextInputLoginServer, text);
        }
        renderBurstUntil_ = std::chrono::steady_clock::now() + 300ms;
    }

    void applySystemTextInputDone(int mode, const std::string& text) {
        systemTextInputMode_ = -1;
        if (mode == kTextInputSearch) {
            searchState_.setQuery(text);
            searchState_.setKeyboard(false);
            searchState_.cancelPending();
            seerrDomain_.cancelSearch();
            searchAsync();
        } else if (mode == kTextInputSettingsSearch) {
            settingsScreen_.setSearchText(text);
        } else if (mode == kTextInputSeerrServer) {
            const bool changed = settings_.seerrServer != text;
            settings_.seerrServer = text;
            if (changed) {
                settings_.seerrSessionCookie.clear();
                seerrDomain_.invalidateStorageTargets();
            }
            systemTextInputOriginal_.clear();
            saveSession(session_);
            showNotice(settings_.seerrServer.empty() ? "SEERR DISCONNECTED" : "SEERR SERVER SAVED", 3s);
            refreshSeerrPendingAsync();
            refreshSeerrStorageAsync(true);
        } else if (mode == kTextInputSeerrApiKey) {
            settings_.seerrApiKey = text;
            systemTextInputOriginal_.clear();
            saveSession(session_);
            showNotice(settings_.seerrApiKey.empty() ? "SEERR API KEY CLEARED" : "SEERR API KEY SAVED", 3s);
            refreshSeerrPendingAsync();
            refreshSeerrStorageAsync(true);
        } else if (mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword) {
            const int field = mode - kTextInputLoginServer;
            accountState_.finishTextField(field, text);
        }
        renderBurstUntil_ = std::chrono::steady_clock::now() + 500ms;
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
                refreshPlaybackTelemetry(true);
                playerScreenState_.beginWindowRestore(suspendPlan.resumePlayback);
                if (suspendPlan.resumePlayback) player_.pause();
                reportProgressAsync(true);
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
        if (systemTextInputMode_ >= 0) return 0;

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

        const char typed = keyCodeToChar(key, meta);
        if (typed != 0 && handleTypedCharacter(typed)) return 1;
        if (key == AKEYCODE_DEL && handleBackspace()) return 1;

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
        return 1;
    }

    bool handleTypedCharacter(char c) {
        if (screen_ == Screen::Login && accountState_.loginFocus() >= 0 && accountState_.loginFocus() < 3) {
            accountState_.appendToFocusedField(c);
            return true;
        }
        if (screen_ == Screen::Search) {
            searchState_.append(c);
            scheduleLiveSearch();
            return true;
        }
        return false;
    }

    bool handleBackspace() {
        if (screen_ == Screen::Login && accountState_.backspaceFocusedField()) return true;
        if (screen_ == Screen::Search && searchState_.backspace()) {
            scheduleLiveSearch();
            return true;
        }
        return false;
    }

    void scheduleLiveSearch() {
        const auto now = std::chrono::steady_clock::now();
        requestEpochs_.search.invalidate();
        const bool seerrConfigured = SeerrClient::configured(settings_.seerrServer, seerrAuth());
        if (seerrDomain_.scheduleSearch(searchState_.query(), now, seerrConfigured)) {
            searchState_.refreshSeerrResults();
            requestEpochs_.seerrSearch.invalidate();
            seerrSearch_.cancelPendingRequests();
        }
        if (!searchState_.scheduleDebounce(now)) error_.clear();
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void runDueLiveSearch() {
        std::scoped_lock lock(stateMutex_);
        if (screen_ != Screen::Search) {
            searchState_.cancelPending();
            seerrDomain_.cancelSearch();
            requestEpochs_.search.invalidate();
            requestEpochs_.seerrSearch.invalidate();
            seerrSearch_.cancelPendingRequests();
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (searchState_.debounceDue(now)) searchAsync(false);
        if (seerrDomain_.searchDebounceDue(now)) searchSeerrAsync(false);
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
        if (shown == JNI_TRUE) {
            systemTextInputMode_ = mode;
            if (mode == kTextInputSeerrServer || mode == kTextInputSeerrApiKey) {
                systemTextInputOriginal_ = initial;
            }
        }
        return shown == JNI_TRUE;
    }

    void hideSystemTextInput() {
        systemTextInputMode_ = -1;
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
        if (accountState_.quickConnectActive()) {
            if (key == AKEYCODE_BACK) {
                api_.cancelPendingRequests();
                requestEpochs_.auth.invalidate();
                accountState_.endQuickConnect(true);
                loading_ = false;
                error_.clear();
            }
            return;
        }
        if (key == AKEYCODE_BACK) {
            if (accountState_.keyboardActive())
                accountState_.setKeyboardActive(false);
            else
                ANativeActivity_finish(app_->activity);
            return;
        }
        if (accountState_.keyboardActive()) {
            if (key == AKEYCODE_DPAD_LEFT)
                moveKeyboard(-1, 0);
            else if (key == AKEYCODE_DPAD_RIGHT)
                moveKeyboard(1, 0);
            else if (key == AKEYCODE_DPAD_UP)
                moveKeyboard(0, -1);
            else if (key == AKEYCODE_DPAD_DOWN)
                moveKeyboard(0, 1);
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
                activateKeyboardKey(false);
            return;
        }

        LoginFormInput input = LoginFormInput::None;
        if (key == AKEYCODE_DPAD_UP)
            input = LoginFormInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = LoginFormInput::Down;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = LoginFormInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = LoginFormInput::Right;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = LoginFormInput::Activate;

        const LoginFormCommand command = accountState_.handleLoginFormInput(input, !sessionRegistry_.empty());
        if (command.type == LoginFormCommandType::EditField) {
            const int field = command.fieldIndex;
            const int mode = kTextInputLoginServer + field;
            static constexpr std::array<const char*, 3> hints{"Jellyfin server URL", "Jellyfin username",
                                                              "Jellyfin password"};
            accountState_.setKeyboardActive(!showSystemTextInput(accountState_.field(field),
                                                                 hints[static_cast<size_t>(field)], mode,
                                                                 field == AccountScreenState::kPasswordField));
            if (accountState_.keyboardActive()) keyboardRow_ = keyboardCol_ = 0;
        } else if (command.type == LoginFormCommandType::Login) {
            loginAsync();
        } else if (command.type == LoginFormCommandType::QuickConnect) {
            quickConnectAsync();
        } else if (command.type == LoginFormCommandType::Discover) {
            discoverServersAsync();
        } else if (command.type == LoginFormCommandType::OpenProfiles) {
            openProfiles();
        }
    }

    void handleProfilesKey(int32_t key) {
        ProfilesScreenInput input = ProfilesScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = ProfilesScreenInput::Back;
        else if (key == AKEYCODE_DPAD_UP)
            input = ProfilesScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = ProfilesScreenInput::Down;
        else if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT)
            input = ProfilesScreenInput::Horizontal;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = ProfilesScreenInput::Activate;

        const int savedCount = static_cast<int>(sessionRegistry_.size());
        const ProfilesScreenCommand command = accountState_.handleProfilesInput(input, savedCount);
        if (command.type == ProfilesScreenCommandType::Back) {
            popScreen(Screen::Login);
        } else if (command.type == ProfilesScreenCommandType::AddAccount) {
            startAddAccount();
        } else if (command.type == ProfilesScreenCommandType::SwitchSession) {
            switchSavedSession(static_cast<size_t>(command.sessionIndex));
        } else if (command.type == ProfilesScreenCommandType::ForgetSession) {
            forgetSavedSession(static_cast<size_t>(command.sessionIndex));
        }
    }

    bool isItemContextKey(int32_t key) const { return key == AKEYCODE_MENU || key == AKEYCODE_INFO; }

    DetailGridScreenInput detailGridInputForKey(int32_t key) const {
        if (key == AKEYCODE_BACK) return DetailGridScreenInput::Back;
        if (isItemContextKey(key)) return DetailGridScreenInput::Context;
        if (key == AKEYCODE_DPAD_LEFT) return DetailGridScreenInput::Left;
        if (key == AKEYCODE_DPAD_RIGHT) return DetailGridScreenInput::Right;
        if (key == AKEYCODE_DPAD_UP) return DetailGridScreenInput::Up;
        if (key == AKEYCODE_DPAD_DOWN) return DetailGridScreenInput::Down;
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) return DetailGridScreenInput::Activate;
        return DetailGridScreenInput::None;
    }

    bool supportsItemContextMenu(const JellyfinItem& item) const {
        return item.type == "Movie" || item.type == "Series" || item.type == "Episode" || item.type == "BoxSet";
    }

    void openItemMenuForItem(const JellyfinItem& item) {
        if (item.id.empty() || !supportsItemContextMenu(item)) return;
        detail_ = item;
        pushScreen(Screen::ItemMenu);
        detailsState_.beginItemMenu();
        error_.clear();
        if (isSeerrItem(item)) return;

        const JellyfinSession session = session_;
        const std::string itemId = item.id;
        detailsAsync_.loadItemMenuDetail(session, itemId);
    }

    int homeVisibleItemCount(const JellyfinHomeRow& row) const { return row.title == "My Media" ? 4 : 5; }

    void beginHomeRowSlide(int fromFirst, int toFirst) {
        if (fromFirst == toFirst) return;
        const auto now = std::chrono::steady_clock::now();
        homeSlideFromFirst_ = fromFirst;
        homeSlideToFirst_ = toFirst;
        homeSlideStarted_ = now;
        renderBurstUntil_ = std::max(renderBurstUntil_, now + 240ms);
    }

    void handleHomeKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            ANativeActivity_finish(app_->activity);
            return;
        }
        if (key == AKEYCODE_SEARCH) {
            openSearch();
            return;
        }
        if (homeState_.row() < 0) {
            HomeToolbarInput input = HomeToolbarInput::None;
            if (key == AKEYCODE_DPAD_LEFT)
                input = HomeToolbarInput::Left;
            else if (key == AKEYCODE_DPAD_RIGHT)
                input = HomeToolbarInput::Right;
            else if (key == AKEYCODE_DPAD_DOWN)
                input = HomeToolbarInput::Down;
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
                input = HomeToolbarInput::Activate;

            const HomeToolbarCommand command =
                homeState_.handleToolbarInput(input, static_cast<int>(home_.rows.size()));
            if (command.type == HomeToolbarCommandType::OpenProfiles)
                openProfiles();
            else if (command.type == HomeToolbarCommandType::OpenSearch)
                openSearch();
            else if (command.type == HomeToolbarCommandType::OpenSettings)
                openSettings();
            homeState_.updateViewport(static_cast<int>(home_.rows.size()));
            return;
        }
        if (home_.rows.empty() || homeState_.row() >= static_cast<int>(home_.rows.size())) {
            homeState_.focusToolbar(homeState_.navIndex());
            return;
        }

        const int rowIndex = homeState_.row();
        const int previousFirstVisibleRow = homeState_.firstVisibleRow();
        auto& section = home_.rows[static_cast<size_t>(rowIndex)];
        auto& items = section.items;

        HomeRowInput input = HomeRowInput::None;
        if (key == AKEYCODE_DPAD_LEFT)
            input = HomeRowInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = HomeRowInput::Right;
        else if (key == AKEYCODE_DPAD_UP)
            input = HomeRowInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = HomeRowInput::Down;
        else if (isItemContextKey(key))
            input = HomeRowInput::Context;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = HomeRowInput::Activate;

        const HomeRowCommand command =
            homeState_.handleRowInput(input, static_cast<int>(home_.rows.size()), static_cast<int>(items.size()),
                                      section.title != "My Media");
        if (command.type == HomeRowCommandType::OpenContext) {
            const int selection = homeState_.selection(rowIndex, static_cast<int>(items.size()));
            openItemMenuForItem(items[static_cast<size_t>(selection)]);
            return;
        }
        if (command.type == HomeRowCommandType::OpenSelected) {
            const int selection = homeState_.selection(rowIndex, static_cast<int>(items.size()));
            const auto& selected = items[static_cast<size_t>(selection)];
            if (section.title == "My Media") {
                openLibrary(selected);
            } else if (const auto* seerrMedia =
                           findSeerrHomeMedia(section.title, selected.id, seerrDomain_.pendingRequests())) {
                if (!seerrMedia->jellyfinId.empty()) {
                    JellyfinItem available = selected;
                    available.id = seerrMedia->jellyfinId;
                    available.externalSource.clear();
                    openDetails(available);
                } else {
                    openItemMenuForItem(selected);
                }
            } else {
                openDetails(selected);
            }
            return;
        }
        homeState_.updateViewport(static_cast<int>(home_.rows.size()));
        beginHomeRowSlide(previousFirstVisibleRow, homeState_.firstVisibleRow());
        if (homeState_.row() >= 0 && homeState_.row() < static_cast<int>(homeState_.selectionCount())) {
            const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
            homeState_.updateItemViewport(homeState_.row(), static_cast<int>(row.items.size()),
                                          homeVisibleItemCount(row));
            prefetchHomeWindow(homeState_.row(),
                               homeState_.selection(homeState_.row(), static_cast<int>(row.items.size())));
        }
    }

    bool isBrowsableContainer(const JellyfinItem& item) const {
        return item.type == "Folder" || item.type == "BoxSet" || item.type == "CollectionFolder";
    }

    void cancelContentLoadForNavigation() {
        if (!loading_) return;
        requestEpochs_.content.invalidate();
        loading_ = false;
    }

    void handleBrowseKey(int32_t key) {
        BrowseScreenInput input = BrowseScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = BrowseScreenInput::Back;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = BrowseScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = BrowseScreenInput::Right;
        else if (key == AKEYCODE_DPAD_UP)
            input = BrowseScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = BrowseScreenInput::Down;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = BrowseScreenInput::Activate;
        else if (isItemContextKey(key))
            input = BrowseScreenInput::Context;

        if (input == BrowseScreenInput::Back) cancelContentLoadForNavigation();
        constexpr int columns = mediaGridColumns();
        const BrowseScreenCommand command = browseState_.handleInput(input, columns);

        if (command.type == BrowseScreenCommandType::Back) {
            if (command.backAction == BrowseBackAction::Reload)
                loadBrowsePageAsync(false);
            else if (command.backAction == BrowseBackAction::LocalPage) {
                loading_ = false;
                error_.clear();
            } else if (command.backAction == BrowseBackAction::Exit) {
                popScreen(Screen::Home);
                if (screen_ == Screen::Home) homeState_.focusToolbar(1);
            }
            return;
        }
        if (command.type == BrowseScreenCommandType::ApplyFilter) {
            applyBrowseFilter(browseState_.filterSelection());
            return;
        }
        if (command.type == BrowseScreenCommandType::OpenContext) {
            const auto& selected = browseState_.items()[static_cast<size_t>(browseState_.selection())];
            if (supportsItemContextMenu(selected)) openItemMenuForItem(selected);
            return;
        }
        if (command.type == BrowseScreenCommandType::OpenSelected) {
            const auto selected = browseState_.items()[static_cast<size_t>(browseState_.selection())];
            if (selected.type == "Genre") {
                browseState_.selectGenre(selected.name);
                loadBrowsePageAsync(false);
            } else if (selected.type == "Letter") {
                browseState_.selectLetter(selected.name);
                loadBrowsePageAsync(false);
            } else if (isBrowsableContainer(selected)) {
                openBrowseContainer(selected, true);
            } else {
                openDetails(selected);
            }
            return;
        }
        if (command.type == BrowseScreenCommandType::SelectionChanged) {
            const auto& items = browseState_.items();
            prefetchBrowseArtworkAhead();
            if (browseState_.hasMore() && !loading_ &&
                browseState_.selection() >= static_cast<int>(items.size()) - 12) {
                loadMoreBrowseAsync();
            }
        }
    }

    void handleSearchKey(int32_t key) {
        SearchScreenInput input = SearchScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = SearchScreenInput::Back;
        else if (key == AKEYCODE_SEARCH)
            input = SearchScreenInput::Search;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = SearchScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = SearchScreenInput::Right;
        else if (key == AKEYCODE_DPAD_UP)
            input = SearchScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = SearchScreenInput::Down;
        else if (key == AKEYCODE_DPAD_CENTER)
            input = SearchScreenInput::Activate;
        else if (key == AKEYCODE_ENTER)
            input = SearchScreenInput::Submit;
        else if (isItemContextKey(key))
            input = SearchScreenInput::Context;
        else
            return;

        constexpr int columns = mediaGridColumns();
        const SearchScreenCommand command = searchState_.handleInput(input, columns);
        switch (command.type) {
        case SearchScreenCommandType::None:
            return;
        case SearchScreenCommandType::Exit:
            searchState_.cancelPending();
            seerrDomain_.cancelSearch();
            requestEpochs_.search.invalidate();
            requestEpochs_.seerrSearch.invalidate();
            seerrSearch_.cancelPendingRequests();
            hideSystemTextInput();
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) {
                homeState_.setRow(0);
                homeState_.updateViewport(static_cast<int>(home_.rows.size()));
            }
            return;
        case SearchScreenCommandType::SubmitSearch:
            searchAsync();
            return;
        case SearchScreenCommandType::MoveKeyboard:
            moveKeyboard(command.dx, command.dy);
            return;
        case SearchScreenCommandType::ActivateKeyboard:
            activateKeyboardKey(true);
            return;
        case SearchScreenCommandType::OpenTextInput:
            searchState_.setKeyboard(
                !showSystemTextInput(searchState_.query(), "Search Jellyfin & Seerr", kTextInputSearch));
            if (searchState_.keyboard()) keyboardRow_ = keyboardCol_ = 0;
            return;
        case SearchScreenCommandType::OpenContext: {
            const auto& results = searchState_.results();
            const auto& selected = results[static_cast<size_t>(searchState_.selection())];
            openItemMenuForItem(selected);
            return;
        }
        case SearchScreenCommandType::OpenDetails: {
            const auto& results = searchState_.results();
            openDetails(results[static_cast<size_t>(searchState_.selection())]);
            return;
        }
        case SearchScreenCommandType::RequestSeerr:
            if (const auto* seerrItem = searchState_.selectedSeerrResult()) requestSeerrMediaAsync(*seerrItem);
            return;
        }
    }

    std::vector<std::string> detailActions() const {
        return detailsState_.actions(detail_, playbackCoordinator_.continuation().stillWatchingPrompt());
    }

    void refreshExternalPlayers() {
        externalPlayers_ = externalPlayer_.availablePlayers();
        if (settings_.externalPlayerComponent.empty()) return;
        const auto selected =
            std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
                return player.componentName == settings_.externalPlayerComponent;
            });
        if (selected == externalPlayers_.end()) settings_.externalPlayerComponent.clear();
    }

    std::string externalPlayerLabel() const {
        if (settings_.externalPlayerComponent.empty()) return "INTERNAL";
        const auto selected =
            std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
                return player.componentName == settings_.externalPlayerComponent;
            });
        return selected == externalPlayers_.end() ? "INTERNAL" : selected->label;
    }

    std::optional<ExternalPlayerApp> selectedExternalPlayer() const {
        if (settings_.externalPlayerComponent.empty()) return std::nullopt;
        const auto selected =
            std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
                return player.componentName == settings_.externalPlayerComponent;
            });
        if (selected == externalPlayers_.end()) return std::nullopt;
        return *selected;
    }

    void cycleExternalPlayer(int direction) {
        if (externalPlayers_.empty()) {
            settings_.externalPlayerComponent.clear();
            return;
        }
        int index = 0;
        if (!settings_.externalPlayerComponent.empty()) {
            const auto selected =
                std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
                    return player.componentName == settings_.externalPlayerComponent;
                });
            if (selected != externalPlayers_.end())
                index = static_cast<int>(std::distance(externalPlayers_.begin(), selected)) + 1;
        }
        index = std::clamp(index + direction, 0, static_cast<int>(externalPlayers_.size()));
        settings_.externalPlayerComponent =
            index == 0 ? std::string{} : externalPlayers_[static_cast<size_t>(index - 1)].componentName;
    }

    void toggleSubtitleLanguageSetting() {
        const int selection = settingsScreen_.subtitleLanguageSelection();
        if (selection <= 0) {
            settings_.subtitleLanguages.clear();
            saveSession(session_);
            return;
        }
        const std::string code = kSubtitleLanguageOptions[static_cast<size_t>(selection - 1)].code;
        auto current = std::find(settings_.subtitleLanguages.begin(), settings_.subtitleLanguages.end(), code);
        if (settings_.subtitleLanguages.empty() || current == settings_.subtitleLanguages.end()) {
            settings_.subtitleLanguages.push_back(code);
        } else {
            settings_.subtitleLanguages.erase(current);
        }
        saveSession(session_);
    }

    void handleSettingsKey(int32_t key) {
        SettingsScreenInput input = SettingsScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = SettingsScreenInput::Back;
        else if (key == AKEYCODE_SEARCH)
            input = SettingsScreenInput::Search;
        else if (key == AKEYCODE_DPAD_UP)
            input = SettingsScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = SettingsScreenInput::Down;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = SettingsScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = SettingsScreenInput::Right;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = SettingsScreenInput::Activate;
        else
            return;

        const SettingsScreenCommand command = settingsScreen_.handleInput(input);
        switch (command.type) {
        case SettingsScreenCommandType::None:
            return;
        case SettingsScreenCommandType::Exit:
            hideSystemTextInput();
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) homeState_.focusToolbar(3);
            return;
        case SettingsScreenCommandType::EditSearch:
            showSystemTextInput(settingsScreen_.searchQuery(), "Search settings", kTextInputSettingsSearch);
            return;
        case SettingsScreenCommandType::Adjust: {
            const SettingChangeEffect effects = adjustSetting(settings_, command.setting, command.direction);
            if (effects == SettingChangeEffect::None) return;
            if (hasSettingEffect(effects, SettingChangeEffect::ApplyVideoZoom))
                playbackCoordinator_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
            if (hasSettingEffect(effects, SettingChangeEffect::RestoreDisplayMode)) displayMode_.restore();
            if (hasSettingEffect(effects, SettingChangeEffect::ResetScreensaver)) {
                lastInteraction_ = std::chrono::steady_clock::now();
                screensaverActive_ = false;
            }
            if (hasSettingEffect(effects, SettingChangeEffect::CycleExternalPlayer))
                cycleExternalPlayer(command.direction);
            if (hasSettingEffect(effects, SettingChangeEffect::Save)) saveSession(session_);
            return;
        }
        case SettingsScreenCommandType::ActivateSetting:
            switch (settingActivation(command.setting)) {
            case SettingActivation::None:
                break;
            case SettingActivation::OpenDiagnostics:
                openDiagnostics();
                break;
            case SettingActivation::SwitchUser:
                openProfiles();
                break;
            case SettingActivation::OpenSubtitleLanguages:
                settingsScreen_.openSubtitleLanguagePicker();
                break;
            case SettingActivation::EditSeerrServer:
                showSystemTextInput(settings_.seerrServer, "Seerr server URL", kTextInputSeerrServer);
                break;
            case SettingActivation::ConnectSeerr:
                connectSeerrAsync();
                break;
            case SettingActivation::ToggleSeerrDriveSelection:
                settings_.seerrSelectDrive = !settings_.seerrSelectDrive;
                saveSession(session_);
                if (settings_.seerrSelectDrive) refreshSeerrStorageAsync(true);
                break;
            case SettingActivation::EditSeerrApiKey:
                showSystemTextInput(settings_.seerrApiKey, "Seerr API key", kTextInputSeerrApiKey, true);
                break;
            case SettingActivation::ToggleAdvanced:
                settingsScreen_.toggleAdvanced();
                break;
            }
            return;
        case SettingsScreenCommandType::ToggleSubtitleLanguage:
            toggleSubtitleLanguageSetting();
            return;
        }
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

    void handleDetailsKey(int32_t key) {
        DetailsScreenInput input = DetailsScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = DetailsScreenInput::Back;
        else if (isItemContextKey(key))
            input = DetailsScreenInput::Context;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = DetailsScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = DetailsScreenInput::Right;
        else if (key == AKEYCODE_DPAD_UP)
            input = DetailsScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = DetailsScreenInput::Down;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = DetailsScreenInput::Activate;

        const auto actions = detailActions();
        const DetailsScreenCommand command =
            detailsState_.handleInput(input, static_cast<int>(actions.size()), detail_.type == "Episode");

        if (command.type == DetailsScreenCommandType::Back) {
            cancelContentLoadForNavigation();
            playbackCoordinator_.resetContinuationPrompt();
            popScreen(Screen::Home);
            return;
        }
        if (command.type == DetailsScreenCommandType::OpenContext) {
            openItemMenu();
            return;
        }
        if (command.type == DetailsScreenCommandType::OpenEpisodeSeries) {
            const JellyfinItem series = detailsState_.seriesDetail();
            if (!series.id.empty()) openDetails(series, true);
            return;
        }
        if (command.type == DetailsScreenCommandType::OpenEpisodeSeason) {
            if (const auto* season = detailsState_.selectedEpisodeContextSeason()) openEpisodes(*season);
            return;
        }
        if (command.type == DetailsScreenCommandType::OpenSimilar) {
            if (const auto* selected = detailsState_.selectedSimilar()) openDetails(*selected);
            return;
        }
        if (command.type != DetailsScreenCommandType::ActivateAction) return;

        const std::string& action = actions[static_cast<size_t>(detailsState_.actionSelection())];
        if (action == "PLAY" || action == "RESUME" || action == "PLAY NEXT" || action == "KEEP WATCHING")
            beginPlayback();
        else if (action == "EPISODES")
            openSeasons();
        else if (action == "PLAY ALL")
            beginSeriesPlayAll();
        else if (action == "FAVORITE" || action == "UNFAVORITE")
            toggleFavoriteAsync();
        else if (action == "MARK WATCHED" || action == "MARK UNWATCHED")
            togglePlayedAsync();
        else if (action == "CAST")
            openCast();
        else if (action == "MORE")
            openItemMenu();
        else if (action == "BACK") {
            playbackCoordinator_.resetContinuationPrompt();
            popScreen(Screen::Home);
        }
    }

    void openCast() {
        if (detail_.people.empty()) return;
        pushScreen(Screen::Cast);
        detailsState_.resetCastSelection();
        error_.clear();
    }

    void handleCastKey(int32_t key) {
        CastScreenInput input = CastScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = CastScreenInput::Back;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = CastScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = CastScreenInput::Right;
        else if (key == AKEYCODE_DPAD_UP)
            input = CastScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = CastScreenInput::Down;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = CastScreenInput::Activate;

        constexpr int columns = mediaGridColumns();
        const CastScreenCommand command = detailsState_.handleCastInput(input, detail_.people, columns);
        if (command.type == CastScreenCommandType::Back) {
            popScreen(Screen::Details);
        } else if (command.type == CastScreenCommandType::OpenPerson) {
            if (const auto* person = detailsState_.selectedCastPerson(detail_.people)) openPersonItems(*person);
        }
    }

    void openPersonItems(const JellyfinPerson& person) {
        if (!session_.valid() || person.id.empty()) return;
        pushScreen(Screen::PersonItems);
        detailsState_.beginPerson(person);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string personId = person.id;
        const uint64_t generation = requestEpochs_.content.begin();
        detailsAsync_.loadPersonItems(session, personId, generation, 60);
    }

    void handlePersonItemsKey(int32_t key) {
        constexpr int columns = mediaGridColumns();
        const DetailGridScreenCommand command =
            detailsState_.handlePersonItemsInput(detailGridInputForKey(key), columns);
        if (command.type == DetailGridScreenCommandType::Back) {
            cancelContentLoadForNavigation();
            popScreen(Screen::Cast);
        } else if (command.type == DetailGridScreenCommandType::OpenContext) {
            if (const auto* item = detailsState_.selectedPersonItem()) openItemMenuForItem(*item);
        } else if (command.type == DetailGridScreenCommandType::OpenSelected) {
            if (const auto* item = detailsState_.selectedPersonItem()) openDetails(*item);
        }
    }

    std::vector<std::string> itemMenuActions() const {
        if (isSeerrItem(detail_)) return {"DELETE REQUEST", "BACK"};
        return detailsState_.itemMenuActions(detail_, selectedExternalPlayer().has_value(), !queueState_.empty(),
                                             isHiddenFromHome(detail_));
    }

    void openItemMenu() {
        if (detail_.id.empty()) return;
        pushScreen(Screen::ItemMenu);
        detailsState_.beginItemMenu();
        error_.clear();
    }

    void handleItemMenuKey(int32_t key) {
        ItemMenuScreenInput input = ItemMenuScreenInput::None;
        if (key == AKEYCODE_BACK)
            input = ItemMenuScreenInput::Back;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = ItemMenuScreenInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = ItemMenuScreenInput::Right;
        else if (key == AKEYCODE_DPAD_UP)
            input = ItemMenuScreenInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = ItemMenuScreenInput::Down;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = ItemMenuScreenInput::Activate;

        const auto actions = itemMenuActions();
        const ItemMenuScreenCommand command =
            detailsState_.handleItemMenuInput(input, static_cast<int>(actions.size()));
        if (command.type == ItemMenuScreenCommandType::Back) {
            popScreen(Screen::Details);
            return;
        }
        if (command.type == ItemMenuScreenCommandType::ConfirmDelete) {
            if (isSeerrItem(detail_))
                deleteSeerrRequestAsync();
            else
                deleteCurrentItemAsync();
            return;
        }
        if (command.type != ItemMenuScreenCommandType::ActivateAction) return;

        const std::string& action = actions[static_cast<size_t>(detailsState_.itemMenuSelection())];
        if (action == "PLAY ALL") {
            popScreen(Screen::Details);
            if (screen_ != Screen::Details) pushScreen(Screen::Details);
            beginSeriesPlayAll();
        } else if (action == "PLAY EXTERNAL") {
            popScreen(Screen::Details);
            if (screen_ != Screen::Details) pushScreen(Screen::Details);
            launchExternalPlaybackAsync();
        } else if (action == "VIEW QUEUE") {
            popScreen(Screen::Details);
            openQueueOverlay();
        } else if (action == "FAVORITE" || action == "UNFAVORITE") {
            popScreen(Screen::Details);
            toggleFavoriteAsync();
        } else if (action == "MARK WATCHED" || action == "MARK UNWATCHED") {
            popScreen(Screen::Details);
            togglePlayedAsync();
        } else if (action == "HIDE FROM HOME" || action == "SHOW ON HOME") {
            popScreen(Screen::Details);
            toggleHiddenFromHome();
        } else if (action == "REFRESH METADATA") {
            popScreen(Screen::Details);
            refreshCurrentItemMetadataAsync();
        } else if (action == "DELETE MEDIA" || action == "DELETE REQUEST") {
            detailsState_.setDeleteConfirmation(true);
        } else {
            popScreen(Screen::Details);
        }
    }

    void launchExternalPlaybackAsync() {
        if (loading_ || !session_.valid() || detail_.id.empty()) return;
        const auto player = selectedExternalPlayer();
        if (!player) {
            error_ = "EXTERNAL PLAYER IS NOT CONFIGURED";
            return;
        }

        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const PlaybackLanguagePreferences preferences = playbackCoordinator_.languagePreferences();
        const PlaybackTrackSelectionPolicy trackPolicy = playbackTrackSelectionPolicy();
        const uint64_t generation = requestEpochs_.playback.begin();
        if (!externalPlaybackAsync_.prepare(
                session, ExternalPlaybackRequest{
                             .generation = generation,
                             .selectedItemId = detail_.id,
                             .selectedItemType = detail_.type,
                             .seriesId = detail_.seriesId,
                             .container = detail_.container,
                             .mediaSourceId = detail_.mediaSourceId,
                             .audios = detail_.audios,
                             .subtitles = detail_.subtitles,
                             .player = *player,
                             .subtitlePreference = preferences.subtitle,
                             .trackPolicy = trackPolicy,
                         })) {
            loading_ = false;
            error_ = "EXTERNAL PLAYER COULD NOT BE STARTED";
        }
    }

    void moveGridSelectionByCount(int32_t key, int itemCount, int& selection) {
        int dx = 0;
        int dy = 0;
        if (key == AKEYCODE_DPAD_LEFT)
            dx = -1;
        else if (key == AKEYCODE_DPAD_RIGHT)
            dx = 1;
        else if (key == AKEYCODE_DPAD_UP)
            dy = -1;
        else if (key == AKEYCODE_DPAD_DOWN)
            dy = 1;
        else
            return;
        selection = gridSelectionAfterMove(selection, itemCount, dx, dy, mediaGridColumns());
    }

    void moveGridSelection(int32_t key, const std::vector<JellyfinItem>& items, int& selection) {
        moveGridSelectionByCount(key, static_cast<int>(items.size()), selection);
    }

    void handleSeasonsKey(int32_t key) {
        constexpr int columns = mediaGridColumns();
        const DetailGridScreenCommand command = detailsState_.handleSeasonsInput(detailGridInputForKey(key), columns);
        if (command.type == DetailGridScreenCommandType::Back) {
            cancelContentLoadForNavigation();
            detail_ = detailsState_.seriesDetail();
            popScreen(Screen::Details);
        } else if (command.type == DetailGridScreenCommandType::OpenSelected) {
            if (const auto* season = detailsState_.selectedSeasonItem()) openEpisodes(*season);
        }
    }

    void handleEpisodesKey(int32_t key) {
        constexpr int columns = mediaGridColumns();
        const DetailGridScreenCommand command = detailsState_.handleEpisodesInput(detailGridInputForKey(key), columns);
        if (command.type == DetailGridScreenCommandType::Back) {
            cancelContentLoadForNavigation();
            popScreen(Screen::Seasons);
        } else if (command.type == DetailGridScreenCommandType::OpenContext) {
            if (const auto* episode = detailsState_.selectedEpisodeItem()) openItemMenuForItem(*episode);
        } else if (command.type == DetailGridScreenCommandType::OpenSelected) {
            if (const auto* episode = detailsState_.selectedEpisodeItem()) openDetails(*episode);
        }
    }

    void refreshPlaybackTelemetry(bool force = false) {
        const auto now = std::chrono::steady_clock::now();
        const PlaybackTelemetryReadPlan plan =
            playbackCoordinator_.consumeTelemetryRead(now, force, playerScreenState_.durationMs());
        if (!plan.read) return;
        playerScreenState_.applyObservedPosition(player_.positionMs(), now);
        if (plan.knownDurationMs > 0) {
            playerScreenState_.setDurationMs(plan.knownDurationMs);
        } else if (plan.probeDuration) {
            const int duration = player_.durationMs();
            if (duration > 0) playerScreenState_.setDurationMs(duration);
        }
    }

    void cycleAudioTrack() {
        const PlaybackAudioCyclePlan plan = playbackCoordinator_.beginAudioTrackCycle(playbackTrackSelectionPolicy());
        if (!plan.available) {
            error_ = "ONLY ONE AUDIO TRACK";
            return;
        }

        refreshPlaybackTelemetry(true);
        const int switchPositionMs = playerScreenState_.positionMs();
        if (plan.tryEmbeddedSwitch && player_.selectEmbeddedAudioStream(plan.audioStreamIndex, plan.audioOrdinal)) {
            playbackCoordinator_.selectAudioStream(plan.audioStreamIndex);
            playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
            reportProgressAsync(false);
            return;
        }
        restartPlaybackAt(switchPositionMs, plan.audioStreamIndex, plan.subtitleStreamIndex);
    }

    PlaybackTrackSelectionPolicy playbackTrackSelectionPolicy() const {
        return {
            .autoSubtitles = settings_.autoSubtitles,
            .autoSubtitleLanguage = settings_.autoSubtitleLanguage,
            .autoSubtitleSourceLanguage = settings_.autoSubtitleSourceLanguage,
            .allowedSubtitleLanguages = settings_.subtitleLanguages,
        };
    }

    PlaybackResolutionOptions playbackResolutionOptions() const {
        const PlaybackLanguagePreferences preferences = playbackCoordinator_.languagePreferences();
        return {
            .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
            .maxAudioChannels = settings_.maxAudioChannels,
            .overrides = playbackOverridesFor(settings_),
            .audioLanguagePreference = preferences.audio,
            .subtitleLanguagePreference = preferences.subtitle,
            .trackPolicy = playbackTrackSelectionPolicy(),
        };
    }

    void loadSubtitleAsync(const JellyfinSubtitleStream& subtitle, const std::string& deliveryUrl = {}) {
        if (!session_.valid()) return;
        auto context = playbackCoordinator_.beginSubtitleLoadContext(subtitle, settings_.subtitleLanguages);
        if (!context) return;
        const JellyfinSession session = session_;
        const std::string dataPath = dataPath_;
        const uint64_t generation = requestEpochs_.playback.snapshot();

        if (!subtitleLoadAsync_.load(
                session, SubtitleLoadRequest{
                             .generation = generation,
                             .itemId = std::move(context->itemId),
                             .mediaSourceId = std::move(context->mediaSourceId),
                             .requestedSubtitleIndex = context->requestedSubtitleStreamIndex,
                             .candidates = std::move(context->candidates),
                             .deliveryUrl = deliveryUrl,
                             .dataPath = dataPath,
                         })) {
            playbackCoordinator_.failSubtitleLoad();
            showNotice("SUBTITLES COULD NOT BE STARTED");
        }
    }

    void cycleSubtitleTrack() {
        const PlaybackSubtitleCycleContext cycle =
            playbackCoordinator_.beginSubtitleTrackCycle(settings_.subtitleLanguages);
        const PlaybackSubtitleCyclePlan& plan = cycle.plan;
        if (cycle.busy) {
            if (plan.action == PlaybackSubtitleCycleAction::NoSubtitles) error_ = "NO SUBTITLE TRACKS";
            return;
        }
        if (plan.action == PlaybackSubtitleCycleAction::NoSubtitles) {
            error_ = "NO SUBTITLE TRACKS";
            return;
        }
        if (plan.action == PlaybackSubtitleCycleAction::NoAllowedTracks) {
            error_ = "NO ALLOWED SUBTITLE TRACKS";
            return;
        }

        if (plan.action == PlaybackSubtitleCycleAction::DisableInPlayer && player_.disableSubtitles()) {
            playbackCoordinator_.disableSubtitleRendering();
            playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
            reportProgressAsync(false);
            return;
        }

        if (plan.directPlayStream) {
            const JellyfinSubtitleStream& selected = *plan.directPlayStream;
            __android_log_print(ANDROID_LOG_INFO, kTag,
                                "Selecting subtitle stream=%d codec=%s external=%d strategy=%d",
                                plan.subtitleStreamIndex, selected.codec.c_str(), selected.isExternal ? 1 : 0,
                                static_cast<int>(plan.strategy));
            if (plan.action == PlaybackSubtitleCycleAction::LoadNative) {
                player_.disableSubtitles();
                playbackCoordinator_.prepareNativeSubtitleLoad(plan.subtitleStreamIndex);
                loadSubtitleAsync(selected);
                playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
                reportProgressAsync(false);
                return;
            }
            if (plan.strategy == SubtitleStrategy::ClientEmbedded) {
                __android_log_print(ANDROID_LOG_INFO, kTag,
                                    "Bitmap subtitle stream=%d requires server burn-in with mediacodec_embed",
                                    plan.subtitleStreamIndex);
            }
        }

        refreshPlaybackTelemetry(true);
        restartPlaybackAt(playerScreenState_.positionMs(), cycle.audioStreamIndex, plan.subtitleStreamIndex);
    }

    void restartPlaybackAt(int positionMs, int audioStreamIndex, int subtitleStreamIndex) {
        if (!session_.valid()) return;
        const int targetPositionMs = std::max(0, positionMs);
        const bool wasPaused = player_.status() == PlayerStatus::Paused;
        const JellyfinSession session = session_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        auto restartPlan = playbackCoordinator_.beginStreamRestart(targetPositionMs);
        if (!restartPlan) return;
        JellyfinItem item = std::move(restartPlan->item);
        const PlaybackTarget previousTarget = std::move(restartPlan->previousTarget);
        const bool shouldReportPrevious = restartPlan->reportPrevious;
        const uint64_t generation = requestEpochs_.playback.begin();

        // A Jellyfin server-stream change is a real playback-session handoff. Resolve the
        // replacement only after closing/reporting the old session: asking Jellyfin for a
        // second transcode while the first one is still active has produced stalled HLS
        // sessions (and, on some servers, PlaybackInfo HTTP 500 responses).
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        playerScreenState_.setPositionMs(targetPositionMs);
        player_.stop();
        videoSurface_.release();

        PlaybackStreamResolutionOptions options{
            .maxStreamingBitrate = maxStreamingBitrate,
            .maxAudioChannels = maxAudioChannels,
            .overrides = playbackOverrides,
            .audioStreamIndex = audioStreamIndex,
            .subtitleStreamIndex = subtitleStreamIndex,
        };
        playbackStreamAsync_.restart(session, std::move(item), previousTarget, shouldReportPrevious, std::move(options),
                                     wasPaused, generation);
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
        if (!trickplayTileAsync_.load(
                session, TrickplayTileRequest{
                             .itemId = playbackCoordinator_.session().activeItem().id,
                             .trickplay = info,
                             .tileIndex = frame.tileIndex,
                         })) {
            trickplayState_.markFailed();
        }
    }

    bool drawTrickplayPreview() {
        if (!trickplayState_.visible(std::chrono::steady_clock::now(), playbackCoordinator_.session().activeItem().id) ||
            !playbackCoordinator_.session().activeItem().trickplay.valid()) {
            return false;
        }
        const auto& info = playbackCoordinator_.session().activeItem().trickplay;
        const TrickplayFrame frame = trickplayFrameForPosition(trickplayState_.positionMs(), info.intervalMs,
                                                               info.thumbnailCount, info.tileWidth, info.tileHeight);
        if (!frame.valid() || frame.tileIndex != trickplayState_.tileIndex()) return false;
        if (trickplayState_.texture() == 0 || trickplayState_.textureGeneration() != renderer_.generation()) {
            const auto& decoded = trickplayState_.decoded();
            trickplayState_.setTexture(renderer_.createTexture(decoded.width, decoded.height, decoded.rgba.data()),
                                       renderer_.generation());
        }
        if (trickplayState_.texture() == 0) return false;

        const auto& decoded = trickplayState_.decoded();
        const std::string positionLabel = formatPlaybackTime(trickplayState_.positionMs());
        return renderPlayerTrickplay(
            renderer_,
            PlayerTrickplayRenderState{
                .texture = trickplayState_.texture(),
                .frame = frame,
                .info = info,
                .decodedWidth = decoded.width,
                .decodedHeight = decoded.height,
                .positionMs = trickplayState_.positionMs(),
                .durationMs = playerScreenState_.durationMs(),
                .logicalWidth = Renderer::logicalWidth(),
                .positionLabel = positionLabel,
            },
            PlayerTrickplayRenderStyle<Color>{
                .previewRadius = material_tv::cornerSmall,
                .backdrop = Color{0.0f, 0.0f, 0.0f, 0.90f},
                .focus = kFocus,
                .text = kText,
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            });
    }

    void seekPlaybackTo(int positionMs) {
        const int targetMs = std::max(0, positionMs);
        player_.seekTo(targetMs);
        playerScreenState_.beginSeek(targetMs, std::chrono::steady_clock::now());
    }

    std::string mediaSegmentSkipLabel(const JellyfinMediaSegment& segment) const {
        if (segment.type == "Intro") return "Skip intro";
        if (segment.type == "Outro") return "Skip credits";
        if (segment.type == "Recap") return "Skip recap";
        if (segment.type == "Preview") return "Skip preview";
        if (segment.type == "Commercial") return "Skip commercial";
        return "Skip";
    }

    bool skipActiveMediaSegment() {
        const auto targetMs = playbackCoordinator_.activeSkippableSegmentEndMs(playerScreenState_.positionMs());
        if (!targetMs) return false;
        seekPlaybackTo(*targetMs);
        reportProgressAsync(false);
        return true;
    }

    void handleSeerrDrivePickerKey(int32_t key) {
        SeerrStorageState::PickerInput input = SeerrStorageState::PickerInput::None;
        if (key == AKEYCODE_BACK)
            input = SeerrStorageState::PickerInput::Back;
        else if (key == AKEYCODE_DPAD_UP)
            input = SeerrStorageState::PickerInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = SeerrStorageState::PickerInput::Down;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = SeerrStorageState::PickerInput::Activate;

        auto command = seerrDomain_.handleStoragePickerInput(input);
        if (command.type == SeerrStorageState::PickerCommandType::Back) {
            popScreen(Screen::Search);
            return;
        }
        if (command.type != SeerrStorageState::PickerCommandType::Selected || !command.selection) return;

        auto selected = std::move(*command.selection);
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
            if (skipActiveMediaSegment()) return;
            [[fallthrough]];
        case PlayerScreenCommandType::TogglePause:
            player_.togglePause();
            reportProgressAsync(true);
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
            seekPlaybackTo(targetMs);
            reportProgressAsync(false);
            return;
        }
        }
    }

    void handleMediaSessionCommand(const MediaSessionCommand& command) {
        if (screen_ != Screen::Player) return;
        switch (command.type) {
        case MediaSessionCommandType::Play:
            player_.play();
            reportProgressAsync(true);
            break;
        case MediaSessionCommandType::Pause:
            player_.pause();
            reportProgressAsync(true);
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
            seekPlaybackTo(positionMs);
            reportProgressAsync(false);
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
                seekPlaybackTo(0);
                reportProgressAsync(false);
            }
            break;
        }
    }

    void showNotice(std::string message, std::chrono::seconds duration = 6s, bool persistent = false) {
        const auto now = std::chrono::steady_clock::now();
        notice_ = std::move(message);
        noticePersistent_ = persistent;
        noticeUntil_ = persistent ? std::chrono::steady_clock::time_point::max() : now + duration;
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
        seerrDomain_.resetSearch();
        searchState_.reset();
        detail_ = {};
        detailsState_.reset();

        artwork_.clearSession(renderer_);
        accountState_.clearSessionUi();
        loading_ = false;
        homeLoading_ = false;
        homeRetryAt_ = {};
        homeRetryAttempt_ = 0;
        mutationLoading_ = false;
        playedRollback_.reset();
        error_.clear();
        notice_.clear();
        noticeUntil_ = {};
        noticePersistent_ = false;
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
        if (loading_ || detail_.id.empty() || detail_.type != "Series") return;
        detailsState_.beginSeries(detail_);
        pushScreen(Screen::Seasons);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string seriesId = detailsState_.seriesDetail().id;
        const uint64_t generation = requestEpochs_.content.begin();
        detailsAsync_.loadSeasons(session, seriesId, generation);
    }

    void openEpisodes(const JellyfinItem& season) {
        if (loading_ || detailsState_.seriesDetail().id.empty() || season.id.empty()) return;
        detailsState_.beginSeason(season);
        pushScreen(Screen::Episodes);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string seriesId = detailsState_.seriesDetail().id;
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
        if (detail_.id.empty()) return;
        const std::string key = hiddenHomeKey(detail_.id);
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

    void updateCachedUserData(const JellyfinItem& updated) {
        auto apply = [&](JellyfinItem& item) {
            if (item.id != updated.id) return;
            item.favorite = updated.favorite;
            item.played = updated.played;
            item.positionTicks = updated.positionTicks;
        };
        for (auto& row : home_.rows)
            for (auto& item : row.items) apply(item);
        for (auto& item : browseState_.items()) apply(item);
        for (auto& item : searchState_.results()) apply(item);
        detailsState_.updateCachedUserData(updated);
        queueState_.updateCachedUserData(updated);

        for (auto& row : home_.rows) {
            if (row.title == "Favorites") {
                const auto existing = std::find_if(row.items.begin(), row.items.end(),
                                                   [&](const JellyfinItem& item) { return item.id == updated.id; });
                if (updated.favorite && existing == row.items.end() && !isHiddenFromHome(updated))
                    row.items.push_back(updated);
                else if (!updated.favorite && existing != row.items.end())
                    row.items.erase(existing);
            } else if (row.title == "Continue Watching" && updated.played) {
                std::erase_if(row.items, [&](const JellyfinItem& item) { return item.id == updated.id; });
            }
        }
        clampHomeSelections();
    }

    void toggleFavoriteAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty()) return;
        const bool desired = !detail_.favorite;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        itemMutationAsync_.setFavorite(session, item, desired, sessionEpoch);
    }

    void togglePlayedAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty()) return;
        const bool desired = !detail_.played;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        JellyfinHomeData previousHome = home_;
        HomeSelectionSnapshot previousHomeSelection = homeState_.snapshot(home_.rows);
        int nextUpReplacementIndex = -1;
        for (const auto& row : home_.rows) {
            if (row.title != "Next Up") continue;
            const auto current = std::find_if(row.items.begin(), row.items.end(),
                                              [&](const JellyfinItem& homeItem) { return homeItem.id == item.id; });
            if (current != row.items.end()) {
                nextUpReplacementIndex = static_cast<int>(std::distance(row.items.begin(), current));
            }
            break;
        }
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        playedRollback_ = PlayedRollbackState{
            .itemId = item.id,
            .sessionEpoch = sessionEpoch,
            .previousHome = std::move(previousHome),
            .previousHomeSelection = std::move(previousHomeSelection),
        };

        JellyfinItem updated = item;
        updated.played = desired;
        if (desired) updated.positionTicks = 0;
        updateCachedUserData(updated);
        detail_.played = desired;
        if (desired) detail_.positionTicks = 0;
        if (desired) {
            for (auto& row : home_.rows) {
                if (!homeRowDropsItemWhenPlayed(row.title)) continue;
                if (row.title == "Next Up" && nextUpReplacementIndex >= 0) continue;
                std::erase_if(row.items, [&](const JellyfinItem& homeItem) { return homeItem.id == item.id; });
            }
            clampHomeSelections();
        }

        itemMutationAsync_.setPlayed(session, item, desired, sessionEpoch, nextUpReplacementIndex);
    }

    void removeCachedItem(const std::string& itemId) {
        if (itemId.empty()) return;
        auto remove = [&](auto& items) {
            std::erase_if(items, [&](const JellyfinItem& item) { return item.id == itemId; });
        };
        for (auto& row : home_.rows) remove(row.items);
        browseState_.removeItem(itemId);
        searchState_.removeItem(itemId);
        detailsState_.removeItem(itemId);
    }

    void refreshCurrentItemMetadataAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty()) return;
        const JellyfinSession session = session_;
        const std::string itemId = detail_.id;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        itemMutationAsync_.refreshMetadata(session, itemId, sessionEpoch);
    }

    void deleteSeerrRequestAsync() {
        const auto deleteRequest = seerrDeleteRequestFromJellyfinItem(detail_);
        if (loading_ || mutationLoading_ || !deleteRequest) {
            if (isSeerrItem(detail_) && detail_.externalRequestId <= 0) {
                error_ = "SEERR REQUEST ID IS NOT AVAILABLE YET";
                detailsState_.setDeleteConfirmation(false);
            }
            return;
        }
        const SeerrEndpoint endpoint = seerrEndpoint();
        if (!endpoint.configured()) {
            error_ = "SEERR IS NOT CONNECTED";
            detailsState_.setDeleteConfirmation(false);
            return;
        }
        const SeerrDeleteRequest request = *deleteRequest;
        mutationLoading_ = true;
        error_.clear();
        seerrAsync_.deleteRequest(endpoint, request);
    }

    void deleteCurrentItemAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty() || !detail_.canDelete) return;
        const JellyfinSession session = session_;
        const std::string itemId = detail_.id;
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
        switch (seerrDomain_.prepareConnect(settings_.seerrServer, session_.valid())) {
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
        const std::string server = settings_.seerrServer;
        const JellyfinSession jellyfin = session_;
        if (announce) {
            error_.clear();
            showNotice("CONNECTING SEERR WITH JELLYFIN…", 30s);
        }
        seerrAsync_.connect(server, jellyfin, announce);
    }

    void refreshSeerrStorageAsync(bool force = false) {
        const SeerrEndpoint endpoint = seerrEndpoint();
        const auto action =
            seerrDomain_.prepareStorageRefresh(endpoint, force, std::chrono::steady_clock::now());
        if (action != SeerrDomainState::RefreshStartAction::Submit) return;
        seerrAsync_.refreshStorage(endpoint);
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
        const SeerrEndpoint endpoint = seerrEndpoint();
        const auto action = seerrDomain_.preparePendingRefresh(endpoint);
        if (action == SeerrDomainState::RefreshStartAction::Reset) {
            syncSeerrHomeRowLocked();
            return;
        }
        if (action != SeerrDomainState::RefreshStartAction::Submit) return;
        seerrAsync_.refreshPending(endpoint);
    }

    void requestSeerrMediaAsync(const SeerrMediaItem& item, const SeerrStorageTarget* selectedTarget = nullptr,
                                bool skipDrivePrompt = false) {
        const SeerrEndpoint endpoint = seerrEndpoint();
        auto plan =
            seerrDomain_.prepareRequest(item, endpoint, settings_.seerrSelectDrive, skipDrivePrompt, selectedTarget);
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
        seerrAsync_.requestMedia(endpoint, item, std::move(plan.target));
    }

    void searchSeerrAsync(bool immediate) {
        if (seerrDomain_.deferSearchIfConnecting()) {
            seerrDomain_.stopSearchLoading();
            return;
        }
        const SeerrEndpoint endpoint = seerrEndpoint();
        const std::string query = searchState_.query();
        bool shouldStart = false;
        if (immediate) {
            const auto start = seerrDomain_.beginImmediateSearch(query, endpoint.configured());
            if (start.resultsChanged) searchState_.refreshSeerrResults();
            shouldStart = start.started;
        } else {
            shouldStart = seerrDomain_.beginDueSearch(std::chrono::steady_clock::now());
        }
        if (!shouldStart) return;

        refreshSeerrStorageAsync();
        const uint64_t generation = requestEpochs_.seerrSearch.begin();
        seerrAsync_.search(endpoint, query, generation);
    }

    void searchAsync(bool includeSeerrImmediately = true) {
        if (searchState_.query().empty()) {
            seerrDomain_.resetSearch();
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

    void openDetails(const JellyfinItem& item, bool replaceCurrent = false) {
        if (replaceCurrent)
            replaceScreen(Screen::Details);
        else
            pushScreen(Screen::Details);
        playbackCoordinator_.dismissStillWatchingPrompt();
        detail_ = item;
        detailsState_.beginDetails();
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string id = item.id;
        const RequestEpoch::Token requestToken = requestEpochs_.content.beginToken();
        detailsAsync_.load(session, id, requestToken);
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

    void moveQueuedItem(int from, int to) {
        if (!queueState_.moveItem(from, to)) return;
        playbackCoordinator_.syncQueueContinuation(queueState_);
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
        PlaybackResolutionOptions resolutionOptions = playbackResolutionOptions();
        const uint64_t generation = requestEpochs_.playback.begin();
        playbackResolutionAsync_.resolveQueued(session, std::move(queued), std::move(resolutionOptions), generation,
                                               originScreen, index, previousQueueIndex, replacingPlayer);
    }

    void playPlayerItemAsync(JellyfinItem selected) {
        if (loading_ || screen_ != Screen::Player || !session_.valid() || selected.id.empty()) return;
        const JellyfinSession session = session_;
        PlaybackResolutionOptions resolutionOptions = playbackResolutionOptions();

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
        QueueOverlayInput input = QueueOverlayInput::None;
        if (key == AKEYCODE_BACK)
            input = QueueOverlayInput::Back;
        else if (key == AKEYCODE_DPAD_UP)
            input = QueueOverlayInput::Up;
        else if (key == AKEYCODE_DPAD_DOWN)
            input = QueueOverlayInput::Down;
        else if (key == AKEYCODE_DPAD_LEFT)
            input = QueueOverlayInput::Left;
        else if (key == AKEYCODE_DPAD_RIGHT)
            input = QueueOverlayInput::Right;
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            input = QueueOverlayInput::Activate;

        const QueueOverlayCommand command = queueState_.handleOverlayInput(input);
        if (command.type != QueueOverlayCommandType::ActivateAction) return;

        const int selection = command.selection;
        const int current = command.currentIndex;
        const int size = command.size;
        if (command.action == 0) {
            if (queueCanPlayNow(selection, current, size)) playQueuedIndexAsync(selection);
        } else if (command.action == 1) {
            if (queueCanPlayNext(selection, current, size)) moveQueuedItem(selection, current + 1);
        } else if (command.action == 2) {
            if (queueCanMoveUp(selection, current, size)) moveQueuedItem(selection, selection - 1);
        } else if (command.action == 3) {
            if (queueCanMoveDown(selection, current, size)) moveQueuedItem(selection, selection + 1);
        } else if (command.action == 4) {
            if (queueState_.removeSelected()) playbackCoordinator_.syncQueueContinuation(queueState_);
        } else if (command.action == 5) {
            shuffleRemainingQueue();
        } else if (command.action == 6) {
            queueState_.cycleRepeatMode();
            playbackCoordinator_.syncQueueContinuation(queueState_);
        }
    }

    void beginSeriesPlayAll() {
        if (loading_ || detail_.type != "Series" || detail_.id.empty() || !session_.valid()) return;
        loading_ = true;
        error_.clear();
        playbackCoordinator_.beginUserPlayback(false);
        const JellyfinSession session = session_;
        const JellyfinItem series = detail_;
        SeriesPlayAllOptions options{
            .maxStreamingBitrate = settings_.maxBitrateMbps * 1000000,
            .maxAudioChannels = settings_.maxAudioChannels,
            .overrides = playbackOverridesFor(settings_),
        };
        const uint64_t generation = requestEpochs_.playback.begin();
        seriesPlaybackAsync_.playAll(session, series, std::move(options), generation);
    }

    void beginPlayback() {
        if (loading_ || detail_.id.empty()) return;
        if (selectedExternalPlayer()) {
            launchExternalPlaybackAsync();
            return;
        }
        const PlaybackUserSelectionPlan selectionPlan =
            playbackCoordinator_.beginUserPlaybackSelection(queueState_, detail_.id);
        if (selectionPlan.resetQueue) queueState_.reset();
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem selected = detail_;
        PlaybackResolutionOptions resolutionOptions = playbackResolutionOptions();
        const uint64_t generation = requestEpochs_.playback.begin();

        playbackResolutionAsync_.resolveSelection(session, selected, std::move(resolutionOptions), generation,
                                                  selectionPlan.queuedPlaybackIndex);
    }

    void requestMediaSegmentsAsync() {
        if (!session_.valid()) return;
        const auto request = playbackCoordinator_.beginMediaSegmentsRequest(std::chrono::steady_clock::now());
        if (!request) return;
        const JellyfinSession session = session_;
        const std::string itemId = *request;
        if (!playbackContinuationAsync_.requestMediaSegments(session, itemId)) {
            playbackCoordinator_.failMediaSegmentsRequest(itemId, std::chrono::steady_clock::now());
        }
    }

    void requestNextEpisodeAsync() {
        const PlaybackNextEpisodePlan plan = playbackCoordinator_.beginNextEpisodePlan(
            queueState_, session_.valid(), std::chrono::steady_clock::now());
        if (!plan.request) return;
        const JellyfinSession session = session_;
        if (!playbackContinuationAsync_.requestNextEpisode(session, plan.request->seriesId,
                                                           plan.request->currentItemId)) {
            playbackCoordinator_.failNextEpisodeSubmission(std::chrono::steady_clock::now());
        }
    }

    void releaseActivePlayback(bool reportStop, bool completed = false) {
        requestEpochs_.playback.invalidate();
        if (player_.status() == PlayerStatus::Playing || player_.status() == PlayerStatus::Paused) {
            refreshPlaybackTelemetry(true);
        }
        const auto session = session_;
        const PlaybackReleaseContext release =
            playbackCoordinator_.releaseContext(reportStop, completed, session.valid(), playerScreenState_.positionMs());
        const PlaybackReleasePlan releasePlan = release.plan;
        const auto& item = release.item;
        const auto& target = release.target;
        if (!item.id.empty()) {
            JellyfinItem updated = item;
            updated.positionTicks = releasePlan.cachedPositionTicks;
            if (releasePlan.markPlayed) updated.played = true;
            updateCachedUserData(updated);
            if (detail_.id == item.id) {
                detail_.played = updated.played;
                detail_.positionTicks = updated.positionTicks;
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
        detail_ = nextItem;
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        const JellyfinSession session = session_;
        PlaybackResolutionOptions resolutionOptions = playbackResolutionOptions();
        const uint64_t generation = requestEpochs_.playback.begin();
        playbackResolutionAsync_.resolveAutoplay(session, std::move(nextItem), std::move(resolutionOptions), generation,
                                                 queuedNextIndex);
    }

    void showStillWatching(JellyfinItem nextItem) {
        releaseActivePlayback(true, true);
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        detail_ = std::move(nextItem);
        detailsState_.beginDetails();
        popScreen(Screen::Details);
        playbackCoordinator_.showStillWatchingPrompt();
        error_.clear();
    }

    void startResolvedPlaybackTarget() {
        const auto now = std::chrono::steady_clock::now();
        const PlaybackPlayerStartContext start = playbackCoordinator_.playerStartContext();
        playbackCoordinator_.beginPreparing(now);
        player_.startAsync(start.url, videoSurface_.surface(), start.startPositionMs, settings_.playbackBufferPreset,
                           start.audioOrdinal, start.subtitleStreamIndex, start.subtitleOrdinal,
                           start.externalSubtitleUrl);
        if (start.startPositionMs > 0) playerScreenState_.beginSeek(start.startPositionMs, now);
    }

    bool retryPlaybackWithoutSubtitle() {
        const PlaybackSubtitleFallbackPlan plan = playbackCoordinator_.subtitleFallbackPlan();
        if (!plan.retry) return false;
        __android_log_print(ANDROID_LOG_WARN, kTag,
                            "Subtitle-selected transcode failed; retrying item without subtitles (stream %d)",
                            plan.failedSubtitleStreamIndex);
        restartPlaybackAt(playerScreenState_.positionMs(), plan.audioStreamIndex, kSubtitleOffIndex);
        return true;
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

    bool retryPlaybackWithTranscodeFallback(bool preferServerStream = false) {
#ifdef SLOPPATV_BENCHMARK
        (void)preferServerStream;
        __android_log_print(ANDROID_LOG_WARN, kTag, "Benchmark build refusing Jellyfin server playback fallback");
        return false;
#else
        auto fallbackAttempt =
            playbackCoordinator_.fallbackAttempt(session_.valid(), playerScreenState_.positionMs(), preferServerStream);
        if (!fallbackAttempt) return false;

        const PlaybackFallbackPlan fallbackPlan = fallbackAttempt->plan;
        const PlaybackTarget failedTarget = std::move(fallbackAttempt->failedTarget);
        JellyfinItem item = std::move(fallbackAttempt->item);
        const JellyfinSession session = session_;
        const int64_t resumeTicks = fallbackPlan.resumeTicks;
        const bool shouldReportPrevious = fallbackPlan.reportPrevious;
        const int audioStreamIndex = fallbackAttempt->audioStreamIndex;
        const int subtitleStreamIndex = fallbackAttempt->subtitleStreamIndex;

        player_.stop();
        videoSurface_.release();
        playbackCoordinator_.beginFallback();
        playerScreenState_.setPositionMs(playbackPositionMsFromTicks(resumeTicks));
        playerScreenState_.setDurationMs(playbackPositionMsFromTicks(item.runtimeTicks));

        // Some PlaybackInfo responses include a TranscodingUrl beside DirectPlay. Use
        // that immediately when available; it avoids a second round-trip to Jellyfin.
        if (fallbackPlan.useOfferedTarget) {
            if (shouldReportPrevious) {
                playbackStreamAsync_.reportPreviousStop(session, item, failedTarget, resumeTicks, "stop-after-failure");
            }
            playbackCoordinator_.useOfferedFallback(fallbackPlan);
            const bool directStreamFallback = fallbackPlan.offeredDirectStream;

            std::string surfaceError;
            if (!renderer_.ready() || !videoSurface_.create(surfaceError)) {
                error_ = surfaceError.empty() ? "VIDEO FALLBACK SURFACE IS NOT AVAILABLE" : surfaceError;
                return false;
            }
            __android_log_print(ANDROID_LOG_WARN, kTag, "Direct play failed; using offered Jellyfin %s fallback",
                                directStreamFallback ? "direct-stream" : "transcode");
            playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 5s);
            startResolvedPlaybackTarget();
            return true;
        }

        // Jellyfin commonly omits TranscodingUrl when it selected DirectPlay, even when
        // SupportsTranscoding=true. Re-negotiate asynchronously with direct paths disabled
        // instead of abandoning playback after an embedded-player prepare failure.
        PlaybackOverrides fallbackOverrides = playbackOverridesFor(settings_);
        fallbackOverrides.forceServerStream = fallbackPlan.forceServerStream;
        fallbackOverrides.forceTranscode = fallbackPlan.forceTranscode;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const uint64_t generation = requestEpochs_.playback.begin();
        loading_ = true;
        playbackCoordinator_.beginFallbackResolution();
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        __android_log_print(ANDROID_LOG_WARN, kTag,
                            "Direct play failed without fallback URL; forcing Jellyfin %s negotiation",
                            preferServerStream ? "server-stream" : "transcode");

        PlaybackStreamResolutionOptions fallbackOptions{
            .maxStreamingBitrate = maxStreamingBitrate,
            .maxAudioChannels = maxAudioChannels,
            .overrides = fallbackOverrides,
            .audioStreamIndex = audioStreamIndex,
            .subtitleStreamIndex = subtitleStreamIndex,
        };
        const bool submitted = playbackStreamAsync_.resolveFallback(
            session, std::move(item), failedTarget, shouldReportPrevious, resumeTicks, std::move(fallbackOptions),
            generation);
        if (!submitted) {
            loading_ = false;
            playbackCoordinator_.finishFallbackResolution();
            error_ = "TRANSCODE FALLBACK COULD NOT BE STARTED";
            return false;
        }
        return true;
#endif
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
            updateCachedUserData(updated);
            if (detail_.id == completed.item.id) {
                detail_.played = updated.played;
                detail_.positionTicks = updated.positionTicks;
            }
        }
        const JellyfinSession reportSession = session_;
        externalPlaybackAsync_.reportStopped(
            reportSession, ExternalPlaybackReportRequest{
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
        startResolvedPlaybackTarget();
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
                if (preparePlan.retryWithTranscodeFallback && retryPlaybackWithTranscodeFallback()) {
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
            if (retryPlaybackWithoutSubtitle()) {
                error_.clear();
                return;
            }
            if (retryPlaybackWithTranscodeFallback()) {
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
                    if (retryPlaybackWithTranscodeFallback(true)) {
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
            refreshPlaybackTelemetry();
            mediaSession_.updateState(status == PlayerStatus::Playing ? MediaSessionState::Playing
                                                                      : MediaSessionState::Paused,
                                      playerScreenState_.positionMs());
        }
        if (plan.requestMediaSegments) requestMediaSegmentsAsync();
        if (plan.reportPlaybackStart) {
            const PlaybackStartContext start =
                playbackCoordinator_.playbackStartContext(playerScreenState_.positionMs());
            playbackTelemetryAsync_.reportStart(session_, start.item, start.target, start.ticks);
        }
        if (plan.reportProgress) reportProgressAsync(false);
        if (plan.requestNextEpisode) requestNextEpisodeAsync();

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
        switch (event.phase) {
        case SystemTextInputPhase::Changed:
            applySystemTextInputChanged(event.mode, event.value);
            return;
        case SystemTextInputPhase::Done:
            applySystemTextInputDone(event.mode, event.value);
            return;
        case SystemTextInputPhase::Cancelled:
            applySystemTextInputCancelled(event.mode, event.value);
            return;
        }
    }

    void applyAsyncCompletion(const SeerrDeleteCompletion& completion) {
        mutationLoading_ = false;
        if (screen_ != Screen::ItemMenu || detail_.id != completion.request.itemId) return;
        const auto outcome =
            seerrDomain_.completeDeleteRequest(completion.endpoint, seerrEndpoint(), completion.request.itemId,
                                               completion.request.requestId, completion.result.ok);
        if (outcome == SeerrDomainState::MutationOutcome::StaleEndpoint) return;
        if (outcome == SeerrDomainState::MutationOutcome::Failed) {
            error_ = "SEERR DELETE: " + completion.result.error;
            detailsState_.setDeleteConfirmation(false);
            return;
        }
        seerrDomain_.markSearchUnrequested(completion.request.itemId);
        searchState_.refreshSeerrResults();
        syncSeerrHomeRowLocked();
        detail_ = {};
        detailsState_.setDeleteConfirmation(false);
        popScreen(Screen::Home);
        if (screen_ != Screen::Home) resetNavigation(Screen::Home);
        showNotice("SEERR REQUEST DELETED", 4s);
        error_.clear();
    }

    void applyAsyncCompletion(const SeerrRequestCompletion& completion) {
        mutationLoading_ = false;
        const auto domainCompletion =
            seerrDomain_.completeRequest(completion.endpoint, seerrEndpoint(), completion.requestedItem,
                                         completion.result.value, completion.result.ok, std::chrono::steady_clock::now());
        if (domainCompletion.outcome == SeerrDomainState::MutationOutcome::StaleEndpoint) return;
        if (domainCompletion.outcome == SeerrDomainState::MutationOutcome::Failed) {
            error_ = "SEERR REQUEST: " + completion.result.error;
            return;
        }

        seerrDomain_.markSearchRequested(completion.requestedItem.id, domainCompletion.status, completion.result.value);
        searchState_.refreshSeerrResults();
        syncSeerrHomeRowLocked();
        showNotice("REQUEST SENT TO SEERR", 4s);
        error_.clear();
        refreshSeerrStorageAsync(true);
    }

    void applyAsyncCompletion(SeerrStorageRefreshCompletion& completion) {
        auto& result = completion.result;
        const auto domainCompletion = seerrDomain_.completeStorageRefresh(
            completion.endpoint, seerrEndpoint(), result.ok, std::move(result.value), result.error,
            std::chrono::steady_clock::now());
        if (domainCompletion.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint) return;
        if (domainCompletion.outcome == SeerrDomainState::RefreshOutcome::Failed) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Seerr storage refresh failed: %s", result.error.c_str());
            if (domainCompletion.reconnect) {
                connectSeerrAsync(false);
                return;
            }
            if (domainCompletion.clearedPendingRequest) showNotice("SEERR STORAGE: " + result.error, 5s);
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Seerr storage refresh found %zu targets",
                            seerrDomain_.storageTargets().size());
        const auto pendingRequest = seerrDomain_.takePendingStorageRequest(settings_.seerrSelectDrive);
        if (pendingRequest) openSeerrDrivePicker(*pendingRequest);
    }

    void applyAsyncCompletion(SeerrPendingRefreshCompletion& completion) {
        auto& result = completion.result;
        const auto outcome = seerrDomain_.completePendingRefresh(
            completion.endpoint, seerrEndpoint(), result.ok, std::move(result.value), std::chrono::steady_clock::now());
        if (outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint) return;
        if (outcome == SeerrDomainState::RefreshOutcome::Failed) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Seerr pending requests unavailable: %s", result.error.c_str());
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Seerr pending refresh found %zu requests",
                            seerrDomain_.pendingRequests().size());
        if (screen_ == Screen::ItemMenu && isSeerrItem(detail_)) {
            if (const SeerrMediaItem* current = seerrDomain_.findPendingRequest(detail_.id)) {
                detail_ = jellyfinItemFromSeerrMedia(*current);
            }
        }
        syncSeerrHomeRowLocked();
    }

    void applyAsyncCompletion(SeerrSearchCompletion& completion) {
        if (!requestEpochs_.seerrSearch.active(completion.generation)) return;
        if (screen_ != Screen::Search) {
            seerrDomain_.stopSearchLoading();
            return;
        }
        auto& result = completion.result;
        if (!result.ok) {
            if (seerrDomain_.failSearch(completion.query, result.error)) searchState_.refreshSeerrResults();
            if (seerrDomain_.completeSearchFailure(result.error, !settings_.seerrSessionCookie.empty())) {
                connectSeerrAsync(false);
            }
            return;
        }
        if (seerrDomain_.completeSearchSuccess(result.value, std::chrono::steady_clock::now())) {
            syncSeerrHomeRowLocked();
        }
        if (seerrDomain_.finishSearch(completion.query, std::move(result.value))) searchState_.refreshSeerrResults();
    }

    void applyAsyncCompletion(JellyfinSearchCompletion& completion) {
        if (!requestEpochs_.search.active(completion.generation)) return;
        if (screen_ != Screen::Search) {
            searchState_.setLoading(false);
            return;
        }
        auto& result = completion.result;
        if (!result.ok) {
            if (searchState_.failLibrarySearch(completion.query)) error_ = result.error;
            return;
        }
        if (!searchState_.finishLibrarySearch(completion.query, std::move(result.value))) return;
        error_.clear();
    }

    void applyAsyncCompletion(ItemMenuDetailCompletion& completion) {
        if (!completion.result.ok || screen_ != Screen::ItemMenu || detail_.id != completion.itemId) return;
        detail_ = std::move(completion.result.value);
    }

    void applyAsyncCompletion(PersonItemsCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::PersonItems || detailsState_.selectedPerson().id != completion.personId) return;
        if (!completion.result.ok) {
            error_ = "PERSON: " + completion.result.error;
            return;
        }
        detailsState_.setPersonItems(std::move(completion.result.value));
    }

    void applyAsyncCompletion(DiagnosticsCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Diagnostics) return;
        if (!completion.result.ok) {
            error_ = "SERVER INFO: " + completion.result.error;
            return;
        }
        serverInfo_ = std::move(completion.result.value);
        const auto compatibility = jellyfinServerCompatibility(serverInfo_.version);
        if (compatibility == ServerCompatibility::TooOld) {
            error_ = "JELLYFIN " + serverInfo_.version + " IS BELOW THE TESTED 10.10+ BASELINE";
        } else if (compatibility == ServerCompatibility::Unknown && !serverInfo_.version.empty()) {
            error_ = "UNRECOGNIZED JELLYFIN VERSION: " + serverInfo_.version;
        }
    }

    void applyAsyncCompletion(SeasonsCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Seasons || detailsState_.seriesDetail().id != completion.seriesId) return;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        detailsState_.setSeasons(std::move(completion.result.value));
    }

    void applyAsyncCompletion(EpisodesCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Episodes || detailsState_.seriesDetail().id != completion.seriesId ||
            detailsState_.selectedSeason().id != completion.seasonId) {
            return;
        }
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        detailsState_.setEpisodes(std::move(completion.result.value));
    }

    void applyAsyncCompletion(BrowsePageCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Browse || browseState_.activeContainer().id != completion.containerId) return;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        if (completion.append)
            browseState_.appendPage(std::move(completion.result.value), completion.startIndex, kBrowsePageSize);
        else
            browseState_.replacePage(std::move(completion.result.value), kBrowsePageSize);
        prefetchBrowseArtworkAhead();
        error_.clear();
    }

    void applyAsyncCompletion(ServerInfoNoticeCompletion& completion) {
        serverInfoLoading_ = false;
        if (!session_.valid() || session_.server != completion.server || session_.userId != completion.userId) return;
        if (!completion.result.ok) return;
        serverInfo_ = std::move(completion.result.value);
        const auto compatibility = jellyfinServerCompatibility(serverInfo_.version);
        if (compatibility == ServerCompatibility::TooOld) {
            showNotice("JELLYFIN " + serverInfo_.version +
                           " IS BELOW THE TESTED 10.10+ BASELINE - SERVER UPGRADE RECOMMENDED",
                       6s, true);
        } else if (compatibility == ServerCompatibility::Unknown && !serverInfo_.version.empty()) {
            showNotice("UNRECOGNIZED JELLYFIN VERSION " + serverInfo_.version + " - PLAYBACK COMPATIBILITY MAY VARY",
                       10s);
        }
    }

    void applyAsyncCompletion(FavoriteCompletion& completion) {
        if (!requestEpochs_.session.active(completion.sessionEpoch)) return;
        if (completion.result.ok) {
            JellyfinItem updated = completion.item;
            updated.favorite = completion.desired;
            updateCachedUserData(updated);
        }
        mutationLoading_ = false;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        if ((screen_ == Screen::Details || screen_ == Screen::ItemMenu) && detail_.id == completion.item.id) {
            detail_.favorite = completion.desired;
        }
    }

    void applyAsyncCompletion(PlayedCompletion& completion) {
        const bool rollbackMatches =
            playedRollback_ && playedRollback_->sessionEpoch == completion.sessionEpoch &&
            playedRollback_->itemId == completion.item.id;
        if (!requestEpochs_.session.active(completion.sessionEpoch)) {
            if (rollbackMatches) playedRollback_.reset();
            return;
        }
        std::optional<PlayedRollbackState> rollback;
        if (rollbackMatches) {
            rollback = std::move(playedRollback_);
            playedRollback_.reset();
        }
        mutationLoading_ = false;
        if (!completion.result.ok) {
            updateCachedUserData(completion.item);
            if (rollback) {
                home_ = std::move(rollback->previousHome);
                HomeRestorePlan rollbackPlan =
                    HomeScreenState::restorePlan(rollback->previousHomeSelection, home_.rows);
                homeState_.setSelections(std::move(rollbackPlan.selections));
                homeState_.setRow(rollbackPlan.focusedRow);
                homeState_.updateViewport(static_cast<int>(home_.rows.size()));
            }
            if ((screen_ == Screen::Details || screen_ == Screen::ItemMenu) && detail_.id == completion.item.id)
                detail_ = completion.item;
            error_ = completion.result.error;
            return;
        }
        const auto nextUpRow =
            std::find_if(home_.rows.begin(), home_.rows.end(),
                         [](const JellyfinHomeRow& candidate) { return candidate.title == "Next Up"; });
        if (completion.nextUpReplacementIndex >= 0 && nextUpRow != home_.rows.end()) {
            const size_t replacementIndex = static_cast<size_t>(completion.nextUpReplacementIndex);
            const bool slotStillMatches = replacementIndex < nextUpRow->items.size() &&
                                          nextUpRow->items[replacementIndex].id == completion.item.id;
            if (completion.nextUpReplacement && slotStillMatches &&
                !isHiddenFromHome(*completion.nextUpReplacement)) {
                const std::string replacementId = completion.nextUpReplacement->id;
                nextUpRow->items[replacementIndex] = std::move(*completion.nextUpReplacement);
                nextUpReplacementFadeIndex_ = completion.nextUpReplacementIndex;
                nextUpReplacementFadeItemId_ = replacementId;
                nextUpReplacementFadeStarted_ = std::chrono::steady_clock::now();
                renderBurstUntil_ = std::max(renderBurstUntil_, nextUpReplacementFadeStarted_ + 320ms);
                if (app_ && app_->looper) ALooper_wake(app_->looper);
            } else if (slotStillMatches) {
                nextUpRow->items.erase(nextUpRow->items.begin() + static_cast<std::ptrdiff_t>(replacementIndex));
                clampHomeSelections();
            }
        }
        if ((screen_ == Screen::Details || screen_ == Screen::ItemMenu) && detail_.id == completion.item.id) {
            detail_.played = completion.desired;
            if (completion.desired) detail_.positionTicks = 0;
        }
    }

    void applyAsyncCompletion(MetadataRefreshCompletion& completion) {
        if (!requestEpochs_.session.active(completion.sessionEpoch)) return;
        mutationLoading_ = false;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        showNotice("METADATA REFRESH REQUESTED");
    }

    void applyAsyncCompletion(DeleteItemCompletion& completion) {
        if (!requestEpochs_.session.active(completion.sessionEpoch)) return;
        if (completion.result.ok) removeCachedItem(completion.itemId);
        mutationLoading_ = false;
        if (screen_ != Screen::ItemMenu || detail_.id != completion.itemId) return;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            detailsState_.setDeleteConfirmation(false);
            return;
        }
        detail_ = {};
        detailsState_.setDeleteConfirmation(false);
        popScreen(Screen::Home);
        if (screen_ == Screen::Details) popScreen(Screen::Home);
        showNotice("MEDIA DELETED");
    }

    void applyAsyncCompletion(DiscoveryCompletion& completion) {
        if (!requestEpochs_.auth.active(completion.generation)) return;
        loading_ = false;
        if (completion.servers.empty()) {
            accountState_.clearDiscoveryStatus();
            error_ = "NO JELLYFIN SERVER FOUND ON THIS NETWORK";
            return;
        }
        accountState_.setField(AccountScreenState::kServerField, completion.servers.front().address);
        std::string discoveryStatus = "FOUND " + (completion.servers.front().name.empty()
                                                        ? std::string("JELLYFIN")
                                                        : completion.servers.front().name);
        if (completion.servers.size() > 1)
            discoveryStatus += " + " + std::to_string(completion.servers.size() - 1) + " MORE";
        accountState_.setDiscoveryStatus(std::move(discoveryStatus));
        accountState_.setLoginFocus(AccountScreenState::kUsernameField);
        error_.clear();
    }

    void applyAsyncCompletion(LoginCompletion& completion) {
        if (!requestEpochs_.auth.active(completion.generation)) return;
        loading_ = false;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        requestEpochs_.session.invalidate();
        session_ = std::move(completion.result.value);
        accountState_.setAuthenticatedAccount(session_.server, session_.username);
        resetNavigation(Screen::Home);
        homeState_.setRow(0);
        homeState_.setFirstVisibleRow(0);
        error_.clear();
        saveSession(session_);
        loadHomeAsync();
    }

    void applyAsyncCompletion(DetailsItemCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Details || detail_.id != completion.itemId) return;
        if (!completion.result.ok) {
            error_ = "DETAILS: " + completion.result.error;
            return;
        }
        detail_ = std::move(completion.result.value);
    }

    void applyAsyncCompletion(DetailsSimilarCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        if (screen_ != Screen::Details || detail_.id != completion.itemId) return;
        detailsState_.setSimilar(std::move(completion.items));
    }

    void applyAsyncCompletion(EpisodeSeriesContextRequestCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        if (screen_ != Screen::Details || !completion.request.matches(detail_)) return;

        const RequestEpoch::Token requestToken = requestEpochs_.content.token(completion.generation);
        detailsAsync_.loadSeriesContext(std::move(completion.session), std::move(completion.request), requestToken);
    }

    void applyAsyncCompletion(EpisodeSeriesContextCompletion& completion) {
        if (!requestEpochs_.content.active(completion.generation)) return;
        if (screen_ != Screen::Details || detail_.id != completion.itemId || detail_.type != "Episode") return;
        detailsState_.setEpisodeSeriesContext(std::move(completion.series), std::move(completion.seasons));
    }

    void applyAsyncCompletion(QuickConnectStartedCompletion& completion) {
        if (!requestEpochs_.auth.active(completion.generation)) return;
        accountState_.setField(AccountScreenState::kServerField, completion.request.server);
        accountState_.setQuickConnectCode(completion.request.code);
        loading_ = false;
    }

    void applyAsyncCompletion(QuickConnectFailedCompletion& completion) {
        if (!requestEpochs_.auth.active(completion.generation)) return;
        loading_ = false;
        accountState_.endQuickConnect();
        error_ = std::move(completion.error);
    }

    void applyAsyncCompletion(QuickConnectAuthenticatedCompletion& completion) {
        if (!requestEpochs_.auth.active(completion.generation)) return;
        requestEpochs_.session.invalidate();
        session_ = std::move(completion.session);
        accountState_.setAuthenticatedAccount(session_.server, session_.username);
        loading_ = false;
        resetNavigation(Screen::Home);
        homeState_.setRow(0);
        homeState_.setFirstVisibleRow(0);
        error_.clear();
        saveSession(session_);
        loadHomeAsync();
    }

    void applyAsyncCompletion(QuickConnectTimedOutCompletion& completion) {
        if (!requestEpochs_.auth.active(completion.generation)) return;
        loading_ = false;
        accountState_.endQuickConnect();
        error_ = "QUICK CONNECT TIMED OUT - TRY AGAIN";
    }

    void applyAsyncCompletion(HomeCoreCompletion& completion) {
        if (!requestEpochs_.home.active(completion.generation)) return;
        homeLoading_ = false;
        if (!completion.result.ok) {
            const std::string& error = completion.result.error;
            if (error.find("HTTP 401") != std::string::npos) {
                homeRetryAt_ = {};
                homeRetryAttempt_ = 0;
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
            if (isTransientHomeLoadError(error)) {
                const int delaySeconds = std::min(30, 1 << std::min(homeRetryAttempt_, 5));
                ++homeRetryAttempt_;
                homeRetryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(delaySeconds);
                __android_log_print(ANDROID_LOG_WARN, kTag,
                                    "Home load failed transiently; retrying in %d seconds: %s", delaySeconds,
                                    error.c_str());
            } else {
                homeRetryAt_ = {};
                homeRetryAttempt_ = 0;
            }
            if (screen_ == Screen::Home) error_ = error;
            return;
        }

        filterHiddenHomeItems(completion.result.value);
        std::vector<JellyfinItem> views = completion.result.value.views;
        HomeRestorePlan restorePlan = HomeScreenState::restorePlan(completion.snapshot, completion.result.value.rows);
        const int coreRestoredRow = restorePlan.focusedRow;
        homeRetryAt_ = {};
        homeRetryAttempt_ = 0;
        home_ = std::move(completion.result.value);
        homeState_.setSelections(std::move(restorePlan.selections));
        homeState_.setRow(coreRestoredRow);
        homeState_.updateViewport(static_cast<int>(home_.rows.size()));
        syncSeerrHomeRowLocked();
        if (screen_ == Screen::Home) error_ = home_.warning;
        if (homeState_.row() >= 0) {
            const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
            prefetchHomeWindow(homeState_.row(),
                               homeState_.selection(homeState_.row(), static_cast<int>(row.items.size())));
        }
        const auto coreMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - completion.startedAt)
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
        homeAsync_.loadSecondary(session_, completion.generation, std::move(views), std::move(completion.snapshot),
                                 coreRestoredRow, completion.startedAt);
    }

    void applyAsyncCompletion(HomeSecondaryCompletion& completion) {
        if (!requestEpochs_.home.active(completion.generation)) return;
        if (!completion.result.ok) {
            if (!home_.warning.empty()) home_.warning += " | ";
            home_.warning += "SECONDARY HOME ROWS UNAVAILABLE";
            if (screen_ == Screen::Home) error_ = home_.warning;
            return;
        }

        filterHiddenHomeItems(completion.result.value);
        const size_t baseRowCount = home_.rows.size();
        for (auto& section : completion.result.value.rows) {
            const int restoredSelection = HomeScreenState::restoredSelection(completion.snapshot, section);
            const bool focusAppended = !completion.snapshot.toolbarFocused &&
                                       section.title == completion.snapshot.focusedRowTitle &&
                                       homeState_.row() == completion.coreRestoredRow;
            home_.rows.push_back(std::move(section));
            homeState_.appendSelection(restoredSelection);
            if (focusAppended) homeState_.setRow(static_cast<int>(home_.rows.size()) - 1);
        }
        if (!completion.result.value.warning.empty()) {
            if (!home_.warning.empty()) home_.warning += " | ";
            home_.warning += completion.result.value.warning;
            if (screen_ == Screen::Home) error_ = home_.warning;
        }
        homeState_.updateViewport(static_cast<int>(home_.rows.size()));
        if (homeState_.row() >= static_cast<int>(baseRowCount) &&
            homeState_.row() < static_cast<int>(homeState_.selectionCount())) {
            const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
            prefetchHomeWindow(homeState_.row(),
                               homeState_.selection(homeState_.row(), static_cast<int>(row.items.size())));
        }
        const auto fullMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - completion.startedAt)
                                .count();
        __android_log_print(ANDROID_LOG_INFO, kTag, "Home enrichment completed in %lld ms",
                            static_cast<long long>(fullMs));
    }

    void applyAsyncCompletion(ExternalPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Details || detail_.id != completion.selectedItemId) return;
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

    void applyAsyncCompletion(SubtitleLoadCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        if (!playbackCoordinator_.subtitleLoadMatches(completion.itemId, completion.requestedSubtitleIndex)) return;
        if (completion.cues.empty()) {
            playbackCoordinator_.failSubtitleLoad();
            showNotice("SUBTITLES UNAVAILABLE FOR THIS FILE");
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, kTag, "Subtitle loaded item=%s stream=%d codec=%s cues=%zu",
                            completion.itemId.c_str(), completion.loadedSubtitle.index,
                            completion.loadedSubtitle.codec.c_str(), completion.cues.size());
        playbackCoordinator_.completeSubtitleLoad(completion.loadedSubtitle.index, completion.loadedSubtitle.language,
                                                   std::move(completion.cues));
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
        reportProgressAsync(false);
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

    void applyAsyncCompletion(MediaSegmentsCompletion& completion) {
        if (screen_ != Screen::Player) return;
        if (!completion.ok) {
            if (playbackCoordinator_.failMediaSegmentsRequest(completion.itemId, completion.completedAt)) {
                __android_log_print(ANDROID_LOG_WARN, kTag, "Media segments unavailable: %s",
                                    completion.error.c_str());
            }
            return;
        }
        const size_t segmentCount = completion.segments.size();
        if (!playbackCoordinator_.completeMediaSegmentsRequest(completion.itemId, std::move(completion.segments)))
            return;
        __android_log_print(ANDROID_LOG_INFO, kTag, "Loaded %zu media segments", segmentCount);
    }

    void applyAsyncCompletion(NextEpisodeCompletion& completion) {
        if (screen_ != Screen::Player) return;
        if (!completion.ok) {
            if (playbackCoordinator_.failNextEpisodeRequest(completion.currentItemId, completion.completedAt)) {
                __android_log_print(ANDROID_LOG_WARN, kTag, "Next episode lookup failed: %s",
                                    completion.error.c_str());
            }
            return;
        }
        playbackCoordinator_.completeNextEpisodeRequest(completion.currentItemId, std::move(completion.item));
    }

    void applyAsyncCompletion(PlaybackAdjacentCompletion& completion) {
        const bool sameItem = playbackCoordinator_.finishAdjacentEpisodeLookup(completion.currentItemId);
        if (screen_ != Screen::Player || !sameItem) return;
        if (!completion.ok) {
            showNotice("EPISODE LIST UNAVAILABLE", 2s);
            return;
        }
        if (!completion.item) {
            showNotice(completion.direction < 0 ? "NO PREVIOUS EPISODE" : "NO NEXT EPISODE", 2s);
            return;
        }
        playPlayerItemAsync(std::move(*completion.item));
    }

    void applyAsyncCompletion(const PlaybackReportCompletion& completion) {
        const char* stage = "progress";
        if (completion.kind == PlaybackReportKind::Start)
            stage = "start";
        else if (completion.kind == PlaybackReportKind::PausedProgress)
            stage = "paused-progress";
        else if (completion.kind == PlaybackReportKind::Stop)
            stage = "stop";
        logPlaybackReportFailure(stage, completion.itemId, completion.result);
        if (completion.kind != PlaybackReportKind::Stop || !completion.result.ok) return;
        if (session_.server != completion.server || session_.userId != completion.userId || screen_ == Screen::Player)
            return;
        playbackCoordinator_.markPlaybackStopReported();
    }

    void applyAsyncCompletion(QueuedPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        playbackCoordinator_.finishPlaybackResolution();
        if (screen_ != completion.originScreen || queueState_.currentIndex() != completion.previousQueueIndex ||
            !queueState_.itemMatches(completion.index, completion.item.id)) {
            return;
        }
        if (!completion.result.ok) {
            if (completion.replacingPlayer && screen_ == Screen::Player) popScreen(Screen::Details);
            error_ = "QUEUE: " + completion.result.error;
            return;
        }
        queueState_.setCurrentIndex(completion.index);
        queueState_.setItemAt(completion.index, completion.item);
        playbackCoordinator_.stageResolvedPlayback(std::move(completion.result.value), std::move(completion.item));
    }

    void applyAsyncCompletion(PlayerItemPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        playbackCoordinator_.finishPlaybackResolution();
        if (screen_ != Screen::Player) return;
        if (!completion.result.ok) {
            error_ = "EPISODE: " + completion.result.error;
            return;
        }
        detail_ = completion.item;
        playbackCoordinator_.stageResolvedPlayback(std::move(completion.result.value), std::move(completion.item));
    }

    void applyAsyncCompletion(AutoplayPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        playbackCoordinator_.finishPlaybackResolution();
        if (!completion.result.ok) {
            popScreen(Screen::Details);
            error_ = "NEXT EPISODE: " + completion.result.error;
            return;
        }
        if (completion.queuedNextIndex >= 0 && queueState_.currentIndex() + 1 == completion.queuedNextIndex &&
            queueState_.itemMatches(completion.queuedNextIndex, completion.item.id)) {
            queueState_.setCurrentIndex(completion.queuedNextIndex);
            queueState_.setItemAt(completion.queuedNextIndex, completion.item);
        }
        playbackCoordinator_.stageResolvedPlayback(std::move(completion.result.value), std::move(completion.item));
    }

    void applyAsyncCompletion(StreamRestartCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        const bool sameItem = playbackCoordinator_.completeStreamRestartRequest(completion.item.id);
        if (screen_ != Screen::Player || !sameItem) return;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        playbackCoordinator_.stageStreamRestart(std::move(completion.result.value), std::move(completion.item),
                                                completion.wasPaused, completion.audioStreamIndex);
    }

    void applyAsyncCompletion(FallbackPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        const bool sameItem = playbackCoordinator_.completeFallbackResolution(completion.item.id);
        if (screen_ != Screen::Player || !sameItem) return;
        if (!completion.result.ok) {
            error_ = "TRANSCODE FALLBACK: " + completion.result.error;
            stopPlayback();
            return;
        }
        playbackCoordinator_.stageResolvedFallback(std::move(completion.result.value), std::move(completion.item),
                                                   completion.audioStreamIndex);
    }

    void applyAsyncCompletion(BeginPlaybackCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Details || detail_.id != completion.selected.id) return;
        if (!completion.result.ok) {
            error_ = completion.result.error;
            return;
        }
        if (completion.queuedPlaybackIndex >= 0 &&
            queueState_.itemMatches(completion.queuedPlaybackIndex, completion.playable.id)) {
            queueState_.setCurrentIndex(completion.queuedPlaybackIndex);
            queueState_.setItemAt(completion.queuedPlaybackIndex, completion.playable);
        }
        restoreHomeVisibilityForPlayback(completion.selected);
        restoreHomeVisibilityForPlayback(completion.playable);
        playbackCoordinator_.stageResolvedPlayback(std::move(completion.result.value), std::move(completion.playable));
    }

    void applyAsyncCompletion(SeriesPlayAllCompletion& completion) {
        if (!requestEpochs_.playback.active(completion.generation)) return;
        loading_ = false;
        if (screen_ != Screen::Details || detail_.id != completion.series.id) return;
        if (!completion.error.empty()) {
            error_ = std::move(completion.error);
            return;
        }
        if (!completion.target) return;
        queueState_.replace(std::move(completion.episodes), 0);
        queueState_.setItemAt(0, completion.first);
        restoreHomeVisibilityForPlayback(completion.series);
        restoreHomeVisibilityForPlayback(completion.first);
        playbackCoordinator_.stageResolvedPlayback(std::move(*completion.target), std::move(completion.first));
    }

    void applyAsyncCompletion(SeerrConnectCompletion& completion) {
        auto& result = completion.result;
        const bool currentRequest =
            settings_.seerrServer == completion.server && session_.userId == completion.jellyfinUserId;
        const auto action = seerrDomain_.completeConnect(
            result.ok, result.failedStage == SeerrQuickConnectStage::AuthenticateSeerr, currentRequest);
        if (action == SeerrDomainState::ConnectCompletionAction::PreAuthenticationFailed) {
            if (completion.announce) {
                notice_.clear();
                error_ =
                    (result.failedStage == SeerrQuickConnectStage::AuthorizeJellyfin ? "JELLYFIN QUICK CONNECT: "
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
        if (completion.announce) notice_.clear();
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
        auto deferred = seerrDomain_.takeDeferredConnectionWork();
        if (deferred.request) requestSeerrMediaAsync(*deferred.request);
        if (deferred.retrySearch && screen_ == Screen::Search && !searchState_.query().empty()) {
            if (seerrDomain_.scheduleSearch(searchState_.query(), std::chrono::steady_clock::now(), false)) {
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
                (screen_ == Screen::Home || (screen_ == Screen::ItemMenu && isSeerrItem(detail_)));
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

    void reportProgressAsync(bool immediate) {
        const PlayerStatus status = player_.status();
        const PlaybackProgressContext progress = playbackCoordinator_.progressContext(
            screen_ == Screen::Player, session_.valid(), immediate, status == PlayerStatus::Preparing,
            status == PlayerStatus::Paused, playerScreenState_.positionMs());
        if (!progress.plan.report) return;
        playbackTelemetryAsync_.reportProgress(session_, progress.item, progress.target, progress.plan.ticks,
                                               progress.plan.paused);
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

    void drawCoverTexture(const ArtworkEntry& entry, float x, float y, float width, float height, float alpha = 1.0f,
                          float radius = material_tv::cornerSmall) {
        const int sourceWidth = entry.sourceWidth > 0 ? entry.sourceWidth : entry.decoded.width;
        const int sourceHeight = entry.sourceHeight > 0 ? entry.sourceHeight : entry.decoded.height;
        if (entry.texture == 0 || sourceWidth <= 0 || sourceHeight <= 0 || width <= 0.0f || height <= 0.0f) return;
        const float sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
        const float targetAspect = width / height;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        if (sourceAspect > targetAspect) {
            const float visible = targetAspect / sourceAspect;
            u0 = (1.0f - visible) * 0.5f;
            u1 = u0 + visible;
        } else if (sourceAspect < targetAspect) {
            const float visible = sourceAspect / targetAspect;
            v0 = (1.0f - visible) * 0.5f;
            v1 = v0 + visible;
        }
        renderer_.roundedImageRegion(entry.texture, x, y, width, height, radius, u0, v0, u1, v1, alpha);
    }

    bool drawHomeArtwork(const JellyfinItem& item, float x, float y, float width, float height,
                         float radius = material_tv::cornerSmall, float alpha = 1.0f) {
        ArtworkEntry* entry = artwork_.homeTexture(session_, item, isSeerrItem(item), renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, width, height, alpha, radius);
        return true;
    }

    void prefetchHomeWindow(int row, int selection) {
        if (row < 0 || row >= static_cast<int>(home_.rows.size())) return;
        const auto& items = home_.rows[static_cast<size_t>(row)].items;
        if (items.empty()) return;
        const int begin = std::max(0, selection - 2);
        const int end = std::min(static_cast<int>(items.size()), selection + 7);
        for (int index = begin; index < end; ++index) {
            const auto& item = items[static_cast<size_t>(index)];
            artwork_.requestHome(session_, item, isSeerrItem(item), renderer_);
        }
    }

    void prefetchBrowseArtworkAhead() {
        const auto& items = browseState_.items();
        if (items.empty() || browseState_.syntheticPage()) return;
        constexpr int columns = mediaGridColumns();
        constexpr int visibleRows = 2;
        constexpr int warmRowsAhead = 3;
        const int selectedRow = std::max(0, browseState_.selection() / columns);
        const int firstVisibleRow = std::max(0, selectedRow - 1);
        const int begin = firstVisibleRow * columns;
        const int end =
            std::min(static_cast<int>(items.size()), (firstVisibleRow + visibleRows + warmRowsAhead) * columns);
        for (int index = begin; index < end; ++index) {
            const auto& item = items[static_cast<size_t>(index)];
            if (usesLandscapeMediaCard(item.type)) {
                artwork_.requestHome(session_, item, isSeerrItem(item), renderer_);
            } else {
                artwork_.requestPoster(session_, item, isSeerrItem(item), renderer_);
            }
        }
    }

    bool drawProfileArtwork(const JellyfinSession& saved, float x, float y, float size) {
        ArtworkEntry* entry = artwork_.profileTexture(saved, renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, size, size, 1.0f, size * 0.5f);
        return true;
    }

    bool drawArtwork(const JellyfinItem& item, float x, float y, float width, float height, float alpha = 1.0f,
                     float radius = material_tv::cornerSmall) {
        ArtworkEntry* entry = artwork_.posterTexture(session_, item, isSeerrItem(item), renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, width, height, alpha, radius);
        return true;
    }

    bool drawBackdrop(const JellyfinItem& item, float alpha = 0.28f) {
        ArtworkEntry* entry = artwork_.backdropTexture(session_, item, settings_.backdropMode, renderer_);
        if (!entry) return false;
        const float effectiveAlpha = settings_.backdropMode == 1 ? std::max(alpha, 0.34f) : alpha;
        drawCoverTexture(*entry, 0.0f, 0.0f, Renderer::logicalWidth(), Renderer::logicalHeight(), effectiveAlpha, 0.0f);
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(),
                       Color{0.0f, 0.0f, 0.0f, settings_.backdropMode == 1 ? 0.26f : 0.12f});
        return true;
    }

    bool drawLogo(const JellyfinItem& item, float x, float y, float maxWidth, float maxHeight) {
        ArtworkEntry* entry = artwork_.logoTexture(session_, item, renderer_);
        if (!entry || entry->sourceWidth <= 0 || entry->sourceHeight <= 0) return false;
        const float aspect = static_cast<float>(entry->sourceWidth) / static_cast<float>(entry->sourceHeight);
        float width = maxWidth;
        float height = width / aspect;
        if (height > maxHeight) {
            height = maxHeight;
            width = height * aspect;
        }
        renderer_.image(entry->texture, x, y + (maxHeight - height) * 0.5f, width, height);
        return true;
    }

    void drawFocusHalo(float x, float y, float width, float height, Color accent = kFocus, float radius = 18.0f) {
        material_tv::focusRing(renderer_, x, y, width, height, radius, accent);
    }

    std::array<float, 4> focusedBounds(float x, float y, float width, float height, bool focused,
                                       float scale = materialCardFocusScale()) const {
        if (!focused) return {x, y, width, height};
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastInteraction_)
                .count();
        float animatedScale = scale;
        const bool homeSelectionPress = screen_ == Screen::Home && homeState_.centerPending();
        if (!homeSelectionPress && elapsedMs >= 0 && elapsedMs < 150) {
            const float t = std::clamp(static_cast<float>(elapsedMs) / 150.0f, 0.0f, 1.0f);
            const float remaining = 1.0f - t;
            const float eased = 1.0f - remaining * remaining * remaining;
            animatedScale = 1.0f + (scale - 1.0f) * eased;
        }
        const float scaledWidth = width * animatedScale;
        const float scaledHeight = height * animatedScale;
        return {
            x - (scaledWidth - width) * 0.5f,
            y - (scaledHeight - height) * 0.5f,
            scaledWidth,
            scaledHeight,
        };
    }

    std::array<float, 4> drawFocusedSurface(float x, float y, float width, float height, bool focused,
                                            bool primary = false, bool destructive = false, float focusScale = 1.035f,
                                            float radius = material_tv::cornerMedium, bool outlinedWhenIdle = false) {
        auto bounds = focusedBounds(x, y, width, height, focused, focusScale);
        // Idle outlined pills need stable pixel-aligned geometry. Focused surfaces
        // deliberately keep their fractional animated bounds for smooth scaling.
        if (!focused) {
            bounds[0] = std::round(bounds[0]);
            bounds[1] = std::round(bounds[1]);
            bounds[2] = std::round(bounds[2]);
            bounds[3] = std::round(bounds[3]);
        }
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        const Color accent = destructive ? kError : kFocus;
        const Color surface =
            destructive ? (focused ? material_tv::errorContainer
                                   : Color{material_tv::errorContainer.r, material_tv::errorContainer.g,
                                           material_tv::errorContainer.b, 0.72f})
                        : (primary ? kFocusSoft : (focused ? material_tv::surfaceContainerHighest : kPanelAlt));
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, surface);
        if (!focused && outlinedWhenIdle) {
            renderer_.roundedOutline(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, 1.5f, kOutline);
        }
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], accent, renderedRadius);
        return bounds;
    }

    void drawModalSurface(float x, float y, float width, float height, float radius = material_tv::cornerLarge) {
        material_tv::dialog(renderer_, x, y, width, height, radius);
    }

    std::array<float, 4> drawListItemSurface(float x, float y, float width, float height, bool focused,
                                             float radius = material_tv::cornerMedium,
                                             float focusScale = materialListItemFocusScale()) {
        const auto bounds = focusedBounds(x, y, width, height, focused, focusScale);
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius,
                              focused ? kPanelElevated : kPanel);
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, renderedRadius);
        return bounds;
    }

    void drawDisabledButtonSurface(float x, float y, float width, float height) {
        x = std::round(x);
        y = std::round(y);
        width = std::round(width);
        height = std::round(height);
        const float radius = std::min(material_tv::cornerLarge, height * 0.5f);
        renderer_.roundedRect(x, y, width, height, radius, kPanel);
        renderer_.roundedOutline(x, y, width, height, radius, 1.0f, kDivider);
    }

    std::array<float, 4> drawButtonSurface(float x, float y, float width, float height, bool focused,
                                           bool primary = false, bool destructive = false) {
        return drawFocusedSurface(x, y, width, height, focused, primary, destructive, materialButtonFocusScale(),
                                  std::min(material_tv::cornerLarge, height * 0.5f), !primary);
    }

    std::array<float, 4> drawTabSurface(float x, float y, float width, float height, bool focused, bool selected) {
        return drawFocusedSurface(x, y, width, height, focused, selected, false, materialTabFocusScale(),
                                  material_tv::cornerLarge, !selected);
    }

    std::array<float, 4> drawInputSurface(float x, float y, float width, float height, bool focused,
                                          float focusScale = materialInputFocusScale()) {
        const auto bounds = focusedBounds(x, y, width, height, focused, focusScale);
        constexpr float radius = material_tv::cornerMedium;
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius,
                              focused ? kPanelElevated : kPanel);
        renderer_.roundedOutline(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, focused ? 3.0f : 1.5f,
                                 focused ? kFocus : kOutline);
        return bounds;
    }

    void drawSwitch(float x, float y, bool on, bool focused) {
        constexpr float width = 112.0f;
        constexpr float height = 56.0f;
        constexpr float thumbSize = 36.0f;
        constexpr float inset = 10.0f;
        const Color track = on ? kFocusSoft : (focused ? material_tv::surfaceContainerHighest : kPanelAlt);
        renderer_.roundedRect(x, y, width, height, height * 0.5f, track);
        if (!on)
            renderer_.roundedOutline(x, y, width, height, height * 0.5f, focused ? 2.0f : 1.5f,
                                     focused ? kFocus : kOutline);
        const float thumbX = on ? x + width - thumbSize - inset : x + inset;
        renderer_.roundedRect(thumbX, y + inset, thumbSize, thumbSize, thumbSize * 0.5f,
                              on ? material_tv::onPrimaryContainer : kSecondaryText);
    }

    void drawRightAlignedSingleLine(float right, float y, float scale, std::string_view value, Color color,
                                    float maxWidth) {
        float fittedScale = scale;
        float width = renderer_.textWidth(fittedScale, value);
        if (maxWidth > 0.0f && width > maxWidth && width > 0.0f) {
            fittedScale *= maxWidth / width;
            width = renderer_.textWidth(fittedScale, value);
        }
        renderer_.text(right - width, y, fittedScale, value, color);
    }

    float fittedSingleLineScale(float scale, std::string_view value, float width, float height) const {
        float fittedScale = scale;
        const float measuredWidth = renderer_.textWidth(fittedScale, value);
        const float visualHeight = 10.0f * fittedScale * uiTextScale(settings_.uiTextSize);
        float fit = 1.0f;
        if (measuredWidth > width && measuredWidth > 0.0f) fit = std::min(fit, width / measuredWidth);
        if (visualHeight > height && visualHeight > 0.0f) fit = std::min(fit, height / visualHeight);
        return fittedScale * fit;
    }

    void drawCenteredSingleLineFit(float x, float y, float width, float height, float scale, std::string_view value,
                                   Color color, float horizontalPadding = 0.0f, float verticalPadding = 0.0f) {
        const float availableWidth = std::max(1.0f, width - horizontalPadding * 2.0f);
        const float availableHeight = std::max(1.0f, height - verticalPadding * 2.0f);
        const float fittedScale = fittedSingleLineScale(scale, value, availableWidth, availableHeight);
        renderer_.textCentered(x + horizontalPadding, y + verticalPadding, availableWidth, availableHeight, fittedScale,
                               value, color);
    }

    void drawLeftAlignedSingleLineFit(float x, float y, float width, float height, float scale, std::string_view value,
                                      Color color) {
        const float fittedScale = fittedSingleLineScale(scale, value, width, height);
        renderer_.textVerticallyCentered(x, y, height, fittedScale, fitTextLines(value, fittedScale, width, 1), color,
                                         width);
    }

    float drawChip(float x, float y, const std::string& label, bool selected = false, float scale = 1.45f,
                   float height = 42.0f, float maxWidth = 360.0f) {
        const std::string_view displayLabel = materialLabel(label);
        const float width = std::round(std::clamp(renderer_.textWidth(scale, displayLabel) + 34.0f, 72.0f, maxWidth));
        x = std::round(x);
        y = std::round(y);
        renderer_.roundedRect(x, y, width, height, height * 0.5f, selected ? kFocusSoft : kPanelAlt);
        if (!selected) renderer_.roundedOutline(x, y, width, height, height * 0.5f, 1.0f, kOutline);
        drawCenteredSingleLineFit(x, y, width, height, scale, displayLabel, selected ? kText : kSecondaryText, 14.0f,
                                  4.0f);
        return width;
    }

    void drawArtworkPlaceholder(const JellyfinItem& item, float x, float y, float width, float height,
                                float radius = material_tv::cornerSmall, float alpha = 1.0f) {
        const auto faded = [alpha](Color color) {
            color.a *= alpha;
            return color;
        };
        renderer_.roundedRect(x, y, width, height, radius, faded(kPanelAlt));
        renderer_.roundedOutline(x, y, width, height, radius, 1.0f, faded(kOutline));
        const std::string& source = item.type == "Episode" && !item.seriesName.empty() ? item.seriesName : item.name;
        std::string initial = "?";
        const auto first =
            std::find_if(source.begin(), source.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
        if (first != source.end())
            initial.assign(1, static_cast<char>(std::toupper(static_cast<unsigned char>(*first))));
        const float scale = height < 100.0f ? 2.0f : (height < 200.0f ? 3.0f : 4.2f);
        renderer_.textCentered(x, y, width, height, scale, initial, faded(kMuted));
    }

    void renderHeader(const std::string& title) {
        renderer_.text(material_tv::layout::pageInset, 28.0f, material_tv::type::supporting, "sloppaTV", kMuted);
        renderer_.text(material_tv::layout::pageInset, 76.0f, material_tv::type::headline,
                       fitTextLines(title, material_tv::type::headline, 1480.0f, 1), kText, 1480.0f);
        if (settings_.showClock) {
            drawRightAlignedSingleLine(1840.0f, 52.0f, 2.05f,
                                       formatLocalClock(std::time(nullptr), settings_.clock24Hour), kMuted, 210.0f);
        }
    }

    void renderEmptyState(const std::string& title, const std::string& message) {
        constexpr float x = 440.0f;
        constexpr float width = 1040.0f;
        renderer_.roundedRect(x, 350.0f, width, 250.0f, material_tv::cornerLarge, kPanelAlt);
        drawCenteredSingleLineFit(x + 48.0f, 386.0f, width - 96.0f, 80.0f, material_tv::type::title, title, kText,
                                  10.0f, 6.0f);
        drawCenteredSingleLineFit(x + 48.0f, 480.0f, width - 96.0f, 64.0f, material_tv::type::label, message, kMuted,
                                  10.0f, 5.0f);
    }

    void renderLogin() {
        renderLoginScreen(
            renderer_, Renderer::logicalWidth(), Renderer::logicalHeight(),
            LoginRenderState{
                .quickConnectActive = accountState_.quickConnectActive(),
                .quickConnectCode = accountState_.quickConnectCode(),
                .loading = loading_,
                .savedUserCount = static_cast<int>(sessionRegistry_.size()),
                .keyboardActive = accountState_.keyboardActive(),
                .loginFocus = accountState_.loginFocus(),
                .fields = {accountState_.field(0), accountState_.field(1), accountState_.field(2)},
                .discoveryStatus = accountState_.discoveryStatus(),
            },
            LoginRenderStyle<Color>{
                .cornerLarge = material_tv::cornerLarge,
                .cornerMedium = material_tv::cornerMedium,
                .displayScale = material_tv::type::display,
                .bodyScale = material_tv::type::body,
                .wideInputFocusScale = materialWideInputFocusScale(),
                .background = kBackground,
                .surfaceContainerHigh = material_tv::surfaceContainerHigh,
                .text = kText,
                .muted = kMuted,
                .secondaryText = kSecondaryText,
                .panelElevated = kPanelElevated,
                .outline = kOutline,
                .panelAlt = kPanelAlt,
                .focusSoft = kFocusSoft,
                .tertiary = kTertiary,
                .focus = kFocus,
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                          verticalPadding);
            },
            [this](float x, float y, float width, float height) { drawModalSurface(x, y, width, height); },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            },
            [this](float x, float y, float width, float height, bool focused, float focusScale) {
                return drawInputSurface(x, y, width, height, focused, focusScale);
            },
            [this](float x, float y, float width, float height, bool focused, bool primary) {
                return drawButtonSurface(x, y, width, height, focused, primary);
            },
            [this](float x, float y, float width, float height, bool focused) {
                return drawFocusedSurface(x, y, width, height, focused);
            },
            [this](std::string_view value, float scale, float width, int lines) {
                return fitTextLines(value, scale, width, lines);
            },
            [this](float top) { renderKeyboard(top); });
    }

    void renderProfiles() {
        renderProfilesScreen(
            renderer_, static_cast<int>(sessionRegistry_.size()), accountState_.profileSelection(),
            accountState_.profileAction(),
            ProfilesRenderStyle<Color>{
                .cornerMedium = material_tv::cornerMedium,
                .text = kText,
                .muted = kMuted,
                .focus = kFocus,
                .panelElevated = kPanelElevated,
                .panelAlt = kPanelAlt,
            },
            [this](std::string_view title) { renderHeader(std::string(title)); },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            },
            [this](float x, float y, float width, float height, bool focused, bool primary) {
                return drawFocusedSurface(x, y, width, height, focused, primary);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                          verticalPadding);
            },
            [this](int index) { return sessionRegistry_.at(static_cast<size_t>(index)); },
            [this](const auto& saved, float x, float y, float size) { return drawProfileArtwork(saved, x, y, size); },
            [this](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
                return drawButtonSurface(x, y, width, height, focused, primary, destructive);
            });
    }

    void renderKeyboard(float top) {
        const auto& rows = keyboardRows();
        renderer_.roundedRect(110.0f, top - 24.0f, 1700.0f, Renderer::logicalHeight() - top + 24.0f,
                              material_tv::cornerLarge, material_tv::surfaceContainerHigh);
        constexpr float startX = 150.0f;
        constexpr float gap = 14.0f;
        const float keyH = keyboardKeyHeight(top, static_cast<int>(rows.size()), gap);
        for (size_t row = 0; row < rows.size(); ++row) {
            const float y = top + static_cast<float>(row) * (keyH + gap);
            const auto& keys = rows[row];
            const float keyW = row == rows.size() - 1 ? 310.0f : 145.0f;
            for (size_t col = 0; col < keys.size(); ++col) {
                const float x = startX + static_cast<float>(col) * (keyW + gap);
                const bool selected = static_cast<int>(row) == keyboardRow_ && static_cast<int>(col) == keyboardCol_;
                const auto bounds = drawButtonSurface(x, y, keyW, keyH, selected, selected);
                const auto& label = keys[col].label;
                const float scale = label.size() > 4 ? 2.15f : 2.65f;
                drawCenteredSingleLineFit(bounds[0], bounds[1], bounds[2], bounds[3], scale, materialLabel(label),
                                          kText, 12.0f, 5.0f);
            }
        }
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
        renderHomeScreen(
            renderer_, home_.rows, homeState_, session_,
            HomeRenderConfig{
                .backdropMode = settings_.backdropMode,
                .showClock = settings_.showClock,
                .clock24Hour = settings_.clock24Hour,
                .uiTextSize = settings_.uiTextSize,
                .loading = homeLoading_,
            },
            HomeSlideState{
                .fromFirst = homeSlideFromFirst_,
                .toFirst = homeSlideToFirst_,
                .started = homeSlideStarted_,
            },
            HomeRenderStyle<Color>{
                .canvasWidth = Renderer::logicalWidth(),
                .canvasHeight = Renderer::logicalHeight(),
                .cornerLarge = material_tv::cornerLarge,
                .buttonFocusScale = materialButtonFocusScale(),
                .background = kBackground,
                .backdropScrim = Color{0.01f, 0.012f, 0.018f, 0.34f},
                .text = kText,
                .muted = kMuted,
                .clockMuted = Color{kMuted.r, kMuted.g, kMuted.b, 0.82f},
                .brandGold = kBrandGold,
                .focusSoft = kFocusSoft,
                .panelElevated = kPanelElevated,
                .panelAlt = kPanelAlt,
                .focus = kFocus,
            },
            [&](const JellyfinItem& item, float alpha) { return drawBackdrop(item, alpha); },
            [&](float x, float y, float size) { return drawBrandMark(x, y, size); },
            [&](float x, float y, float width, float height, bool focused, bool selected) {
                return drawTabSurface(x, y, width, height, focused, selected);
            },
            [&](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
            },
            [&](float x, float y, float width, float height, bool focused, float focusScale) {
                return focusedBounds(x, y, width, height, focused, focusScale);
            },
            [&](const JellyfinSession& saved, float x, float y, float size) {
                return drawProfileArtwork(saved, x, y, size);
            },
            [&](float x, float y, float width, float height, Color color, float radius) {
                drawFocusHalo(x, y, width, height, color, radius);
            },
            [&](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
                drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
            },
            [](bool clock24Hour) { return formatLocalClock(std::time(nullptr), clock24Hour); },
            [&](std::string_view title, std::string_view message) {
                renderEmptyState(std::string(title), std::string(message));
            },
            [&](std::string_view title, const std::vector<JellyfinItem>& items, int row, float top) {
                renderHomeRow(std::string(title), items, row, top);
            },
            [] { return std::chrono::steady_clock::now(); });
    }

    void renderHomeRow(const std::string& title, const std::vector<JellyfinItem>& items, int row, float top) {
        if (items.empty()) return;
        const auto now = std::chrono::steady_clock::now();
        const int selected = homeState_.selection(row, static_cast<int>(items.size()));
        const float imageOffset = homeRowImageOffset(settings_.uiTextSize);

        if (title == "My Media") {
            renderer_.text(72.0f, top, 3.05f, "My media", homeState_.row() == row ? kText : kSecondaryText, 420.0f);
            constexpr float cardW = 420.0f;
            constexpr float cardH = 225.0f;
            constexpr float gap = 28.0f;
            const float imageY = top + imageOffset;
            float x = 72.0f;
            const int start = homeState_.firstVisibleItem(row, static_cast<int>(items.size()), 4);
            for (int index = start; index < static_cast<int>(items.size()); ++index) {
                if (x + cardW > 1885.0f && index > start) break;
                const bool focused = homeState_.row() == row && index == selected;
                const auto bounds = focusedBounds(x, imageY, cardW, cardH, focused, materialCardFocusScale());
                const float cardRadius = material_tv::cornerSmall * bounds[3] / cardH;
                const auto& item = items[static_cast<size_t>(index)];
                const bool hasArtwork = drawHomeArtwork(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius);
                if (!hasArtwork) drawArtworkPlaceholder(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius);
                if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, cardRadius);
                renderer_.text(x + 4.0f, imageY + cardH + 24.0f, 2.05f,
                               fitTextLines(items[static_cast<size_t>(index)].name, 2.05f, cardW - 8.0f, 1),
                               focused ? kText : kSecondaryText, cardW - 8.0f);
                x += cardW + gap;
            }
            return;
        }

        renderer_.text(72.0f, top, 3.05f, fitTextLines(title, 3.05f, 900.0f, 1),
                       homeState_.row() == row ? kText : kSecondaryText, 900.0f);
        const int start = homeState_.firstVisibleItem(row, static_cast<int>(items.size()), 5);
        constexpr float cardH = 202.0f;
        constexpr float cardW = 350.0f;
        constexpr float gap = 18.0f;
        const float imageY = top + imageOffset;
        float x = 72.0f;

        auto singleLine = [&](std::string_view value, float scale, float width) {
            return fitTextLines(value, scale, width, 1);
        };

        for (int index = start; index < static_cast<int>(items.size()); ++index) {
            if (x + cardW > 1908.0f && index > start) break;
            const auto& item = items[static_cast<size_t>(index)];
            float itemAlpha = 1.0f;
            if (title == "Next Up" && index == nextUpReplacementFadeIndex_ && item.id == nextUpReplacementFadeItemId_ &&
                nextUpReplacementFadeStarted_ != std::chrono::steady_clock::time_point{}) {
                const auto fadeElapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - nextUpReplacementFadeStarted_).count();
                const float progress = std::clamp(static_cast<float>(fadeElapsed) / 300.0f, 0.0f, 1.0f);
                itemAlpha = progress * progress * (3.0f - 2.0f * progress);
            }
            const auto faded = [itemAlpha](Color color) {
                color.a *= itemAlpha;
                return color;
            };
            const bool focused = homeState_.row() == row && index == selected;
            const auto bounds = focusedBounds(x, imageY, cardW, cardH, focused, materialCardFocusScale());
            const float cardRadius = material_tv::cornerSmall * bounds[3] / cardH;
            const bool hasArtwork =
                drawHomeArtwork(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, itemAlpha);
            if (!hasArtwork)
                drawArtworkPlaceholder(item, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, itemAlpha);
            if (item.externalProgressPercent >= 0) {
                const double progress = std::clamp(static_cast<double>(item.externalProgressPercent) / 100.0, 0.0, 1.0);
                renderer_.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f, bounds[2] - 16.0f, 4.0f, 2.0f,
                                      faded(kTrack));
                renderer_.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f,
                                      static_cast<float>((bounds[2] - 16.0f) * progress), 4.0f, 2.0f, faded(kFocus));
            } else if (item.positionTicks > 0 && item.runtimeTicks > 0) {
                const double progress = std::clamp(
                    static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
                renderer_.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f, bounds[2] - 16.0f, 4.0f, 2.0f,
                                      faded(kTrack));
                renderer_.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f,
                                      static_cast<float>((bounds[2] - 16.0f) * progress), 4.0f, 2.0f, faded(kFocus));
            }
            if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], faded(kFocus), cardRadius);

            std::string primary = item.type == "Episode" && !item.seriesName.empty() ? item.seriesName : item.name;
            primary = singleLine(primary, 2.45f, cardW - 18.0f);
            const float titleY = imageY + cardH + 22.0f;
            renderer_.text(x + 2.0f, titleY, 2.45f, primary, faded(focused ? kText : kSecondaryText), cardW - 4.0f);
            if (isSeerrItem(item) && !item.externalStatus.empty()) {
                const float secondaryY = titleY + 11.0f * 2.45f * uiTextScale(settings_.uiTextSize) + 4.0f;
                if (item.externalProgressPercent >= 0 && !item.externalProgressLabel.empty()) {
                    const std::string percentLabel = std::to_string(item.externalProgressPercent) + "%";
                    const float percentWidth = std::ceil(renderer_.textWidth(1.16f, percentLabel));
                    const float etaWidth =
                        item.externalProgressEta.empty()
                            ? 0.0f
                            : std::round(std::clamp(
                                  renderer_.textWidth(1.08f, materialLabel(item.externalProgressEta)) + 34.0f, 72.0f,
                                  210.0f));
                    constexpr float metadataGap = 12.0f;
                    constexpr float statusToMetadataGap = 22.0f;
                    const float rightEdge = x + cardW - 2.0f;
                    const float etaX = rightEdge - etaWidth;
                    const float percentX =
                        item.externalProgressEta.empty() ? rightEdge - percentWidth : etaX - metadataGap - percentWidth;
                    const float statusWidth = std::max(60.0f, percentX - (x + 2.0f) - statusToMetadataGap);
                    renderer_.text(x + 2.0f, secondaryY, 1.48f,
                                   singleLine(item.externalProgressLabel, 1.48f, statusWidth), faded(kMuted),
                                   statusWidth);
                    renderer_.textVerticallyCentered(percentX, secondaryY - 3.0f, 30.0f, 1.16f, percentLabel,
                                                     kSecondaryText, percentWidth);
                    if (!item.externalProgressEta.empty()) {
                        drawChip(etaX, secondaryY - 3.0f, item.externalProgressEta, false, 1.08f, 30.0f, 210.0f);
                    }
                } else {
                    renderer_.text(x + 2.0f, secondaryY, 1.58f, singleLine(item.externalStatus, 1.58f, cardW - 4.0f),
                                   faded(kMuted), cardW - 4.0f);
                }
            } else if (item.type == "Episode") {
                std::string episode = episodeNumberLabel(item);
                if (!item.name.empty() && item.name != item.seriesName) {
                    if (!episode.empty()) episode += "  |  ";
                    episode += item.name;
                }
                if (!episode.empty()) {
                    const float secondaryY = titleY + 11.0f * 2.45f * uiTextScale(settings_.uiTextSize) + 4.0f;
                    renderer_.text(x + 2.0f, secondaryY, 1.58f, singleLine(episode, 1.58f, cardW - 4.0f), faded(kMuted),
                                   cardW - 4.0f);
                }
            }
            x += cardW + gap;
        }
    }

    void renderMediaArtworkCard(const JellyfinItem& item, float x, float y, float slotWidth, bool focused,
                                bool showState = true, bool preferSeriesCover = false, bool alignToPortraitBand = false,
                                int titleLineLimit = 0) {
        const bool seriesCoverForEpisode = preferSeriesCover && item.type == "Episode" && !item.seriesId.empty() &&
                                           !item.seriesPrimaryImageTag.empty();
        const bool landscape = usesLandscapeMediaCard(item.type) && !seriesCoverForEpisode;
        const float imageWidth = landscape ? slotWidth : mediaPosterWidth();
        const float imageHeight = landscape ? 180.0f : mediaPosterHeight();
        const float artworkBandHeight = alignToPortraitBand ? mediaPosterHeight() : imageHeight;
        const float imageX = x + (slotWidth - imageWidth) * 0.5f;
        const float imageY = y + std::max(0.0f, (artworkBandHeight - imageHeight) * 0.5f);
        const auto bounds = focusedBounds(imageX, imageY, imageWidth, imageHeight, focused, materialCardFocusScale());
        const float cardRadius = material_tv::cornerSmall * bounds[3] / imageHeight;
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], cardRadius, kPanelAlt);
        JellyfinItem cover = item;
        if (seriesCoverForEpisode) {
            cover.id = item.seriesId;
            cover.imageTag = item.seriesPrimaryImageTag;
            cover.type = "Series";
        }
        const bool hasArtwork = landscape
                                    ? drawHomeArtwork(cover, bounds[0], bounds[1], bounds[2], bounds[3], cardRadius)
                                    : drawArtwork(cover, bounds[0], bounds[1], bounds[2], bounds[3], 1.0f, cardRadius);
        if (!hasArtwork) {
            drawArtworkPlaceholder(item, bounds[0] + 1.0f, bounds[1] + 1.0f, bounds[2] - 2.0f, bounds[3] - 2.0f,
                                   std::max(0.0f, cardRadius - 1.0f));
        }
        if (item.positionTicks > 0 && item.runtimeTicks > 0) {
            const double fraction =
                std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
            renderer_.roundedRect(bounds[0] + 10.0f, bounds[1] + bounds[3] - 14.0f, bounds[2] - 20.0f, 5.0f, 2.5f,
                                  kTrack);
            renderer_.roundedRect(bounds[0] + 10.0f, bounds[1] + bounds[3] - 14.0f,
                                  static_cast<float>((bounds[2] - 20.0f) * fraction), 5.0f, 2.5f, kFocus);
        }
        if (showState && (item.favorite || (settings_.showWatchedIndicators && item.played))) {
            const std::string label = item.favorite ? "Favorite" : "Watched";
            const float badgeWidth = item.favorite ? 132.0f : 118.0f;
            const float badgeX = bounds[0] + bounds[2] - badgeWidth - 12.0f;
            const float badgeY = bounds[1] + 12.0f;
            const Color badgeSurface = item.favorite ? kFocusSoft : kPanelElevated;
            renderer_.roundedRect(badgeX, badgeY, badgeWidth, 34.0f, 17.0f, badgeSurface);
            renderer_.roundedOutline(badgeX, badgeY, badgeWidth, 34.0f, 17.0f, 1.0f, item.favorite ? kFocus : kOutline);
            drawCenteredSingleLineFit(badgeX, badgeY, badgeWidth, 34.0f, 1.12f, label, kText, 10.0f, 3.0f);
        }
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, cardRadius);

        const float titleY = y + artworkBandHeight + 24.0f;
        const int titleLines = titleLineLimit > 0 ? titleLineLimit : (landscape ? 1 : 2);
        const std::string fittedTitle =
            fitTextLines(item.name, material_tv::type::label, imageWidth - 4.0f, titleLines);
        renderer_.text(imageX + 2.0f, titleY, material_tv::type::label, fittedTitle, kText, imageWidth - 4.0f);
        const std::string secondary =
            isSeerrItem(item) ? (item.externalRequested ? item.externalStatus : std::string("Press OK to request"))
                              : episodeLabel(item);
        if (!secondary.empty()) {
            const int renderedTitleLines =
                fittedTitle.empty() ? 0
                                    : 1 + static_cast<int>(std::count(fittedTitle.begin(), fittedTitle.end(), '\n'));
            const float titleLineHeight = 11.0f * material_tv::type::label * uiTextScale(settings_.uiTextSize);
            const float secondaryY = titleY + titleLineHeight * static_cast<float>(renderedTitleLines) + 3.0f;
            const float secondaryHeight = 10.0f * 1.45f * uiTextScale(settings_.uiTextSize);
            if (secondaryY + secondaryHeight <= Renderer::logicalHeight() - 8.0f) {
                renderer_.text(imageX + 2.0f, secondaryY, 1.45f, fitTextLines(secondary, 1.45f, imageWidth - 4.0f, 1),
                               kMuted, imageWidth - 4.0f);
            }
        }
    }

    void renderTextTile(const JellyfinItem& item, float x, float y, float width, float height, bool focused) {
        const auto bounds = focusedBounds(x, y, width, height, focused, materialCardFocusScale());
        const float tileRadius = material_tv::cornerMedium * bounds[3] / height;
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], tileRadius,
                              focused ? kPanelElevated : kPanel);
        drawCenteredSingleLineFit(bounds[0] + 28.0f, bounds[1] + 18.0f, bounds[2] - 56.0f, 42.0f, 1.25f, item.type,
                                  kTertiary, 8.0f, 3.0f);
        drawCenteredSingleLineFit(bounds[0] + 28.0f, bounds[1] + 58.0f, bounds[2] - 56.0f, bounds[3] - 76.0f, 2.55f,
                                  item.name, kText, 10.0f, 6.0f);
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, tileRadius);
    }

    void renderBrowse() {
        renderBrowseScreen(
            renderer_, browseState_, loading_,
            BrowseRenderStyle<Color>{
                .cornerLarge = material_tv::cornerLarge,
                .focusSoft = kFocusSoft,
                .panel = kPanel,
                .outline = kOutline,
                .text = kText,
                .muted = kMuted,
            },
            [&](std::string_view heading) { renderHeader(std::string(heading)); },
            [&](std::string_view title, std::string_view message) {
                renderEmptyState(std::string(title), std::string(message));
            },
            [&](float x, float y, float width, float height, bool focused, bool selected) {
                return drawTabSurface(x, y, width, height, focused, selected);
            },
            [&](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height, bool focused) {
                renderTextTile(item, x, y, width, height, focused);
            },
            [&](const JellyfinItem& item, float x, float y, float width, bool focused, bool showState,
                bool seriesCoverForEpisode, bool alignMixedHeights, int titleLineLimit) {
                renderMediaArtworkCard(item, x, y, width, focused, showState, seriesCoverForEpisode, alignMixedHeights,
                                       titleLineLimit);
            });
    }

    void renderSearch() {
        renderSearchScreen(
            renderer_, searchState_, SeerrClient::configured(settings_.seerrServer, seerrAuth()),
            systemTextInputMode_ == kTextInputSearch, seerrDomain_.searchLoading(), seerrDomain_.searchError(),
            seerrDomain_.storageLoading(), seerrDomain_.storageTargets(),
            SearchRenderStyle<Color>{
                .headlineScale = material_tv::type::headline,
                .cornerSmall = material_tv::cornerSmall,
                .wideInputFocusScale = materialWideInputFocusScale(),
                .cardFocusScale = materialCardFocusScale(),
                .text = kText,
                .muted = kMuted,
                .secondaryText = kSecondaryText,
                .focus = kFocus,
                .divider = kDivider,
                .panel = kPanel,
                .panelAlt = kPanelAlt,
                .panelElevated = kPanelElevated,
                .error = kError,
            },
            [&](float x, float y, float width, float height, bool focused, float focusScale) {
                return drawInputSurface(x, y, width, height, focused, focusScale);
            },
            [&](std::string_view value, float scale, float maxWidth, int maxLines) {
                return fitTextLines(value, scale, maxWidth, maxLines);
            },
            [&](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            },
            [&](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
            },
            [&](float top) { renderKeyboard(top); },
            [&](std::string_view title, std::string_view message) {
                renderEmptyState(std::string(title), std::string(message));
            },
            [&](float x, float y, float width, float height, bool focused, float focusScale) {
                return focusedBounds(x, y, width, height, focused, focusScale);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                return drawHomeArtwork(item, x, y, width, height, radius);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                drawArtworkPlaceholder(item, x, y, width, height, radius);
            },
            [&](float x, float y, float width, float height, Color color, float radius) {
                drawFocusHalo(x, y, width, height, color, radius);
            },
            [&](float x, float y, float scale, std::string_view value, float maxWidth, Color color,
                std::chrono::steady_clock::time_point now) {
                drawLingeringTitle(x, y, scale, value, maxWidth, color, now);
            },
            [&](const JellyfinItem& item, float x, float y, float width, bool focused, bool showState,
                bool seriesCoverForEpisode, bool alignMixedHeights, int titleLineLimit) {
                renderMediaArtworkCard(item, x, y, width, focused, showState, seriesCoverForEpisode, alignMixedHeights,
                                       titleLineLimit);
            },
            [] { return std::chrono::steady_clock::now(); },
            [](Color color, float alpha) { return Color{color.r, color.g, color.b, alpha}; });
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
                drawListItemSurface(x, y, width, height, focused, radius, focusScale);
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return fitTextLines(value, scale, maxWidth, maxLines);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                          verticalPadding);
            },
            [this](const std::string& title, const std::string& message) { renderEmptyState(title, message); });
    }

    void renderPlayer() {
        const PlayerStatus status = player_.status();
        std::string videoError;
        if (videoSurface_.ready()) {
            videoSurface_.update(videoError);
            renderPlayerVideo(
                renderer_,
                PlayerVideoRenderState{
                    .texture = videoSurface_.texture(),
                    .sourceWidth = player_.videoWidth(),
                    .sourceHeight = player_.videoHeight(),
                    .zoomMode = playbackCoordinator_.session().zoomMode(),
                    .logicalWidth = Renderer::logicalWidth(),
                    .logicalHeight = Renderer::logicalHeight(),
                },
                videoSurface_.transform());
        }

        const auto now = std::chrono::steady_clock::now();
        const int remainingMs = playerScreenState_.durationMs() > 0
                                    ? std::max(0, playerScreenState_.durationMs() - playerScreenState_.positionMs())
                                    : 0;
        const auto skipSegment = playbackCoordinator_.activeSkippableSegment(playerScreenState_.positionMs());
        const bool userOverlayVisible = playerScreenState_.overlayVisible(now);
        const bool showNextUp = shouldShowNextUpCard(playbackCoordinator_.continuation().nextItem().has_value(), remainingMs,
                                                     userOverlayVisible, skipSegment != nullptr);
        const bool showOverlay = status == PlayerStatus::Preparing || status == PlayerStatus::Paused ||
                                 playbackCoordinator_.transitionLoading() || playbackCoordinator_.fallbackResolving() ||
                                 userOverlayVisible;
        if (showOverlay) {
            renderer_.verticalGradient(0.0f, 0.0f, 1920.0f, 250.0f, Color{0.0f, 0.0f, 0.0f, 0.74f},
                                       Color{0.0f, 0.0f, 0.0f, 0.0f});
            renderer_.verticalGradient(0.0f, 650.0f, 1920.0f, 430.0f, Color{0.0f, 0.0f, 0.0f, 0.0f},
                                       Color{0.0f, 0.0f, 0.0f, 0.90f});
        }
        std::string subtitleText = player_.subtitleText();
        if (const SubtitleCue* cue = playbackCoordinator_.activeSubtitleCue(playerScreenState_.positionMs()))
            subtitleText = cue->text;
        if (!subtitleText.empty()) {
            const float textScale = subtitleTextScale(settings_.subtitleSize);
            renderPlayerSubtitle(
                renderer_,
                PlayerSubtitleRenderState{
                    .text = subtitleText,
                    .boxMaxWidth = subtitleBoxMaxWidth(skipSegment != nullptr),
                    .textScale = textScale,
                    .lineHeight = 11.0f * textScale * uiTextScale(settings_.uiTextSize),
                    .logicalWidth = Renderer::logicalWidth(),
                    .bottomY = subtitleBottomY(showOverlay, playerScreenState_.controlsActive(now),
                                               settings_.subtitlePosition, skipSegment != nullptr),
                    .showBackground = settings_.subtitleBackground,
                },
                PlayerSubtitleRenderStyle<Color>{
                    .cornerRadius = material_tv::cornerMedium,
                    .text = kText,
                    .background = Color{0.0f, 0.0f, 0.0f, 0.80f},
                    .outline = Color{0.0f, 0.0f, 0.0f, 0.92f},
                },
                [this](std::string value, float scale, float maxWidth, int maxLines) {
                    return fitTextLines(value, scale, maxWidth, maxLines);
                });
        }
        if (skipSegment) {
            const std::string skipLabel = mediaSegmentSkipLabel(*skipSegment);
            renderPlayerSkipButton(
                renderer_,
                PlayerSkipButtonRenderState{
                    .label = skipLabel,
                    .y = skipButtonY(showOverlay),
                },
                PlayerSkipButtonRenderStyle<Color>{.text = kText},
                [this](float x, float y, float width, float height, bool focused, bool primary) {
                    return drawButtonSurface(x, y, width, height, focused, primary);
                },
                [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                    return fitTextLines(value, scale, maxWidth, maxLines);
                });
        }
        if (playerScreenState_.seekFeedbackVisible(now)) {
            renderPlayerSeekFeedback(
                renderer_,
                PlayerSeekFeedbackRenderState{
                    .seconds = playerScreenState_.seekFeedbackSeconds(),
                    .fade = playerScreenState_.seekFeedbackAlpha(now),
                },
                [](float r, float g, float b, float a) { return Color{r, g, b, a}; });
        }
        if (!showOverlay) return;

        if (showNextUp && playbackCoordinator_.continuation().nextItem()) {
            renderPlayerNextUp(
                PlayerNextUpRenderState{
                    .item = *playbackCoordinator_.continuation().nextItem(),
                    .remainingMs = remainingMs,
                },
                PlayerNextUpRenderStyle<Color>{
                    .panelCornerRadius = material_tv::cornerMedium,
                    .artworkCornerRadius = material_tv::cornerExtraSmall,
                    .focus = kFocus,
                    .text = kText,
                    .muted = kMuted,
                },
                [this](float x, float y, float width, float height, float radius) {
                    drawModalSurface(x, y, width, height, radius);
                },
                [this](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                    return drawHomeArtwork(item, x, y, width, height, radius);
                },
                [this](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                    drawArtworkPlaceholder(item, x, y, width, height, radius);
                },
                [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                    drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
                },
                [](const JellyfinItem& item) { return episodeLabel(item); });
        }

        const std::string heading = playbackCoordinator_.session().activeItem().seriesName.empty()
                                        ? playbackCoordinator_.session().activeItem().name
                                        : playbackCoordinator_.session().activeItem().seriesName;
        const std::string playerEpisodeNumber = episodeNumberLabel(playbackCoordinator_.session().activeItem());
        const std::string secondary =
            playbackCoordinator_.session().activeItem().seriesName.empty()
                ? episodeLabel(playbackCoordinator_.session().activeItem())
                : playerEpisodeNumber + (playbackCoordinator_.session().activeItem().name.empty()
                                             ? ""
                                             : "  |  " + playbackCoordinator_.session().activeItem().name);
        renderPlayerHeader(
            renderer_,
            PlayerHeaderRenderState{
                .heading = heading,
                .secondary = secondary,
                .showNextUp = showNextUp,
                .headlineScale = material_tv::type::headline,
                .secondaryY = 42.0f + 11.0f * material_tv::type::headline * uiTextScale(settings_.uiTextSize) + 8.0f,
            },
            PlayerHeaderRenderStyle<Color>{
                .text = kText,
                .muted = kMuted,
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return fitTextLines(value, scale, maxWidth, maxLines);
            });

        const int position = playerScreenState_.positionMs();
        const int duration = playerScreenState_.durationMs();
        if (settings_.showClock) {
            const std::time_t wallNow = std::time(nullptr);
            drawRightAlignedSingleLine(1840.0f, 46.0f, 2.05f, formatLocalClock(wallNow, settings_.clock24Hour), kMuted,
                                       210.0f);
            if (remainingMs > 0 && status == PlayerStatus::Playing && !skipSegment) {
                const std::time_t finishAt = wallNow + static_cast<std::time_t>((remainingMs + 999) / 1000);
                const std::string finishLabel = "Ends " + formatLocalClock(finishAt, settings_.clock24Hour);
                const float finishWidth = renderer_.textWidth(1.75f, finishLabel);
                renderer_.text(std::max(1180.0f, 1770.0f - finishWidth), 772.0f, 1.75f, finishLabel, kSecondaryText,
                               590.0f);
            }
        }
        const std::string state =
            playbackCoordinator_.fallbackResolving()
                ? "Retrying playback"
                : (playbackCoordinator_.transitionLoading()
                       ? (playbackCoordinator_.session().activeItem().id.empty() ? "Loading episode" : "Switching track")
                       : (status == PlayerStatus::Preparing ? "Loading" : ""));
        if (!state.empty()) renderer_.text(80.0f, 772.0f, 2.0f, state, kSecondaryText, 580.0f);

        const std::string positionText = formatPlaybackTime(position);
        const std::string durationText = formatPlaybackTime(duration);
        renderPlayerProgress(
            renderer_,
            PlayerProgressRenderState{
                .positionMs = position,
                .durationMs = duration,
                .skipButtonVisible = skipSegment != nullptr,
                .positionText = positionText,
                .durationText = durationText,
            },
            PlayerProgressRenderStyle<Color>{
                .text = kText,
                .track = kTrack,
                .focus = kFocus,
            },
            [this](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
                drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
            });
        drawTrickplayPreview();

        if (playerScreenState_.controlsActive(now)) {
            renderPlayerControls(
                renderer_,
                PlayerControlsRenderState{
                    .paused = status == PlayerStatus::Paused,
                    .selection = playerScreenState_.controlSelection(),
                    .logicalWidth = Renderer::logicalWidth(),
                    .audioTrackLabel = playbackCoordinator_.trackLabel(PlaybackTrackLabelKind::Audio),
                    .subtitleTrackLabel = playbackCoordinator_.trackLabel(PlaybackTrackLabelKind::Subtitle),
                },
                PlayerControlsRenderStyle<Color>{.text = kText},
                [this](float x, float y, float width, float height, bool focused, bool primary) {
                    return drawButtonSurface(x, y, width, height, focused, primary);
                },
                [this](float scale, std::string_view value, float width, float height) {
                    return fittedSingleLineScale(scale, value, width, height);
                },
                [](std::string_view value) { return materialLabel(value); });
        }
    }

    void renderQueueOverlay() {
        if (queueState_.empty()) return;
        queueState_.setSelection(queueState_.selection());
        renderQueueOverlayScreen(
            renderer_,
            QueueOverlayRenderState{
                .items = queueState_.items(),
                .currentIndex = queueState_.currentIndex(),
                .selection = queueState_.selection(),
                .actionSelection = queueState_.actionSelection(),
                .repeatMode = queueState_.repeatMode(),
            },
            QueueOverlayRenderStyle<Color>{
                .artworkCornerRadius = material_tv::cornerExtraSmall,
                .scrim = kScrim,
                .text = kText,
                .muted = kMuted,
                .tertiary = kTertiary,
                .focusSoft = kFocusSoft,
                .panelAlt = kPanelAlt,
            },
            [this](float x, float y, float width, float height) { drawModalSurface(x, y, width, height); },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
            },
            [this](float x, float y, float width, float height, bool focused) {
                return drawListItemSurface(x, y, width, height, focused);
            },
            [this](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                return drawHomeArtwork(item, x, y, width, height, radius);
            },
            [this](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                drawArtworkPlaceholder(item, x, y, width, height, radius);
            },
            [](const JellyfinItem& item) { return episodeLabel(item); },
            [this](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
                return drawButtonSurface(x, y, width, height, focused, primary, destructive);
            },
            [this](float x, float y, float width, float height) { drawDisabledButtonSurface(x, y, width, height); },
            [](std::string_view value) { return std::string(materialLabel(std::string(value))); });
    }

    void renderScreensaver() {
        const int64_t elapsedSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        const std::string clock = formatLocalClock(std::time(nullptr), settings_.clock24Hour);
        renderScreensaverScreen(
            renderer_, elapsedSeconds, clock, Renderer::logicalWidth(), Renderer::logicalHeight(),
            ScreensaverRenderStyle<Color>{
                .background = Color{0.006f, 0.008f, 0.012f, 1.0f},
                .primary = material_tv::primary,
                .text = kText,
                .tertiary = kTertiary,
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, value, color);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                          verticalPadding);
            });
    }

    void renderSettings() {
        const std::string externalPlayer = externalPlayerLabel();
        renderSettingsScreen(
            renderer_,
            SettingsRenderState{
                .screen = settingsScreen_,
                .settings = settings_,
                .maxAudioOutputChannels = api_.deviceCodecSupport().maxAudioOutputChannels,
                .externalPlayer = externalPlayer,
                .username = session_.username,
                .systemSettingsInputActive = systemTextInputMode_ == kTextInputSettingsSearch,
                .seerrApiKeyTyping = systemTextInputMode_ == kTextInputSeerrApiKey,
            },
            SettingsRenderStyle<Color>{
                .headlineScale = material_tv::type::headline,
                .cornerMedium = material_tv::cornerMedium,
                .cornerSmall = material_tv::cornerSmall,
                .wideInputFocusScale = materialWideInputFocusScale(),
                .wideListItemFocusScale = materialWideListItemFocusScale(),
                .listItemFocusScale = materialListItemFocusScale(),
                .text = kText,
                .secondaryText = kSecondaryText,
                .muted = kMuted,
                .focus = kFocus,
                .focusSoft = kFocusSoft,
                .panelAlt = kPanelAlt,
                .outline = kOutline,
                .scrim = kScrim,
            },
            [this](float x, float y, float width, float height, bool focused, float focusScale) {
                return drawInputSurface(x, y, width, height, focused, focusScale);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return fitTextLines(std::string(value), scale, maxWidth, maxLines);
            },
            [this](std::string_view title, std::string_view message) {
                renderEmptyState(std::string(title), std::string(message));
            },
            [this](float x, float y, float width, float height, bool focused, float radius, float focusScale) {
                return drawListItemSurface(x, y, width, height, focused, radius, focusScale);
            },
            [](std::string_view value) { return std::string(materialLabel(std::string(value))); },
            [this](float x, float y, bool on, bool focused) { drawSwitch(x, y, on, focused); },
            [this](float x, float y, std::string_view label, bool selected, float scale, float height, float maxWidth) {
                return drawChip(x, y, std::string(label), selected, scale, height, maxWidth);
            },
            [this](float x, float y, float width, float height) { drawModalSurface(x, y, width, height); });
    }

    void renderDiagnostics() {
        const auto& codecs = api_.deviceCodecSupport();
        const auto videoCodecs = codecs.jellyfinVideoCodecs();
        const auto audioCodecs = codecs.jellyfinAudioCodecs(codecs.maxAudioOutputChannels);
        std::vector<std::string> hdr;
        if (codecs.displayHdr10) hdr.emplace_back("HDR10");
        if (codecs.displayHdr10Plus) hdr.emplace_back("HDR10+");
        if (codecs.displayDolbyVision) hdr.emplace_back("DOLBY VISION");
        if (codecs.displayHlg) hdr.emplace_back("HLG");

        std::string architecture = "UNKNOWN";
#if defined(__aarch64__)
        architecture = "ARM64";
#elif defined(__arm__)
        architecture = "ARM32";
#elif defined(__x86_64__)
        architecture = "X86_64";
#endif

        renderDiagnosticsScreen(
            renderer_,
            DiagnosticsScreenData{
                .appVersion = SLOPPATV_VERSION_NAME,
                .architecture = std::move(architecture),
                .sessionServer = session_.server,
                .serverName = serverInfo_.name,
                .serverVersion = serverInfo_.version,
                .serverLoading = loading_,
                .videoCodecs = videoCodecs,
                .audioCodecs = audioCodecs,
                .maxAudioOutputChannels = codecs.maxAudioOutputChannels,
                .maxHevcWidth = codecs.maxHevcWidth,
                .maxHevcHeight = codecs.maxHevcHeight,
                .hdrFormats = std::move(hdr),
                .lastPlaybackSummary = playbackCoordinator_.session().lastPlaybackSummary(),
            },
            DiagnosticsRenderStyle<Color>{
                .cornerLarge = material_tv::cornerLarge,
                .panelAlt = kPanelAlt,
                .outline = kOutline,
                .divider = kDivider,
                .tertiary = kTertiary,
                .text = kText,
                .muted = kMuted,
            },
            [this](std::string_view title) { renderHeader(std::string(title)); },
            [this](float x, float y, std::string_view label, bool selected, float scale, float height, float maxWidth) {
                return drawChip(x, y, std::string(label), selected, scale, height, maxWidth);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color) {
                drawLeftAlignedSingleLineFit(x, y, width, height, scale, std::string(value), color);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, std::string(value), color, horizontalPadding,
                                          verticalPadding);
            });
    }

    void renderItemMenu() {
        std::vector<std::string> actions;
        if (!detailsState_.deleteConfirmation()) actions = itemMenuActions();
        renderItemMenuScreen(
            renderer_, Renderer::logicalWidth(), Renderer::logicalHeight(),
            ItemMenuRenderState{
                .deleteConfirmation = detailsState_.deleteConfirmation(),
                .deleteConfirmationSelection = detailsState_.deleteConfirmationSelection(),
                .itemMenuSelection = detailsState_.itemMenuSelection(),
                .seerrRequest = isSeerrItem(detail_),
                .itemName = detail_.name,
                .itemType = detail_.type,
                .externalStatus = detail_.externalStatus,
                .externalProgressPercent = detail_.externalProgressPercent,
                .externalProgressLabel = detail_.externalProgressLabel,
                .externalProgressEta = detail_.externalProgressEta,
            },
            actions,
            ItemMenuRenderStyle<Color>{
                .cornerLarge = material_tv::cornerLarge,
                .scrim = kScrim,
                .error = kError,
                .text = kText,
                .muted = kMuted,
                .tertiary = kTertiary,
                .focus = kFocus,
                .secondaryText = kSecondaryText,
                .divider = kDivider,
            },
            [this](float x, float y, float width, float height) { drawModalSurface(x, y, width, height); },
            [this](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
                return drawButtonSurface(x, y, width, height, focused, primary, destructive);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                          verticalPadding);
            },
            [this](std::string_view value, float scale, float width, int lines) {
                return fitTextLines(value, scale, width, lines);
            },
            [this](float x, float y, std::string_view label, bool selected, float scale, float height, float maxWidth) {
                return drawChip(x, y, std::string(label), selected, scale, height, maxWidth);
            },
            [this](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
                return drawFocusedSurface(x, y, width, height, focused, primary, destructive);
            },
            [](std::string_view value) { return materialLabel(value); });
    }

    void drawLingeringTitle(float x, float y, float scale, std::string_view value, float maxWidth, Color color,
                            std::chrono::steady_clock::time_point now) {
        const std::string displayValue = displayText(value);
        if (displayValue.empty()) return;
        const float titleWidth = renderer_.textWidth(scale, displayValue);
        if (titleWidth <= maxWidth) {
            renderer_.text(x, y, scale, displayValue, color, maxWidth);
            return;
        }

        constexpr auto linger = 1200ms;
        const auto marqueeStart = lastInteraction_ + linger;
        if (now < marqueeStart) {
            renderer_.text(x, y, scale, fitTextLines(displayValue, scale, maxWidth, 1), color, maxWidth);
            return;
        }

        constexpr float speedPixelsPerSecond = 28.0f;
        const std::string gap = "      ";
        const float cycleWidth = titleWidth + renderer_.textWidth(scale, gap);
        const float elapsedSeconds = std::chrono::duration<float>(now - marqueeStart).count();
        const float offset = std::fmod(elapsedSeconds * speedPixelsPerSecond, cycleWidth);
        const std::string track = displayValue + gap + displayValue;
        renderer_.beginClipRect(x, y - 4.0f, maxWidth, 64.0f);
        renderer_.text(x - offset, y, scale, track, color);
        renderer_.endClipRect();
    }

    std::string fitTextLines(std::string_view value, float scale, float maxWidth, int maxLines) const {
        if (value.empty() || maxWidth <= 0.0f || maxLines <= 0) return {};
        const std::string displayValue = displayText(value);
        auto ellipsize = [&](std::string line) {
            while (!line.empty() && renderer_.textWidth(scale, line + "...") > maxWidth) line.pop_back();
            return line + "...";
        };
        std::istringstream words(displayValue);
        std::string word;
        std::string current;
        std::string fitted;
        int line = 1;
        while (words >> word) {
            const std::string candidate = current.empty() ? word : current + " " + word;
            if (renderer_.textWidth(scale, candidate) <= maxWidth) {
                current = candidate;
                continue;
            }
            if (current.empty()) {
                if (!fitted.empty()) fitted += '\n';
                fitted += ellipsize(word);
                if (line >= maxLines) return fitted;
                ++line;
                current.clear();
                continue;
            }
            if (line >= maxLines) {
                if (!fitted.empty()) fitted += '\n';
                fitted += ellipsize(current);
                return fitted;
            }
            if (!fitted.empty()) fitted += '\n';
            fitted += current;
            current = word;
            ++line;
        }
        if (!current.empty()) {
            if (!fitted.empty()) fitted += '\n';
            fitted += renderer_.textWidth(scale, current) <= maxWidth ? current : ellipsize(current);
        }
        return fitted;
    }

    void renderMediaGrid(const std::string& title, const std::vector<JellyfinItem>& items, int selection) {
        renderMediaGridScreen(
            title, items,
            MediaGridRenderState{
                .loading = loading_,
                .selection = selection,
                .uiTextSize = settings_.uiTextSize,
            },
            [this](std::string_view heading) { renderHeader(std::string(heading)); },
            [this](std::string_view emptyTitle, std::string_view message) {
                renderEmptyState(std::string(emptyTitle), std::string(message));
            },
            [this](const JellyfinItem& item, const MediaGridCardPlacement& placement) {
                renderMediaArtworkCard(item, placement.x, placement.y, placement.slotWidth, placement.focused,
                                       placement.showState, placement.preferSeriesCover, placement.alignToPortraitBand,
                                       placement.titleLineLimit);
            });
    }

    JellyfinItem personArtworkItem(const JellyfinPerson& person) const {
        JellyfinItem item;
        item.id = person.id;
        item.name = person.name;
        item.type = "Person";
        item.imageTag = person.imageTag;
        return item;
    }

    void renderCast() {
        renderCastScreen(
            renderer_, detail_.name, detail_.people, detailsState_.castSelection(), settings_.uiTextSize,
            CastRenderStyle<Color>{
                .cornerSmall = material_tv::cornerSmall,
                .labelScale = material_tv::type::label,
                .supportingScale = material_tv::type::supporting,
                .panelAlt = kPanelAlt,
                .focus = kFocus,
                .text = kText,
                .muted = kMuted,
                .tertiary = kTertiary,
            },
            [this](std::string_view heading) { renderHeader(std::string(heading)); },
            [this](std::string_view title, std::string_view message) {
                renderEmptyState(std::string(title), std::string(message));
            },
            [this](float x, float y, float width, float height, bool focused) {
                return focusedBounds(x, y, width, height, focused);
            },
            [this](const JellyfinPerson& person, float x, float y, float width, float height, float radius) {
                const JellyfinItem artworkItem = personArtworkItem(person);
                if (!drawArtwork(artworkItem, x, y, width, height, 1.0f, radius)) {
                    drawArtworkPlaceholder(artworkItem, x, y, width, height, radius);
                }
            },
            [this](float x, float y, float width, float height, Color color, float radius) {
                drawFocusHalo(x, y, width, height, color, radius);
            },
            [this](std::string_view value, float scale, float maxWidth, int maxLines) {
                return fitTextLines(value, scale, maxWidth, maxLines);
            },
            [this](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                   float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding,
                                          verticalPadding);
            });
    }

    void renderPersonItems() {
        const auto& person = detailsState_.selectedPerson();
        const std::string heading = person.name.empty() ? "Person" : "Featuring " + person.name;
        renderMediaGrid(heading, detailsState_.personItems(), detailsState_.personItemSelection());
    }

    void renderSeasons() {
        const auto& series = detailsState_.seriesDetail();
        renderMediaGrid(series.name.empty() ? "Seasons" : series.name + " | Seasons", detailsState_.seasons(),
                        detailsState_.seasonSelection());
    }

    void renderEpisodes() {
        const auto& series = detailsState_.seriesDetail();
        const auto& season = detailsState_.selectedSeason();
        const std::string heading = season.name.empty() ? "Episodes" : series.name + " - " + season.name;
        renderMediaGrid(heading, detailsState_.episodes(), detailsState_.episodeSelection());
    }

    void renderDetails() {
        const auto actions = detailActions();
        renderDetailsScreen(
            renderer_, detail_, detailsState_, actions,
            DetailsRenderConfig{
                .showClock = settings_.showClock,
                .clock24Hour = settings_.clock24Hour,
                .showWatchedIndicators = settings_.showWatchedIndicators,
                .uiTextSize = settings_.uiTextSize,
                .stillWatchingPrompt = playbackCoordinator_.continuation().stillWatchingPrompt(),
                .overlayOpen = screen_ == Screen::ItemMenu,
            },
            DetailsRenderStyle<Color>{
                .canvasWidth = Renderer::logicalWidth(),
                .canvasHeight = Renderer::logicalHeight(),
                .cornerLarge = material_tv::cornerLarge,
                .cornerExtraSmall = material_tv::cornerExtraSmall,
                .cardFocusScale = materialCardFocusScale(),
                .labelScale = material_tv::type::label,
                .background = kBackground,
                .text = kText,
                .muted = kMuted,
                .secondaryText = kSecondaryText,
                .focus = kFocus,
                .panelElevated = kPanelElevated,
                .outline = kOutline,
                .track = kTrack,
                .backdropHorizontalStart = Color{0.0f, 0.0f, 0.0f, 0.92f},
                .backdropHorizontalEnd = Color{0.0f, 0.0f, 0.0f, 0.03f},
                .backdropVerticalStart = Color{0.0f, 0.0f, 0.0f, 0.08f},
                .backdropVerticalEnd = Color{0.0f, 0.0f, 0.0f, 0.92f},
            },
            [&](const JellyfinItem& item, float alpha) { return drawBackdrop(item, alpha); },
            [&](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
                drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
            },
            [](bool clock24Hour) { return formatLocalClock(std::time(nullptr), clock24Hour); },
            [&](float x, float y, float width, float height, float scale, std::string_view value, Color color,
                float horizontalPadding, float verticalPadding) {
                drawCenteredSingleLineFit(x, y, width, height, scale, value, color, horizontalPadding, verticalPadding);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height) {
                return drawLogo(item, x, y, width, height);
            },
            [&](std::string_view value, float scale, float maxWidth, int maxLines) {
                return fitTextLines(value, scale, maxWidth, maxLines);
            },
            [](const JellyfinItem& item) { return episodeNumberLabel(item); },
            [](const JellyfinItem& item) { return episodeLabel(item); },
            [](int milliseconds) { return formatPlaybackTime(milliseconds); },
            [&](float x, float y, std::string_view value, bool active, float scale, float height, float maxWidth) {
                return drawChip(x, y, std::string(value), active, scale, height, maxWidth);
            },
            [](std::string_view value) { return materialLabel(value); },
            [&](float x, float y, float width, float height, bool focused, bool primary) {
                return drawButtonSurface(x, y, width, height, focused, primary);
            },
            [&](float x, float y, float width, float height, bool focused, float focusScale) {
                return focusedBounds(x, y, width, height, focused, focusScale);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                return drawHomeArtwork(item, x, y, width, height, radius);
            },
            [&](const JellyfinItem& item, float x, float y, float width, float height, float radius) {
                drawArtworkPlaceholder(item, x, y, width, height, radius);
            },
            [&](float x, float y, float width, float height, Color color, float radius) {
                drawFocusHalo(x, y, width, height, color, radius);
            });
    }

    void renderStatus() {
        const auto now = std::chrono::steady_clock::now();
        if (!noticePersistent_ && !notice_.empty() && now >= noticeUntil_) {
            notice_.clear();
            noticeUntil_ = {};
        }
        const bool noticeVisible = !notice_.empty() && (noticePersistent_ || now < noticeUntil_);

        if (error_.empty()) {
            presentedError_.clear();
            errorUntil_ = {};
        } else if (error_ != presentedError_) {
            presentedError_ = error_;
            errorUntil_ = now + 6s;
        }
        if (!presentedError_.empty() && errorUntil_ != std::chrono::steady_clock::time_point{} && now >= errorUntil_) {
            errorUntil_ = {};
        }
        const bool errorVisible =
            !presentedError_.empty() && errorUntil_ != std::chrono::steady_clock::time_point{} && now < errorUntil_;

        renderStatusOverlay(
            renderer_,
            StatusOverlayRenderState{
                .loading = loading_ || homeLoading_ || mutationLoading_,
                .playerScreen = screen_ == Screen::Player,
                .noticeVisible = noticeVisible,
                .notice = notice_,
                .errorVisible = errorVisible,
                .error = presentedError_,
            },
            StatusOverlayRenderStyle<Color>{
                .cornerMedium = material_tv::cornerMedium,
                .panelElevated = kPanelElevated,
                .focus = kFocus,
                .error = kError,
                .errorOutline = Color{kError.r, kError.g, kError.b, 0.55f},
                .text = kText,
            },
            [this](std::string_view value, float scale, float width, int lines) {
                return fitTextLines(value, scale, width, lines);
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
    SeerrClient seerr_;
    SeerrClient seerrSearch_;
    DisplayModeController displayMode_;
    NativeMediaPlayer player_;
    NativeMediaSession mediaSession_;
    NativeExternalPlayer externalPlayer_;
    JniImageDecoder imageDecoder_;
    VideoSurface videoSurface_;
    TaskRunner tasks_;
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
    PlaybackTelemetryExecutor<JellyfinClient, TaskRunner, AsyncCompletionQueue<AsyncCompletion>> playbackTelemetryAsync_;
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
    ArtworkProvider<JellyfinClient, SeerrClient, JniImageDecoder, TaskRunner, std::recursive_mutex> artwork_;
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
    std::string presentedError_;
    std::chrono::steady_clock::time_point errorUntil_{};
    std::string notice_;
    std::chrono::steady_clock::time_point noticeUntil_{};
    bool noticePersistent_ = false;
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
    DecodedImage brandMarkDecoded_;
    GLuint brandMarkTexture_ = 0;
    uint64_t brandMarkTextureGeneration_ = 0;
    HomeScreenState homeState_;
    std::unordered_set<std::string> hiddenHomeItems_;
    int nextUpReplacementFadeIndex_ = -1;
    std::string nextUpReplacementFadeItemId_;
    std::chrono::steady_clock::time_point nextUpReplacementFadeStarted_{};
    BrowseScreenState browseState_;

    int systemTextInputMode_ = -1;
    std::string systemTextInputOriginal_;

    SearchScreenState searchState_;

    int keyboardRow_ = 0;
    int keyboardCol_ = 0;

    JellyfinItem detail_;
    DetailsScreenState detailsState_;

    PlaybackQueueState queueState_;

    ExternalPlaybackState externalPlaybackState_;
    PlaybackCoordinator playbackCoordinator_;
    PlayerScreenState playerScreenState_;
    TrickplayPreviewState trickplayState_;
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
