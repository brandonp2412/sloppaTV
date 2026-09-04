#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "account_screen.hpp"
#include "app_settings.hpp"
#include "artwork_cache.hpp"
#include "audio_policy.hpp"
#include "browse_screen.hpp"
#include "details_screen.hpp"
#include "deep_link.hpp"
#include "discovery.hpp"
#include "display_mode.hpp"
#include "external_playback_state.hpp"
#include "external_player.hpp"
#include "home_image_disk_cache.hpp"
#include "home_screen.hpp"
#include "image_decoder.hpp"
#include "jellyfin.hpp"
#include "jni_env.hpp"
#include "launch_intent.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "media_session.hpp"
#include "navigation_stack.hpp"
#include "playback_continuation.hpp"
#include "playback_queue.hpp"
#include "playback_session.hpp"
#include "playback_telemetry.hpp"
#include "playback_transition.hpp"
#include "player_screen.hpp"
#include "player_tracks.hpp"
#include "request_epoch.hpp"
#include "screensaver_policy.hpp"
#include "search_screen.hpp"
#include "session_registry.hpp"
#include "session_store.hpp"
#include "settings_screen.hpp"
#include "ui_policy.hpp"
#include "renderer.hpp"
#include "task_runner.hpp"
#include "trickplay_policy.hpp"
#include "trickplay_preview.hpp"
#include "video_surface.hpp"
#include "version_policy.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

#ifndef SLOPPATV_VERSION_NAME
#define SLOPPATV_VERSION_NAME "dev"
#endif

namespace {
constexpr const char* kTag = "sloppaTV";

constexpr Color kBackground{0.000f, 0.027f, 0.082f, 1.0f};
constexpr Color kPanel{0.035f, 0.075f, 0.133f, 0.90f};
constexpr Color kPanelAlt{0.090f, 0.118f, 0.153f, 0.92f};
constexpr Color kPanelElevated{0.082f, 0.192f, 0.298f, 0.96f};
constexpr Color kText{0.985f, 0.988f, 0.998f, 1.0f};
constexpr Color kMuted{0.67f, 0.72f, 0.79f, 1.0f};
constexpr Color kSecondaryText{0.84f, 0.87f, 0.92f, 1.0f};
constexpr Color kTertiary{0.45f, 0.52f, 0.61f, 1.0f};
constexpr Color kFocus{0.153f, 0.439f, 0.694f, 1.0f};
constexpr Color kFocusSoft{0.067f, 0.259f, 0.431f, 0.76f};
constexpr Color kBrandGold{0.757f, 0.596f, 0.431f, 1.0f};
constexpr Color kError{0.95f, 0.28f, 0.30f, 1.0f};

void logPlaybackReportFailure(const char* stage, const std::string& itemId, const ApiResult& result) {
    if (result.ok) return;
    __android_log_print(
        ANDROID_LOG_WARN,
        kTag,
        "Playback %s report failed for %s: %s",
        stage,
        itemId.c_str(),
        result.error.c_str()
    );
}

std::string wrapText(const std::string& input, size_t maxColumns, size_t maxLines) {
    if (input.empty()) return "NO DESCRIPTION";
    std::istringstream words(input);
    std::string word;
    std::string line;
    std::string output;
    size_t lines = 1;
    while (words >> word) {
        if (line.size() + word.size() + (line.empty() ? 0 : 1) > maxColumns) {
            if (!output.empty()) output += '\n';
            output += line;
            line.clear();
            if (++lines > maxLines) {
                output += "...";
                return output;
            }
        }
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty() && lines <= maxLines) {
        if (!output.empty()) output += '\n';
        output += line;
    }
    return output;
}

std::string episodeNumberLabel(const JellyfinItem& item) {
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

std::string formatPlaybackTime(int milliseconds) {
    const int totalSeconds = std::max(0, milliseconds / 1000);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;
    std::ostringstream out;
    if (hours > 0) out << hours << ':' << std::setw(2) << std::setfill('0') << minutes;
    else out << minutes;
    out << ':' << std::setw(2) << std::setfill('0') << seconds;
    return out.str();
}

std::string joinGenres(const std::vector<std::string>& genres, size_t limit = 5) {
    std::string result;
    for (size_t i = 0; i < std::min(limit, genres.size()); ++i) {
        if (!result.empty()) result += " / ";
        result += genres[i];
    }
    return result;
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

const std::vector<std::vector<VirtualKey>>& keyboardRows() {
    static const std::vector<std::vector<VirtualKey>> rows = {
        {{"A","A"},{"B","B"},{"C","C"},{"D","D"},{"E","E"},{"F","F"},{"G","G"},{"H","H"},{"I","I"},{"J","J"}},
        {{"K","K"},{"L","L"},{"M","M"},{"N","N"},{"O","O"},{"P","P"},{"Q","Q"},{"R","R"},{"S","S"},{"T","T"}},
        {{"U","U"},{"V","V"},{"W","W"},{"X","X"},{"Y","Y"},{"Z","Z"},{"0","0"},{"1","1"},{"2","2"},{"3","3"}},
        {{"4","4"},{"5","5"},{"6","6"},{"7","7"},{"8","8"},{"9","9"},{".","."},{"-","-"},{"_","_"},{"/","/"}},
        {{":",":"},{"@","@"},{"SPACE"," "},{"BACK","",KeyAction::Backspace},{"DONE","",KeyAction::Done}},
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
        case AKEYCODE_SPACE: return ' ';
        case AKEYCODE_PERIOD: return shift ? '>' : '.';
        case AKEYCODE_COMMA: return shift ? '<' : ',';
        case AKEYCODE_SLASH: return shift ? '?' : '/';
        case AKEYCODE_BACKSLASH: return shift ? '|' : '\\';
        case AKEYCODE_MINUS: return shift ? '_' : '-';
        case AKEYCODE_EQUALS: return shift ? '+' : '=';
        case AKEYCODE_SEMICOLON: return shift ? ':' : ';';
        case AKEYCODE_APOSTROPHE: return shift ? '"' : '\'';
        case AKEYCODE_AT: return '@';
        default: return 0;
    }
}

struct PendingTickWork {
    std::optional<ExternalPlayerResult> externalResult;
    std::optional<ExternalPlaybackLaunch> completedExternalPlayback;
    std::optional<ExternalPlaybackLaunch> externalLaunch;
    std::optional<PendingPlaybackTransition> playbackTransition;
};

class SloppaApp;
SloppaApp* gActiveApp = nullptr;
std::mutex gActiveAppMutex;

class SloppaApp {
public:
    explicit SloppaApp(android_app* app)
        : app_(app),
          renderer_(app->activity->vm, app->activity->clazz),
          api_(app->activity->vm, app->activity->clazz),
          player_(app->activity->vm, app->activity->clazz),
          mediaSession_(app->activity->vm, app->activity->clazz),
          externalPlayer_(app->activity->vm, app->activity->clazz),
          imageDecoder_(app->activity->vm),
          videoSurface_(app->activity->vm),
          tasks_(
              4,
              [app] {
                  if (app && app->looper) ALooper_wake(app->looper);
              },
              [](const std::string& error) {
                  __android_log_print(ANDROID_LOG_ERROR, kTag, "Background task exception: %s", error.c_str());
              }
          ) {
        __android_log_print(ANDROID_LOG_INFO, kTag, "Startup init: platform bridges ready");
        dataPath_ = app->activity->internalDataPath ? app->activity->internalDataPath : "";
        homeDiskCache_.setDataPath(dataPath_);
        loadBundledBrandMark();
        const LaunchRequest launchRequest = readLaunchRequest(app_);
        pendingDeepLinkItemId_ = launchRequest.itemId;
        pendingSearchQuery_ = launchRequest.searchQuery;
        loadSession();
        __android_log_print(ANDROID_LOG_INFO, kTag, "Startup init: session loaded valid=%d", session_.valid() ? 1 : 0);
        if (session_.valid()) {
            resetNavigation(Screen::Home);
            loadHomeAsync();
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
    }

    ~SloppaApp() {
        api_.cancelPendingRequests();
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
                if (screen_ == Screen::Player || burstActive) timeoutMs = 0;
                else if (screensaverActive_) timeoutMs = 30000;
                else if (loading_ || homeLoading_ || mutationLoading_ || accountState_.quickConnectActive()) timeoutMs = 100;
                else {
                    const int64_t delayMs = screensaverDelayMs(settings_.screensaverMinutes);
                    if (delayMs > 0) {
                        const int64_t idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(pollNow - lastInteraction_).count();
                        timeoutMs = static_cast<int>(std::clamp<int64_t>(delayMs - idleMs, 0, delayMs));
                    }
                }
                if (searchState_.debouncePending()) {
                    const int64_t searchDelayMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        searchState_.debounceDeadline() - pollNow
                    ).count();
                    const int searchTimeoutMs = static_cast<int>(std::max<int64_t>(0, searchDelayMs));
                    timeoutMs = timeoutMs < 0 ? searchTimeoutMs : std::min(timeoutMs, searchTimeoutMs);
                }
            }

            int events = 0;
            android_poll_source* source = nullptr;
            int pollResult = ALooper_pollOnce(timeoutMs, nullptr, &events, reinterpret_cast<void**>(&source));
            bool shouldRender = pollResult == ALOOPER_POLL_WAKE
                || (pollResult == ALOOPER_POLL_TIMEOUT && timeoutMs >= 0);

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
                    const int64_t idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInteraction_).count();
                    screensaverActive_ = shouldActivateScreensaver(
                        settings_.screensaverMinutes,
                        idleMs,
                        false,
                        loading_ || homeLoading_ || mutationLoading_ || accountState_.quickConnectActive()
                    );
                }
                screensaver = screensaverActive_;
            }
            burstActive = std::chrono::steady_clock::now() < renderBurstUntil_;
            if (renderer_.ready() && (playerScreen || screensaver || shouldRender || burstActive)) render();
        }
    }

    void onSystemTextInputChanged(int mode, const std::string& value) {
        std::scoped_lock lock(stateMutex_);
        const std::string text = value.substr(0, 160);
        if (mode == kTextInputSearch) {
            searchState_.setQuery(text);
            searchState_.setKeyboard(false);
            scheduleLiveSearch();
        } else if (mode == kTextInputSettingsSearch) {
            settingsScreen_.setSearchText(text);
        } else if (mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword) {
            accountState_.setField(mode - kTextInputLoginServer, text);
        }
        systemTextInputMode_ = mode;
        renderBurstUntil_ = std::chrono::steady_clock::now() + 300ms;
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void onSystemTextInputCancelled(int mode, const std::string& value) {
        std::scoped_lock lock(stateMutex_);
        const std::string text = value.substr(0, 160);
        systemTextInputMode_ = -1;
        if (mode == kTextInputSearch) {
            searchState_.setQuery(text);
            searchState_.setKeyboard(false);
            scheduleLiveSearch();
        } else if (mode == kTextInputSettingsSearch) {
            settingsScreen_.setSearchText(text);
        } else if (mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword) {
            accountState_.setField(mode - kTextInputLoginServer, text);
        }
        renderBurstUntil_ = std::chrono::steady_clock::now() + 300ms;
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void onSystemTextInputDone(int mode, const std::string& value) {
        std::scoped_lock lock(stateMutex_);
        const std::string text = value.substr(0, 160);
        systemTextInputMode_ = -1;
        if (mode == kTextInputSearch) {
            searchState_.setQuery(text);
            searchState_.setKeyboard(false);
            searchState_.cancelPending();
            searchAsync();
        } else if (mode == kTextInputSettingsSearch) {
            settingsScreen_.setSearchText(text);
        } else if (mode >= kTextInputLoginServer && mode <= kTextInputLoginPassword) {
            const int field = mode - kTextInputLoginServer;
            accountState_.finishTextField(field, text);
        }
        renderBurstUntil_ = std::chrono::steady_clock::now() + 500ms;
        if (app_ && app_->looper) ALooper_wake(app_->looper);
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

private:
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
                if (screen_ == Screen::Player && playerScreenState_.windowRestorePending() && renderer_.ready() && !activeTarget_.url.empty()) {
                    if (settings_.refreshRateSwitching && activePlaybackItem_.videoFrameRate > 0.0f) {
                        displayMode_.matchVideo(app_->window, activePlaybackItem_.videoFrameRate);
                    }
                    const PlayerStatus restoreStatus = player_.status();
                    const bool shouldResumePlayback = playerScreenState_.resumeOnFocusRequested();
                    const bool preservedPlayer = reusedRendererContext
                        && videoSurface_.ready()
                        && restoreStatus != PlayerStatus::Idle
                        && restoreStatus != PlayerStatus::Error;
                    if (preservedPlayer) {
                        if (shouldResumePlayback) player_.play();
                        mediaSession_.updateState(
                            shouldResumePlayback ? MediaSessionState::Playing : MediaSessionState::Paused,
                            playerScreenState_.positionMs()
                        );
                        __android_log_print(ANDROID_LOG_INFO, kTag, "Restored playback with preserved Media3 and GLES context");
                    } else {
                        videoSurface_.release();
                        std::string surfaceError;
                        if (!videoSurface_.create(surfaceError)) {
                            error_ = surfaceError.empty() ? "VIDEO SURFACE COULD NOT BE RESTORED" : surfaceError;
                            break;
                        }
                        player_.startAsync(
                            activeTarget_.url,
                            videoSurface_.surface(),
                            playerScreenState_.positionMs(),
                            settings_.playbackBufferPreset,
                            playerAudioOrdinal(activeTarget_, activePlaybackItem_)
                        );
                        transitionState_.setPauseAfterRestart(!shouldResumePlayback);
                        mediaSession_.updateState(MediaSessionState::Buffering, playerScreenState_.positionMs());
                        __android_log_print(ANDROID_LOG_WARN, kTag, "GLES context was not reusable during window restore; recreated playback surface");
                    }
                    playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
                    playerScreenState_.completeWindowRestore();
                }
                break;
            }
            case APP_CMD_TERM_WINDOW:
                if (screen_ == Screen::Player && !activeTarget_.url.empty()) {
                    const PlayerStatus status = player_.status();
                    refreshPlaybackTelemetry(true);
                    const bool resumePlayback = status == PlayerStatus::Playing || status == PlayerStatus::Preparing;
                    playerScreenState_.beginWindowRestore(resumePlayback);
                    if (resumePlayback) player_.pause();
                    reportProgressAsync(true);
                    displayMode_.restore();
                    mediaSession_.updateState(MediaSessionState::Paused, playerScreenState_.positionMs());
                }
                if (!renderer_.detachWindow()) renderer_.shutdown();
                break;
            case APP_CMD_GAINED_FOCUS:
                lastInteraction_ = std::chrono::steady_clock::now();
                screensaverActive_ = false;
                if (screen_ == Screen::Player && playerScreenState_.takeResumeOnFocus()) {
                    player_.play();
                    mediaSession_.updateState(MediaSessionState::Playing, playerScreenState_.positionMs());
                    __android_log_print(ANDROID_LOG_INFO, kTag, "Resumed playback after focus restoration");
                }
                break;
            case APP_CMD_LOST_FOCUS:
                screensaverActive_ = false;
                if (screen_ == Screen::Player) {
                    const PlayerStatus status = player_.status();
                    if (status == PlayerStatus::Playing || status == PlayerStatus::Preparing) {
                        playerScreenState_.requestResumeOnFocus();
                        player_.pause();
                    }
                }
                break;
            default:
                break;
        }
    }

    int32_t onInput(AInputEvent* event) {
        if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY) return 0;
        const int32_t action = AKeyEvent_getAction(event);
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) return 0;

        const int32_t key = AKeyEvent_getKeyCode(event);
        const int32_t meta = AKeyEvent_getMetaState(event);
        const auto inputNow = std::chrono::steady_clock::now();
        renderBurstUntil_ = inputNow + 150ms;
        std::scoped_lock lock(stateMutex_);
        if (systemTextInputMode_ >= 0) return 0;

        if (action == AKEY_EVENT_ACTION_UP) {
            if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && homeState_.centerPending()) {
                const bool activate = homeState_.consumeCenterRelease(screen_ == Screen::Home);
                if (activate) handleHomeKey(key);
                return 1;
            }
            return 0;
        }
        if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)
            && homeState_.centerPending() && homeState_.centerLongPressed()) {
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
            handlePlayerKey(key);
            return 1;
        }

        if (screen_ == Screen::Home
            && homeState_.row() >= 0
            && (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER)) {
            const int repeatCount = AKeyEvent_getRepeatCount(event);
            if (repeatCount == 0) {
                homeState_.beginCenterPress();
                return 1;
            }
            if (homeState_.centerPending() && !homeState_.centerLongPressed()
                && homeState_.row() < static_cast<int>(home_.rows.size())) {
                auto& section = home_.rows[static_cast<size_t>(homeState_.row())];
                if (section.title != "My Media" && !section.items.empty()) {
                    const int selection = homeState_.selection(homeState_.row(), static_cast<int>(section.items.size()));
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
            case Screen::Login: handleLoginKey(key); break;
            case Screen::Profiles: handleProfilesKey(key); break;
            case Screen::Home: handleHomeKey(key); break;
            case Screen::Browse: handleBrowseKey(key); break;
            case Screen::Search: handleSearchKey(key); break;
            case Screen::Settings: handleSettingsKey(key); break;
            case Screen::Diagnostics: handleDiagnosticsKey(key); break;
            case Screen::Details: handleDetailsKey(key); break;
            case Screen::Cast: handleCastKey(key); break;
            case Screen::PersonItems: handlePersonItemsKey(key); break;
            case Screen::ItemMenu: handleItemMenuKey(key); break;
            case Screen::Seasons: handleSeasonsKey(key); break;
            case Screen::Episodes: handleEpisodesKey(key); break;
            case Screen::Player: break;
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
        requestEpochs_.search.invalidate();
        if (!searchState_.scheduleDebounce(std::chrono::steady_clock::now())) {
            error_.clear();
            return;
        }
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    void runDueLiveSearch() {
        std::scoped_lock lock(stateMutex_);
        if (!searchState_.debouncePending()) return;
        if (screen_ != Screen::Search) {
            searchState_.cancelPending();
            requestEpochs_.search.invalidate();
            return;
        }
        if (!searchState_.debounceDue(std::chrono::steady_clock::now())) return;
        searchAsync();
    }

    bool showSystemTextInput(const std::string& initial, const std::string& hint, int mode, bool password = false) {
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return false;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return false;
        jobject activity = app_->activity->clazz;
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID method = activityClass
            ? env->GetMethodID(
                activityClass,
                "showTextInput",
                "(Ljava/lang/String;Ljava/lang/String;IZ)Z"
            )
            : nullptr;
        jstring jInitial = env->NewStringUTF(initial.c_str());
        jstring jHint = env->NewStringUTF(hint.c_str());
        jboolean shown = JNI_FALSE;
        if (method && jInitial && jHint) {
            shown = env->CallBooleanMethod(activity, method, jInitial, jHint, static_cast<jint>(mode), password ? JNI_TRUE : JNI_FALSE);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            shown = JNI_FALSE;
        }
        if (jHint) env->DeleteLocalRef(jHint);
        if (jInitial) env->DeleteLocalRef(jInitial);
        if (activityClass) env->DeleteLocalRef(activityClass);
        if (shown == JNI_TRUE) systemTextInputMode_ = mode;
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
            keyboardCol_ = std::clamp(
                keyboardCol_,
                0,
                static_cast<int>(rows[static_cast<size_t>(keyboardRow_)].size()) - 1
            );
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
            case KeyAction::Backspace: accountState_.backspaceFocusedField(); break;
            case KeyAction::Done: accountState_.setKeyboardActive(false); break;
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
            if (accountState_.keyboardActive()) accountState_.setKeyboardActive(false);
            else ANativeActivity_finish(app_->activity);
            return;
        }
        if (accountState_.keyboardActive()) {
            if (key == AKEYCODE_DPAD_LEFT) moveKeyboard(-1, 0);
            else if (key == AKEYCODE_DPAD_RIGHT) moveKeyboard(1, 0);
            else if (key == AKEYCODE_DPAD_UP) moveKeyboard(0, -1);
            else if (key == AKEYCODE_DPAD_DOWN) moveKeyboard(0, 1);
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) activateKeyboardKey(false);
            return;
        }

        if (key == AKEYCODE_DPAD_UP) {
            accountState_.moveLoginVertical(-1);
        } else if (key == AKEYCODE_DPAD_DOWN) {
            accountState_.moveLoginVertical(1);
        } else if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT) {
            accountState_.moveLoginAction(key == AKEYCODE_DPAD_LEFT ? -1 : 1, !sessionRegistry_.empty());
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            const int focus = accountState_.loginFocus();
            if (focus < AccountScreenState::kLoginAction) {
                const int mode = kTextInputLoginServer + focus;
                static constexpr std::array<const char*, 3> hints{
                    "Jellyfin server URL", "Jellyfin username", "Jellyfin password"
                };
                accountState_.setKeyboardActive(!showSystemTextInput(
                    accountState_.field(focus),
                    hints[static_cast<size_t>(focus)],
                    mode,
                    focus == AccountScreenState::kPasswordField
                ));
                if (accountState_.keyboardActive()) keyboardRow_ = keyboardCol_ = 0;
            } else if (focus == AccountScreenState::kLoginAction) {
                loginAsync();
            } else if (focus == AccountScreenState::kQuickConnectAction) {
                quickConnectAsync();
            } else if (focus == AccountScreenState::kDiscoverAction) {
                discoverServersAsync();
            } else {
                openProfiles();
            }
        }
    }

    void handleProfilesKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Login);
            return;
        }
        const int addIndex = static_cast<int>(sessionRegistry_.size());
        if (key == AKEYCODE_DPAD_UP) {
            accountState_.moveProfile(-1, addIndex);
        } else if (key == AKEYCODE_DPAD_DOWN) {
            accountState_.moveProfile(1, addIndex);
        } else if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT) {
            accountState_.toggleProfileAction(addIndex);
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (accountState_.profileSelection() == addIndex) {
                startAddAccount();
            } else if (accountState_.profileSelection() >= 0 && accountState_.profileSelection() < addIndex) {
                if (accountState_.profileAction() == 0) switchSavedSession(static_cast<size_t>(accountState_.profileSelection()));
                else forgetSavedSession(static_cast<size_t>(accountState_.profileSelection()));
            }
        }
    }

    bool isItemContextKey(int32_t key) const {
        return key == AKEYCODE_MENU || key == AKEYCODE_INFO;
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

        const JellyfinSession session = session_;
        const std::string itemId = item.id;
        tasks_.submit([this, session, itemId] {
            auto result = api_.getItem(session, itemId);
            if (!result.ok) return;
            std::scoped_lock lock(stateMutex_);
            if (screen_ == Screen::ItemMenu && detail_.id == itemId) detail_ = std::move(result.value);
        });
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
            if (key == AKEYCODE_DPAD_LEFT) homeState_.moveToolbar(-1);
            else if (key == AKEYCODE_DPAD_RIGHT) homeState_.moveToolbar(1);
            else if (key == AKEYCODE_DPAD_DOWN && !home_.rows.empty()) homeState_.setRow(0);
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                if (homeState_.navIndex() == 0) openProfiles();
                else if (homeState_.navIndex() == 2) openSearch();
                else if (homeState_.navIndex() == 3) openSettings();
            }
            homeState_.updateViewport(static_cast<int>(home_.rows.size()));
            return;
        }
        if (home_.rows.empty() || homeState_.row() >= static_cast<int>(home_.rows.size())) {
            homeState_.focusToolbar(homeState_.navIndex());
            return;
        }

        const int rowIndex = homeState_.row();
        auto& section = home_.rows[static_cast<size_t>(rowIndex)];
        auto& items = section.items;
        if (key == AKEYCODE_DPAD_LEFT && !items.empty()) {
            homeState_.moveSelection(rowIndex, -1, static_cast<int>(items.size()));
        } else if (key == AKEYCODE_DPAD_RIGHT && !items.empty()) {
            homeState_.moveSelection(rowIndex, 1, static_cast<int>(items.size()));
        } else if (key == AKEYCODE_DPAD_UP) {
            homeState_.moveRow(-1, static_cast<int>(home_.rows.size()));
        } else if (key == AKEYCODE_DPAD_DOWN) {
            homeState_.moveRow(1, static_cast<int>(home_.rows.size()));
        } else if (isItemContextKey(key) && !items.empty() && section.title != "My Media") {
            const int selection = homeState_.selection(rowIndex, static_cast<int>(items.size()));
            openItemMenuForItem(items[static_cast<size_t>(selection)]);
            return;
        } else if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && !items.empty()) {
            const int selection = homeState_.selection(rowIndex, static_cast<int>(items.size()));
            if (section.title == "My Media") openLibrary(items[static_cast<size_t>(selection)]);
            else openDetails(items[static_cast<size_t>(selection)]);
            return;
        }
        homeState_.updateViewport(static_cast<int>(home_.rows.size()));
        if (homeState_.row() >= 0 && homeState_.row() < static_cast<int>(homeState_.selectionCount())) {
            const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
            prefetchHomeWindow(homeState_.row(), homeState_.selection(homeState_.row(), static_cast<int>(row.items.size())));
        }
    }

    bool isBrowsableContainer(const JellyfinItem& item) const {
        return item.type == "Folder" || item.type == "BoxSet" || item.type == "CollectionFolder";
    }

    void handleBrowseKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            const BrowseBackAction action = browseState_.back();
            if (action == BrowseBackAction::Reload) {
                loadBrowsePageAsync(false);
            } else if (action == BrowseBackAction::LocalPage) {
                loading_ = false;
                error_.clear();
            } else if (action == BrowseBackAction::Exit) {
                popScreen(Screen::Home);
                if (screen_ == Screen::Home) homeState_.focusToolbar(1);
            }
            return;
        }

        if (browseState_.filterFocused() && browseState_.hasFilterBar()) {
            if (key == AKEYCODE_DPAD_LEFT) browseState_.moveFilter(-1);
            else if (key == AKEYCODE_DPAD_RIGHT) browseState_.moveFilter(1);
            else if (key == AKEYCODE_DPAD_DOWN) browseState_.setFilterFocused(false);
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                browseState_.setFilterFocused(false);
                applyBrowseFilter(browseState_.filterSelection());
            }
            return;
        }

        if (key == AKEYCODE_DPAD_UP && browseState_.hasFilterBar() && isTopMediaGridSelection(browseState_.selection())) {
            browseState_.setFilterFocused(true);
            return;
        }
        if (browseState_.items().empty()) return;
        if (isItemContextKey(key)) {
            const auto& selected = browseState_.items()[static_cast<size_t>(browseState_.selection())];
            if (supportsItemContextMenu(selected)) openItemMenuForItem(selected);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
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
        int selection = browseState_.selection();
        auto& items = browseState_.items();
        moveGridSelection(key, items, selection);
        browseState_.setSelection(selection);
        if (browseState_.hasMore() && !loading_
            && browseState_.selection() >= static_cast<int>(items.size()) - 12) {
            loadMoreBrowseAsync();
        }
    }

    void handleSearchKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            if (searchState_.keyboard()) {
                searchState_.setKeyboard(false);
            } else {
                searchState_.cancelPending();
                requestEpochs_.search.invalidate();
                hideSystemTextInput();
                popScreen(Screen::Home);
                if (screen_ == Screen::Home) {
                    homeState_.setRow(0);
                    homeState_.updateViewport(static_cast<int>(home_.rows.size()));
                }
            }
            return;
        }
        if (searchState_.keyboard()) {
            if (key == AKEYCODE_ENTER) {
                searchState_.setKeyboard(false);
                searchAsync();
            } else if (key == AKEYCODE_DPAD_LEFT) moveKeyboard(-1, 0);
            else if (key == AKEYCODE_DPAD_RIGHT) moveKeyboard(1, 0);
            else if (key == AKEYCODE_DPAD_UP) moveKeyboard(0, -1);
            else if (key == AKEYCODE_DPAD_DOWN) moveKeyboard(0, 1);
            else if (key == AKEYCODE_DPAD_CENTER) activateKeyboardKey(true);
            return;
        }

        const auto& results = searchState_.results();
        if (key == AKEYCODE_SEARCH
            || (key == AKEYCODE_DPAD_UP && (results.empty() || isTopMediaGridSelection(searchState_.selection())))
            || ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && results.empty())) {
            searchState_.setKeyboard(!showSystemTextInput(searchState_.query(), "Search Jellyfin", kTextInputSearch));
            if (searchState_.keyboard()) keyboardRow_ = keyboardCol_ = 0;
            return;
        }
        if (results.empty()) return;
        if (isItemContextKey(key)) {
            openItemMenuForItem(results[static_cast<size_t>(searchState_.selection())]);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            openDetails(results[static_cast<size_t>(searchState_.selection())]);
            return;
        }
        constexpr int columns = mediaGridColumns();
        if (key == AKEYCODE_DPAD_LEFT) searchState_.moveSelection(-1, 0, columns);
        else if (key == AKEYCODE_DPAD_RIGHT) searchState_.moveSelection(1, 0, columns);
        else if (key == AKEYCODE_DPAD_UP) searchState_.moveSelection(0, -1, columns);
        else if (key == AKEYCODE_DPAD_DOWN) searchState_.moveSelection(0, 1, columns);
    }

    std::vector<std::string> detailActions() const {
        return detailsState_.actions(detail_, continuationState_.stillWatchingPrompt());
    }

    void refreshExternalPlayers() {
        externalPlayers_ = externalPlayer_.availablePlayers();
        if (settings_.externalPlayerComponent.empty()) return;
        const auto selected = std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
            return player.componentName == settings_.externalPlayerComponent;
        });
        if (selected == externalPlayers_.end()) settings_.externalPlayerComponent.clear();
    }

    std::string externalPlayerLabel() const {
        if (settings_.externalPlayerComponent.empty()) return "INTERNAL";
        const auto selected = std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
            return player.componentName == settings_.externalPlayerComponent;
        });
        return selected == externalPlayers_.end() ? "INTERNAL" : selected->label;
    }

    std::optional<ExternalPlayerApp> selectedExternalPlayer() const {
        if (settings_.externalPlayerComponent.empty()) return std::nullopt;
        const auto selected = std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
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
            const auto selected = std::find_if(externalPlayers_.begin(), externalPlayers_.end(), [&](const ExternalPlayerApp& player) {
                return player.componentName == settings_.externalPlayerComponent;
            });
            if (selected != externalPlayers_.end()) index = static_cast<int>(std::distance(externalPlayers_.begin(), selected)) + 1;
        }
        index = std::clamp(index + direction, 0, static_cast<int>(externalPlayers_.size()));
        settings_.externalPlayerComponent = index == 0
            ? std::string{}
            : externalPlayers_[static_cast<size_t>(index - 1)].componentName;
    }

    void handleSettingsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            hideSystemTextInput();
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) homeState_.focusToolbar(3);
            return;
        }
        if (key == AKEYCODE_SEARCH) {
            settingsScreen_.focusSearch();
            showSystemTextInput(settingsScreen_.searchQuery(), "Search settings", kTextInputSettingsSearch);
            return;
        }
        if (settingsScreen_.searchFocused()) {
            if (key == AKEYCODE_DPAD_DOWN) {
                settingsScreen_.moveDown();
            } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                showSystemTextInput(settingsScreen_.searchQuery(), "Search settings", kTextInputSettingsSearch);
            }
            return;
        }
        if (key == AKEYCODE_DPAD_UP) {
            settingsScreen_.moveUp();
            return;
        }
        if (key == AKEYCODE_DPAD_DOWN) {
            settingsScreen_.moveDown();
            return;
        }
        if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT) {
            const int selection = settingsScreen_.selection();
            if (selection == kAdvancedSettingsToggle) return;
            const int direction = key == AKEYCODE_DPAD_RIGHT ? 1 : -1;
            const SettingChangeEffect effects = adjustSetting(settings_, selection, direction);
            if (effects == SettingChangeEffect::None) return;
            if (selection == 4) playbackSessionState_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
            if (hasSettingEffect(effects, SettingChangeEffect::RestoreDisplayMode)) displayMode_.restore();
            if (hasSettingEffect(effects, SettingChangeEffect::ResetScreensaver)) {
                lastInteraction_ = std::chrono::steady_clock::now();
                screensaverActive_ = false;
            }
            if (hasSettingEffect(effects, SettingChangeEffect::CycleExternalPlayer)) cycleExternalPlayer(direction);
            if (hasSettingEffect(effects, SettingChangeEffect::Save)) saveSession(session_);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            const int selection = settingsScreen_.selection();
            if (selection == 22) {
                openDiagnostics();
            } else if (selection == 23) {
                openProfiles();
            } else if (selection == kAdvancedSettingsToggle) {
                settingsScreen_.toggleAdvanced();
            }
        }
    }

    void handleDiagnosticsKey(int32_t key) {
        if (key == AKEYCODE_BACK || key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            popScreen(Screen::Settings);
        }
    }

    void handleDetailsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            continuationState_.setStillWatchingPrompt(false);
            continuationState_.resetAutoplayChain();
            popScreen(Screen::Home);
            return;
        }
        const auto actions = detailActions();
        if (detailsState_.similarFocused()) {
            if (key == AKEYCODE_DPAD_UP) {
                detailsState_.setSimilarFocused(false);
            } else if (key == AKEYCODE_DPAD_LEFT) {
                detailsState_.moveSimilar(-1);
            } else if (key == AKEYCODE_DPAD_RIGHT) {
                detailsState_.moveSimilar(1);
            } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                if (const auto* selected = detailsState_.selectedSimilar()) openDetails(*selected);
            }
            return;
        }
        if (key == AKEYCODE_DPAD_LEFT) {
            detailsState_.moveAction(-1, static_cast<int>(actions.size()));
        } else if (key == AKEYCODE_DPAD_RIGHT) {
            detailsState_.moveAction(1, static_cast<int>(actions.size()));
        } else if (key == AKEYCODE_DPAD_DOWN && !detailsState_.similar().empty()) {
            detailsState_.setSimilarFocused(true);
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            const std::string& action = actions[static_cast<size_t>(detailsState_.actionSelection())];
            if (action == "PLAY" || action == "RESUME" || action == "PLAY NEXT" || action == "KEEP WATCHING") beginPlayback();
            else if (action == "EPISODES") openSeasons();
            else if (action == "FAVORITE" || action == "UNFAVORITE") toggleFavoriteAsync();
            else if (action == "MARK WATCHED" || action == "MARK UNWATCHED") togglePlayedAsync();
            else if (action == "CAST") openCast();
            else if (action == "MORE") openItemMenu();
            else if (action == "BACK") {
                continuationState_.setStillWatchingPrompt(false);
                continuationState_.resetAutoplayChain();
                popScreen(Screen::Home);
            }
        }
    }

    void openCast() {
        if (detail_.people.empty()) return;
        pushScreen(Screen::Cast);
        detailsState_.resetCastSelection();
        error_.clear();
    }

    void handleCastKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Details);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (const auto* person = detailsState_.selectedCastPerson(detail_.people)) openPersonItems(*person);
            return;
        }
        constexpr int columns = mediaGridColumns();
        if (key == AKEYCODE_DPAD_LEFT) detailsState_.moveCastSelection(detail_.people, -1, 0, columns);
        else if (key == AKEYCODE_DPAD_RIGHT) detailsState_.moveCastSelection(detail_.people, 1, 0, columns);
        else if (key == AKEYCODE_DPAD_UP) detailsState_.moveCastSelection(detail_.people, 0, -1, columns);
        else if (key == AKEYCODE_DPAD_DOWN) detailsState_.moveCastSelection(detail_.people, 0, 1, columns);
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
        tasks_.submit([this, session, personId, generation] {
            auto result = api_.getItemsForPerson(session, personId, 60);
            if (!requestEpochs_.content.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::PersonItems || detailsState_.selectedPerson().id != personId) return;
            if (!result.ok) {
                error_ = "PERSON: " + result.error;
                return;
            }
            detailsState_.setPersonItems(std::move(result.value));
        });
    }

    void handlePersonItemsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Cast);
            return;
        }
        if (isItemContextKey(key)) {
            if (const auto* item = detailsState_.selectedPersonItem()) openItemMenuForItem(*item);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (const auto* item = detailsState_.selectedPersonItem()) openDetails(*item);
            return;
        }
        constexpr int columns = mediaGridColumns();
        if (key == AKEYCODE_DPAD_LEFT) detailsState_.movePersonItem(-1, 0, columns);
        else if (key == AKEYCODE_DPAD_RIGHT) detailsState_.movePersonItem(1, 0, columns);
        else if (key == AKEYCODE_DPAD_UP) detailsState_.movePersonItem(0, -1, columns);
        else if (key == AKEYCODE_DPAD_DOWN) detailsState_.movePersonItem(0, 1, columns);
    }

    std::vector<std::string> itemMenuActions() const {
        return detailsState_.itemMenuActions(
            detail_,
            selectedExternalPlayer().has_value(),
            !queueState_.empty(),
            isHiddenFromHome(detail_)
        );
    }

    void openItemMenu() {
        if (detail_.id.empty()) return;
        pushScreen(Screen::ItemMenu);
        detailsState_.beginItemMenu();
        error_.clear();
    }

    void handleItemMenuKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            detailsState_.setDeleteConfirmation(false);
            popScreen(Screen::Details);
            return;
        }
        if (detailsState_.deleteConfirmation()) {
            if (key == AKEYCODE_DPAD_UP || key == AKEYCODE_DPAD_LEFT) detailsState_.setDeleteConfirmationSelection(0);
            else if (key == AKEYCODE_DPAD_DOWN || key == AKEYCODE_DPAD_RIGHT) detailsState_.setDeleteConfirmationSelection(1);
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                if (detailsState_.deleteConfirmationSelection() == 0) deleteCurrentItemAsync();
                else detailsState_.setDeleteConfirmation(false);
            }
            return;
        }

        const auto actions = itemMenuActions();
        if (key == AKEYCODE_DPAD_UP) detailsState_.moveItemMenu(-1, static_cast<int>(actions.size()));
        else if (key == AKEYCODE_DPAD_DOWN) detailsState_.moveItemMenu(1, static_cast<int>(actions.size()));
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
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
                toggleFavoriteAsync();
            } else if (action == "MARK WATCHED" || action == "MARK UNWATCHED") {
                togglePlayedAsync();
            } else if (action == "HIDE FROM HOME" || action == "SHOW ON HOME") {
                toggleHiddenFromHome();
            } else if (action == "REFRESH METADATA") refreshCurrentItemMetadataAsync();
            else if (action == "DELETE MEDIA") {
                detailsState_.setDeleteConfirmation(true);
            } else {
                popScreen(Screen::Details);
            }
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
        const JellyfinItem selected = detail_;
        const uint64_t generation = requestEpochs_.playback.begin();
        if (!tasks_.submit([this, session, selected, player = *player, generation]() mutable {
            JellyfinItem playable = selected;
            if (playable.type == "Series") {
                auto next = api_.getNextUpForSeries(session, playable.id);
                if (!next.ok) {
                    if (!requestEpochs_.playback.active(generation)) return;
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    if (screen_ != Screen::Details || detail_.id != selected.id) return;
                    error_ = "EXTERNAL PLAYER: " + next.error;
                    return;
                }
                playable = std::move(next.value);
            }
            auto detailed = api_.getItem(session, playable.id);
            if (detailed.ok) playable = std::move(detailed.value);

            const std::string videoUrl = api_.staticVideoUrl(session, playable);
            std::string subtitleUrl;
            auto subtitle = std::find_if(playable.subtitles.begin(), playable.subtitles.end(), [](const JellyfinSubtitleStream& candidate) {
                return candidate.isExternal && candidate.isDefault;
            });
            if (subtitle == playable.subtitles.end()) {
                subtitle = std::find_if(playable.subtitles.begin(), playable.subtitles.end(), [](const JellyfinSubtitleStream& candidate) {
                    return candidate.isExternal;
                });
            }
            if (subtitle != playable.subtitles.end()) {
                subtitleUrl = api_.subtitleSrtUrl(session, playable, subtitle->index);
            }

            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::Details || detail_.id != selected.id) return;
            if (videoUrl.empty()) {
                error_ = "EXTERNAL PLAYER: NO STATIC STREAM";
                return;
            }
            restoreHomeVisibilityForPlayback(selected);
            restoreHomeVisibilityForPlayback(playable);
            externalPlaybackState_.stage(ExternalPlaybackLaunch{
                .item = std::move(playable),
                .player = std::move(player),
                .url = videoUrl,
                .subtitleUrl = std::move(subtitleUrl),
            });
        })) {
            loading_ = false;
            error_ = "EXTERNAL PLAYER COULD NOT BE STARTED";
        }
    }

    void moveGridSelectionByCount(int32_t key, int itemCount, int& selection) {
        if (itemCount <= 0) return;
        constexpr int columns = mediaGridColumns();
        const int rows = (itemCount + columns - 1) / columns;
        int row = selection / columns;
        int col = selection % columns;
        if (key == AKEYCODE_DPAD_LEFT) col = std::max(0, col - 1);
        else if (key == AKEYCODE_DPAD_RIGHT) col = std::min(columns - 1, col + 1);
        else if (key == AKEYCODE_DPAD_UP) row = std::max(0, row - 1);
        else if (key == AKEYCODE_DPAD_DOWN) row = std::min(rows - 1, row + 1);
        const int next = row * columns + col;
        if (next >= 0 && next < itemCount) selection = next;
    }

    void moveGridSelection(int32_t key, const std::vector<JellyfinItem>& items, int& selection) {
        moveGridSelectionByCount(key, static_cast<int>(items.size()), selection);
    }

    void handleSeasonsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            detail_ = detailsState_.seriesDetail();
            popScreen(Screen::Details);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (const auto* season = detailsState_.selectedSeasonItem()) openEpisodes(*season);
            return;
        }
        constexpr int columns = mediaGridColumns();
        if (key == AKEYCODE_DPAD_LEFT) detailsState_.moveSeason(-1, 0, columns);
        else if (key == AKEYCODE_DPAD_RIGHT) detailsState_.moveSeason(1, 0, columns);
        else if (key == AKEYCODE_DPAD_UP) detailsState_.moveSeason(0, -1, columns);
        else if (key == AKEYCODE_DPAD_DOWN) detailsState_.moveSeason(0, 1, columns);
    }

    void handleEpisodesKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Seasons);
            return;
        }
        if (isItemContextKey(key)) {
            if (const auto* episode = detailsState_.selectedEpisodeItem()) openItemMenuForItem(*episode);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (const auto* episode = detailsState_.selectedEpisodeItem()) openDetails(*episode);
            return;
        }
        constexpr int columns = mediaGridColumns();
        if (key == AKEYCODE_DPAD_LEFT) detailsState_.moveEpisode(-1, 0, columns);
        else if (key == AKEYCODE_DPAD_RIGHT) detailsState_.moveEpisode(1, 0, columns);
        else if (key == AKEYCODE_DPAD_UP) detailsState_.moveEpisode(0, -1, columns);
        else if (key == AKEYCODE_DPAD_DOWN) detailsState_.moveEpisode(0, 1, columns);
    }

    void refreshPlaybackTelemetry(bool force = false) {
        const auto now = std::chrono::steady_clock::now();
        if (!telemetryState_.shouldReadPlayback(now, force)) return;
        playerScreenState_.applyObservedPosition(player_.positionMs(), now);
        if (activePlaybackItem_.runtimeTicks > 0) {
            playerScreenState_.setDurationMs(playbackPositionMsFromTicks(activePlaybackItem_.runtimeTicks));
        } else if (force || playerScreenState_.durationMs() <= 0) {
            if (telemetryState_.shouldProbeDuration(now, force)) {
                const int duration = player_.durationMs();
                if (duration > 0) playerScreenState_.setDurationMs(duration);
                telemetryState_.markDurationProbe(now);
            }
        }
        telemetryState_.markPlaybackRead(now);
    }

    std::string playerTrackLabel(int type) const {
        if (type == 2 && !activePlaybackItem_.audios.empty()) {
            const auto selected = std::find_if(
                activePlaybackItem_.audios.begin(),
                activePlaybackItem_.audios.end(),
                [&](const JellyfinAudioStream& audio) { return audio.index == trackState_.selectedAudioServerIndex(); }
            );
            const auto& audio = selected == activePlaybackItem_.audios.end()
                ? activePlaybackItem_.audios.front()
                : *selected;
            std::string label = audio.language.empty() ? "AUDIO" : audio.language;
            std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            if (activePlaybackItem_.audios.size() > 1) {
                label += " " + std::to_string(std::distance(activePlaybackItem_.audios.begin(),
                    selected == activePlaybackItem_.audios.end() ? activePlaybackItem_.audios.begin() : selected) + 1)
                    + "/" + std::to_string(activePlaybackItem_.audios.size());
            }
            return label;
        }
        if (type == 4 && trackState_.subtitleBusy()) return "LOADING";
        if (type == 4 && trackState_.selectedSubtitleServerIndex() >= 0) {
            const auto selected = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == trackState_.selectedSubtitleServerIndex(); }
            );
            if (selected != activePlaybackItem_.subtitles.end()) {
                std::string label = selected->language.empty() ? "ON" : selected->language;
                std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return label;
            }
        }
        if (type == 4 && !trackState_.subtitleCues().empty()) {
            if (!trackState_.subtitleEnabled()) return "OFF";
            std::string label = trackState_.subtitleLanguage().empty() ? "ON" : trackState_.subtitleLanguage();
            std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            const auto subtitle = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [&](const JellyfinSubtitleStream& candidate) { return candidate.index == trackState_.activeSubtitleServerIndex(); }
            );
            if (subtitle != activePlaybackItem_.subtitles.end() && activePlaybackItem_.subtitles.size() > 1) {
                label += " " + std::to_string(std::distance(activePlaybackItem_.subtitles.begin(), subtitle) + 1)
                    + "/" + std::to_string(activePlaybackItem_.subtitles.size());
            }
            return label;
        }
        return type == 2 ? "DEFAULT" : "OFF";
    }

    int audioIndexForPlaybackItem(
        const JellyfinItem& item,
        const std::optional<std::string>& languagePreference
    ) const {
        std::vector<AudioPreferenceCandidate> candidates;
        candidates.reserve(item.audios.size());
        for (const auto& audio : item.audios) {
            candidates.push_back({audio.index, audio.language});
        }
        return audioIndexForQueuePreference(candidates, languagePreference);
    }

    void rememberPlaybackAudioPreference(int streamIndex) {
        const auto selected = std::find_if(
            activePlaybackItem_.audios.begin(),
            activePlaybackItem_.audios.end(),
            [&](const JellyfinAudioStream& audio) { return audio.index == streamIndex; }
        );
        if (selected != activePlaybackItem_.audios.end() && !selected->language.empty()) {
            trackState_.setAudioLanguagePreference(normalizeAudioLanguage(selected->language));
        } else {
            trackState_.setAudioLanguagePreference(std::nullopt);
        }
    }

    void cycleAudioTrack() {
        const auto& tracks = activePlaybackItem_.audios;
        if (tracks.size() < 2) {
            error_ = "ONLY ONE AUDIO TRACK";
            return;
        }
        auto selected = std::find_if(tracks.begin(), tracks.end(), [&](const JellyfinAudioStream& audio) {
            return audio.index == trackState_.selectedAudioServerIndex();
        });
        const size_t next = selected == tracks.end()
            ? 0
            : (static_cast<size_t>(std::distance(tracks.begin(), selected)) + 1) % tracks.size();
        rememberPlaybackAudioPreference(tracks[next].index);
        if (activeTarget_.playMethod == PlaybackMethod::DirectPlay
            && player_.selectEmbeddedAudioOrdinal(static_cast<int>(next))) {
            trackState_.setSelectedAudioServerIndex(tracks[next].index);
            activeTarget_.audioStreamIndex = tracks[next].index;
            playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
            reportProgressAsync(false);
            return;
        }
        restartPlaybackAt(playerScreenState_.positionMs(), tracks[next].index, trackState_.selectedSubtitleServerIndex());
    }

    int subtitleIndexForPlaybackItem(
        const JellyfinItem& item,
        const std::optional<std::string>& languagePreference
    ) const {
        std::vector<SubtitlePreferenceCandidate> candidates;
        candidates.reserve(item.subtitles.size());
        for (const auto& subtitle : item.subtitles) {
            candidates.push_back({subtitle.index, subtitle.language});
        }
        return subtitleIndexForQueuePreference(candidates, languagePreference);
    }

    void rememberPlaybackSubtitlePreference(int streamIndex) {
        if (streamIndex < 0) {
            trackState_.setSubtitleLanguagePreference(std::string{});
            return;
        }
        const auto selected = std::find_if(
            activePlaybackItem_.subtitles.begin(),
            activePlaybackItem_.subtitles.end(),
            [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == streamIndex; }
        );
        if (selected != activePlaybackItem_.subtitles.end() && !selected->language.empty()) {
            trackState_.setSubtitleLanguagePreference(normalizeSubtitleLanguage(selected->language));
        } else {
            // An unlabelled subtitle can be selected for this item, but there is no stable
            // language key to carry to a different episode. Let Jellyfin choose again next time.
            trackState_.setSubtitleLanguagePreference(std::nullopt);
        }
    }

    int preferredSubtitlePosition() const {
        if (activePlaybackItem_.subtitles.empty()) return -1;
        auto preferred = std::find_if(
            activePlaybackItem_.subtitles.begin(),
            activePlaybackItem_.subtitles.end(),
            [](const JellyfinSubtitleStream& subtitle) {
                return subtitle.isDefault && !isLikelySignsOnlySubtitle(subtitle.title);
            }
        );
        if (preferred == activePlaybackItem_.subtitles.end()) {
            preferred = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [](const JellyfinSubtitleStream& subtitle) {
                    return !subtitle.forced && !isLikelySignsOnlySubtitle(subtitle.title);
                }
            );
        }
        if (preferred == activePlaybackItem_.subtitles.end()) preferred = activePlaybackItem_.subtitles.begin();
        return static_cast<int>(std::distance(activePlaybackItem_.subtitles.begin(), preferred));
    }

    void loadSubtitleAsync(const JellyfinSubtitleStream& subtitle, const std::string& deliveryUrl = {}) {
        if (!session_.valid() || subtitle.index < 0 || !trackState_.beginSubtitleWork()) return;
        const JellyfinSession session = session_;
        const JellyfinItem item = activePlaybackItem_;
        const std::string dataPath = dataPath_;
        const uint64_t generation = requestEpochs_.playback.snapshot();
        if (!tasks_.submit([this, session, item, subtitle, deliveryUrl, dataPath, generation] {
            std::string clean;
            std::filesystem::path cacheFile;
            bool fromCache = false;
            if (!dataPath.empty()) {
                const std::filesystem::path directory = std::filesystem::path(dataPath) / "subtitles";
                cacheFile = directory / (item.id + "-" + std::to_string(subtitle.index) + ".srt");
                std::ifstream cached(cacheFile, std::ios::binary);
                if (cached) {
                    clean.assign(std::istreambuf_iterator<char>(cached), std::istreambuf_iterator<char>());
                    fromCache = !clean.empty();
                }
            }
            const auto fetchSubtitle = [&] {
                return deliveryUrl.empty()
                    ? api_.downloadSubtitleSrt(session, item, subtitle.index)
                    : api_.downloadSubtitleUrl(session, deliveryUrl);
            };
            std::string subtitleFailure;
            if (clean.empty()) {
                auto response = fetchSubtitle();
                if (response.ok && !response.value.empty()) {
                    clean = sanitizeSubtitleText(std::move(response.value));
                    if (!cacheFile.empty() && !clean.empty()) {
                        std::error_code ec;
                        std::filesystem::create_directories(cacheFile.parent_path(), ec);
                        if (!ec) {
                            std::ofstream output(cacheFile, std::ios::binary | std::ios::trunc);
                            if (output) output.write(clean.data(), static_cast<std::streamsize>(clean.size()));
                        }
                    }
                } else {
                    subtitleFailure = response.error.empty() ? "empty subtitle response" : response.error;
                }
            }
            clean = sanitizeSubtitleText(std::move(clean));
            std::vector<SubtitleCue> cues = parseSubRipCues(clean);
            if (cues.empty() && fromCache) {
                std::error_code ec;
                std::filesystem::remove(cacheFile, ec);
                auto response = fetchSubtitle();
                if (response.ok) {
                    clean = sanitizeSubtitleText(std::move(response.value));
                    cues = parseSubRipCues(clean);
                } else {
                    subtitleFailure = response.error.empty() ? "subtitle cache refresh failed" : response.error;
                }
            }
            const bool loaded = !cues.empty();
            if (!loaded) {
                if (subtitleFailure.empty()) subtitleFailure = clean.empty() ? "subtitle body was empty" : "subtitle contained no parseable SRT cues";
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kTag,
                    "Subtitle load failed item=%s stream=%d codec=%s reason=%s",
                    item.id.c_str(),
                    subtitle.index,
                    subtitle.codec.c_str(),
                    subtitleFailure.c_str()
                );
            }
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            if (shouldApplyLoadedSubtitle(
                    activePlaybackItem_.id,
                    item.id,
                    trackState_.selectedSubtitleServerIndex(),
                    subtitle.index,
                    loaded
                )) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kTag,
                    "Subtitle loaded item=%s stream=%d codec=%s cues=%zu",
                    item.id.c_str(),
                    subtitle.index,
                    subtitle.codec.c_str(),
                    cues.size()
                );
                trackState_.applySubtitle(subtitle.index, subtitle.language, std::move(cues));
                playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
            }
            if (activePlaybackItem_.id == item.id && trackState_.selectedSubtitleServerIndex() == subtitle.index) {
                trackState_.endSubtitleWork();
                if (!loaded) {
                    trackState_.failSelectedSubtitle();
                    showNotice("SUBTITLES UNAVAILABLE FOR THIS FILE");
                }
            }
        })) {
            trackState_.failSelectedSubtitle();
            showNotice("SUBTITLES COULD NOT BE STARTED");
        }
    }

    void cycleSubtitleTrack() {
        if (trackState_.subtitleBusy() || activePlaybackItem_.subtitles.empty()) {
            if (activePlaybackItem_.subtitles.empty()) error_ = "NO SUBTITLE TRACKS";
            return;
        }
        int nextIndex = -1;
        if (trackState_.selectedSubtitleServerIndex() < 0) {
            const int preferred = preferredSubtitlePosition();
            if (preferred >= 0) nextIndex = activePlaybackItem_.subtitles[static_cast<size_t>(preferred)].index;
        } else {
            const auto selected = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == trackState_.selectedSubtitleServerIndex(); }
            );
            if (selected != activePlaybackItem_.subtitles.end() && std::next(selected) != activePlaybackItem_.subtitles.end()) {
                nextIndex = std::next(selected)->index;
            }
        }
        rememberPlaybackSubtitlePreference(nextIndex);
        restartPlaybackAt(playerScreenState_.positionMs(), trackState_.selectedAudioServerIndex(), nextIndex);
    }

    void restartPlaybackAt(int positionMs, int audioStreamIndex, int subtitleStreamIndex) {
        if (trackState_.subtitleBusy() || !session_.valid() || activePlaybackItem_.id.empty()) return;
        const int targetPositionMs = std::max(0, positionMs);
        const bool wasPaused = player_.status() == PlayerStatus::Paused;
        const JellyfinSession session = session_;
        JellyfinItem item = activePlaybackItem_;
        item.positionTicks = playbackTicksFromPositionMs(targetPositionMs);
        const PlaybackTarget previousTarget = activeTarget_;
        const bool shouldReportPrevious = telemetryState_.playbackStartReported() && !previousTarget.url.empty();
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const uint64_t generation = requestEpochs_.playback.begin();

        // A Jellyfin server-stream change is a real playback-session handoff. Resolve the
        // replacement only after closing/reporting the old session: asking Jellyfin for a
        // second transcode while the first one is still active has produced stalled HLS
        // sessions (and, on some servers, PlaybackInfo HTTP 500 responses).
        trackState_.beginSubtitleWork();
        transitionState_.setLoading(true);
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        playerScreenState_.setPositionMs(targetPositionMs);
        telemetryState_.clearPlaybackStartReported();
        player_.stop();
        videoSurface_.release();

        tasks_.submit([
            this,
            session,
            item,
            previousTarget,
            shouldReportPrevious,
            maxStreamingBitrate,
            maxAudioChannels,
            playbackOverrides,
            audioStreamIndex,
            subtitleStreamIndex,
            wasPaused,
            generation
        ] {
            if (shouldReportPrevious) {
                logPlaybackReportFailure(
                    "stop",
                    item.id,
                    api_.reportPlaybackStopped(session, item, previousTarget, item.positionTicks)
                );
            }
            auto target = api_.resolvePlayback(
                session,
                item,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                audioStreamIndex,
                subtitleStreamIndex
            );
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            trackState_.endSubtitleWork();
            transitionState_.setLoading(false);
            if (screen_ != Screen::Player || activePlaybackItem_.id != item.id) return;
            if (!target.ok) {
                error_ = target.error;
                return;
            }
            transitionState_.stage(std::move(target.value), item, true, wasPaused, audioStreamIndex);
        });
    }

    void clearTrickplayPreview() {
        if (trickplayState_.texture() != 0 && trickplayState_.textureGeneration() == renderer_.generation()) {
            renderer_.deleteTexture(trickplayState_.texture());
        }
        trickplayState_.reset();
    }

    void requestTrickplayPreview(int positionMs) {
        const auto& info = activePlaybackItem_.trickplay;
        if (!session_.valid() || activePlaybackItem_.id.empty() || !info.valid()) return;
        const TrickplayFrame frame = trickplayFrameForPosition(
            positionMs,
            info.intervalMs,
            info.thumbnailCount,
            info.tileWidth,
            info.tileHeight
        );
        if (!frame.valid()) return;

        trickplayState_.showAt(positionMs, std::chrono::steady_clock::now());
        if (trickplayState_.matchesTile(activePlaybackItem_.id, frame.tileIndex) && !trickplayState_.failed()) return;

        if (trickplayState_.texture() != 0 && trickplayState_.textureGeneration() == renderer_.generation()) {
            renderer_.deleteTexture(trickplayState_.texture());
        }
        trickplayState_.beginTile(activePlaybackItem_.id, frame.tileIndex);
        const JellyfinSession session = session_;
        const JellyfinItem item = activePlaybackItem_;
        const int tileIndex = frame.tileIndex;
        if (!tasks_.submit([this, session, item, tileIndex] {
            auto image = api_.downloadTrickplayTile(session, item, tileIndex);
            DecodedImage decoded;
            std::string decodeError;
            if (image.ok) decoded = imageDecoder_.decode(image.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            if (!trickplayState_.matchesTile(item.id, tileIndex)) return;
            if (!image.ok || !decoded.valid()) {
                trickplayState_.markFailed();
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kTag,
                    "Trickplay tile %d unavailable: %s",
                    tileIndex,
                    image.ok ? decodeError.c_str() : image.error.c_str()
                );
                return;
            }
            trickplayState_.applyDecoded(std::move(decoded));
        })) {
            trickplayState_.markFailed();
        }
    }

    bool drawTrickplayPreview() {
        if (!trickplayState_.visible(std::chrono::steady_clock::now(), activePlaybackItem_.id)
            || !activePlaybackItem_.trickplay.valid()) {
            return false;
        }
        const auto& info = activePlaybackItem_.trickplay;
        const TrickplayFrame frame = trickplayFrameForPosition(
            trickplayState_.positionMs(),
            info.intervalMs,
            info.thumbnailCount,
            info.tileWidth,
            info.tileHeight
        );
        if (!frame.valid() || frame.tileIndex != trickplayState_.tileIndex()) return false;
        if (trickplayState_.texture() == 0 || trickplayState_.textureGeneration() != renderer_.generation()) {
            const auto& decoded = trickplayState_.decoded();
            trickplayState_.setTexture(
                renderer_.createTexture(decoded.width, decoded.height, decoded.rgba.data()),
                renderer_.generation()
            );
        }
        if (trickplayState_.texture() == 0) return false;

        constexpr float previewWidth = 420.0f;
        const float previewHeight = std::clamp(
            previewWidth * static_cast<float>(info.height) / static_cast<float>(info.width),
            180.0f,
            270.0f
        );
        const double progress = playerScreenState_.durationMs() > 0
            ? std::clamp(static_cast<double>(trickplayState_.positionMs()) / playerScreenState_.durationMs(), 0.0, 1.0)
            : 0.5;
        const float centerX = 155.0f + static_cast<float>(1610.0 * progress);
        const float x = std::clamp(centerX - previewWidth * 0.5f, 80.0f, Renderer::logicalWidth() - 80.0f - previewWidth);
        constexpr float y = 555.0f;
        const auto& decoded = trickplayState_.decoded();
        const float sourceWidth = static_cast<float>(decoded.width);
        const float sourceHeight = static_cast<float>(decoded.height);
        const float u0 = std::clamp((frame.cellX * info.width) / sourceWidth, 0.0f, 1.0f);
        const float v0 = std::clamp((frame.cellY * info.height) / sourceHeight, 0.0f, 1.0f);
        const float u1 = std::clamp(((frame.cellX + 1) * info.width) / sourceWidth, 0.0f, 1.0f);
        const float v1 = std::clamp(((frame.cellY + 1) * info.height) / sourceHeight, 0.0f, 1.0f);
        if (u1 <= u0 || v1 <= v0) return false;

        renderer_.rect(x - 7.0f, y - 7.0f, previewWidth + 14.0f, previewHeight + 58.0f, Color{0.0f, 0.0f, 0.0f, 0.90f});
        renderer_.outline(x - 7.0f, y - 7.0f, previewWidth + 14.0f, previewHeight + 58.0f, 4.0f, kFocus);
        renderer_.imageRegion(trickplayState_.texture(), x, y, previewWidth, previewHeight, u0, v0, u1, v1);
        renderer_.text(x + 14.0f, y + previewHeight + 13.0f, 1.65f, formatPlaybackTime(trickplayState_.positionMs()), kText, previewWidth - 28.0f);
        return true;
    }

    void seekPlaybackTo(int positionMs) {
        const int targetMs = std::max(0, positionMs);
        player_.seekTo(targetMs);
        playerScreenState_.beginSeek(targetMs, std::chrono::steady_clock::now());
    }

    const SubtitleCue* activeSubtitleCue() const {
        return trackState_.activeSubtitleCue(playerScreenState_.positionMs());
    }

    std::optional<JellyfinMediaSegment> activeSkippableSegment() const {
        const int64_t positionTicks = static_cast<int64_t>(playerScreenState_.positionMs()) * 10000;
        return playbackSessionState_.activeSkippableSegment(positionTicks);
    }

    std::string mediaSegmentSkipLabel(const JellyfinMediaSegment& segment) const {
        if (segment.type == "Intro") return "SKIP INTRO";
        if (segment.type == "Outro") return "SKIP CREDITS";
        if (segment.type == "Recap") return "SKIP RECAP";
        if (segment.type == "Preview") return "SKIP PREVIEW";
        if (segment.type == "Commercial") return "SKIP COMMERCIAL";
        return "SKIP";
    }

    bool skipActiveMediaSegment() {
        const auto segment = activeSkippableSegment();
        if (!segment) return false;
        const int targetMs = static_cast<int>(segment->endTicks / 10000);
        seekPlaybackTo(targetMs);
        reportProgressAsync(false);
        return true;
    }

    void activatePlayerControl() {
        switch (playerScreenState_.controlSelection()) {
            case 0:
                player_.togglePause();
                reportProgressAsync(true);
                break;
            case 1: cycleAudioTrack(); break;
            case 2: cycleSubtitleTrack(); break;
            default: break;
        }
    }

    void handlePlayerKey(int32_t key) {
        const auto now = std::chrono::steady_clock::now();
        if (key == AKEYCODE_BACK) {
            if (playerScreenState_.shouldDismissOnBack(now)) playerScreenState_.dismissOverlay(now);
            else stopPlayback();
            return;
        }
        playerScreenState_.showOverlayFor(now, playerScreenState_.controlsActive() ? 10s : 5s);
        if (playerScreenState_.controlsActive()) {
            if (key == AKEYCODE_DPAD_DOWN) {
                playerScreenState_.hideControls();
            } else if (key == AKEYCODE_DPAD_LEFT) {
                playerScreenState_.moveControl(-1);
            } else if (key == AKEYCODE_DPAD_RIGHT) {
                playerScreenState_.moveControl(1);
            } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                activatePlayerControl();
            }
            return;
        }
        if (key == AKEYCODE_DPAD_DOWN) {
            openQueueOverlay();
        } else if (key == AKEYCODE_MEDIA_NEXT && queueState_.currentIndex() >= 0) {
            const int next = queueState_.nextIndex(true);
            if (next >= 0) playQueuedIndexAsync(next);
        } else if (key == AKEYCODE_DPAD_UP) {
            playerScreenState_.showControls(now);
        } else if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && skipActiveMediaSegment()) {
            // A visible media-segment action owns OK, matching TV skip-button behavior.
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER || key == AKEYCODE_MEDIA_PLAY_PAUSE) {
            player_.togglePause();
            reportProgressAsync(true);
        } else if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_MEDIA_REWIND) {
            const int targetMs = std::max(0, playerScreenState_.positionMs() - settings_.seekBackSeconds * 1000);
            requestTrickplayPreview(targetMs);
            seekPlaybackTo(targetMs);
            reportProgressAsync(false);
        } else if (key == AKEYCODE_DPAD_RIGHT || key == AKEYCODE_MEDIA_FAST_FORWARD) {
            const int targetMs = playerScreenState_.positionMs() + settings_.seekForwardSeconds * 1000;
            requestTrickplayPreview(targetMs);
            seekPlaybackTo(targetMs);
            reportProgressAsync(false);
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
        tasks_.submit([this, session] {
            auto result = api_.getServerInfo(session);
            std::scoped_lock lock(stateMutex_);
            serverInfoLoading_ = false;
            if (!session_.valid() || session_.server != session.server || session_.userId != session.userId) return;
            if (!result.ok) return;
            serverInfo_ = std::move(result.value);
            const auto compatibility = jellyfinServerCompatibility(serverInfo_.version);
            if (compatibility == ServerCompatibility::TooOld) {
                showNotice(
                    "JELLYFIN " + serverInfo_.version + " IS BELOW THE TESTED 10.10+ BASELINE - SERVER UPGRADE RECOMMENDED",
                    6s,
                    true
                );
            } else if (compatibility == ServerCompatibility::Unknown && !serverInfo_.version.empty()) {
                showNotice("UNRECOGNIZED JELLYFIN VERSION " + serverInfo_.version + " - PLAYBACK COMPATIBILITY MAY VARY", 10s);
            }
        });
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
        tasks_.submit([this, session, generation] {
            auto result = api_.getServerInfo(session);
            if (!requestEpochs_.content.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            if (screen_ != Screen::Diagnostics) return;
            loading_ = false;
            if (!result.ok) {
                error_ = "SERVER INFO: " + result.error;
                return;
            }
            serverInfo_ = std::move(result.value);
            const auto compatibility = jellyfinServerCompatibility(serverInfo_.version);
            if (compatibility == ServerCompatibility::TooOld) {
                error_ = "JELLYFIN " + serverInfo_.version + " IS BELOW THE TESTED 10.10+ BASELINE";
            } else if (compatibility == ServerCompatibility::Unknown && !serverInfo_.version.empty()) {
                error_ = "UNRECOGNIZED JELLYFIN VERSION: " + serverInfo_.version;
            }
        });
    }

    void clearCurrentSessionUi() {
        const bool hadAuthenticatedSession = session_.valid();
        // A saved-user/server change is a hard ownership boundary. Cancel old async
        // work first, then stop/report any active player while the old session token is
        // still available. No queue, track preference, pending playback or browsed item
        // from one Jellyfin user should survive into another account.
        api_.cancelPendingRequests();
        requestEpochs_.invalidateAll();
        if (screen_ == Screen::Player || player_.status() != PlayerStatus::Idle || !activePlaybackItem_.id.empty()) {
            releaseActivePlayback(true);
        }

        queueState_.reset();
        continuationState_.reset();
        trackState_.clearLanguagePreferences();
        transitionState_.reset();
        externalPlaybackState_.reset();
        activeTarget_ = {};
        activePlaybackItem_ = {};
        playbackSessionState_.reset();
        telemetryState_.reset();
        playerScreenState_.resetSession();
        trackState_.resetSession();
        clearTrickplayPreview();
        lastPlaybackSummary_.clear();

        if (hadAuthenticatedSession) {
            pendingDeepLinkItemId_.clear();
            pendingSearchQuery_.clear();
            pendingRuntimeLaunchRequest_.reset();
        }
        session_ = {};
        serverInfo_ = {};
        serverInfoLoading_ = false;
        home_ = {};
        homeState_.reset();
        browseState_.clear();
        searchState_.reset();
        detail_ = {};
        detailsState_.reset();

        clearArtworkCache(artwork_);
        clearArtworkCache(homeArtwork_);
        clearArtworkCache(backdrops_);
        clearArtworkCache(logos_);
        accountState_.clearSessionUi();
        loading_ = false;
        homeLoading_ = false;
        mutationLoading_ = false;
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
        eraseArtworkEntry(profileArtwork_, profileArtworkKey(removed));
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
        tasks_.submit([this, session, container, startIndex, append, generation, mode, genre, letter, nested] {
            ApiValueResult<std::vector<JellyfinItem>> result;
            if (nested || mode == BrowseContentMode::All) {
                result = api_.browseLibrary(session, container.id, startIndex, kBrowsePageSize);
                if (result.ok && result.value.empty() && startIndex == 0 && container.type == "BoxSet") {
                    auto fallback = api_.browseCollectionMembersFallback(session, container);
                    if (fallback.ok) result = std::move(fallback);
                }
            } else if (mode == BrowseContentMode::Favorites) {
                result = api_.browseVideoFilter(session, container, startIndex, kBrowsePageSize, true);
            } else if (mode == BrowseContentMode::Genres) {
                result = api_.listGenres(session, container, 100);
            } else if (mode == BrowseContentMode::GenreItems) {
                result = api_.browseVideoFilter(session, container, startIndex, kBrowsePageSize, false, genre);
            } else if (mode == BrowseContentMode::LetterItems) {
                result = api_.browseVideoFilter(session, container, startIndex, kBrowsePageSize, false, {}, letter);
            } else if (mode == BrowseContentMode::Collections) {
                result = api_.browseCollections(session, startIndex, kBrowsePageSize);
            } else {
                result.ok = true;
            }
            if (!requestEpochs_.content.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::Browse || browseState_.activeContainer().id != container.id) return;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            if (append) browseState_.appendPage(std::move(result.value), startIndex, kBrowsePageSize);
            else browseState_.replacePage(std::move(result.value), kBrowsePageSize);
            error_.clear();
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
        tasks_.submit([this, session, seriesId, generation] {
            auto result = api_.getSeasons(session, seriesId);
            if (!requestEpochs_.content.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::Seasons || detailsState_.seriesDetail().id != seriesId) return;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            detailsState_.setSeasons(std::move(result.value));
        });
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
        tasks_.submit([this, session, seriesId, seasonId, generation] {
            auto result = api_.getEpisodes(session, seriesId, seasonId);
            if (!requestEpochs_.content.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::Episodes
                || detailsState_.seriesDetail().id != seriesId
                || detailsState_.selectedSeason().id != seasonId) {
                return;
            }
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            detailsState_.setEpisodes(std::move(result.value));
        });
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
        if (hiding) hiddenHomeItems_.insert(key);
        else hiddenHomeItems_.erase(key);
        filterHiddenHomeItems(home_);
        clampHomeSelections();
        saveSession(session_);
        showNotice(hiding ? "HIDDEN FROM HOME" : "HOME VISIBILITY RESTORED");
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
        for (auto& row : home_.rows) for (auto& item : row.items) apply(item);
        for (auto& item : browseState_.items()) apply(item);
        for (auto& item : searchState_.results()) apply(item);
        detailsState_.updateCachedUserData(updated);

        for (auto& row : home_.rows) {
            if (row.title == "FAVORITES") {
                const auto existing = std::find_if(row.items.begin(), row.items.end(), [&](const JellyfinItem& item) {
                    return item.id == updated.id;
                });
                if (updated.favorite && existing == row.items.end() && !isHiddenFromHome(updated)) row.items.push_back(updated);
                else if (!updated.favorite && existing != row.items.end()) row.items.erase(existing);
            } else if (row.title == "CONTINUE WATCHING" && updated.played) {
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
        tasks_.submit([this, session, item, desired, sessionEpoch] {
            auto result = api_.setFavorite(session, item, desired);
            std::scoped_lock lock(stateMutex_);
            if (!requestEpochs_.session.active(sessionEpoch)) return;
            if (result.ok) {
                JellyfinItem updated = item;
                updated.favorite = desired;
                updateCachedUserData(updated);
            }
            mutationLoading_ = false;
            if ((screen_ != Screen::Details && screen_ != Screen::ItemMenu) || detail_.id != item.id) return;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            detail_.favorite = desired;
        });
    }

    void togglePlayedAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty()) return;
        const bool desired = !detail_.played;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        tasks_.submit([this, session, item, desired, sessionEpoch] {
            auto result = api_.setPlayed(session, item, desired);
            std::scoped_lock lock(stateMutex_);
            if (!requestEpochs_.session.active(sessionEpoch)) return;
            if (result.ok) {
                JellyfinItem updated = item;
                updated.played = desired;
                if (desired) updated.positionTicks = 0;
                updateCachedUserData(updated);
            }
            mutationLoading_ = false;
            if ((screen_ != Screen::Details && screen_ != Screen::ItemMenu) || detail_.id != item.id) return;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            detail_.played = desired;
            if (desired) detail_.positionTicks = 0;
        });
    }

    void removeCachedItem(const std::string& itemId) {
        if (itemId.empty()) return;
        auto remove = [&](auto& items) {
            std::erase_if(items, [&](const JellyfinItem& item) { return item.id == itemId; });
        };
        for (auto& row : home_.rows) remove(row.items);
        browseState_.removeItem(itemId);
        remove(searchState_.results());
        detailsState_.removeItem(itemId);
    }

    void refreshCurrentItemMetadataAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty()) return;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        tasks_.submit([this, session, item, sessionEpoch] {
            auto result = api_.refreshMetadata(session, item);
            std::scoped_lock lock(stateMutex_);
            if (!requestEpochs_.session.active(sessionEpoch)) return;
            mutationLoading_ = false;
            if (screen_ != Screen::ItemMenu || detail_.id != item.id) return;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            popScreen(Screen::Details);
            showNotice("METADATA REFRESH REQUESTED");
        });
    }

    void deleteCurrentItemAsync() {
        if (loading_ || mutationLoading_ || detail_.id.empty() || !detail_.canDelete) return;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        const uint64_t sessionEpoch = requestEpochs_.session.snapshot();
        mutationLoading_ = true;
        error_.clear();
        tasks_.submit([this, session, item, sessionEpoch] {
            auto result = api_.deleteItem(session, item);
            std::scoped_lock lock(stateMutex_);
            if (!requestEpochs_.session.active(sessionEpoch)) return;
            if (result.ok) removeCachedItem(item.id);
            mutationLoading_ = false;
            if (screen_ != Screen::ItemMenu || detail_.id != item.id) return;
            if (!result.ok) {
                error_ = result.error;
                detailsState_.setDeleteConfirmation(false);
                return;
            }
            detail_ = {};
            detailsState_.setDeleteConfirmation(false);
            popScreen(Screen::Home);
            if (screen_ == Screen::Details) popScreen(Screen::Home);
            showNotice("MEDIA DELETED");
        });
    }

    void openSearch() {
        pushScreen(Screen::Search);
        searchState_.setKeyboard(!showSystemTextInput(searchState_.query(), "Search Jellyfin", kTextInputSearch));
        keyboardRow_ = keyboardCol_ = 0;
        error_.clear();
    }

    void discoverServersAsync() {
        if (loading_) return;
        loading_ = true;
        error_.clear();
        accountState_.setDiscoveryStatus("SEARCHING LOCAL NETWORK...");
        const uint64_t generation = requestEpochs_.auth.begin();
        tasks_.submit([this, generation] {
            auto servers = discoverJellyfinServers(1600);
            if (!requestEpochs_.auth.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (servers.empty()) {
                accountState_.clearDiscoveryStatus();
                error_ = "NO JELLYFIN SERVER FOUND ON THIS NETWORK";
                return;
            }
            accountState_.setField(AccountScreenState::kServerField, servers.front().address);
            std::string discoveryStatus = "FOUND " + (servers.front().name.empty() ? std::string("JELLYFIN") : servers.front().name);
            if (servers.size() > 1) discoveryStatus += " + " + std::to_string(servers.size() - 1) + " MORE";
            accountState_.setDiscoveryStatus(std::move(discoveryStatus));
            accountState_.setLoginFocus(AccountScreenState::kUsernameField);
            error_.clear();
        });
    }

    void loginAsync() {
        if (loading_) return;
        loading_ = true;
        error_.clear();
        const auto fields = accountState_.fields();
        const std::string deviceId = deviceId_;
        const uint64_t generation = requestEpochs_.auth.begin();

        tasks_.submit([this, fields, deviceId, generation] {
            auto result = api_.login(fields[0], fields[1], fields[2], deviceId);
            if (!requestEpochs_.auth.active(generation)) return;
            if (!result.ok) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                error_ = result.error;
                return;
            }
            {
                std::scoped_lock lock(stateMutex_);
                requestEpochs_.session.invalidate();
                session_ = result.value;
                accountState_.setAuthenticatedAccount(session_.server, session_.username);
                resetNavigation(Screen::Home);
                loading_ = false;
                homeState_.setRow(0);
                homeState_.setFirstVisibleRow(0);
                error_.clear();
                saveSession(session_);
            }
            loadHomeAsync();
        });
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
        const uint64_t generation = requestEpochs_.auth.begin();

        tasks_.submit([this, server, deviceId, generation] {
            auto initiated = api_.initiateQuickConnect(server, deviceId);
            if (!requestEpochs_.auth.active(generation)) return;
            if (!initiated.ok) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                accountState_.endQuickConnect();
                error_ = initiated.error;
                return;
            }

            const QuickConnectRequest request = initiated.value;
            {
                std::scoped_lock lock(stateMutex_);
                accountState_.setField(AccountScreenState::kServerField, request.server);
                accountState_.setQuickConnectCode(request.code);
                loading_ = false;
            }

            for (int attempt = 0; attempt < 60; ++attempt) {
                std::this_thread::sleep_for(5s);
                if (!requestEpochs_.auth.active(generation)) return;

                auto state = api_.pollQuickConnect(request, deviceId);
                if (!state.ok) {
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    accountState_.endQuickConnect();
                    error_ = state.error;
                    return;
                }
                if (!state.value) continue;

                auto authenticated = api_.completeQuickConnect(request, deviceId);
                if (!requestEpochs_.auth.active(generation)) return;
                if (!authenticated.ok) {
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    accountState_.endQuickConnect();
                    error_ = authenticated.error;
                    return;
                }

                {
                    std::scoped_lock lock(stateMutex_);
                    requestEpochs_.session.invalidate();
                    session_ = authenticated.value;
                    accountState_.setAuthenticatedAccount(session_.server, session_.username);
                    loading_ = false;
                    resetNavigation(Screen::Home);
                    homeState_.setRow(0);
                    homeState_.setFirstVisibleRow(0);
                    error_.clear();
                    saveSession(session_);
                }
                loadHomeAsync();
                return;
            }

            if (requestEpochs_.auth.active(generation)) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                accountState_.endQuickConnect();
                error_ = "QUICK CONNECT TIMED OUT - TRY AGAIN";
            }
        });
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
        }

        const uint64_t generation = requestEpochs_.home.begin();
        const auto homeLoadStarted = std::chrono::steady_clock::now();
        tasks_.submit([this, session, generation, homeSnapshot = std::move(homeSnapshot), homeLoadStarted] {
            auto core = api_.loadHomeCore(session);
            if (!requestEpochs_.home.active(generation)) return;
            if (!core.ok) {
                std::scoped_lock lock(stateMutex_);
                homeLoading_ = false;
                if (core.error.find("HTTP 401") != std::string::npos) {
                    const JellyfinSession expired = session_;
                    eraseArtworkEntry(profileArtwork_, profileArtworkKey(expired));
                    sessionRegistry_.removeIdentity(expired);
                    requestEpochs_.invalidateAll();
                    loading_ = false;
                    mutationLoading_ = false;
                    searchState_.setLoading(false);
                    session_.token.clear();
                    session_.userId.clear();
                    resetNavigation(Screen::Login);
                    error_ = "SESSION EXPIRED - LOG IN AGAIN";
                    saveSession(session_);
                } else if (screen_ == Screen::Home) {
                    error_ = core.error;
                }
                return;
            }

            filterHiddenHomeItems(core.value);
            std::vector<JellyfinItem> views = core.value.views;
            HomeRestorePlan coreRestore = HomeScreenState::restorePlan(homeSnapshot, core.value.rows);
            const int coreRestoredRow = coreRestore.focusedRow;
            {
                std::scoped_lock lock(stateMutex_);
                homeLoading_ = false;
                home_ = std::move(core.value);
                homeState_.setSelections(std::move(coreRestore.selections));
                homeState_.setRow(coreRestoredRow);
                homeState_.updateViewport(static_cast<int>(home_.rows.size()));
                if (screen_ == Screen::Home) error_ = home_.warning;
                if (homeState_.row() >= 0) {
                    const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
                    prefetchHomeWindow(homeState_.row(), homeState_.selection(homeState_.row(), static_cast<int>(row.items.size())));
                }
                const auto coreMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - homeLoadStarted).count();
                __android_log_print(ANDROID_LOG_INFO, kTag, "Home primary rows ready in %lld ms", static_cast<long long>(coreMs));
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
            }

            tasks_.submit([this, session, generation, views = std::move(views), homeSnapshot, coreRestoredRow, homeLoadStarted] {
                auto secondary = api_.loadHomeSecondary(session, views);
                if (!requestEpochs_.home.active(generation)) return;
                std::scoped_lock lock(stateMutex_);
                if (!secondary.ok) {
                    if (!home_.warning.empty()) home_.warning += " | ";
                    home_.warning += "SECONDARY HOME ROWS UNAVAILABLE";
                    if (screen_ == Screen::Home) error_ = home_.warning;
                    return;
                }

                filterHiddenHomeItems(secondary.value);
                const size_t baseRowCount = home_.rows.size();
                for (auto& section : secondary.value.rows) {
                    const int restoredSelection = HomeScreenState::restoredSelection(homeSnapshot, section);
                    const bool focusAppended = !homeSnapshot.toolbarFocused
                        && section.title == homeSnapshot.focusedRowTitle
                        && homeState_.row() == coreRestoredRow;
                    home_.rows.push_back(std::move(section));
                    homeState_.appendSelection(restoredSelection);
                    if (focusAppended) homeState_.setRow(static_cast<int>(home_.rows.size()) - 1);
                }
                if (!secondary.value.warning.empty()) {
                    if (!home_.warning.empty()) home_.warning += " | ";
                    home_.warning += secondary.value.warning;
                    if (screen_ == Screen::Home) error_ = home_.warning;
                }
                homeState_.updateViewport(static_cast<int>(home_.rows.size()));
                if (homeState_.row() >= static_cast<int>(baseRowCount)
                    && homeState_.row() < static_cast<int>(homeState_.selectionCount())) {
                    const auto& row = home_.rows[static_cast<size_t>(homeState_.row())];
                    prefetchHomeWindow(homeState_.row(), homeState_.selection(homeState_.row(), static_cast<int>(row.items.size())));
                }
                const auto fullMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - homeLoadStarted).count();
                __android_log_print(ANDROID_LOG_INFO, kTag, "Home enrichment completed in %lld ms", static_cast<long long>(fullMs));
            });
        });
    }

    void searchAsync() {
        if (!session_.valid() || !searchState_.beginSearch()) return;
        const JellyfinSession session = session_;
        const std::string query = searchState_.query();
        error_.clear();
        const uint64_t generation = requestEpochs_.search.begin();
        tasks_.submit([this, session, query, generation] {
            auto result = api_.search(session, query);
            std::scoped_lock lock(stateMutex_);
            if (!requestEpochs_.search.active(generation)) return;
            if (screen_ != Screen::Search) {
                searchState_.setLoading(false);
                return;
            }
            if (!result.ok) {
                if (searchState_.failSearch(query)) error_ = result.error;
                return;
            }
            if (!searchState_.finishSearch(query, std::move(result.value))) return;
            error_.clear();
        });
    }

    void openDetails(const JellyfinItem& item) {
        pushScreen(Screen::Details);
        continuationState_.setStillWatchingPrompt(false);
        detail_ = item;
        detailsState_.beginDetails();
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string id = item.id;
        const uint64_t generation = requestEpochs_.content.begin();
        tasks_.submit([this, session, id, generation] {
            auto result = api_.getItem(session, id);
            if (!requestEpochs_.content.active(generation)) return;
            {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                if (screen_ != Screen::Details || detail_.id != id) return;
                if (!result.ok) {
                    error_ = "DETAILS: " + result.error;
                    return;
                }
                detail_ = std::move(result.value);
            }

            auto similar = api_.getSimilar(session, id, 18);
            if (!requestEpochs_.content.active(generation) || !similar.ok) return;
            std::scoped_lock lock(stateMutex_);
            if (screen_ != Screen::Details || detail_.id != id) return;
            detailsState_.setSimilar(std::move(similar.value));
        });
    }

    void syncNextPlaybackFromQueue() {
        const int next = queueState_.nextIndex(false);
        if (const auto* item = queueState_.itemAt(next)) continuationState_.setNextItem(*item);
        else continuationState_.clearNextItem();
    }

    void shuffleRemainingQueue() {
        static thread_local std::mt19937 generator(std::random_device{}());
        if (!queueState_.shuffleRemaining(generator)) return;
        syncNextPlaybackFromQueue();
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
        syncNextPlaybackFromQueue();
    }

    void playQueuedIndexAsync(int index, bool restartCurrent = false) {
        if (loading_ || index < 0 || index >= queueState_.size() || !session_.valid()) return;
        if (index == queueState_.currentIndex() && screen_ == Screen::Player && !restartCurrent) {
            queueState_.closeOverlay();
            return;
        }

        const Screen originScreen = screen_;
        const bool replacingPlayer = screen_ == Screen::Player && !activePlaybackItem_.id.empty();
        if (replacingPlayer) releaseActivePlayback(true);
        const int previousQueueIndex = queueState_.currentIndex();
        queueState_.closeOverlay();
        loading_ = true;
        transitionState_.setLoading(replacingPlayer);
        continuationState_.setStillWatchingPrompt(false);
        error_.clear();
        const JellyfinSession session = session_;
        JellyfinItem queued = *queueState_.itemAt(index);
        if (restartCurrent) queued.positionTicks = 0;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const auto audioPreference = trackState_.audioLanguagePreference();
        const auto subtitlePreference = trackState_.subtitleLanguagePreference();
        const uint64_t generation = requestEpochs_.playback.begin();
        tasks_.submit([this, session, queued = std::move(queued), index, previousQueueIndex, originScreen, replacingPlayer, maxStreamingBitrate, maxAudioChannels, playbackOverrides, audioPreference, subtitlePreference, generation]() mutable {
            auto detailed = api_.getItem(session, queued.id);
            if (detailed.ok) queued = std::move(detailed.value);
            const int audioStreamIndex = audioIndexForPlaybackItem(queued, audioPreference);
            const int subtitleStreamIndex = subtitleIndexForPlaybackItem(queued, subtitlePreference);
            auto target = api_.resolvePlayback(
                session,
                queued,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                audioStreamIndex,
                subtitleStreamIndex
            );
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            transitionState_.setLoading(false);
            if (screen_ != originScreen
                || queueState_.currentIndex() != previousQueueIndex
                || !queueState_.itemMatches(index, queued.id)) {
                return;
            }
            if (!target.ok) {
                if (replacingPlayer && screen_ == Screen::Player) popScreen(Screen::Details);
                error_ = "QUEUE: " + target.error;
                return;
            }
            queueState_.setCurrentIndex(index);
            queueState_.setItemAt(index, queued);
            transitionState_.stage(std::move(target.value), std::move(queued));
        });
    }

    void handleQueueOverlayKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            queueState_.closeOverlay();
            return;
        }
        const int size = queueState_.size();
        if (size <= 0) {
            queueState_.closeOverlay();
            return;
        }
        if (key == AKEYCODE_DPAD_UP) {
            queueState_.moveSelection(-1);
            return;
        }
        if (key == AKEYCODE_DPAD_DOWN) {
            queueState_.moveSelection(1);
            return;
        }
        if (key == AKEYCODE_DPAD_LEFT) {
            queueState_.moveAction(-1);
            return;
        }
        if (key == AKEYCODE_DPAD_RIGHT) {
            queueState_.moveAction(1);
            return;
        }
        if (key != AKEYCODE_DPAD_CENTER && key != AKEYCODE_ENTER) return;

        const int selection = queueState_.selection();
        const int current = queueState_.currentIndex();
        if (queueState_.actionSelection() == 0) {
            if (queueCanPlayNow(selection, current, size)) playQueuedIndexAsync(selection);
        } else if (queueState_.actionSelection() == 1) {
            if (queueCanPlayNext(selection, current, size)) moveQueuedItem(selection, current + 1);
        } else if (queueState_.actionSelection() == 2) {
            if (queueCanMoveUp(selection, current, size)) moveQueuedItem(selection, selection - 1);
        } else if (queueState_.actionSelection() == 3) {
            if (queueCanMoveDown(selection, current, size)) moveQueuedItem(selection, selection + 1);
        } else if (queueState_.actionSelection() == 4) {
            if (queueState_.removeSelected()) syncNextPlaybackFromQueue();
        } else if (queueState_.actionSelection() == 5) {
            shuffleRemainingQueue();
        } else if (queueState_.actionSelection() == 6) {
            queueState_.cycleRepeatMode();
            syncNextPlaybackFromQueue();
        }
    }

    void beginSeriesPlayAll() {
        if (loading_ || detail_.type != "Series" || detail_.id.empty() || !session_.valid()) return;
        loading_ = true;
        error_.clear();
        continuationState_.resetAutoplayChain();
        continuationState_.setStillWatchingPrompt(false);
        trackState_.clearLanguagePreferences();
        const JellyfinSession session = session_;
        const JellyfinItem series = detail_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const uint64_t generation = requestEpochs_.playback.begin();
        tasks_.submit([this, session, series, maxStreamingBitrate, maxAudioChannels, playbackOverrides, generation] {
            auto episodes = api_.getSeriesEpisodes(session, series.id, 1000);
            if (!episodes.ok || episodes.value.empty()) {
                if (!requestEpochs_.playback.active(generation)) return;
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                if (screen_ != Screen::Details || detail_.id != series.id) return;
                error_ = episodes.ok ? "PLAY ALL: NO EPISODES" : "PLAY ALL: " + episodes.error;
                return;
            }

            const bool hasRegularEpisodes = std::any_of(episodes.value.begin(), episodes.value.end(), [](const JellyfinItem& item) {
                return item.parentIndexNumber > 0;
            });
            if (hasRegularEpisodes) {
                episodes.value.erase(std::remove_if(episodes.value.begin(), episodes.value.end(), [](const JellyfinItem& item) {
                    return item.parentIndexNumber <= 0;
                }), episodes.value.end());
            }
            std::sort(episodes.value.begin(), episodes.value.end(), [](const JellyfinItem& left, const JellyfinItem& right) {
                if (left.parentIndexNumber != right.parentIndexNumber) return left.parentIndexNumber < right.parentIndexNumber;
                if (left.indexNumber != right.indexNumber) return left.indexNumber < right.indexNumber;
                return left.name < right.name;
            });

            std::vector<JellyfinItem> deduplicated;
            deduplicated.reserve(episodes.value.size());
            for (size_t begin = 0; begin < episodes.value.size();) {
                size_t end = begin + 1;
                while (end < episodes.value.size() && sameEpisodeSlot(
                    episodes.value[begin].parentIndexNumber,
                    episodes.value[begin].indexNumber,
                    episodes.value[end].parentIndexNumber,
                    episodes.value[end].indexNumber
                )) {
                    ++end;
                }

                size_t selected = begin;
                if (end - begin > 1) {
                    bool selectedAvailable = api_.isStaticStreamAvailable(session, episodes.value[selected]);
                    for (size_t candidate = begin + 1; candidate < end && !selectedAvailable; ++candidate) {
                        const bool candidateAvailable = api_.isStaticStreamAvailable(session, episodes.value[candidate]);
                        if (preferAvailableDuplicate(selectedAvailable, candidateAvailable)) {
                            selected = candidate;
                            selectedAvailable = true;
                        }
                    }
                    if (!selectedAvailable) {
                        __android_log_print(
                            ANDROID_LOG_WARN,
                            kTag,
                            "No available source found for duplicate S%02dE%02d slot; retaining first server result",
                            episodes.value[begin].parentIndexNumber,
                            episodes.value[begin].indexNumber
                        );
                    }
                }
                deduplicated.push_back(std::move(episodes.value[selected]));
                begin = end;
            }
            episodes.value = std::move(deduplicated);
            if (episodes.value.empty()) {
                if (!requestEpochs_.playback.active(generation)) return;
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                if (screen_ != Screen::Details || detail_.id != series.id) return;
                error_ = "PLAY ALL: NO REGULAR EPISODES";
                return;
            }

            JellyfinItem first = episodes.value.front();
            auto detailed = api_.getItem(session, first.id);
            if (detailed.ok) first = std::move(detailed.value);
            auto target = api_.resolvePlayback(
                session,
                first,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides
            );
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::Details || detail_.id != series.id) return;
            if (!target.ok) {
                error_ = "PLAY ALL: " + target.error;
                return;
            }
            queueState_.replace(std::move(episodes.value), 0);
            queueState_.setItemAt(0, first);
            restoreHomeVisibilityForPlayback(series);
            restoreHomeVisibilityForPlayback(first);
            transitionState_.stage(std::move(target.value), std::move(first));
        });
    }

    void beginPlayback() {
        if (loading_ || detail_.id.empty()) return;
        const bool continuingPlaybackChain = continuationState_.stillWatchingPrompt();
        const bool continuingQueuedPrompt = continuingPlaybackChain && !queueState_.empty();
        int queuedPlaybackIndex = -1;
        if (!continuingPlaybackChain) trackState_.clearLanguagePreferences();
        if (continuingQueuedPrompt) {
            queuedPlaybackIndex = queueState_.findItemIndex(detail_.id);
            if (queuedPlaybackIndex < 0) queueState_.reset();
        } else {
            queueState_.reset();
        }
        continuationState_.resetAutoplayChain();
        continuationState_.setStillWatchingPrompt(false);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem selected = detail_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const auto audioPreference = trackState_.audioLanguagePreference();
        const auto subtitlePreference = trackState_.subtitleLanguagePreference();
        const uint64_t generation = requestEpochs_.playback.begin();

        tasks_.submit([this, session, selected, maxStreamingBitrate, maxAudioChannels, playbackOverrides, audioPreference, subtitlePreference, queuedPlaybackIndex, generation] {
            JellyfinItem playable = selected;
            if (selected.type == "Series") {
                auto next = api_.getNextUpForSeries(session, selected.id);
                if (!next.ok) {
                    if (!requestEpochs_.playback.active(generation)) return;
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    if (screen_ != Screen::Details || detail_.id != selected.id) return;
                    error_ = next.error;
                    return;
                }
                playable = std::move(next.value);
                auto detailed = api_.getItem(session, playable.id);
                if (detailed.ok) playable = std::move(detailed.value);
            }

            const int audioStreamIndex = audioIndexForPlaybackItem(playable, audioPreference);
            const int subtitleStreamIndex = subtitleIndexForPlaybackItem(playable, subtitlePreference);
            auto target = api_.resolvePlayback(
                session,
                playable,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                audioStreamIndex,
                subtitleStreamIndex
            );
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::Details || detail_.id != selected.id) return;
            if (!target.ok) {
                error_ = target.error;
                return;
            }
            if (queuedPlaybackIndex >= 0 && queueState_.itemMatches(queuedPlaybackIndex, playable.id)) {
                queueState_.setCurrentIndex(queuedPlaybackIndex);
                queueState_.setItemAt(queuedPlaybackIndex, playable);
            }
            restoreHomeVisibilityForPlayback(selected);
            restoreHomeVisibilityForPlayback(playable);
            transitionState_.stage(std::move(target.value), std::move(playable));
        });
    }

    void requestMediaSegmentsAsync() {
        if (!session_.valid() || activePlaybackItem_.id.empty() || !playbackSessionState_.beginMediaSegmentsRequest()) return;
        const JellyfinSession session = session_;
        const std::string itemId = activePlaybackItem_.id;
        tasks_.submit([this, session, itemId] {
            auto result = api_.getMediaSegments(session, itemId);
            std::scoped_lock lock(stateMutex_);
            if (screen_ != Screen::Player || activePlaybackItem_.id != itemId) return;
            if (!result.ok) {
                __android_log_print(ANDROID_LOG_WARN, kTag, "Media segments unavailable: %s", result.error.c_str());
                return;
            }
            const size_t segmentCount = result.value.size();
            playbackSessionState_.setMediaSegments(std::move(result.value));
            __android_log_print(ANDROID_LOG_INFO, kTag, "Loaded %zu media segments", segmentCount);
        });
    }

    void requestNextEpisodeAsync() {
        if (queueState_.currentIndex() >= 0 && queueState_.currentIndex() < queueState_.size()) {
            continuationState_.markNextEpisodeRequested();
            syncNextPlaybackFromQueue();
            return;
        }
        if (!session_.valid() || activePlaybackItem_.type != "Episode"
            || activePlaybackItem_.seriesId.empty() || activePlaybackItem_.id.empty()
            || !continuationState_.beginNextEpisodeRequest()) {
            return;
        }
        const JellyfinSession session = session_;
        const std::string seriesId = activePlaybackItem_.seriesId;
        const std::string currentItemId = activePlaybackItem_.id;
        tasks_.submit([this, session, seriesId, currentItemId] {
            auto next = api_.getFollowingEpisodeForSeries(session, seriesId, currentItemId);
            if (!next.ok || next.value.id.empty() || next.value.id == currentItemId) return;
            auto detailed = api_.getItem(session, next.value.id);
            JellyfinItem item = detailed.ok ? std::move(detailed.value) : std::move(next.value);
            std::scoped_lock lock(stateMutex_);
            if (screen_ != Screen::Player || activePlaybackItem_.id != currentItemId) return;
            continuationState_.setNextItem(std::move(item));
        });
    }

    void releaseActivePlayback(bool reportStop) {
        requestEpochs_.playback.invalidate();
        if (player_.status() == PlayerStatus::Playing || player_.status() == PlayerStatus::Paused) {
            refreshPlaybackTelemetry(true);
        }
        const int64_t ticks = playbackTicksFromPositionMs(playerScreenState_.positionMs());
        const auto session = session_;
        const auto item = activePlaybackItem_;
        const auto target = activeTarget_;
        const bool shouldReport = reportStop && telemetryState_.playbackStartReported() && session.valid()
            && !item.id.empty() && !target.url.empty();
        if (!item.id.empty() && detail_.id == item.id) {
            detail_.positionTicks = ticks;
        }
        player_.stop();
        videoSurface_.release();
        displayMode_.restore();
        mediaSession_.clear();
        clearTrickplayPreview();
        telemetryState_.clearPlaybackStartReported();
        transitionState_.setFallbackResolving(false);
        activeTarget_ = {};
        activePlaybackItem_ = {};
        playerScreenState_.resetPosition();
        telemetryState_.resetReadIntervals();
        continuationState_.clearNextEpisode();
        trackState_.resetPlayback();
        playbackSessionState_.resetMediaSegments();
        if (shouldReport) {
            tasks_.submit([this, session, item, target, ticks] {
                logPlaybackReportFailure("stop", item.id, api_.reportPlaybackStopped(session, item, target, ticks));
            });
        }
    }

    void queueAutoplayNext(JellyfinItem nextItem) {
        const int queuedNextIndex = queueState_.autoplayAdvanceIndex(nextItem);
        releaseActivePlayback(true);
        continuationState_.incrementAutoplayChain();
        loading_ = true;
        transitionState_.setLoading(true);
        detail_ = nextItem;
        continuationState_.setStillWatchingPrompt(false);
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        const JellyfinSession session = session_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const auto audioPreference = trackState_.audioLanguagePreference();
        const auto subtitlePreference = trackState_.subtitleLanguagePreference();
        const uint64_t generation = requestEpochs_.playback.begin();
        tasks_.submit([this, session, maxStreamingBitrate, maxAudioChannels, playbackOverrides, audioPreference, subtitlePreference, nextItem = std::move(nextItem), queuedNextIndex, generation]() mutable {
            auto detailed = api_.getItem(session, nextItem.id);
            if (detailed.ok) nextItem = std::move(detailed.value);
            const int audioStreamIndex = audioIndexForPlaybackItem(nextItem, audioPreference);
            const int subtitleStreamIndex = subtitleIndexForPlaybackItem(nextItem, subtitlePreference);
            auto target = api_.resolvePlayback(
                session,
                nextItem,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                audioStreamIndex,
                subtitleStreamIndex
            );
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            transitionState_.setLoading(false);
            if (!target.ok) {
                popScreen(Screen::Details);
                error_ = "NEXT EPISODE: " + target.error;
                return;
            }
            if (queuedNextIndex >= 0
                && queueState_.currentIndex() + 1 == queuedNextIndex
                && queueState_.itemMatches(queuedNextIndex, nextItem.id)) {
                queueState_.setCurrentIndex(queuedNextIndex);
                queueState_.setItemAt(queuedNextIndex, nextItem);
            }
            transitionState_.stage(std::move(target.value), std::move(nextItem));
        });
    }

    void showStillWatching(JellyfinItem nextItem) {
        releaseActivePlayback(true);
        continuationState_.resetAutoplayChain();
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        detail_ = std::move(nextItem);
        detailsState_.beginDetails();
        popScreen(Screen::Details);
        continuationState_.setStillWatchingPrompt(true);
        error_.clear();
    }

    int playerAudioOrdinal(const PlaybackTarget& target, const JellyfinItem& item) const {
        if (target.playMethod != PlaybackMethod::DirectPlay || target.audioStreamIndex < 0) return -1;
        for (size_t index = 0; index < item.audios.size(); ++index) {
            if (item.audios[index].index == target.audioStreamIndex) return static_cast<int>(index);
        }
        return -1;
    }

    void startResolvedPlaybackTarget(const PlaybackTarget& target) {
        player_.startAsync(
            target.url,
            videoSurface_.surface(),
            initialPlayerSeekMs(target.startTicks),
            settings_.playbackBufferPreset,
            playerAudioOrdinal(target, activePlaybackItem_)
        );
    }

    bool retryPlaybackWithoutSubtitle() {
        if (!shouldRetryFailedSubtitleTranscode(
                activeTarget_.playMethod == PlaybackMethod::Transcode,
                trackState_.selectedSubtitleServerIndex()
            )) {
            return false;
        }
        __android_log_print(
            ANDROID_LOG_WARN,
            kTag,
            "Subtitle-selected transcode failed; retrying item without subtitles (stream %d)",
            trackState_.selectedSubtitleServerIndex()
        );
        restartPlaybackAt(playerScreenState_.positionMs(), trackState_.selectedAudioServerIndex(), kSubtitleOffIndex);
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
            trackState_.clearLanguagePreferences();
            transitionState_.setLoading(false);
        }
        resetNavigation(Screen::Home);
        queueState_.closeOverlay();
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        error_.clear();

        if (!request.itemId.empty()) {
            JellyfinItem linked;
            linked.id = request.itemId;
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening runtime ACTION_VIEW Jellyfin item %s", linked.id.c_str());
            openDetails(linked);
        } else if (!request.searchQuery.empty()) {
            searchState_.setQuery(request.searchQuery);
            searchState_.setKeyboard(false);
            pushScreen(Screen::Search);
            __android_log_print(ANDROID_LOG_INFO, kTag, "Opening runtime ACTION_SEARCH query");
            searchAsync();
        }
    }

    bool retryPlaybackWithTranscodeFallback() {
        if (playbackSessionState_.fallbackAttempted() || activeTarget_.transcoding || !session_.valid() || activePlaybackItem_.id.empty()) {
            return false;
        }

        const PlaybackTarget failedTarget = activeTarget_;
        JellyfinItem item = activePlaybackItem_;
        const JellyfinSession session = session_;
        const int64_t resumeTicks = playbackTicksFromPositionMs(playerScreenState_.positionMs());
        const bool shouldReportPrevious = telemetryState_.playbackStartReported() && !failedTarget.url.empty();
        item.positionTicks = resumeTicks;

        player_.stop();
        videoSurface_.release();
        telemetryState_.clearPlaybackStartReported();
        playbackSessionState_.markFallbackAttempted();
        playerScreenState_.setPositionMs(playbackPositionMsFromTicks(resumeTicks));
        playerScreenState_.setDurationMs(playbackPositionMsFromTicks(item.runtimeTicks));
        telemetryState_.resetReadIntervals();

        // Some PlaybackInfo responses include a TranscodingUrl beside DirectPlay. Use
        // that immediately when available; it avoids a second round-trip to Jellyfin.
        if (!activeTarget_.fallbackTranscodeUrl.empty()) {
            if (shouldReportPrevious) {
                tasks_.submit([this, session, item, failedTarget, resumeTicks] {
                    logPlaybackReportFailure(
                        "stop-after-failure",
                        item.id,
                        api_.reportPlaybackStopped(session, item, failedTarget, resumeTicks)
                    );
                });
            }
            activeTarget_.url = std::move(activeTarget_.fallbackTranscodeUrl);
            activeTarget_.fallbackTranscodeUrl.clear();
            activeTarget_.transcoding = true;
            activeTarget_.playMethod = PlaybackMethod::Transcode;
            activeTarget_.startTicks = resumeTicks;

            std::string surfaceError;
            if (!renderer_.ready() || !videoSurface_.create(surfaceError)) {
                error_ = surfaceError.empty() ? "VIDEO FALLBACK SURFACE IS NOT AVAILABLE" : surfaceError;
                return false;
            }
            __android_log_print(ANDROID_LOG_WARN, kTag, "Direct play failed; using offered Jellyfin transcode fallback");
            playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 5s);
            startResolvedPlaybackTarget(activeTarget_);
            return true;
        }

        // Jellyfin commonly omits TranscodingUrl when it selected DirectPlay, even when
        // SupportsTranscoding=true. Re-negotiate asynchronously with direct paths disabled
        // instead of abandoning playback after a Media3 decoder/source prepare failure.
        PlaybackOverrides fallbackOverrides = playbackOverridesFor(settings_);
        fallbackOverrides.forceTranscode = true;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const int audioStreamIndex = trackState_.selectedAudioServerIndex();
        const int subtitleStreamIndex = trackState_.selectedSubtitleServerIndex();
        const uint64_t generation = requestEpochs_.playback.begin();
        loading_ = true;
        transitionState_.setFallbackResolving(true);
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 10s);
        __android_log_print(ANDROID_LOG_WARN, kTag, "Direct play failed without fallback URL; forcing Jellyfin transcode negotiation");

        const bool submitted = tasks_.submit([
            this,
            session,
            item,
            failedTarget,
            shouldReportPrevious,
            resumeTicks,
            maxStreamingBitrate,
            maxAudioChannels,
            fallbackOverrides,
            audioStreamIndex,
            subtitleStreamIndex,
            generation
        ]() mutable {
            if (shouldReportPrevious) {
                logPlaybackReportFailure(
                    "stop-after-failure",
                    item.id,
                    api_.reportPlaybackStopped(session, item, failedTarget, resumeTicks)
                );
            }
            auto target = api_.resolvePlayback(
                session,
                item,
                maxStreamingBitrate,
                maxAudioChannels,
                fallbackOverrides,
                audioStreamIndex,
                subtitleStreamIndex
            );
            if (!requestEpochs_.playback.active(generation)) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            transitionState_.setFallbackResolving(false);
            if (screen_ != Screen::Player || activePlaybackItem_.id != item.id) return;
            if (!target.ok) {
                error_ = "TRANSCODE FALLBACK: " + target.error;
                stopPlayback();
                return;
            }
            transitionState_.stage(std::move(target.value), std::move(item), true, false, audioStreamIndex);
        });
        if (!submitted) {
            loading_ = false;
            transitionState_.setFallbackResolving(false);
            error_ = "TRANSCODE FALLBACK COULD NOT BE STARTED";
            return false;
        }
        return true;
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
        if (!transitionState_.hasPending() || !app_->window) return work;

        work.playbackTransition = transitionState_.take();
        if (!work.playbackTransition) return work;
        auto& transition = *work.playbackTransition;
        auto& target = transition.target;
        auto& item = transition.item;
        const bool streamRestart = transition.streamRestart;
        transitionState_.setPauseAfterRestart(streamRestart && transition.restartPaused);
        activePlaybackItem_ = item;
        activeTarget_ = target;
        std::ostringstream playbackSummary;
        playbackSummary << playbackMethodName(target.playMethod);
        if (!item.videoCodec.empty()) playbackSummary << " / " << item.videoCodec;
        if (item.videoWidth > 0 && item.videoHeight > 0) playbackSummary << " / " << item.videoWidth << 'X' << item.videoHeight;
        lastPlaybackSummary_ = playbackSummary.str();
        playbackSessionState_.resetFallbackAttempted();
        telemetryState_.beginPlayback(std::chrono::steady_clock::now());
        playerScreenState_.beginPlayback(
            playbackPositionMsFromTicks(target.startTicks),
            playbackPositionMsFromTicks(item.runtimeTicks)
        );
        playbackSessionState_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
        if (!streamRestart) {
            continuationState_.clearNextEpisode();
            syncNextPlaybackFromQueue();
        }
        trackState_.resetPlayback();
        int selectedAudioServerIndex = transition.audioStreamIndex >= 0
            ? transition.audioStreamIndex
            : target.audioStreamIndex;
        if (selectedAudioServerIndex < 0 && !item.audios.empty()) {
            const auto preferred = std::find_if(item.audios.begin(), item.audios.end(), [](const JellyfinAudioStream& audio) {
                return audio.isDefault;
            });
            selectedAudioServerIndex = preferred == item.audios.end() ? item.audios.front().index : preferred->index;
        }
        trackState_.setSelectedAudioServerIndex(selectedAudioServerIndex);
        trackState_.setSelectedSubtitleServerIndex(target.subtitleStreamIndex);
        if (trackState_.selectedSubtitleServerIndex() >= 0) {
            const auto selectedSubtitle = std::find_if(
                item.subtitles.begin(),
                item.subtitles.end(),
                [&](const JellyfinSubtitleStream& subtitle) {
                    return subtitle.index == trackState_.selectedSubtitleServerIndex();
                }
            );
            if (selectedSubtitle != item.subtitles.end()) {
                const SubtitleStrategy strategy = subtitleStrategy(selectedSubtitle->codec);
                if (useNativeSubtitleRenderer(strategy, true)) {
                    loadSubtitleAsync(
                        *selectedSubtitle,
                        strategy == SubtitleStrategy::ClientText ? target.subtitleUrl : std::string{}
                    );
                }
            }
        }
        if (!streamRestart) playbackSessionState_.resetMediaSegments();
        transitionState_.setLoading(false);
        if (!streamRestart) {
            if (screen_ == Screen::Player) replaceScreen(Screen::Player);
            else pushScreen(Screen::Player);
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

        std::optional<int64_t> positionTicks;
        if (result.positionMs >= 0) {
            positionTicks = static_cast<int64_t>(result.positionMs) * 10000;
        } else if (result.completionKnown && result.completed && completed.item.runtimeTicks > 0) {
            positionTicks = completed.item.runtimeTicks;
        }
        if (positionTicks) {
            const int64_t boundedTicks = completed.item.runtimeTicks > 0
                ? std::clamp<int64_t>(*positionTicks, 0, completed.item.runtimeTicks)
                : std::max<int64_t>(0, *positionTicks);
            positionTicks = boundedTicks;
            std::scoped_lock lock(stateMutex_);
            if (detail_.id == completed.item.id) detail_.positionTicks = boundedTicks;
        }
        const JellyfinSession reportSession = session_;
        const JellyfinItem reportItem = completed.item;
        tasks_.submit([this, reportSession, reportItem, positionTicks] {
            const auto reported = api_.reportExternalPlaybackStopped(reportSession, reportItem, positionTicks);
            if (!reported.ok) {
                __android_log_print(ANDROID_LOG_WARN, kTag, "External playback stop report failed: %s", reported.error.c_str());
            }
        });
        std::scoped_lock lock(stateMutex_);
        error_.clear();
    }

    bool launchPendingExternalPlayback(PendingTickWork& work) {
        if (!work.externalLaunch) return false;
        auto launch = std::move(*work.externalLaunch);
        std::string launchError;
        const std::string title = launch.item.seriesName.empty()
            ? launch.item.name
            : launch.item.seriesName + " - " + launch.item.name;
        const int positionMs = playbackPositionMsFromTicks(launch.item.positionTicks);
        if (!externalPlayer_.launch(
                launch.player,
                launch.url,
                title,
                positionMs,
                launch.subtitleUrl,
                launchError
            )) {
            std::scoped_lock lock(stateMutex_);
            error_ = launchError.empty() ? "EXTERNAL PLAYER COULD NOT BE LAUNCHED" : launchError;
        } else {
            std::scoped_lock lock(stateMutex_);
            error_.clear();
            lastPlaybackSummary_ = "EXTERNAL / " + launch.player.label;
            externalPlaybackState_.beginActive(std::move(launch));
        }
        return true;
    }

    bool startPendingPlaybackTransition(const PendingTickWork& work) {
        if (!work.playbackTransition) return false;
        const auto& transition = *work.playbackTransition;
        const auto& target = transition.target;
        const auto& item = transition.item;
        if (transition.streamRestart) {
            player_.stop();
            videoSurface_.release();
        }
        std::string surfaceError;
        if (!renderer_.ready() || !videoSurface_.create(surfaceError)) {
            std::scoped_lock lock(stateMutex_);
            error_ = surfaceError.empty() ? "VIDEO SURFACE IS NOT AVAILABLE" : surfaceError;
            popScreen(Screen::Details);
            activeTarget_ = {};
            activePlaybackItem_ = {};
            return true;
        }
        if (settings_.refreshRateSwitching && item.videoFrameRate > 0.0f) {
            displayMode_.matchVideo(app_->window, item.videoFrameRate);
        }
        mediaSession_.updateMetadata(
            item.name,
            episodeLabel(item),
            playbackPositionMsFromTicks(item.runtimeTicks)
        );
        mediaSession_.updateState(
            MediaSessionState::Buffering,
            playbackPositionMsFromTicks(target.startTicks)
        );
        playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 5s);
        startResolvedPlaybackTarget(target);
        return true;
    }

    void tickActivePlayer() {
        if (screen_ != Screen::Player) return;
        PlayerStatus status = player_.status();
        if (status == PlayerStatus::Preparing) {
            mediaSession_.updateState(MediaSessionState::Buffering, playerScreenState_.positionMs());
        }
        if (status == PlayerStatus::Playing && transitionState_.pauseAfterRestart()) {
            player_.togglePause();
            transitionState_.clearPauseAfterRestart();
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
        if (status != PlayerStatus::Playing && status != PlayerStatus::Paused) return;

        refreshPlaybackTelemetry();
        mediaSession_.updateState(
            status == PlayerStatus::Playing ? MediaSessionState::Playing : MediaSessionState::Paused,
            playerScreenState_.positionMs()
        );
        if (!playbackSessionState_.mediaSegmentsRequested()) requestMediaSegmentsAsync();
        if (telemetryState_.markPlaybackStartReported()) {
            const int64_t ticks = playbackTicksFromPositionMs(playerScreenState_.positionMs());
            const auto session = session_;
            const auto itemCopy = activePlaybackItem_;
            const auto targetCopy = activeTarget_;
            tasks_.submit([this, session, itemCopy, targetCopy, ticks] {
                logPlaybackReportFailure(
                    "start",
                    itemCopy.id,
                    api_.reportPlaybackStart(session, itemCopy, targetCopy, ticks)
                );
            });
        }
        const auto now = std::chrono::steady_clock::now();
        if (telemetryState_.progressReportDue(now)) {
            telemetryState_.markProgressReport(now);
            reportProgressAsync(false);
        }
        if (!continuationState_.nextEpisodeRequested() && activePlaybackItem_.type == "Episode"
            && playerScreenState_.positionMs() >= 30000) {
            requestNextEpisodeAsync();
        }
        if (playerScreenState_.durationMs() <= 1000
            || playerScreenState_.positionMs() < playerScreenState_.durationMs() - 1000) {
            return;
        }
        if (queueState_.currentIndex() >= 0 && queueState_.repeatMode() != QueueRepeatMode::Off) {
            const int next = queueState_.nextIndex(false);
            if (next >= 0) playQueuedIndexAsync(next, next == queueState_.currentIndex());
            else stopPlayback();
        } else if (continuationState_.nextItem()) {
            JellyfinItem next = *continuationState_.nextItem();
            if (shouldAutoplayNextEpisode(settings_.autoplayNext, continuationState_.autoplayChainCount(), settings_.stillWatchingAfter)) {
                queueAutoplayNext(std::move(next));
            } else {
                showStillWatching(std::move(next));
            }
        } else {
            stopPlayback();
            continuationState_.resetAutoplayChain();
        }
    }

    void tick() {
        const auto mediaSessionCommand = mediaSession_.takeCommand();
        if (mediaSessionCommand) handleMediaSessionCommand(*mediaSessionCommand);
        applyPendingRuntimeLaunchRequest();

        auto work = collectPendingTickWork();
        finishExternalPlayback(work);
        if (launchPendingExternalPlayback(work)) return;
        if (startPendingPlaybackTransition(work)) return;
        tickActivePlayer();
    }

    void reportProgressAsync(bool immediate) {
        if (screen_ != Screen::Player || !activeTarget_.url.size() || !session_.valid() || !telemetryState_.playbackStartReported()) return;
        if (!immediate && player_.status() == PlayerStatus::Preparing) return;
        const int64_t ticks = playbackTicksFromPositionMs(playerScreenState_.positionMs());
        const bool paused = player_.status() == PlayerStatus::Paused;
        const auto session = session_;
        const auto item = activePlaybackItem_;
        const auto target = activeTarget_;
        tasks_.submit([this, session, item, target, ticks, paused] {
            logPlaybackReportFailure(
                paused ? "paused-progress" : "progress",
                item.id,
                api_.reportPlaybackProgress(session, item, target, ticks, paused)
            );
        });
    }

    void stopPlayback() {
        if (screen_ != Screen::Player && player_.status() == PlayerStatus::Idle) return;
        releaseActivePlayback(true);
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        trackState_.clearLanguagePreferences();
        transitionState_.setLoading(false);
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
        renderer_.setUiTransform(
            uiSafeAreaFraction(settings_.safeAreaPercent),
            uiTextScale(settings_.uiTextSize)
        );
        auto renderScreen = [&](Screen target) {
            switch (target) {
                case Screen::Login: renderLogin(); break;
                case Screen::Profiles: renderProfiles(); break;
                case Screen::Home: renderHome(); break;
                case Screen::Browse: renderBrowse(); break;
                case Screen::Search: renderSearch(); break;
                case Screen::Settings: renderSettings(); break;
                case Screen::Diagnostics: renderDiagnostics(); break;
                case Screen::Details: renderDetails(); break;
                case Screen::Cast: renderCast(); break;
                case Screen::PersonItems: renderPersonItems(); break;
                case Screen::ItemMenu: renderDetails(); break;
                case Screen::Seasons: renderSeasons(); break;
                case Screen::Episodes: renderEpisodes(); break;
                case Screen::Player: renderPlayer(); break;
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

    std::string profileArtworkKey(const JellyfinSession& saved) const {
        return saved.server + ":user:" + saved.userId;
    }

    std::string artworkKey(const JellyfinItem& item) const {
        return session_.server + ":user:" + session_.userId + ":" + item.id + ":primary:" + item.imageTag;
    }

    std::string backdropKey(const JellyfinItem& item) const {
        const std::string artworkItemId = item.backdropItemId.empty() ? item.id : item.backdropItemId;
        return session_.server + ":user:" + session_.userId + ":" + artworkItemId
            + ":backdrop:" + item.backdropTag + ":mode:" + std::to_string(settings_.backdropMode);
    }

    std::string logoKey(const JellyfinItem& item) const {
        const std::string artworkItemId = item.logoItemId.empty() ? item.id : item.logoItemId;
        return session_.server + ":user:" + session_.userId + ":" + artworkItemId + ":logo:" + item.logoTag;
    }

    std::string homeArtworkKey(const JellyfinItem& item) const {
        const ArtworkReference artwork = homeArtworkReference(
            item.id,
            item.imageTag,
            item.seriesId,
            item.seriesPrimaryImageTag,
            preferHomeLandscapeArtwork(item.type),
            item.thumbTag,
            item.backdropTag,
            item.backdropItemId
        );
        return session_.server + ":user:" + session_.userId + ":" + artwork.itemId
            + ":home:v5-480x270:" + std::to_string(static_cast<int>(artwork.kind)) + ":" + artwork.tag;
    }

    void releaseArtworkTexture(ArtworkEntry& entry) {
        if (entry.texture != 0 && entry.textureGeneration == renderer_.generation()) {
            renderer_.deleteTexture(entry.texture);
        }
        entry.texture = 0;
        entry.textureGeneration = 0;
    }

    void clearArtworkCache(ArtworkCache& cache) {
        cache.clear([this](ArtworkEntry& entry) { releaseArtworkTexture(entry); });
    }

    void eraseArtworkEntry(ArtworkCache& cache, const std::string& key) {
        cache.erase(key, [this](ArtworkEntry& entry) { releaseArtworkTexture(entry); });
    }

    void requestHomeArtwork(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty()) return;
        const std::string key = homeArtworkKey(item);
        if (!homeArtwork_.beginLoad(key, [this](ArtworkEntry& entry) { releaseArtworkTexture(entry); })) return;

        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            bool fromDisk = false;
            std::string encoded;
            if (auto cached = homeDiskCache_.read(key)) {
                encoded = std::move(*cached);
                fromDisk = true;
            } else {
                auto bytes = api_.downloadHomeImage(session, itemCopy, 480, 270);
                if (!bytes.ok) {
                    __android_log_print(
                        ANDROID_LOG_WARN,
                        kTag,
                        "Home artwork download failed item=%s type=%s reason=%s",
                        itemCopy.id.c_str(),
                        itemCopy.type.c_str(),
                        bytes.error.c_str()
                    );
                    std::scoped_lock lock(stateMutex_);
                    homeArtwork_.markFailed(key);
                    return;
                }
                encoded = std::move(bytes.value);
            }

            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(encoded, decodeError);
            if (!decoded.valid() && fromDisk) {
                homeDiskCache_.erase(key);
                auto bytes = api_.downloadHomeImage(session, itemCopy, 480, 270);
                if (bytes.ok) {
                    encoded = std::move(bytes.value);
                    decodeError.clear();
                    decoded = imageDecoder_.decode(encoded, decodeError);
                    fromDisk = false;
                }
            }
            if (decoded.valid() && !fromDisk) homeDiskCache_.write(key, encoded);

            std::scoped_lock lock(stateMutex_);
            if (!decoded.valid()) {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kTag,
                    "Home artwork decode failed item=%s reason=%s",
                    itemCopy.id.c_str(),
                    decodeError.c_str()
                );
                homeArtwork_.markFailed(key);
                return;
            }
            homeArtwork_.markReady(key, std::move(decoded));
        });
    }

    void drawCoverTexture(const ArtworkEntry& entry, float x, float y, float width, float height, float alpha = 1.0f) {
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
        renderer_.imageRegion(entry.texture, x, y, width, height, u0, v0, u1, v1, alpha);
    }

    bool drawHomeArtwork(const JellyfinItem& item, float x, float y, float width, float height) {
        if (item.id.empty()) return false;
        const std::string key = homeArtworkKey(item);
        auto* cached = homeArtwork_.find(key);
        if (!cached) {
            requestHomeArtwork(item);
            return false;
        }
        auto& entry = *cached;
        if (entry.state == ArtworkState::Failed) {
            requestHomeArtwork(item);
            return false;
        }
        if (entry.state != ArtworkState::Ready) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            if (!entry.decoded.valid()) {
                eraseArtworkEntry(homeArtwork_, key);
                requestHomeArtwork(item);
                return false;
            }
            entry.sourceWidth = entry.decoded.width;
            entry.sourceHeight = entry.decoded.height;
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
            if (entry.texture != 0) {
                std::vector<uint8_t>().swap(entry.decoded.rgba);
            }
        }
        if (entry.texture == 0) return false;
        drawCoverTexture(entry, x, y, width, height);
        return true;
    }

    void prefetchHomeWindow(int row, int selection) {
        if (row < 0 || row >= static_cast<int>(home_.rows.size())) return;
        const auto& items = home_.rows[static_cast<size_t>(row)].items;
        if (items.empty()) return;
        const int begin = std::max(0, selection - 2);
        const int end = std::min(static_cast<int>(items.size()), selection + 7);
        for (int index = begin; index < end; ++index) requestHomeArtwork(items[static_cast<size_t>(index)]);
    }

    void requestProfileArtwork(const JellyfinSession& saved) {
        if (!saved.valid()) return;
        const std::string key = profileArtworkKey(saved);
        if (!profileArtwork_.beginLoad(key, [this](ArtworkEntry& entry) { releaseArtworkTexture(entry); })) return;
        tasks_.submit([this, saved, key] {
            auto bytes = api_.downloadUserImage(saved, 180, 180);
            if (!bytes.ok) {
                std::scoped_lock lock(stateMutex_);
                profileArtwork_.markFailed(key);
                return;
            }
            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(bytes.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            if (!decoded.valid()) {
                profileArtwork_.markFailed(key);
                return;
            }
            profileArtwork_.markReady(key, std::move(decoded));
        });
    }

    bool drawProfileArtwork(const JellyfinSession& saved, float x, float y, float size) {
        if (!saved.valid()) return false;
        const std::string key = profileArtworkKey(saved);
        auto* cached = profileArtwork_.find(key);
        if (!cached) {
            requestProfileArtwork(saved);
            return false;
        }
        auto& entry = *cached;
        if (entry.state == ArtworkState::Failed) {
            requestProfileArtwork(saved);
            return false;
        }
        if (entry.state != ArtworkState::Ready) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            if (!entry.decoded.valid()) {
                eraseArtworkEntry(profileArtwork_, key);
                requestProfileArtwork(saved);
                return false;
            }
            entry.sourceWidth = entry.decoded.width;
            entry.sourceHeight = entry.decoded.height;
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
            if (entry.texture != 0) std::vector<uint8_t>().swap(entry.decoded.rgba);
        }
        if (entry.texture == 0) return false;
        renderer_.image(entry.texture, x, y, size, size);
        return true;
    }

    void requestArtwork(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty()) return;
        const std::string key = artworkKey(item);
        if (!artwork_.beginLoad(key, [this](ArtworkEntry& entry) { releaseArtworkTexture(entry); })) return;

        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            auto bytes = api_.downloadPrimaryImage(session, itemCopy, 720, 1080);
            if (!bytes.ok) {
                std::scoped_lock lock(stateMutex_);
                artwork_.markFailed(key);
                return;
            }

            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(bytes.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            if (!decoded.valid()) {
                artwork_.markFailed(key);
                return;
            }
            artwork_.markReady(key, std::move(decoded));
        });
    }

    bool drawArtwork(const JellyfinItem& item, float x, float y, float width, float height, float alpha = 1.0f) {
        if (item.id.empty()) return false;
        const std::string key = artworkKey(item);
        auto* cached = artwork_.find(key);
        if (!cached) {
            requestArtwork(item);
            return false;
        }
        auto& entry = *cached;
        if (entry.state == ArtworkState::Failed) {
            requestArtwork(item);
            return false;
        }
        if (entry.state != ArtworkState::Ready) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            if (!entry.decoded.valid()) {
                eraseArtworkEntry(artwork_, key);
                requestArtwork(item);
                return false;
            }
            entry.sourceWidth = entry.decoded.width;
            entry.sourceHeight = entry.decoded.height;
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
            if (entry.texture != 0) std::vector<uint8_t>().swap(entry.decoded.rgba);
        }
        if (entry.texture == 0) return false;
        drawCoverTexture(entry, x, y, width, height, alpha);
        return true;
    }

    void requestBackdrop(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty() || item.backdropTag.empty() || settings_.backdropMode <= 0) return;
        const std::string key = backdropKey(item);
        if (!backdrops_.beginLoad(key, [this](ArtworkEntry& entry) { releaseArtworkTexture(entry); })) return;
        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            auto bytes = api_.downloadBackdropImage(session, itemCopy, 1280, 720);
            if (!bytes.ok) {
                std::scoped_lock lock(stateMutex_);
                backdrops_.markFailed(key);
                return;
            }
            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(bytes.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            if (!decoded.valid()) {
                backdrops_.markFailed(key);
                return;
            }
            backdrops_.markReady(key, std::move(decoded));
        });
    }

    bool drawBackdrop(const JellyfinItem& item, float alpha = 0.28f) {
        if (settings_.backdropMode <= 0 || item.id.empty() || item.backdropTag.empty()) return false;
        const std::string key = backdropKey(item);
        auto* cached = backdrops_.find(key);
        if (!cached) {
            requestBackdrop(item);
            return false;
        }
        auto& entry = *cached;
        if (entry.state == ArtworkState::Failed) {
            requestBackdrop(item);
            return false;
        }
        if (entry.state != ArtworkState::Ready) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            if (!entry.decoded.valid()) {
                eraseArtworkEntry(backdrops_, key);
                requestBackdrop(item);
                return false;
            }
            entry.sourceWidth = entry.decoded.width;
            entry.sourceHeight = entry.decoded.height;
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
            if (entry.texture != 0) std::vector<uint8_t>().swap(entry.decoded.rgba);
        }
        if (entry.texture == 0) return false;
        const float effectiveAlpha = settings_.backdropMode == 1 ? std::max(alpha, 0.34f) : alpha;
        renderer_.image(entry.texture, 0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), effectiveAlpha);
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), Color{0.0f, 0.0f, 0.0f, settings_.backdropMode == 1 ? 0.26f : 0.12f});
        return true;
    }

    void requestLogo(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty() || item.logoTag.empty()) return;
        const std::string key = logoKey(item);
        if (!logos_.beginLoad(key, [this](ArtworkEntry& entry) { releaseArtworkTexture(entry); })) return;
        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            auto bytes = api_.downloadLogoImage(session, itemCopy, 800, 240);
            if (!bytes.ok) {
                std::scoped_lock lock(stateMutex_);
                logos_.markFailed(key);
                return;
            }
            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(bytes.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            if (!decoded.valid()) {
                logos_.markFailed(key);
                return;
            }
            logos_.markReady(key, std::move(decoded));
        });
    }

    bool drawLogo(const JellyfinItem& item, float x, float y, float maxWidth, float maxHeight) {
        if (item.id.empty() || item.logoTag.empty()) return false;
        const std::string key = logoKey(item);
        auto* cached = logos_.find(key);
        if (!cached) {
            requestLogo(item);
            return false;
        }
        auto& entry = *cached;
        if (entry.state == ArtworkState::Failed) {
            requestLogo(item);
            return false;
        }
        if (entry.state != ArtworkState::Ready) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            if (!entry.decoded.valid()) {
                eraseArtworkEntry(logos_, key);
                requestLogo(item);
                return false;
            }
            entry.sourceWidth = entry.decoded.width;
            entry.sourceHeight = entry.decoded.height;
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
            if (entry.texture != 0) std::vector<uint8_t>().swap(entry.decoded.rgba);
        }
        if (entry.texture == 0 || entry.sourceWidth <= 0 || entry.sourceHeight <= 0) return false;
        const float aspect = static_cast<float>(entry.sourceWidth) / static_cast<float>(entry.sourceHeight);
        float width = maxWidth;
        float height = width / aspect;
        if (height > maxHeight) {
            height = maxHeight;
            width = height * aspect;
        }
        renderer_.image(entry.texture, x, y + (maxHeight - height) * 0.5f, width, height);
        return true;
    }

    void drawFocusHalo(float x, float y, float width, float height, Color accent = kFocus, float radius = 18.0f) {
        renderer_.roundedOutline(x - 10.0f, y - 10.0f, width + 20.0f, height + 20.0f, radius + 10.0f, 8.0f,
            Color{accent.r, accent.g, accent.b, 0.10f});
        renderer_.roundedOutline(x - 3.0f, y - 3.0f, width + 6.0f, height + 6.0f, radius + 3.0f, 3.0f,
            Color{accent.r, accent.g, accent.b, 0.92f});
    }

    std::array<float, 4> focusedBounds(float x, float y, float width, float height, bool focused, float scale = 1.045f) const {
        if (!focused) return {x, y, width, height};
        const float scaledWidth = width * scale;
        const float scaledHeight = height * scale;
        return {
            x - (scaledWidth - width) * 0.5f,
            y - (scaledHeight - height) * 0.5f,
            scaledWidth,
            scaledHeight,
        };
    }

    std::array<float, 4> drawFocusedSurface(float x, float y, float width, float height, bool focused, bool primary = false, bool destructive = false) {
        const auto bounds = focusedBounds(x, y, width, height, focused, 1.045f);
        const Color accent = destructive ? kError : kFocus;
        if (focused) {
            renderer_.roundedRect(bounds[0] - 12.0f, bounds[1] - 12.0f, bounds[2] + 24.0f, bounds[3] + 24.0f, 28.0f,
                Color{accent.r, accent.g, accent.b, 0.08f});
        }
        const Color surface = destructive && focused
            ? Color{kError.r, kError.g, kError.b, 0.90f}
            : (primary ? (focused ? Color{0.49f, 0.28f, 0.88f, 0.98f} : kFocusSoft) : (focused ? kPanelElevated : kPanelAlt));
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 18.0f, surface);
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], accent, 18.0f);
        return bounds;
    }


    void renderHeader(const std::string& title) {
        renderer_.text(72, 46, 3.9f, "SLOPPATV", kText);
        renderer_.text(75, 103, 2.25f, title, kMuted);
        if (settings_.showClock) {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_r(&now, &local);
            std::ostringstream clock;
            clock << std::put_time(&local, "%H:%M");
            renderer_.text(1680, 52, 2.05f, clock.str(), kMuted, 170);
        }
    }

    void renderLogin() {
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), kBackground);
        renderer_.verticalGradient(0.0f, 0.0f, 1920.0f, 1080.0f,
            Color{0.10f, 0.05f, 0.18f, 0.42f},
            Color{0.01f, 0.04f, 0.07f, 0.12f});
        renderer_.text(676.0f, 76.0f, 6.6f, "SLOPPATV", kText, 700.0f);
        renderer_.text(730.0f, 150.0f, 2.05f, "Connect to your Jellyfin server", kMuted, 560.0f);

        if (accountState_.quickConnectActive()) {
            renderer_.roundedRect(465.0f, 250.0f, 990.0f, 560.0f, 34.0f, Color{0.035f, 0.040f, 0.055f, 0.92f});
            renderer_.text(775.0f, 300.0f, 2.4f, "QUICK CONNECT", kMuted, 400.0f);
            const float codeWidth = renderer_.textWidth(8.8f, accountState_.quickConnectCode());
            renderer_.text(960.0f - codeWidth * 0.5f, 390.0f, 8.8f, accountState_.quickConnectCode(), kText, 760.0f);
            renderer_.text(625.0f, 560.0f, 2.25f, "1  OPEN JELLYFIN ON ANOTHER DEVICE", kText, 700.0f);
            renderer_.text(625.0f, 620.0f, 2.25f, "2  SETTINGS  >  QUICK CONNECT  >  ENTER CODE", kText, 700.0f);
            renderer_.text(695.0f, 710.0f, 1.9f, loading_ ? "STARTING..." : "WAITING FOR AUTHORIZATION...", kFocus, 560.0f);
            renderer_.text(790.0f, 775.0f, 1.55f, "BACK TO CANCEL", kMuted, 340.0f);
            return;
        }

        renderer_.roundedRect(410.0f, 225.0f, 1100.0f, 615.0f, 34.0f, Color{0.028f, 0.033f, 0.046f, 0.90f});
        static constexpr std::array<const char*, 3> labels{"SERVER", "USERNAME", "PASSWORD"};
        for (int i = 0; i < 3; ++i) {
            const float y = 320.0f + static_cast<float>(i) * 118.0f;
            renderer_.text(495.0f, y - 30.0f, 1.45f, labels[static_cast<size_t>(i)], kMuted);
            const bool focused = !accountState_.keyboardActive() && accountState_.loginFocus() == i;
            const auto bounds = drawFocusedSurface(490.0f, y, 940.0f, 70.0f, focused);
            std::string value = accountState_.field(i);
            if (i == 2 && !value.empty()) value.assign(value.size(), '*');
            if (value.empty()) value = i == 0 ? "HTTPS://YOUR-JELLYFIN-SERVER" : "";
            renderer_.textVerticallyCentered(bounds[0] + 30.0f, bounds[1], bounds[3], 2.35f, value,
                value.empty() ? kTertiary : kText, bounds[2] - 60.0f);
        }

        const bool loginFocused = accountState_.loginFocus() == AccountScreenState::kLoginAction && !accountState_.keyboardActive();
        const auto loginBounds = drawFocusedSurface(490.0f, 690.0f, 330.0f, 72.0f, loginFocused, true);
        renderer_.textCentered(loginBounds[0], loginBounds[1], loginBounds[2], loginBounds[3], 2.15f, "LOG IN", kText);
        const bool quickFocused = accountState_.loginFocus() == AccountScreenState::kQuickConnectAction && !accountState_.keyboardActive();
        const auto quickBounds = drawFocusedSurface(840.0f, 690.0f, 310.0f, 72.0f, quickFocused);
        renderer_.textCentered(quickBounds[0], quickBounds[1], quickBounds[2], quickBounds[3], 1.8f, "QUICK CONNECT", kText);
        const bool discoverFocused = accountState_.loginFocus() == AccountScreenState::kDiscoverAction && !accountState_.keyboardActive();
        const auto discoverBounds = drawFocusedSurface(1170.0f, 690.0f, 260.0f, 72.0f, discoverFocused);
        renderer_.textCentered(discoverBounds[0], discoverBounds[1], discoverBounds[2], discoverBounds[3], 1.8f, "DISCOVER", kText);

        if (!sessionRegistry_.empty()) {
            const bool savedFocused = accountState_.loginFocus() == AccountScreenState::kSavedUsersAction && !accountState_.keyboardActive();
            const auto savedBounds = drawFocusedSurface(650.0f, 785.0f, 620.0f, 58.0f, savedFocused);
            renderer_.textCentered(savedBounds[0], savedBounds[1], savedBounds[2], savedBounds[3], 1.65f,
                "SAVED USERS (" + std::to_string(sessionRegistry_.size()) + ")", savedFocused ? kText : kMuted);
        }
        if (!accountState_.keyboardActive()) {
            const std::string hint = !accountState_.discoveryStatus().empty()
                ? accountState_.discoveryStatus()
                : "DISCOVER SEARCHES YOUR LOCAL NETWORK";
            renderer_.text(555.0f, 870.0f, 1.65f, hint, accountState_.discoveryStatus().empty() ? kTertiary : kFocus, 810.0f);
        }
        if (accountState_.keyboardActive()) renderKeyboard(610.0f);
    }

    void renderProfiles() {
        renderHeader("USERS & SERVERS");
        renderer_.text(105.0f, 180.0f, 2.15f, "CHOOSE WHO IS WATCHING", kMuted, 640.0f);
        const int totalRows = static_cast<int>(sessionRegistry_.size()) + 1;
        constexpr int visibleRows = 5;
        const int maxFirst = std::max(0, totalRows - visibleRows);
        const int first = std::clamp(accountState_.profileSelection() - visibleRows + 1, 0, maxFirst);
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int index = first + slot;
            if (index >= totalRows) break;
            const float y = 245.0f + static_cast<float>(slot) * 138.0f;
            const bool focused = index == accountState_.profileSelection();
            if (index == static_cast<int>(sessionRegistry_.size())) {
                const auto bounds = drawFocusedSurface(250.0f, y, 1420.0f, 108.0f, focused, false);
                renderer_.textCentered(bounds[0] + 30.0f, bounds[1], 90.0f, bounds[3], 3.0f, "+", kFocus);
                renderer_.textVerticallyCentered(bounds[0] + 120.0f, bounds[1], bounds[3], 2.35f,
                    "ADD ANOTHER ACCOUNT", kText, bounds[2] - 160.0f);
                continue;
            }
            drawFocusedSurface(250.0f, y, 1420.0f, 108.0f, focused, false, focused && accountState_.profileAction() == 1);
            const auto* savedSession = sessionRegistry_.at(static_cast<size_t>(index));
            if (!savedSession) continue;
            const auto& saved = *savedSession;
            if (!drawProfileArtwork(saved, 280.0f, y + 12.0f, 84.0f)) {
                renderer_.roundedRect(280.0f, y + 12.0f, 84.0f, 84.0f, 24.0f, kPanelAlt);
                std::string initial = saved.username.empty() ? "?" : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(saved.username.front()))));
                renderer_.textCentered(280.0f, y + 12.0f, 84.0f, 84.0f, 3.0f, initial, kText);
            }
            renderer_.text(395.0f, y + 20.0f, 2.45f, saved.username.empty() ? "USER" : saved.username, kText, 480.0f);
            renderer_.text(395.0f, y + 65.0f, 1.55f, saved.server, kMuted, 650.0f);
            const bool useFocused = focused && accountState_.profileAction() == 0;
            const bool forgetFocused = focused && accountState_.profileAction() == 1;
            const auto useBounds = drawFocusedSurface(1110.0f, y + 18.0f, 210.0f, 72.0f, useFocused, true);
            renderer_.textCentered(useBounds[0], useBounds[1], useBounds[2], useBounds[3], 1.85f, "USE", kText);
            const auto forgetBounds = drawFocusedSurface(1340.0f, y + 18.0f, 270.0f, 72.0f, forgetFocused, false, true);
            renderer_.textCentered(forgetBounds[0], forgetBounds[1], forgetBounds[2], forgetBounds[3], 1.75f, "FORGET",
                forgetFocused ? kText : kMuted);
        }
        renderer_.text(275.0f, 972.0f, 1.65f, "UP / DOWN CHOOSES ACCOUNT   |   LEFT / RIGHT CHOOSES ACTION", kTertiary, 1380.0f);
    }

    void renderKeyboard(float top) {
        const auto& rows = keyboardRows();
        constexpr float startX = 150.0f;
        constexpr float keyH = 82.0f;
        constexpr float gap = 14.0f;
        for (size_t row = 0; row < rows.size(); ++row) {
            const float y = top + static_cast<float>(row) * (keyH + gap);
            const auto& keys = rows[row];
            const float keyW = row == rows.size() - 1 ? 310.0f : 145.0f;
            for (size_t col = 0; col < keys.size(); ++col) {
                const float x = startX + static_cast<float>(col) * (keyW + gap);
                const bool selected = static_cast<int>(row) == keyboardRow_ && static_cast<int>(col) == keyboardCol_;
                const auto bounds = drawFocusedSurface(x, y, keyW, keyH, selected, selected);
                const auto& label = keys[col].label;
                const float scale = label.size() > 4 ? 2.15f : 2.65f;
                renderer_.textCentered(bounds[0], bounds[1], bounds[2], bounds[3], scale, label, kText);
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
            brandMarkTexture_ = renderer_.createTexture(
                brandMarkDecoded_.width,
                brandMarkDecoded_.height,
                brandMarkDecoded_.rgba.data()
            );
        }
        if (brandMarkTexture_ == 0) return false;
        renderer_.image(brandMarkTexture_, x, y, size, size);
        return true;
    }

    void renderHome() {
        bool backdropVisible = false;
        if (settings_.backdropMode > 0 && !home_.rows.empty()) {
            const int backdropRow = std::clamp(
                homeState_.row() >= 0 ? homeState_.row() : homeState_.firstVisibleRow(),
                0,
                static_cast<int>(home_.rows.size()) - 1
            );
            const auto& row = home_.rows[static_cast<size_t>(backdropRow)];
            if (!row.items.empty() && backdropRow < static_cast<int>(homeState_.selectionCount())) {
                const int selection = homeState_.selection(backdropRow, static_cast<int>(row.items.size()));
                backdropVisible = drawBackdrop(row.items[static_cast<size_t>(selection)], 0.24f);
            }
        }
        if (!backdropVisible) {
            renderer_.rect(0.0f, 0.0f, Renderer::logicalWidth(), Renderer::logicalHeight(), kBackground);
        } else {
            renderer_.rect(0.0f, 0.0f, Renderer::logicalWidth(), Renderer::logicalHeight(), Color{0.01f, 0.012f, 0.018f, 0.34f});
        }

        const bool toolbarFocused = homeState_.row() < 0;
        const bool hasBrandMark = drawBrandMark(72.0f, 27.0f, 72.0f);
        renderer_.text(hasBrandMark ? 160.0f : 72.0f, 42.0f, 3.35f, "sloppaTV", kText, 430.0f);
        renderer_.roundedRect(hasBrandMark ? 160.0f : 72.0f, 99.0f, 86.0f, 3.0f, 1.5f, kBrandGold);

        const std::array<std::string, 3> navLabels{"HOME", "SEARCH", "SETTINGS"};
        const std::array<int, 3> navIndices{1, 2, 3};
        const std::array<float, 3> navXs{1165.0f, 1315.0f, 1490.0f};
        const std::array<float, 3> navWidths{110.0f, 130.0f, 150.0f};
        for (size_t i = 0; i < navLabels.size(); ++i) {
            const bool focused = toolbarFocused && homeState_.navIndex() == navIndices[i];
            const bool active = navIndices[i] == 1;
            if (focused) {
                renderer_.roundedRect(navXs[i] - 14.0f, 40.0f, navWidths[i] + 28.0f, 54.0f, 22.0f,
                    Color{0.12f, 0.10f, 0.16f, 0.88f});
                renderer_.textCentered(navXs[i] - 14.0f, 40.0f, navWidths[i] + 28.0f, 54.0f, 2.0f,
                    navLabels[i], kText);
            } else {
                renderer_.textCentered(navXs[i], 40.0f, navWidths[i], 54.0f, 2.0f, navLabels[i], active ? kText : kMuted);
            }
            if (active) renderer_.roundedRect(navXs[i], 94.0f, 38.0f, 3.0f, 1.5f, kFocus);
        }

        const float profileX = 1660.0f;
        const float profileY = 36.0f;
        const float profileSize = 62.0f;
        const bool profileFocused = toolbarFocused && homeState_.navIndex() == 0;
        const auto profileBounds = focusedBounds(profileX, profileY, profileSize, profileSize, profileFocused, 1.10f);
        renderer_.roundedRect(profileBounds[0], profileBounds[1], profileBounds[2], profileBounds[3], 31.0f,
            profileFocused ? kPanelElevated : kPanelAlt);
        if (!drawProfileArtwork(session_, profileBounds[0], profileBounds[1], profileBounds[2])) {
            const std::string initial = session_.username.empty()
                ? "U"
                : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(session_.username.front()))));
            renderer_.textCentered(profileBounds[0], profileBounds[1], profileBounds[2], profileBounds[3], 2.35f, initial, kText);
        }
        if (profileFocused) renderer_.roundedOutline(profileBounds[0] - 3.0f, profileBounds[1] - 3.0f,
            profileBounds[2] + 6.0f, profileBounds[3] + 6.0f, 34.0f, 3.0f, kFocus);

        if (settings_.showClock) {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_r(&now, &local);
            std::ostringstream clock;
            clock << std::put_time(&local, "%H:%M");
            renderer_.text(1712.0f, 53.0f, 2.10f, clock.str(), Color{kMuted.r, kMuted.g, kMuted.b, 0.82f}, 140.0f);
        }

        if (home_.rows.empty()) {
            renderer_.text(72.0f, 220.0f, 2.3f, homeLoading_ ? "LOADING HOME..." : "NO VIDEO HOME SECTIONS", kMuted);
            return;
        }

        const int firstVisibleRow = homeFirstVisibleRow(
            homeState_.firstVisibleRow(),
            homeState_.row(),
            static_cast<int>(home_.rows.size()),
            2
        );
        renderHomeRow(
            home_.rows[static_cast<size_t>(firstVisibleRow)].title,
            home_.rows[static_cast<size_t>(firstVisibleRow)].items,
            firstVisibleRow,
            150.0f
        );
        if (firstVisibleRow + 1 < static_cast<int>(home_.rows.size())) {
            const int secondRow = firstVisibleRow + 1;
            renderHomeRow(
                home_.rows[static_cast<size_t>(secondRow)].title,
                home_.rows[static_cast<size_t>(secondRow)].items,
                secondRow,
                520.0f
            );
        }
    }

    void renderHomeRow(const std::string& title, const std::vector<JellyfinItem>& items, int row, float top) {
        if (items.empty()) return;
        const int selected = homeState_.selection(row, static_cast<int>(items.size()));

        if (title == "My Media") {
            renderer_.text(72.0f, top, 3.05f, "MY MEDIA", homeState_.row() == row ? kText : kSecondaryText, 420.0f);
            constexpr float cardW = 420.0f;
            constexpr float cardH = 225.0f;
            constexpr float gap = 28.0f;
            const float imageY = top + 52.0f;
            float x = 72.0f;
            for (int index = 0; index < static_cast<int>(items.size()); ++index) {
                if (x + cardW > 1885.0f && index > 0) break;
            const bool focused = homeState_.row() == row && index == selected;
                const auto bounds = focusedBounds(x, imageY, cardW, cardH, focused, 1.07f);
                if (focused) renderer_.roundedRect(bounds[0] - 10.0f, bounds[1] - 10.0f, bounds[2] + 20.0f, bounds[3] + 20.0f, 24.0f,
                    Color{kFocus.r, kFocus.g, kFocus.b, 0.10f});
                const bool hasArtwork = drawHomeArtwork(items[static_cast<size_t>(index)], bounds[0], bounds[1], bounds[2], bounds[3]);
                if (!hasArtwork) {
                    renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 16.0f, kPanelAlt);
                    renderer_.textCentered(bounds[0] + 22.0f, bounds[1] + 22.0f, bounds[2] - 44.0f, bounds[3] - 44.0f,
                        2.45f, items[static_cast<size_t>(index)].name, kText);
                }
                if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, 16.0f);
                renderer_.text(x + 4.0f, imageY + cardH + 12.0f, 2.05f,
                    items[static_cast<size_t>(index)].name, focused ? kText : kSecondaryText, cardW - 8.0f);
                x += cardW + gap;
            }
            return;
        }

        renderer_.text(72.0f, top, 3.05f, title, homeState_.row() == row ? kText : kSecondaryText, 900.0f);
        const int start = std::max(0, selected - 1);
        constexpr float cardH = 202.0f;
        constexpr float cardW = 350.0f;
        constexpr float gap = 24.0f;
        const float imageY = top + 70.0f;
        float x = 54.0f;

        auto singleLine = [&](std::string value, float scale, float width) {
            if (renderer_.textWidth(scale, value) <= width) return value;
            while (value.size() > 4 && renderer_.textWidth(scale, value + "...") > width) value.pop_back();
            return value + "...";
        };

        for (int index = start; index < static_cast<int>(items.size()); ++index) {
            if (x + cardW > 1908.0f && index > start) break;
            const auto& item = items[static_cast<size_t>(index)];
            const bool focused = homeState_.row() == row && index == selected;
            const auto bounds = focusedBounds(x, imageY, cardW, cardH, focused, 1.045f);
            if (focused) renderer_.roundedRect(bounds[0] - 10.0f, bounds[1] - 10.0f, bounds[2] + 20.0f, bounds[3] + 20.0f, 22.0f,
                Color{kFocus.r, kFocus.g, kFocus.b, 0.10f});
            const bool hasArtwork = drawHomeArtwork(item, bounds[0], bounds[1], bounds[2], bounds[3]);
            if (!hasArtwork) renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 12.0f, Color{0.055f, 0.06f, 0.075f, 0.94f});
            if (item.positionTicks > 0 && item.runtimeTicks > 0) {
                const double progress = std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
                renderer_.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f, bounds[2] - 16.0f, 4.0f, 2.0f, Color{0.05f, 0.05f, 0.06f, 0.72f});
                renderer_.roundedRect(bounds[0] + 8.0f, bounds[1] + bounds[3] - 10.0f,
                    static_cast<float>((bounds[2] - 16.0f) * progress), 4.0f, 2.0f, kFocus);
            }
            if (focused) renderer_.roundedOutline(bounds[0] - 2.0f, bounds[1] - 2.0f, bounds[2] + 4.0f, bounds[3] + 4.0f, 15.0f, 2.5f, kFocus);

            std::string primary = item.type == "Episode" && !item.seriesName.empty() ? item.seriesName : item.name;
            primary = singleLine(primary, 2.45f, cardW - 18.0f);
            const float titleY = imageY + cardH + 10.0f;
            renderer_.text(x + 2.0f, titleY, 2.45f, primary, focused ? kText : kSecondaryText, cardW - 4.0f);
            if (item.type == "Episode") {
                std::string episode = episodeNumberLabel(item);
                if (!item.name.empty() && item.name != item.seriesName) {
                    if (!episode.empty()) episode += "  |  ";
                    episode += item.name;
                }
                if (!episode.empty()) {
                    renderer_.text(x + 2.0f, titleY + 48.0f, 1.58f,
                        singleLine(episode, 1.58f, cardW - 4.0f), kMuted, cardW - 4.0f);
                }
            }
            x += cardW + gap;
        }
    }


    void renderMediaArtworkCard(
        const JellyfinItem& item,
        float x,
        float y,
        float slotWidth,
        bool focused,
        bool showState = true,
        bool preferSeriesCover = false
    ) {
        const bool seriesCoverForEpisode = preferSeriesCover
            && item.type == "Episode"
            && !item.seriesId.empty()
            && !item.seriesPrimaryImageTag.empty();
        const bool landscape = usesLandscapeMediaCard(item.type) && !seriesCoverForEpisode;
        const float imageWidth = landscape ? slotWidth : 232.0f;
        const float imageHeight = landscape ? 180.0f : 348.0f;
        const float imageX = x + (slotWidth - imageWidth) * 0.5f;
        const auto bounds = focusedBounds(imageX, y, imageWidth, imageHeight, focused, 1.075f);
        if (focused) {
            renderer_.roundedRect(bounds[0] - 14.0f, bounds[1] - 14.0f, bounds[2] + 28.0f, bounds[3] + 28.0f, 26.0f,
                Color{kFocus.r, kFocus.g, kFocus.b, 0.09f});
        }
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 16.0f, kPanelAlt);
        JellyfinItem cover = item;
        if (seriesCoverForEpisode) {
            cover.id = item.seriesId;
            cover.imageTag = item.seriesPrimaryImageTag;
            cover.type = "Series";
        }
        const bool hasArtwork = drawArtwork(cover, bounds[0], bounds[1], bounds[2], bounds[3]);
        if (!hasArtwork) {
            renderer_.roundedRect(bounds[0] + 1.0f, bounds[1] + 1.0f, bounds[2] - 2.0f, bounds[3] - 2.0f, 15.0f, kPanel);
            if (landscape) {
                renderer_.textCentered(bounds[0] + 24.0f, bounds[1] + 24.0f, bounds[2] - 48.0f, bounds[3] - 48.0f,
                    2.0f, fitTextLines(item.name, 2.0f, bounds[2] - 48.0f, 1), kMuted);
            }
        }
        if (item.positionTicks > 0 && item.runtimeTicks > 0) {
            const double fraction = std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
            renderer_.roundedRect(bounds[0] + 10.0f, bounds[1] + bounds[3] - 14.0f, bounds[2] - 20.0f, 5.0f, 2.5f,
                Color{0.08f, 0.09f, 0.12f, 0.94f});
            renderer_.roundedRect(bounds[0] + 10.0f, bounds[1] + bounds[3] - 14.0f,
                static_cast<float>((bounds[2] - 20.0f) * fraction), 5.0f, 2.5f, kFocus);
        }
        if (showState && (item.favorite || (settings_.showWatchedIndicators && item.played))) {
            const std::string label = item.favorite ? "FAVORITE" : "WATCHED";
            const float badgeWidth = item.favorite ? 132.0f : 118.0f;
            const float badgeX = bounds[0] + bounds[2] - badgeWidth - 12.0f;
            const float badgeY = bounds[1] + 12.0f;
            renderer_.roundedRect(badgeX, badgeY, badgeWidth, 34.0f, 17.0f, Color{0.02f, 0.04f, 0.07f, 0.88f});
            renderer_.textCentered(badgeX, badgeY, badgeWidth, 34.0f, 1.12f, label, item.favorite ? kFocus : kSecondaryText);
        }
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, 16.0f);

        const float titleY = y + imageHeight + 18.0f;
        renderer_.text(x + 2.0f, titleY, 2.05f, fitTextLines(item.name, 2.05f, slotWidth - 4.0f, 1), kText, slotWidth - 4.0f);
        const std::string secondary = episodeLabel(item);
        if (!secondary.empty()) renderer_.text(x + 2.0f, titleY + 46.0f, 1.45f, secondary, kMuted, slotWidth - 4.0f);
    }

    void renderTextTile(const JellyfinItem& item, float x, float y, float width, float height, bool focused) {
        const auto bounds = focusedBounds(x, y, width, height, focused, 1.055f);
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 22.0f, focused ? kPanelElevated : kPanel);
        renderer_.textCentered(bounds[0] + 28.0f, bounds[1] + 18.0f, bounds[2] - 56.0f, 42.0f, 1.25f, item.type, kTertiary);
        renderer_.textCentered(bounds[0] + 28.0f, bounds[1] + 58.0f, bounds[2] - 56.0f, bounds[3] - 76.0f,
            2.55f, item.name, kText);
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, 22.0f);
    }

    void renderBrowse() {
        renderHeader(browseState_.heading());

        if (browseState_.hasFilterBar()) {
            const auto labels = browseState_.filterLabels();
            float x = 88.0f;
            for (size_t index = 0; index < labels.size(); ++index) {
                const float width = labels[index] == "COLLECTIONS" ? 235.0f : 176.0f;
                const bool focused = browseState_.filterFocused() && static_cast<int>(index) == browseState_.filterSelection();
                const bool active = static_cast<int>(index) == browseState_.filterSelection();
                if (focused) {
                    const auto bounds = drawFocusedSurface(x, 160.0f, width, 58.0f, true, false);
                    renderer_.textCentered(bounds[0], bounds[1], bounds[2], bounds[3], 1.65f, labels[index], kText);
                } else {
                    renderer_.textCentered(x, 160.0f, width, 58.0f, 1.65f, labels[index], active ? kText : kMuted);
                }
                if (active && !focused) renderer_.roundedRect(x + 18.0f, 211.0f, width - 36.0f, 4.0f, 2.0f, kFocus);
                x += width + 10.0f;
            }
        }

        const auto& items = browseState_.items();
        if (items.empty()) {
            renderer_.text(690.0f, 450.0f, 3.5f, loading_ ? "LOADING..." : "NOTHING HERE", kText, 600.0f);
            renderer_.text(690.0f, 520.0f, 1.8f, loading_ ? "FETCHING YOUR JELLYFIN LIBRARY" : "TRY ANOTHER FILTER", kMuted, 600.0f);
            return;
        }

        constexpr int columns = mediaGridColumns();
        constexpr float slotWidth = mediaCardWidth();
        constexpr float xGap = 32.0f;
        const bool syntheticPage = browseState_.syntheticPage();
        const float rowStep = syntheticPage ? 190.0f : 430.0f;
        const int visibleRows = syntheticPage ? 4 : 2;
        const int selectedRow = browseState_.selection() / columns;
        const int firstRow = std::max(0, selectedRow - 1);
        for (int index = firstRow * columns; index < static_cast<int>(items.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= visibleRows) break;
            const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
            const float y = 250.0f + static_cast<float>(row) * rowStep;
            const bool focused = !browseState_.filterFocused() && index == browseState_.selection();
            const auto& item = items[static_cast<size_t>(index)];
            if (syntheticPage) renderTextTile(item, x, y, slotWidth, 160.0f, focused);
            else {
                const bool showState = item.type != "BoxSet" && item.type != "CollectionFolder";
                renderMediaArtworkCard(item, x, y, slotWidth, focused, showState);
            }
        }
    }

    void renderSearch() {
        const auto& results = searchState_.results();
        const auto& query = searchState_.query();
        renderer_.text(72.0f, 58.0f, 4.0f, "SEARCH", kText, 520.0f);
        constexpr float searchTop = 145.0f;
        constexpr float searchWidth = 1450.0f;
        renderer_.roundedRect(72.0f, searchTop, searchWidth, 68.0f, 22.0f, Color{0.035f, 0.04f, 0.052f, 0.90f});
        if (!searchState_.keyboard() && results.empty()) {
            renderer_.roundedOutline(70.0f, searchTop - 2.0f, searchWidth + 4.0f, 72.0f, 24.0f, 2.5f, kFocus);
        }
        renderer_.textVerticallyCentered(106.0f, searchTop, 68.0f, 2.15f,
            query.empty() ? "Movies, shows and episodes" : query,
            query.empty() ? kMuted : kText, searchWidth - 68.0f);
        renderer_.text(1575.0f, searchTop + 22.0f, 1.45f,
            searchState_.keyboard() ? "FALLBACK KEYS" : "OK TO TYPE", searchState_.keyboard() ? kFocus : kSecondaryText, 250.0f);

        if (searchState_.keyboard()) {
            renderKeyboard(270.0f);
            renderer_.text(650.0f, 900.0f, 1.38f, "DONE RUNS SEARCH   |   BACK CLOSES KEYS", kMuted, 680.0f);
            return;
        }

        if (results.empty()) {
            renderer_.text(72.0f, 300.0f, 2.15f, searchState_.loading() ? "SEARCHING..." : "TYPE A TITLE, ACTOR OR EPISODE", kSecondaryText, 780.0f);
            renderer_.text(72.0f, 350.0f, 1.45f, "Use the TV keyboard or press SEARCH from anywhere in sloppaTV.", kMuted, 900.0f);
            return;
        }

        renderer_.text(72.0f, 270.0f, 1.75f, "RESULTS", kSecondaryText, 300.0f);
        constexpr int columns = mediaGridColumns();
        constexpr float slotWidth = mediaCardWidth();
        constexpr float xGap = 32.0f;
        auto rowHasPortrait = [&](int row) {
            const int begin = row * columns;
            const int end = std::min(begin + columns, static_cast<int>(results.size()));
            for (int index = begin; index < end; ++index) {
                const auto& item = results[static_cast<size_t>(index)];
                const bool seriesCoverForEpisode = item.type == "Episode"
                    && !item.seriesId.empty()
                    && !item.seriesPrimaryImageTag.empty();
                if (!usesLandscapeMediaCard(item.type) || seriesCoverForEpisode) return true;
            }
            return false;
        };
        const int selectedRow = searchState_.selection() / columns;
        const int firstRow = selectedRow > 0 && rowHasPortrait(selectedRow) ? selectedRow : std::max(0, selectedRow - 1);
        float y = 315.0f;
        for (int visibleRow = 0, absoluteRow = firstRow; visibleRow < 2; ++visibleRow, ++absoluteRow) {
            const int begin = absoluteRow * columns;
            if (begin >= static_cast<int>(results.size())) break;
            const bool portraitRow = rowHasPortrait(absoluteRow);
            const float rowHeight = searchMediaRowHeight(portraitRow);
            if (visibleRow > 0 && y + rowHeight > 1060.0f) break;
            const int end = std::min(begin + columns, static_cast<int>(results.size()));
            for (int index = begin; index < end; ++index) {
                const int col = index % columns;
                const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
                renderMediaArtworkCard(results[static_cast<size_t>(index)], x, y, slotWidth, index == searchState_.selection(), true, true);
            }
            y += rowHeight;
        }
    }

    void renderPlayer() {
        const PlayerStatus status = player_.status();
        std::string videoError;
        if (videoSurface_.ready()) {
            videoSurface_.update(videoError);
            float videoX = 0.0f;
            float videoY = 0.0f;
            float videoW = Renderer::logicalWidth();
            float videoH = Renderer::logicalHeight();
            const int sourceWidth = player_.videoWidth();
            const int sourceHeight = player_.videoHeight();
            if (sourceWidth > 0 && sourceHeight > 0 && playbackSessionState_.zoomMode() != VideoZoomMode::Stretch) {
                const float widthScale = Renderer::logicalWidth() / static_cast<float>(sourceWidth);
                const float heightScale = Renderer::logicalHeight() / static_cast<float>(sourceHeight);
                const float scale = playbackSessionState_.zoomMode() == VideoZoomMode::Fit
                    ? std::min(widthScale, heightScale)
                    : std::max(widthScale, heightScale);
                videoW = static_cast<float>(sourceWidth) * scale;
                videoH = static_cast<float>(sourceHeight) * scale;
                videoX = (Renderer::logicalWidth() - videoW) * 0.5f;
                videoY = (Renderer::logicalHeight() - videoH) * 0.5f;
            }
            renderer_.externalImage(
                videoSurface_.texture(),
                videoX,
                videoY,
                videoW,
                videoH,
                videoSurface_.transform()
            );
        }

        const auto now = std::chrono::steady_clock::now();
        const int remainingMs = playerScreenState_.durationMs() > 0
            ? std::max(0, playerScreenState_.durationMs() - playerScreenState_.positionMs())
            : 0;
        const bool showNextUp = continuationState_.nextItem().has_value() && remainingMs > 0 && remainingMs <= 30000;
        const auto skipSegment = activeSkippableSegment();
        const bool showOverlay = status == PlayerStatus::Preparing
            || status == PlayerStatus::Paused
            || transitionState_.loading()
            || transitionState_.fallbackResolving()
            || showNextUp
            || playerScreenState_.overlayVisible(now);
        if (const SubtitleCue* cue = activeSubtitleCue()) {
            const std::string subtitle = wrapText(cue->text, 46, 3);
            const float textScale = subtitleTextScale(settings_.subtitleSize);
            const float lineHeight = 11.0f * textScale * uiTextScale(settings_.uiTextSize);
            std::istringstream stream(subtitle);
            std::vector<std::string> lines;
            std::string line;
            float widest = 0.0f;
            while (std::getline(stream, line)) {
                if (line.empty()) continue;
                widest = std::max(widest, renderer_.textWidth(textScale, line));
                lines.push_back(line);
            }
            if (lines.empty()) lines.push_back(subtitle);
            const float horizontalPadding = 32.0f;
            const float verticalPadding = 20.0f;
            const float boxWidth = std::clamp(widest + horizontalPadding * 2.0f, 320.0f, 1520.0f);
            const float boxHeight = verticalPadding * 2.0f + lineHeight * static_cast<float>(lines.size());
            const float boxX = (Renderer::logicalWidth() - boxWidth) * 0.5f;
            const float bottomY = subtitleBottomY(showOverlay, settings_.subtitlePosition);
            const float boxY = bottomY - boxHeight;
            if (settings_.subtitleBackground) {
                renderer_.roundedRect(boxX, boxY, boxWidth, boxHeight, 22.0f, Color{0.0f, 0.0f, 0.0f, 0.80f});
            }
            for (size_t i = 0; i < lines.size(); ++i) {
                const float width = renderer_.textWidth(textScale, lines[i]);
                const float textX = (Renderer::logicalWidth() - width) * 0.5f;
                const float textY = boxY + verticalPadding + static_cast<float>(i) * lineHeight;
                renderer_.text(textX + 2.0f, textY + 2.0f, textScale, lines[i], Color{0.0f, 0.0f, 0.0f, 0.74f}, widest);
                renderer_.text(textX, textY, textScale, lines[i], kText, widest);
            }
        }
        if (skipSegment) {
            renderer_.roundedRect(1460.0f, 640.0f, 360.0f, 86.0f, 24.0f, Color{0.10f, 0.07f, 0.16f, 0.90f});
            drawFocusHalo(1460.0f, 640.0f, 360.0f, 86.0f, kFocus, 24.0f);
            const std::string skipLabel = mediaSegmentSkipLabel(*skipSegment);
            renderer_.textCentered(1460.0f, 644.0f, 360.0f, 38.0f, 2.25f, skipLabel, kText);
            renderer_.textCentered(1460.0f, 682.0f, 360.0f, 36.0f, 1.70f, "OK TO SKIP", kMuted);
        }
        if (!showOverlay) return;

        renderer_.verticalGradient(0.0f, 0.0f, 1920.0f, 210.0f,
            Color{0.0f, 0.0f, 0.0f, 0.74f},
            Color{0.0f, 0.0f, 0.0f, 0.0f});
        renderer_.verticalGradient(0.0f, 650.0f, 1920.0f, 430.0f,
            Color{0.0f, 0.0f, 0.0f, 0.0f},
            Color{0.0f, 0.0f, 0.0f, 0.90f});

        if (showNextUp && continuationState_.nextItem()) {
            const auto& nextItem = *continuationState_.nextItem();
            renderer_.roundedRect(1195.0f, 185.0f, 625.0f, 205.0f, 26.0f, Color{0.02f, 0.024f, 0.034f, 0.90f});
            const bool hasNextArtwork = drawHomeArtwork(nextItem, 1210.0f, 200.0f, 260.0f, 146.0f);
            const float textX = hasNextArtwork ? 1500.0f : 1230.0f;
            renderer_.text(textX, 205.0f, 1.65f, "NEXT UP  |  " + std::to_string(std::max(0, remainingMs / 1000)) + "S", kFocus, 285.0f);
            renderer_.text(textX, 250.0f, 2.15f, nextItem.name, kText, 285.0f);
            const std::string nextLabel = episodeLabel(nextItem);
            if (!nextLabel.empty()) renderer_.text(textX, 315.0f, 1.5f, nextLabel, kMuted, 285.0f);
        }

        const std::string heading = activePlaybackItem_.seriesName.empty()
            ? activePlaybackItem_.name
            : activePlaybackItem_.seriesName;
        renderer_.text(76.0f, 34.0f, 4.8f, heading.empty() ? "PLAYBACK" : heading, kText, 1540.0f);
        const std::string playerEpisodeNumber = episodeNumberLabel(activePlaybackItem_);
        const std::string secondary = activePlaybackItem_.seriesName.empty()
            ? episodeLabel(activePlaybackItem_)
            : playerEpisodeNumber + (activePlaybackItem_.name.empty() ? "" : "  |  " + activePlaybackItem_.name);
        if (!secondary.empty() && secondary != heading) renderer_.text(80.0f, 116.0f, 2.6f, secondary, kMuted, 1500.0f);

        const int position = playerScreenState_.positionMs();
        const int duration = playerScreenState_.durationMs();
        const std::string state = transitionState_.fallbackResolving() ? "RETRYING TRANSCODE" :
            (transitionState_.loading() ? "LOADING NEXT EPISODE" :
            (status == PlayerStatus::Paused ? "PAUSED" : (status == PlayerStatus::Preparing ? "LOADING" : "PLAYING")));
        renderer_.text(80.0f, 772.0f, 2.0f, state, kSecondaryText, 580.0f);

        constexpr float progressX = 150.0f;
        constexpr float progressWidth = 1620.0f;
        renderer_.text(progressX, 834.0f, 2.0f, formatPlaybackTime(position), kText, 180.0f);
        const std::string durationText = formatPlaybackTime(duration);
        renderer_.text(1610.0f, 834.0f, 2.0f, durationText, kText, 180.0f);
        renderer_.roundedRect(progressX, 878.0f, progressWidth, 7.0f, 3.5f, Color{0.30f, 0.31f, 0.35f, 0.78f});
        if (duration > 0) {
            const double progress = std::clamp(static_cast<double>(position) / static_cast<double>(duration), 0.0, 1.0);
            renderer_.roundedRect(progressX, 878.0f, static_cast<float>(progressWidth * progress), 7.0f, 3.5f, kFocus);
        }
        drawTrickplayPreview();

        if (playerScreenState_.controlsActive()) {
            const std::array<std::string, 3> controls{
                status == PlayerStatus::Paused ? "PLAY" : "PAUSE",
                "AUDIO  " + playerTrackLabel(2),
                "SUBTITLES  " + playerTrackLabel(4),
            };
            const std::array<float, 3> widths{190.0f, 310.0f, 350.0f};
            float x = 535.0f;
            for (size_t i = 0; i < controls.size(); ++i) {
                const bool selected = static_cast<int>(i) == playerScreenState_.controlSelection();
                if (i == 0) {
                    const auto bounds = focusedBounds(x, 925.0f, widths[i], 66.0f, selected, 1.06f);
                    renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 28.0f,
                        selected ? Color{0.50f, 0.27f, 0.91f, 0.98f} : Color{0.36f, 0.20f, 0.68f, 0.90f});
                    renderer_.textCentered(bounds[0], bounds[1], bounds[2], bounds[3], 2.05f, controls[i], kText);
                } else {
                    renderer_.text(x + 14.0f, 943.0f, 1.95f, controls[i], selected ? kText : kSecondaryText, widths[i] - 28.0f);
                    if (selected) renderer_.roundedRect(x + 14.0f, 988.0f, 62.0f, 3.0f, 1.5f, kFocus);
                }
                x += widths[i] + 28.0f;
            }
        } else {
            const std::string queueHint = queueState_.empty() ? "" : "   |   DOWN QUEUE";
            renderer_.text(
                330.0f,
                946.0f,
                1.75f,
                "LEFT/RIGHT SEEK   |   OK PLAY/PAUSE   |   UP OPTIONS" + queueHint + "   |   BACK EXIT",
                kMuted,
                1280.0f
            );
        }
    }

    void renderQueueOverlay() {
        if (queueState_.empty()) return;
        const int size = queueState_.size();
        const int current = std::clamp(queueState_.currentIndex(), 0, size - 1);
        queueState_.setSelection(queueState_.selection());
        const int selection = queueState_.selection();

        renderer_.rect(0.0f, 0.0f, 1920.0f, 1080.0f, Color{0.0f, 0.0f, 0.0f, 0.28f});
        renderer_.roundedRect(790.0f, 28.0f, 1090.0f, 1020.0f, 34.0f, Color{0.012f, 0.015f, 0.022f, 0.96f});
        renderer_.text(842.0f, 72.0f, 3.35f, "PLAYBACK QUEUE", kText, 620.0f);
        renderer_.roundedRect(1555.0f, 70.0f, 255.0f, 46.0f, 18.0f, kPanelAlt);
        renderer_.textCentered(1555.0f, 70.0f, 255.0f, 46.0f, 1.35f, std::to_string(size - current) + " REMAINING", kMuted);

        constexpr int visibleRows = 5;
        const int first = std::clamp(selection - 2, current, std::max(current, size - visibleRows));
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int index = first + slot;
            if (index >= size) break;
            const float y = 145.0f + static_cast<float>(slot) * 108.0f;
            const bool selected = index == selection;
            const bool isCurrent = index == current;
            const auto& item = queueState_.items()[static_cast<size_t>(index)];
            const auto bounds = focusedBounds(830.0f, y, 990.0f, 90.0f, selected, 1.018f);
            renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 20.0f, selected ? kPanelElevated : Color{0.05f, 0.055f, 0.070f, 0.78f});
            drawHomeArtwork(item, bounds[0] + 12.0f, bounds[1] + 10.0f, 124.0f, 70.0f);
            const std::string marker = isCurrent ? "CURRENT" : (index == current + 1 ? "NEXT" : std::to_string(index - current + 1));
            const float markerWidth = isCurrent ? 122.0f : (index == current + 1 ? 88.0f : 58.0f);
            renderer_.roundedRect(bounds[0] + 154.0f, bounds[1] + 24.0f, markerWidth, 40.0f, 16.0f, isCurrent ? kFocusSoft : kPanelAlt);
            renderer_.textCentered(bounds[0] + 154.0f, bounds[1] + 24.0f, markerWidth, 40.0f, 1.20f, marker,
                isCurrent ? kText : kMuted);
            renderer_.text(bounds[0] + 300.0f, bounds[1] + 18.0f, 1.95f, item.name, kText, 610.0f);
            const std::string secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(bounds[0] + 300.0f, bounds[1] + 54.0f, 1.35f, secondary, kMuted, 610.0f);
            if (selected) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], kFocus, 20.0f);
        }

        const std::array<std::string, 7> actions{
            "PLAY NOW",
            "PLAY NEXT",
            "MOVE UP",
            "MOVE DOWN",
            "REMOVE",
            "SHUFFLE",
            std::string("REPEAT ") + queueRepeatModeName(queueState_.repeatMode()),
        };
        auto enabled = [&](int action) {
            if (action == 0) return queueCanPlayNow(selection, current, size);
            if (action == 1) return queueCanPlayNext(selection, current, size);
            if (action == 2) return queueCanMoveUp(selection, current, size);
            if (action == 3) return queueCanMoveDown(selection, current, size);
            if (action == 4) return queueCanRemove(selection, current, size);
            if (action == 5) return queueCanShuffle(current, size);
            return true;
        };
        for (size_t i = 0; i < actions.size(); ++i) {
            const bool firstActionRow = i < 4;
            const int column = firstActionRow ? static_cast<int>(i) : static_cast<int>(i) - 4;
            const float width = firstActionRow ? 230.0f : 310.0f;
            const float x = 835.0f + static_cast<float>(column) * (width + 16.0f);
            const float y = firstActionRow ? 715.0f : 805.0f;
            const bool focused = queueState_.actionSelection() == static_cast<int>(i);
            const bool available = enabled(static_cast<int>(i));
            std::array<float, 4> actionBounds{x, y, width, 68.0f};
            if (available) actionBounds = drawFocusedSurface(x, y, width, 68.0f, focused, i < 2, i == 4);
            else renderer_.roundedRect(x, y, width, 68.0f, 18.0f, Color{0.06f, 0.065f, 0.075f, 0.72f});
            renderer_.textCentered(actionBounds[0], actionBounds[1], actionBounds[2], actionBounds[3], 1.45f, actions[i],
                available ? kText : kTertiary);
        }
        renderer_.text(905.0f, 918.0f, 1.35f, "UP / DOWN  SELECT ITEM   |   LEFT / RIGHT  CHOOSE ACTION   |   OK  APPLY", kMuted, 850.0f);
        renderer_.text(1250.0f, 970.0f, 1.25f, "BACK  CLOSE QUEUE", kTertiary, 320.0f);
    }

    void renderScreensaver() {
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), Color{0.006f, 0.008f, 0.012f, 1.0f});
        const int64_t elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        static constexpr std::array<std::array<float, 2>, 8> positions{{
            {{170.0f, 170.0f}},
            {{1120.0f, 170.0f}},
            {{170.0f, 675.0f}},
            {{1120.0f, 675.0f}},
            {{650.0f, 245.0f}},
            {{650.0f, 635.0f}},
            {{340.0f, 410.0f}},
            {{980.0f, 410.0f}},
        }};
        const auto& position = positions[static_cast<size_t>(screensaverPositionSlot(elapsedSeconds))];

        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        char clock[16]{};
        std::strftime(clock, sizeof(clock), "%H:%M", &local);

        renderer_.text(position[0], position[1], 4.2f, "SLOPPATV", Color{0.82f, 0.78f, 1.0f, 0.92f}, 600.0f);
        renderer_.text(position[0], position[1] + 88.0f, 9.0f, clock, kText, 650.0f);
        renderer_.text(735.0f, 1020.0f, 1.45f, "PRESS ANY BUTTON TO RETURN", kTertiary, 520.0f);
    }

    void renderSettings() {
        renderer_.text(72.0f, 58.0f, 4.0f, "SETTINGS", kText, 560.0f);
        const auto& labels = settingsLabels();
        const auto values = settingsValues(
            settings_,
            api_.deviceCodecSupport().maxAudioOutputChannels,
            externalPlayerLabel(),
            session_.username,
            settingsScreen_.advanced()
        );

        renderer_.roundedRect(1070.0f, 52.0f, 760.0f, 58.0f, 20.0f, Color{0.035f, 0.04f, 0.052f, 0.88f});
        if (settingsScreen_.searchFocused()) renderer_.roundedOutline(1068.0f, 50.0f, 764.0f, 62.0f, 22.0f, 2.5f, kFocus);
        renderer_.textVerticallyCentered(1102.0f, 52.0f, 58.0f, 2.20f,
            settingsScreen_.searchQuery().empty() ? "Search settings" : settingsScreen_.searchQuery(),
            settingsScreen_.searchQuery().empty() ? kMuted : kText, 570.0f);
        renderer_.textCentered(1640.0f, 52.0f, 170.0f, 58.0f, 1.60f, "SEARCH", settingsScreen_.searchFocused() ? kFocus : kMuted);

        const auto matches = settingsScreen_.matches();
        if (matches.empty()) {
            renderer_.text(610.0f, 445.0f, 3.2f, "NO SETTINGS MATCH", kText, 700.0f);
            renderer_.text(570.0f, 510.0f, 1.75f, "PRESS OK OR SEARCH TO EDIT THE FILTER", kMuted, 820.0f);
            return;
        }

        renderer_.text(120.0f, 165.0f, 2.10f,
            settingsScreen_.advanced() ? "ADVANCED / TECHNICAL" : "COMMON SETTINGS", kSecondaryText, 620.0f);
        renderer_.text(120.0f, 205.0f, 1.40f,
            settingsScreen_.advanced()
                ? "Codec overrides, compatibility and device-level controls"
                : "The settings you are most likely to change",
            kMuted,
            920.0f);

        constexpr int visibleRows = 6;
        const int maxFirst = std::max(0, static_cast<int>(matches.size()) - visibleRows);
        const int first = std::clamp(settingsScreen_.firstVisible(), 0, maxFirst);
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int matchPosition = first + slot;
            if (matchPosition >= static_cast<int>(matches.size())) break;
            const int i = matches[static_cast<size_t>(matchPosition)];
            const float y = 270.0f + static_cast<float>(slot) * 112.0f;
            const bool focused = !settingsScreen_.searchFocused() && i == settingsScreen_.selection();
            const bool actionRow = i == 22 || i == 23 || i == kAdvancedSettingsToggle;
            if (focused) {
                renderer_.roundedRect(110.0f, y - 8.0f, 1700.0f, 88.0f, 22.0f, Color{0.07f, 0.065f, 0.09f, 0.72f});
                renderer_.roundedRect(110.0f, y + 77.0f, 64.0f, 3.0f, 1.5f, kFocus);
            } else {
                renderer_.rect(132.0f, y + 82.0f, 1650.0f, 1.0f, Color{0.25f, 0.27f, 0.32f, 0.14f});
            }
            const std::string rowLabel = i == kAdvancedSettingsToggle && settingsScreen_.advanced()
                ? "BASIC SETTINGS"
                : labels[static_cast<size_t>(i)];
            renderer_.textVerticallyCentered(145.0f, y - 8.0f, 88.0f, 2.20f, rowLabel,
                focused ? kText : kSecondaryText, 900.0f);
            const float valueScale = actionRow ? 1.70f : 1.95f;
            const float valueWidth = renderer_.textWidth(valueScale, values[static_cast<size_t>(i)]);
            renderer_.textVerticallyCentered(std::max(1190.0f, 1760.0f - valueWidth), y - 8.0f, 88.0f, valueScale,
                values[static_cast<size_t>(i)], actionRow ? (focused ? kFocus : kText) : (focused ? kFocus : kMuted), 570.0f);
        }
        renderer_.text(470.0f, 985.0f, 1.52f,
            "LEFT / RIGHT CHANGE   |   UP TO SEARCH   |   CHANGES SAVE IMMEDIATELY", kTertiary, 1120.0f);
    }

    void renderDiagnostics() {
        renderHeader("DIAGNOSTICS");
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

        std::vector<std::pair<std::string, std::string>> rows{
            {"APP VERSION", SLOPPATV_VERSION_NAME},
            {"ABI", architecture},
            {"SERVER", serverInfo_.name.empty() ? session_.server : serverInfo_.name},
            {"JELLYFIN VERSION", serverInfo_.version.empty() ? (loading_ ? "LOADING..." : "UNKNOWN") : serverInfo_.version},
            {"VIDEO DECODERS", videoCodecs.empty() ? "NONE DETECTED" : joinGenres(videoCodecs, videoCodecs.size())},
            {"AUDIO DIRECT", audioCodecs.empty() ? "AAC TRANSCODE FALLBACK" : joinGenres(audioCodecs, audioCodecs.size())},
            {"AUDIO OUTPUT", std::to_string(codecs.maxAudioOutputChannels) + " CHANNELS"},
            {"HEVC MAX", codecs.maxHevcWidth > 0 ? std::to_string(codecs.maxHevcWidth) + "X" + std::to_string(codecs.maxHevcHeight) : "UNKNOWN"},
            {"HDR DISPLAY", hdr.empty() ? "SDR / NONE DETECTED" : joinGenres(hdr, hdr.size())},
            {"LAST PLAYBACK", lastPlaybackSummary_.empty() ? "NOT YET PLAYED THIS SESSION" : lastPlaybackSummary_},
        };
        auto renderPanel = [&](float x, float y, float width, float height, const std::string& title, std::initializer_list<int> indices) {
            renderer_.roundedRect(x, y, width, height, 26.0f, Color{0.035f, 0.041f, 0.055f, 0.90f});
            renderer_.text(x + 28.0f, y + 25.0f, 2.35f, title, kText, width - 56.0f);
            float rowY = y + 82.0f;
            for (const int index : indices) {
                if (index < 0 || index >= static_cast<int>(rows.size())) continue;
                renderer_.text(x + 28.0f, rowY, 1.4f, rows[static_cast<size_t>(index)].first, kTertiary, 260.0f);
                renderer_.text(x + 300.0f, rowY - 2.0f, 1.7f, rows[static_cast<size_t>(index)].second, kText, width - 330.0f);
                rowY += 58.0f;
            }
        };

        renderPanel(85.0f, 175.0f, 840.0f, 300.0f, "APP & SERVER", {0, 1, 2, 3});
        renderPanel(995.0f, 175.0f, 840.0f, 300.0f, "VIDEO & DISPLAY", {4, 7, 8});
        renderPanel(85.0f, 515.0f, 840.0f, 250.0f, "AUDIO", {5, 6});
        renderPanel(995.0f, 515.0f, 840.0f, 250.0f, "LAST PLAYBACK", {9});
        renderer_.text(690.0f, 965.0f, 1.75f, "BACK OR OK  |  RETURN TO SETTINGS", kMuted, 620.0f);
    }

    void renderItemMenu() {
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), Color{0.0f, 0.0f, 0.0f, 0.46f});

        if (detailsState_.deleteConfirmation()) {
            renderer_.roundedRect(405.0f, 275.0f, 1110.0f, 520.0f, 34.0f, Color{0.025f, 0.029f, 0.040f, 0.98f});
            renderer_.text(470.0f, 335.0f, 3.25f, "DELETE THIS MEDIA?", kError, 980.0f);
            renderer_.text(
                470.0f,
                415.0f,
                2.0f,
                "JELLYFIN WILL DELETE THIS ITEM AND ITS MEDIA FILES.\nTHIS CANNOT BE UNDONE.",
                kText,
                980.0f
            );

            const std::array<std::string, 2> actions{"DELETE PERMANENTLY", "CANCEL"};
            for (int i = 0; i < 2; ++i) {
                const float x = i == 0 ? 470.0f : 995.0f;
                const bool focused = detailsState_.deleteConfirmationSelection() == i;
                const auto bounds = drawFocusedSurface(x, 610.0f, 450.0f, 92.0f, focused, false, i == 0);
                renderer_.textCentered(bounds[0], bounds[1], bounds[2], bounds[3], i == 0 ? 1.75f : 2.0f,
                    actions[static_cast<size_t>(i)], focused || i == 1 ? kText : kMuted);
            }
            renderer_.text(670.0f, 745.0f, 1.55f, "CANCEL IS SELECTED BY DEFAULT   |   BACK ALSO CANCELS", kTertiary, 660.0f);
            return;
        }

        const auto actions = itemMenuActions();
        constexpr float panelX = 1110.0f;
        constexpr float panelWidth = 700.0f;
        constexpr float rowStep = 66.0f;
        const float panelHeight = 170.0f + static_cast<float>(actions.size()) * rowStep + 46.0f;
        const float panelY = std::max(72.0f, (Renderer::logicalHeight() - panelHeight) * 0.5f);
        renderer_.roundedRect(panelX, panelY, panelWidth, panelHeight, 32.0f, Color{0.025f, 0.029f, 0.040f, 0.98f});
        renderer_.roundedOutline(panelX, panelY, panelWidth, panelHeight, 32.0f, 1.5f, Color{0.30f, 0.32f, 0.40f, 0.42f});
        renderer_.text(panelX + 38.0f, panelY + 30.0f, 2.35f,
            fitTextLines(detail_.name.empty() ? "ITEM" : detail_.name, 2.35f, panelWidth - 76.0f, 1), kText, panelWidth - 76.0f);
        renderer_.text(panelX + 40.0f, panelY + 79.0f, 1.35f,
            detail_.type.empty() ? "MEDIA" : detail_.type, kMuted, panelWidth - 80.0f);
        renderer_.rect(panelX + 34.0f, panelY + 118.0f, panelWidth - 68.0f, 1.0f, Color{0.30f, 0.32f, 0.40f, 0.22f});

        const float firstActionY = panelY + 136.0f;
        for (size_t i = 0; i < actions.size(); ++i) {
            const float y = firstActionY + static_cast<float>(i) * rowStep;
            const bool focused = detailsState_.itemMenuSelection() == static_cast<int>(i);
            const bool destructive = actions[i] == "DELETE MEDIA";
            const bool primary = actions[i] == "PLAY ALL" || actions[i] == "PLAY EXTERNAL" || actions[i] == "VIEW QUEUE";
            const auto bounds = drawFocusedSurface(panelX + 30.0f, y, panelWidth - 60.0f, 52.0f, focused, primary && focused, destructive);
            renderer_.textVerticallyCentered(bounds[0] + 24.0f, bounds[1], bounds[3], 1.70f, actions[i],
                destructive && !focused ? kMuted : kText, bounds[2] - 48.0f);
        }
        renderer_.text(panelX + 170.0f, panelY + panelHeight - 34.0f, 1.30f,
            "OK SELECT   |   BACK CLOSE", kTertiary, 390.0f);
    }

    std::string fitTextLines(const std::string& value, float scale, float maxWidth, int maxLines) const {
        if (value.empty() || maxWidth <= 0.0f || maxLines <= 0) return {};
        auto ellipsize = [&](std::string line) {
            while (!line.empty() && renderer_.textWidth(scale, line + "...") > maxWidth) line.pop_back();
            return line + "...";
        };
        std::istringstream words(value);
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
            if (current.empty()) current = ellipsize(word);
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
        renderHeader(title);
        if (items.empty()) {
            renderer_.text(690.0f, 430.0f, 3.5f, loading_ ? "LOADING..." : "NOTHING HERE", kText, 600.0f);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float slotWidth = mediaCardWidth();
        constexpr float xGap = 32.0f;
        const int selectedRow = selection / columns;
        const int firstRow = std::max(0, selectedRow - 1);
        for (int index = firstRow * columns; index < static_cast<int>(items.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 2) break;
            const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
            const float y = 195.0f + static_cast<float>(row) * 430.0f;
            renderMediaArtworkCard(items[static_cast<size_t>(index)], x, y, slotWidth, index == selection);
        }
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
        const std::string heading = detail_.name.empty() ? "CAST" : detail_.name + " - CAST";
        renderHeader(heading);
        if (detail_.people.empty()) {
            renderer_.text(700.0f, 430.0f, 3.5f, "NO CAST DATA", kMuted, 520.0f);
            return;
        }
        constexpr int columns = 5;
        constexpr float slotWidth = mediaCardWidth();
        constexpr float xGap = 32.0f;
        constexpr float imageWidth = 190.0f;
        constexpr float imageHeight = 285.0f;
        const int selectedRow = detailsState_.castSelection() / columns;
        const int firstRow = std::max(0, selectedRow - 1);
        for (int index = firstRow * columns; index < static_cast<int>(detail_.people.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 2) break;
            const float x = 80.0f + static_cast<float>(col) * (slotWidth + xGap);
            const float y = 195.0f + static_cast<float>(row) * 420.0f;
            const bool focused = index == detailsState_.castSelection();
            const float imageX = x + (slotWidth - imageWidth) * 0.5f;
            const auto bounds = focusedBounds(imageX, y, imageWidth, imageHeight, focused);
            renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 16.0f, focused ? kPanelElevated : kPanelAlt);
            const auto& person = detail_.people[static_cast<size_t>(index)];
            const JellyfinItem artworkItem = personArtworkItem(person);
            drawArtwork(artworkItem, bounds[0], bounds[1], bounds[2], bounds[3]);
            if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3]);
            renderer_.text(x + 4.0f, y + imageHeight + 18.0f, 2.25f, person.name, kText, slotWidth - 8.0f);
            if (!person.role.empty()) renderer_.text(x + 4.0f, y + imageHeight + 64.0f, 1.55f, person.role, kMuted, slotWidth - 8.0f);
        }
        renderer_.text(590.0f, 1015.0f, 1.55f, "OK OPENS TITLES FEATURING THIS PERSON", kTertiary, 780.0f);
    }

    void renderPersonItems() {
        const auto& person = detailsState_.selectedPerson();
        const std::string heading = person.name.empty() ? "PERSON" : "FEATURING " + person.name;
        renderMediaGrid(heading, detailsState_.personItems(), detailsState_.personItemSelection());
    }

    void renderSeasons() {
        const auto& series = detailsState_.seriesDetail();
        renderMediaGrid(series.name.empty() ? "SEASONS" : series.name + " - SEASONS", detailsState_.seasons(), detailsState_.seasonSelection());
    }

    void renderEpisodes() {
        const auto& series = detailsState_.seriesDetail();
        const auto& season = detailsState_.selectedSeason();
        const std::string heading = season.name.empty() ? "EPISODES" : series.name + " - " + season.name;
        renderMediaGrid(heading, detailsState_.episodes(), detailsState_.episodeSelection());
    }

    void renderDetails() {
        const bool backdropVisible = drawBackdrop(detail_, 0.84f);
        if (backdropVisible) {
            renderer_.horizontalGradient(0.0f, 0.0f, 1350.0f, 1080.0f,
                Color{0.0f, 0.0f, 0.0f, 0.92f},
                Color{0.0f, 0.0f, 0.0f, 0.03f});
            renderer_.verticalGradient(0.0f, 0.0f, 1920.0f, 1080.0f,
                Color{0.0f, 0.0f, 0.0f, 0.08f},
                Color{0.0f, 0.0f, 0.0f, 0.92f});
        } else {
            renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), kBackground);
        }

        if (settings_.showClock) {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_r(&now, &local);
            std::ostringstream clock;
            clock << std::put_time(&local, "%H:%M");
            renderer_.text(1710.0f, 50.0f, 2.10f, clock.str(), kMuted, 140.0f);
        }

        if (continuationState_.stillWatchingPrompt()) {
            renderer_.roundedRect(1110.0f, 54.0f, 580.0f, 54.0f, 20.0f, Color{0.12f, 0.08f, 0.18f, 0.90f});
            renderer_.textCentered(1110.0f, 54.0f, 580.0f, 54.0f, 1.95f, "STILL WATCHING?  OK TO CONTINUE", kText);
        }

        constexpr float contentX = 72.0f;
        constexpr float contentWidth = 820.0f;
        const bool episode = detail_.type == "Episode";
        const std::string mainTitle = episode && !detail_.seriesName.empty() ? detail_.seriesName : detail_.name;
        const bool hasLogo = drawLogo(detail_, contentX, 132.0f, 700.0f, 138.0f);
        if (!hasLogo) {
            renderer_.text(contentX, 142.0f, 6.0f,
                fitTextLines(mainTitle.empty() ? "LOADING..." : mainTitle, 6.0f, contentWidth, 1), kText, contentWidth);
        }

        const std::string episodeNumber = episodeNumberLabel(detail_);
        const std::string secondary = episode
            ? (detail_.name.empty() || detail_.name == detail_.seriesName
                ? episodeNumber
                : episodeNumber + (episodeNumber.empty() ? "" : "  |  ") + detail_.name)
            : episodeLabel(detail_);
        if (!secondary.empty()) {
            renderer_.text(contentX, 286.0f, 2.80f,
                fitTextLines(secondary, 2.80f, contentWidth, 1), kSecondaryText, contentWidth);
        }

        std::string metadata;
        auto appendMetadata = [&](const std::string& value) {
            if (value.empty()) return;
            if (!metadata.empty()) metadata += "   |   ";
            metadata += value;
        };
        if (detail_.productionYear > 0) appendMetadata(std::to_string(detail_.productionYear));
        if (!detail_.officialRating.empty()) appendMetadata(detail_.officialRating);
        if (detail_.runtimeTicks > 0) appendMetadata(formatPlaybackTime(static_cast<int>(detail_.runtimeTicks / 10000)));
        if (detail_.communityRating >= 0.0f) {
            std::ostringstream rating;
            rating << std::fixed << std::setprecision(1) << detail_.communityRating << "/10";
            appendMetadata(rating.str());
        }
        const std::string genres = joinGenres(detail_.genres);
        if (!genres.empty()) appendMetadata(genres);
        renderer_.text(contentX, 340.0f, 1.80f,
            fitTextLines(metadata, 1.80f, contentWidth, 1), kMuted, contentWidth);

        if (!detail_.overview.empty()) {
            renderer_.text(contentX, 392.0f, 2.35f,
                fitTextLines(detail_.overview, 2.35f, contentWidth, 3), Color{0.92f, 0.93f, 0.96f, 0.96f}, contentWidth);
        }
        std::string state;
        if (detail_.favorite) state = "FAVORITE";
        if (settings_.showWatchedIndicators && detail_.played) {
            if (!state.empty()) state += "   |   ";
            state += "WATCHED";
        }
        if (!state.empty()) renderer_.text(contentX, 582.0f, 1.70f, state, kFocus, 420.0f);

        const auto actions = detailActions();
        constexpr float actionY = 615.0f;
        float actionX = contentX;
        for (size_t i = 0; i < actions.size(); ++i) {
            const bool primary = i == 0;
            const bool focused = !detailsState_.similarFocused() && detailsState_.actionSelection() == static_cast<int>(i);
            const float width = primary ? 250.0f : std::max(145.0f, renderer_.textWidth(1.80f, actions[i]) + 46.0f);
            if (primary) {
                const auto bounds = focusedBounds(actionX, actionY, width, 64.0f, focused, 1.05f);
                renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 26.0f,
                    focused ? Color{0.50f, 0.27f, 0.91f, 0.98f} : Color{0.42f, 0.23f, 0.78f, 0.94f});
                renderer_.textCentered(bounds[0], bounds[1], bounds[2], bounds[3], 2.05f, actions[i], kText);
                if (focused) renderer_.roundedOutline(bounds[0] - 3.0f, bounds[1] - 3.0f, bounds[2] + 6.0f, bounds[3] + 6.0f, 29.0f, 2.5f, Color{0.82f, 0.68f, 1.0f, 0.95f});
            } else {
                renderer_.text(actionX + 12.0f, actionY + 17.0f, 1.80f, actions[i], focused ? kText : kSecondaryText, width - 24.0f);
                if (focused) renderer_.roundedRect(actionX + 12.0f, actionY + 58.0f, std::min(58.0f, width - 24.0f), 3.0f, 1.5f, kFocus);
            }
            actionX += width + 18.0f;
        }

        if (detail_.positionTicks > 0 && detail_.runtimeTicks > 0) {
            const double fraction = std::clamp(static_cast<double>(detail_.positionTicks) / static_cast<double>(detail_.runtimeTicks), 0.0, 1.0);
            renderer_.roundedRect(contentX, 700.0f, 560.0f, 4.0f, 2.0f, Color{0.45f, 0.46f, 0.50f, 0.45f});
            renderer_.roundedRect(contentX, 700.0f, static_cast<float>(560.0 * fraction), 4.0f, 2.0f, kFocus);
        }

        const auto& similarItems = detailsState_.similar();
        if (!similarItems.empty()) {
            renderer_.text(72.0f, 748.0f, 2.30f, "MORE LIKE THIS", detailsState_.similarFocused() ? kText : kSecondaryText, 440.0f);
            constexpr int visible = 5;
            const int maxStart = std::max(0, static_cast<int>(similarItems.size()) - visible);
            const int start = std::clamp(detailsState_.similarSelection() - 1, 0, maxStart);
            constexpr float cardWidth = 320.0f;
            constexpr float cardHeight = 144.0f;
            constexpr float cardGap = 26.0f;
            for (int slot = 0; slot < visible; ++slot) {
                const int index = start + slot;
                if (index >= static_cast<int>(similarItems.size())) break;
                const auto& similar = similarItems[static_cast<size_t>(index)];
                const float x = 72.0f + static_cast<float>(slot) * (cardWidth + cardGap);
                const float y = 800.0f;
                const bool focused = detailsState_.similarFocused() && index == detailsState_.similarSelection();
                const auto bounds = focusedBounds(x, y, cardWidth, cardHeight, focused, 1.075f);
                if (!drawHomeArtwork(similar, bounds[0], bounds[1], bounds[2], bounds[3])) {
                    renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], 12.0f, kPanelAlt);
                }
                if (focused) renderer_.roundedOutline(bounds[0] - 2.0f, bounds[1] - 2.0f, bounds[2] + 4.0f, bounds[3] + 4.0f, 15.0f, 2.5f, kFocus);
                renderer_.text(x + 2.0f, y + cardHeight + 12.0f, 2.10f, similar.name, focused ? kText : kSecondaryText, cardWidth - 10.0f);
            }
        }
    }

    void renderStatus() {
        if (loading_ || homeLoading_ || mutationLoading_) {
            renderer_.rect(1490, 985, 350, 55, kPanelAlt);
            renderer_.text(1545, 1004, 1.9f, "LOADING...", kText);
        }
        const bool noticeVisible = !notice_.empty()
            && (noticePersistent_ || std::chrono::steady_clock::now() < noticeUntil_);
        if (noticeVisible) {
            const float noticeY = screen_ == Screen::Player ? 670.0f : 925.0f;
            renderer_.roundedRect(70, noticeY, 1760, 58, 18.0f, kPanelAlt);
            renderer_.roundedOutline(70, noticeY, 1760, 58, 18.0f, 2.0f, kFocus);
            renderer_.textVerticallyCentered(94.0f, noticeY, 58.0f, 1.6f, notice_, kText, 1710.0f);
        }
        if (!error_.empty()) {
            renderer_.roundedRect(70, 995, 1360, 55, 16.0f, kError);
            renderer_.textVerticallyCentered(90.0f, 995.0f, 55.0f, 1.7f, error_, kText, 1320.0f);
        }
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
            eraseArtworkEntry(profileArtwork_, profileArtworkKey(session_));
            sessionRegistry_.remember(session_, deviceId_);
        }
        playbackSessionState_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
        accountState_.setAuthenticatedAccount(session_.server, session_.username);
    }

    void saveSession(const JellyfinSession& session) {
        if (session.valid()) {
            eraseArtworkEntry(profileArtwork_, profileArtworkKey(session));
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
    DisplayModeController displayMode_;
    NativeMediaPlayer player_;
    NativeMediaSession mediaSession_;
    NativeExternalPlayer externalPlayer_;
    JniImageDecoder imageDecoder_;
    VideoSurface videoSurface_;
    TaskRunner tasks_;

    mutable std::recursive_mutex stateMutex_;
    RequestEpochs requestEpochs_;
    std::string dataPath_;
    std::string deviceId_;

    Screen screen_ = Screen::Login;
    NavigationStack<Screen> navigation_{Screen::Login};
    bool loading_ = false;
    bool homeLoading_ = false;
    bool mutationLoading_ = false;
    AppSettings settings_;
    SettingsScreenState settingsScreen_;
    std::vector<ExternalPlayerApp> externalPlayers_;
    std::string error_;
    std::string notice_;
    std::chrono::steady_clock::time_point noticeUntil_{};
    bool noticePersistent_ = false;

    JellyfinSession session_;
    std::string pendingDeepLinkItemId_;
    std::string pendingSearchQuery_;
    std::optional<LaunchRequest> pendingRuntimeLaunchRequest_;
    SessionRegistry sessionRegistry_;
    AccountScreenState accountState_;
    JellyfinServerInfo serverInfo_;
    bool serverInfoLoading_ = false;
    JellyfinHomeData home_;
    ArtworkCache artwork_{12};
    ArtworkCache profileArtwork_;
    ArtworkCache homeArtwork_{48};
    HomeImageDiskCache homeDiskCache_;
    ArtworkCache backdrops_{8};
    ArtworkCache logos_{12};
    DecodedImage brandMarkDecoded_;
    GLuint brandMarkTexture_ = 0;
    uint64_t brandMarkTextureGeneration_ = 0;
    HomeScreenState homeState_;
    std::unordered_set<std::string> hiddenHomeItems_;
    BrowseScreenState browseState_;

    int systemTextInputMode_ = -1;

    SearchScreenState searchState_;

    int keyboardRow_ = 0;
    int keyboardCol_ = 0;

    JellyfinItem detail_;
    DetailsScreenState detailsState_;

    PlaybackQueueState queueState_;

    ExternalPlaybackState externalPlaybackState_;
    PlaybackTransitionState transitionState_;
    PlaybackTarget activeTarget_;
    JellyfinItem activePlaybackItem_;
    PlaybackContinuationState continuationState_;
    PlaybackSessionState playbackSessionState_;
    PlaybackTelemetryState telemetryState_;
    PlayerScreenState playerScreenState_;
    PlayerTrackState trackState_;
    TrickplayPreviewState trickplayState_;
    std::chrono::steady_clock::time_point renderBurstUntil_{};
    std::chrono::steady_clock::time_point lastInteraction_ = std::chrono::steady_clock::now();
    bool screensaverActive_ = false;
    std::string lastPlaybackSummary_;
};
}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaNativeActivity_nativeOnNewIntent(
    JNIEnv* env,
    jclass,
    jstring action,
    jstring data,
    jstring query
) {
    const std::string actionValue = jniString(env, action);
    const std::string dataValue = jniString(env, data);
    const std::string queryValue = jniString(env, query);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onNewLaunchIntent(actionValue, dataValue, queryValue);
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaNativeActivity_nativeOnSystemTextInputChanged(
    JNIEnv* env,
    jclass,
    jint mode,
    jstring text
) {
    const std::string value = jniString(env, text);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onSystemTextInputChanged(static_cast<int>(mode), value);
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaNativeActivity_nativeOnSystemTextInputDone(
    JNIEnv* env,
    jclass,
    jint mode,
    jstring text
) {
    const std::string value = jniString(env, text);
    std::scoped_lock lock(gActiveAppMutex);
    if (gActiveApp) gActiveApp->onSystemTextInputDone(static_cast<int>(mode), value);
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaNativeActivity_nativeOnSystemTextInputCancelled(
    JNIEnv* env,
    jclass,
    jint mode,
    jstring text
) {
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
