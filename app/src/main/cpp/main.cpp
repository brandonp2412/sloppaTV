#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "deep_link.hpp"
#include "discovery.hpp"
#include "display_mode.hpp"
#include "external_player.hpp"
#include "image_decoder.hpp"
#include "jellyfin.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "media_session.hpp"
#include "navigation_stack.hpp"
#include "playback_queue.hpp"
#include "screensaver_policy.hpp"
#include "ui_policy.hpp"
#include "renderer.hpp"
#include "task_runner.hpp"
#include "trickplay_policy.hpp"
#include "video_surface.hpp"
#include "version_policy.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
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
#include <unordered_map>
#include <utility>
#include <vector>

using nlohmann::json;
using namespace std::chrono_literals;

#ifndef SLOPPATV_VERSION_NAME
#define SLOPPATV_VERSION_NAME "dev"
#endif

namespace {
constexpr const char* kTag = "sloppaTV";

constexpr Color kPanel{0.090f, 0.098f, 0.122f, 1.0f};
constexpr Color kPanelAlt{0.125f, 0.137f, 0.165f, 1.0f};
constexpr Color kText{0.95f, 0.96f, 0.98f, 1.0f};
constexpr Color kMuted{0.60f, 0.63f, 0.69f, 1.0f};
constexpr Color kFocus{0.56f, 0.38f, 0.98f, 1.0f};
constexpr Color kError{0.95f, 0.28f, 0.30f, 1.0f};

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

std::string episodeLabel(const JellyfinItem& item) {
    std::string result;
    if (!item.seriesName.empty()) result = item.seriesName;
    if (item.parentIndexNumber >= 0 || item.indexNumber >= 0) {
        if (!result.empty()) result += " - ";
        if (item.parentIndexNumber >= 0) result += "S" + std::to_string(item.parentIndexNumber);
        if (item.indexNumber >= 0) result += "E" + std::to_string(item.indexNumber);
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

std::string sanitizeSubRip(std::string text) {
    std::string clean;
    clean.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        if (text[index] == '{' && index + 1 < text.size() && text[index + 1] == '\\') {
            const size_t end = text.find('}', index + 2);
            if (end != std::string::npos) {
                index = end + 1;
                continue;
            }
        }
        clean.push_back(text[index++]);
    }
    return clean;
}

std::string generateDeviceId() {
    std::random_device rd;
    std::mt19937_64 generator(
        (static_cast<uint64_t>(rd()) << 32u)
        ^ static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    std::ostringstream out;
    out << "sloppatv-" << std::hex << generator();
    return out.str();
}

enum class Screen {
    Login,
    Profiles,
    Home,
    Libraries,
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

enum class ArtworkState {
    Loading,
    Ready,
    Failed,
};

struct ArtworkEntry {
    ArtworkState state = ArtworkState::Loading;
    DecodedImage decoded;
    GLuint texture = 0;
    uint64_t textureGeneration = 0;
    uint64_t lastUse = 0;
};

struct TrickplayPreviewEntry {
    std::string itemId;
    int tileIndex = -1;
    ArtworkState state = ArtworkState::Failed;
    DecodedImage decoded;
    GLuint texture = 0;
    uint64_t textureGeneration = 0;
};

struct PendingExternalLaunch {
    JellyfinItem item;
    ExternalPlayerApp player;
    std::string url;
    std::string subtitleUrl;
};

struct BrowseSnapshot {
    JellyfinItem container;
    std::vector<JellyfinItem> items;
    int selection = 0;
    int nextIndex = 0;
    bool hasMore = false;
};

enum class BrowseContentMode {
    All,
    Favorites,
    Genres,
    GenreItems,
    Letters,
    LetterItems,
    Collections,
};

struct SubtitleCue {
    int startMs = 0;
    int endMs = 0;
    std::string text;
};

int parseSubtitleTimestamp(std::string value) {
    std::replace(value.begin(), value.end(), ',', '.');
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int milliseconds = 0;
    if (std::sscanf(value.c_str(), "%d:%d:%d.%d", &hours, &minutes, &seconds, &milliseconds) != 4) return -1;
    return (((hours * 60) + minutes) * 60 + seconds) * 1000 + milliseconds;
}

std::vector<SubtitleCue> parseSubRipCues(const std::string& input) {
    std::vector<SubtitleCue> cues;
    std::istringstream stream(input);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.find("-->") == std::string::npos) {
            if (!std::getline(stream, line)) break;
            if (!line.empty() && line.back() == '\r') line.pop_back();
        }
        const size_t arrow = line.find("-->");
        if (arrow == std::string::npos) continue;
        std::string left = line.substr(0, arrow);
        std::string right = line.substr(arrow + 3);
        auto trim = [](std::string& value) {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
        };
        trim(left);
        trim(right);
        const int start = parseSubtitleTimestamp(left);
        const int end = parseSubtitleTimestamp(right);
        if (start < 0 || end <= start) continue;
        std::string text;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            if (!text.empty()) text += ' ';
            text += line;
        }
        if (!text.empty()) cues.push_back({start, end, std::move(text)});
    }
    return cues;
}

enum class VideoZoomMode {
    Fit = 0,
    Fill = 1,
    Stretch = 2,
};

std::string videoZoomName(VideoZoomMode mode) {
    switch (mode) {
        case VideoZoomMode::Fit: return "FIT";
        case VideoZoomMode::Fill: return "FILL";
        case VideoZoomMode::Stretch: return "STRETCH";
    }
    return "FIT";
}

std::string subtitleSizeName(int size) {
    static constexpr std::array<const char*, 3> names{"SMALL", "MEDIUM", "LARGE"};
    return names[static_cast<size_t>(std::clamp(size, 0, 2))];
}

std::string subtitlePositionName(int position) {
    static constexpr std::array<const char*, 3> names{"LOW", "MIDDLE", "HIGH"};
    return names[static_cast<size_t>(std::clamp(position, 0, 2))];
}

std::string uiTextSizeName(int size) {
    static constexpr std::array<const char*, 3> names{"NORMAL", "LARGE", "EXTRA LARGE"};
    return names[static_cast<size_t>(std::clamp(size, 0, 2))];
}

std::string screensaverName(int minutes) {
    minutes = normalizedScreensaverMinutes(minutes);
    return minutes <= 0 ? "OFF" : std::to_string(minutes) + " MINUTES";
}

std::string avcLevelName(int level) {
    if (level <= 0) return "AUTO";
    return std::to_string(level / 10) + "." + std::to_string(level % 10);
}

std::string hevcLevelName(int level) {
    switch (level) {
        case 120: return "4.0";
        case 123: return "4.1";
        case 150: return "5.0";
        case 153: return "5.1";
        case 156: return "5.2";
        case 180: return "6.0";
        case 183: return "6.1";
        case 186: return "6.2";
        default: return "AUTO";
    }
}

std::string hdrOverrideName(int mode) {
    switch (static_cast<HdrOverrideMode>(std::clamp(mode, 0, 2))) {
        case HdrOverrideMode::ForceSdr: return "SDR ONLY";
        case HdrOverrideMode::AllowAllHdr: return "ALLOW ALL HDR";
        case HdrOverrideMode::Auto: return "AUTO";
    }
    return "AUTO";
}

float subtitleTextScale(int size) {
    static constexpr std::array<float, 3> scales{1.85f, 2.3f, 2.8f};
    return scales[static_cast<size_t>(std::clamp(size, 0, 2))];
}

struct AppSettings {
    int maxBitrateMbps = 120;
    int seekBackSeconds = 10;
    int seekForwardSeconds = 10;
    int zoomMode = static_cast<int>(VideoZoomMode::Fit);
    bool autoplayNext = true;
    int stillWatchingAfter = 3;
    bool refreshRateSwitching = false;
    bool showWatchedIndicators = true;
    bool showClock = true;
    bool showBackdrops = true;
    int subtitleSize = 1;
    bool subtitleBackground = true;
    int subtitlePosition = 0;
    int maxAudioChannels = 8;
    int avcLevelOverride = 0;
    int hevcLevelOverride = 0;
    int hdrOverride = static_cast<int>(HdrOverrideMode::Auto);
    int uiTextSize = 0;
    int safeAreaPercent = 0;
    int screensaverMinutes = 0;
    std::string externalPlayerComponent;
};

PlaybackOverrides playbackOverridesFor(const AppSettings& settings) {
    return {
        .maxAvcLevel = settings.avcLevelOverride,
        .maxHevcLevel = settings.hevcLevelOverride,
        .hdrMode = static_cast<HdrOverrideMode>(std::clamp(settings.hdrOverride, 0, 2)),
    };
}

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

struct LaunchRequest {
    std::string itemId;
    std::string searchQuery;
};

class ScopedLaunchEnv {
public:
    explicit ScopedLaunchEnv(JavaVM* vm) : vm_(vm) {
        if (!vm_) return;
        const jint result = vm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED && vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) attached_ = true;
    }
    ~ScopedLaunchEnv() { if (attached_ && vm_) vm_->DetachCurrentThread(); }
    JNIEnv* get() const { return env_; }
private:
    JavaVM* vm_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};

LaunchRequest readLaunchRequest(android_app* app) {
    LaunchRequest request;
    if (!app || !app->activity || !app->activity->vm || !app->activity->clazz) return request;
    ScopedLaunchEnv scoped(app->activity->vm);
    JNIEnv* env = scoped.get();
    if (!env) return request;
    jobject activity = app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    if (!activityClass) return request;
    jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    if (!getIntent || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        return request;
    }
    jobject intent = env->CallObjectMethod(activity, getIntent);
    if (!intent || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        return request;
    }
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID getAction = intentClass ? env->GetMethodID(intentClass, "getAction", "()Ljava/lang/String;") : nullptr;
    jmethodID getDataString = intentClass ? env->GetMethodID(intentClass, "getDataString", "()Ljava/lang/String;") : nullptr;
    jmethodID getStringExtra = intentClass ? env->GetMethodID(intentClass, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;") : nullptr;
    if (!getAction || !getDataString || !getStringExtra || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (intentClass) env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
        env->DeleteLocalRef(activityClass);
        return request;
    }

    auto toString = [&](jstring value) {
        std::string result;
        if (!value) return result;
        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (chars) {
            result = chars;
            env->ReleaseStringUTFChars(value, chars);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return result;
    };

    auto actionValue = static_cast<jstring>(env->CallObjectMethod(intent, getAction));
    auto dataValue = static_cast<jstring>(env->CallObjectMethod(intent, getDataString));
    const std::string action = toString(actionValue);
    const std::string data = toString(dataValue);
    if (action == "android.intent.action.VIEW") {
        request.itemId = normalizeJellyfinItemId(data);
    } else if (action == "android.intent.action.SEARCH") {
        jstring queryKey = env->NewStringUTF("query");
        auto queryValue = queryKey
            ? static_cast<jstring>(env->CallObjectMethod(intent, getStringExtra, queryKey))
            : nullptr;
        request.searchQuery = normalizeExternalSearchQuery(toString(queryValue));
        if (queryValue) env->DeleteLocalRef(queryValue);
        if (queryKey) env->DeleteLocalRef(queryKey);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (actionValue) env->DeleteLocalRef(actionValue);
    if (dataValue) env->DeleteLocalRef(dataValue);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(activityClass);
    return request;
}

class SloppaApp {
public:
    explicit SloppaApp(android_app* app)
        : app_(app),
          api_(app->activity->vm, app->activity->clazz),
          player_(app->activity->vm),
          mediaSession_(app->activity->vm, app->activity->clazz),
          externalPlayer_(app->activity->vm, app->activity->clazz),
          imageDecoder_(app->activity->vm),
          videoSurface_(app->activity->vm),
          tasks_(4, [app] {
              if (app && app->looper) ALooper_wake(app->looper);
          }) {
        dataPath_ = app->activity->internalDataPath ? app->activity->internalDataPath : "";
        const LaunchRequest launchRequest = readLaunchRequest(app_);
        pendingDeepLinkItemId_ = launchRequest.itemId;
        pendingSearchQuery_ = launchRequest.searchQuery;
        loadSession();
        refreshExternalPlayers();
        if (session_.valid()) {
            resetNavigation(Screen::Home);
            loadHomeAsync();
        } else {
            resetNavigation(Screen::Login);
        }
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
        ++taskGeneration_;
        tasks_.shutdown();
        stopPlayback();
        renderer_.shutdown();
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
                else if (loading_ || quickConnectActive_) timeoutMs = 100;
                else {
                    const int64_t delayMs = screensaverDelayMs(settings_.screensaverMinutes);
                    if (delayMs > 0) {
                        const int64_t idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(pollNow - lastInteraction_).count();
                        timeoutMs = static_cast<int>(std::clamp<int64_t>(delayMs - idleMs, 0, delayMs));
                    }
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
                        loading_ || quickConnectActive_
                    );
                }
                screensaver = screensaverActive_;
            }
            burstActive = std::chrono::steady_clock::now() < renderBurstUntil_;
            if (renderer_.ready() && (playerScreen || screensaver || shouldRender || burstActive)) render();
        }
    }

private:
    void onAppCommand(int32_t command) {
        std::scoped_lock lock(stateMutex_);
        switch (command) {
            case APP_CMD_INIT_WINDOW:
                if (app_->window) renderer_.init(app_->window);
                lastInteraction_ = std::chrono::steady_clock::now();
                screensaverActive_ = false;
                break;
            case APP_CMD_TERM_WINDOW:
                if (screen_ == Screen::Player) stopPlayback();
                renderer_.shutdown();
                break;
            case APP_CMD_GAINED_FOCUS:
                lastInteraction_ = std::chrono::steady_clock::now();
                screensaverActive_ = false;
                break;
            case APP_CMD_LOST_FOCUS:
                screensaverActive_ = false;
                if (screen_ == Screen::Player && player_.status() == PlayerStatus::Playing) {
                    player_.togglePause();
                }
                break;
            default:
                break;
        }
    }

    int32_t onInput(AInputEvent* event) {
        if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY) return 0;
        if (AKeyEvent_getAction(event) != AKEY_EVENT_ACTION_DOWN) return 0;

        const int32_t key = AKeyEvent_getKeyCode(event);
        const int32_t meta = AKeyEvent_getMetaState(event);
        const auto inputNow = std::chrono::steady_clock::now();
        renderBurstUntil_ = inputNow + 150ms;
        std::scoped_lock lock(stateMutex_);
        lastInteraction_ = inputNow;
        if (screensaverActive_) {
            screensaverActive_ = false;
            return 1;
        }

        if (queueOverlayActive_) {
            handleQueueOverlayKey(key);
            return 1;
        }

        if (screen_ == Screen::Player) {
            handlePlayerKey(key);
            return 1;
        }

        const char typed = keyCodeToChar(key, meta);
        if (typed != 0 && handleTypedCharacter(typed)) return 1;
        if (key == AKEYCODE_DEL && handleBackspace()) return 1;

        switch (screen_) {
            case Screen::Login: handleLoginKey(key); break;
            case Screen::Profiles: handleProfilesKey(key); break;
            case Screen::Home: handleHomeKey(key); break;
            case Screen::Libraries: handleLibrariesKey(key); break;
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
        if (screen_ == Screen::Login && loginField_ >= 0 && loginField_ < 3) {
            loginFields_[static_cast<size_t>(loginField_)].push_back(c);
            return true;
        }
        if (screen_ == Screen::Search) {
            searchQuery_.push_back(c);
            searchKeyboard_ = true;
            return true;
        }
        return false;
    }

    bool handleBackspace() {
        if (screen_ == Screen::Login && loginField_ >= 0 && loginField_ < 3) {
            auto& value = loginFields_[static_cast<size_t>(loginField_)];
            if (!value.empty()) value.pop_back();
            return true;
        }
        if (screen_ == Screen::Search && !searchQuery_.empty()) {
            searchQuery_.pop_back();
            return true;
        }
        return false;
    }

    void moveKeyboard(int dx, int dy) {
        const auto& rows = keyboardRows();
        keyboardRow_ = std::clamp(keyboardRow_ + dy, 0, static_cast<int>(rows.size()) - 1);
        keyboardCol_ = std::clamp(keyboardCol_ + dx, 0, static_cast<int>(rows[static_cast<size_t>(keyboardRow_)].size()) - 1);
    }

    void activateKeyboardKey(bool forSearch) {
        const auto& rows = keyboardRows();
        const auto& key = rows[static_cast<size_t>(keyboardRow_)][static_cast<size_t>(keyboardCol_)];
        std::string* target = nullptr;
        if (forSearch) {
            target = &searchQuery_;
        } else if (loginField_ >= 0 && loginField_ < 3) {
            target = &loginFields_[static_cast<size_t>(loginField_)];
        }
        if (!target) return;

        switch (key.action) {
            case KeyAction::Insert:
                *target += key.value;
                break;
            case KeyAction::Backspace:
                if (!target->empty()) target->pop_back();
                break;
            case KeyAction::Done:
                if (forSearch) {
                    searchKeyboard_ = false;
                    searchSelection_ = 0;
                    searchAsync();
                } else {
                    loginKeyboard_ = false;
                }
                break;
        }
    }

    void handleLoginKey(int32_t key) {
        if (quickConnectActive_) {
            if (key == AKEYCODE_BACK) {
                ++taskGeneration_;
                quickConnectActive_ = false;
                quickConnectCode_.clear();
                loading_ = false;
                error_.clear();
                loginField_ = 4;
            }
            return;
        }
        if (key == AKEYCODE_BACK) {
            if (loginKeyboard_) loginKeyboard_ = false;
            else ANativeActivity_finish(app_->activity);
            return;
        }
        if (loginKeyboard_) {
            if (key == AKEYCODE_DPAD_LEFT) moveKeyboard(-1, 0);
            else if (key == AKEYCODE_DPAD_RIGHT) moveKeyboard(1, 0);
            else if (key == AKEYCODE_DPAD_UP) moveKeyboard(0, -1);
            else if (key == AKEYCODE_DPAD_DOWN) moveKeyboard(0, 1);
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) activateKeyboardKey(false);
            return;
        }

        if (key == AKEYCODE_DPAD_UP) {
            loginField_ = loginField_ >= 3 ? 2 : std::max(0, loginField_ - 1);
        } else if (key == AKEYCODE_DPAD_DOWN) {
            if (loginField_ < 2) ++loginField_;
            else if (loginField_ == 2) loginField_ = 3;
        } else if ((key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT) && loginField_ >= 3) {
            const int maxAction = savedSessions_.empty() ? 5 : 6;
            if (key == AKEYCODE_DPAD_LEFT) loginField_ = loginField_ <= 3 ? maxAction : loginField_ - 1;
            else loginField_ = loginField_ >= maxAction ? 3 : loginField_ + 1;
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (loginField_ < 3) {
                loginKeyboard_ = true;
                keyboardRow_ = keyboardCol_ = 0;
            } else if (loginField_ == 3) {
                loginAsync();
            } else if (loginField_ == 4) {
                quickConnectAsync();
            } else if (loginField_ == 5) {
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
        const int addIndex = static_cast<int>(savedSessions_.size());
        if (key == AKEYCODE_DPAD_UP) {
            profilesSelection_ = std::max(0, profilesSelection_ - 1);
            profileAction_ = 0;
        } else if (key == AKEYCODE_DPAD_DOWN) {
            profilesSelection_ = std::min(addIndex, profilesSelection_ + 1);
            profileAction_ = 0;
        } else if ((key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT) && profilesSelection_ < addIndex) {
            profileAction_ = profileAction_ == 0 ? 1 : 0;
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (profilesSelection_ == addIndex) {
                startAddAccount();
            } else if (profilesSelection_ >= 0 && profilesSelection_ < addIndex) {
                if (profileAction_ == 0) switchSavedSession(static_cast<size_t>(profilesSelection_));
                else forgetSavedSession(static_cast<size_t>(profilesSelection_));
            }
        }
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
        if (homeRow_ < 0) {
            if (key == AKEYCODE_DPAD_LEFT) navIndex_ = std::max(0, navIndex_ - 1);
            else if (key == AKEYCODE_DPAD_RIGHT) navIndex_ = std::min(4, navIndex_ + 1);
            else if (key == AKEYCODE_DPAD_DOWN && !home_.rows.empty()) homeRow_ = 0;
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                if (navIndex_ == 1) openLibraryByCollectionType("movies");
                else if (navIndex_ == 2) openLibraryByCollectionType("tvshows");
                else if (navIndex_ == 3) openSearch();
                else if (navIndex_ == 4) openSettings();
            }
            return;
        }
        if (home_.rows.empty() || homeRow_ >= static_cast<int>(home_.rows.size())) {
            homeRow_ = -1;
            return;
        }

        auto& items = home_.rows[static_cast<size_t>(homeRow_)].items;
        int& selection = homeSelection_[static_cast<size_t>(homeRow_)];
        if (key == AKEYCODE_DPAD_LEFT && !items.empty()) selection = std::max(0, selection - 1);
        else if (key == AKEYCODE_DPAD_RIGHT && !items.empty()) selection = std::min(static_cast<int>(items.size()) - 1, selection + 1);
        else if (key == AKEYCODE_DPAD_UP) {
            if (homeRow_ == 0) homeRow_ = -1;
            else --homeRow_;
        } else if (key == AKEYCODE_DPAD_DOWN) {
            if (homeRow_ + 1 < static_cast<int>(home_.rows.size())) ++homeRow_;
        } else if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && !items.empty()) {
            openDetails(items[static_cast<size_t>(selection)]);
            return;
        }
        if (homeRow_ >= 0 && homeRow_ < static_cast<int>(homeSelection_.size())) {
            prefetchHomeWindow(homeRow_, homeSelection_[static_cast<size_t>(homeRow_)]);
        }
    }

    void handleLibrariesKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Home);
            homeRow_ = -1;
            navIndex_ = 1;
            return;
        }
        if (home_.views.empty()) return;
        if (key == AKEYCODE_DPAD_LEFT) librarySelection_ = std::max(0, librarySelection_ - 1);
        else if (key == AKEYCODE_DPAD_RIGHT) librarySelection_ = std::min(static_cast<int>(home_.views.size()) - 1, librarySelection_ + 1);
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            openLibrary(home_.views[static_cast<size_t>(librarySelection_)]);
        }
    }

    bool isBrowsableContainer(const JellyfinItem& item) const {
        return item.type == "Folder" || item.type == "BoxSet" || item.type == "CollectionFolder";
    }

    void handleBrowseKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            browseFilterFocused_ = false;
            if (!browseStack_.empty()) {
                auto previous = std::move(browseStack_.back());
                browseStack_.pop_back();
                activeLibrary_ = std::move(previous.container);
                browseItems_ = std::move(previous.items);
                browseSelection_ = previous.selection;
                browseNextIndex_ = previous.nextIndex;
                browseHasMore_ = previous.hasMore;
            } else if (browseContentMode_ == BrowseContentMode::GenreItems) {
                browseContentMode_ = BrowseContentMode::Genres;
                browseGenre_.clear();
                browseItems_.clear();
                browseSelection_ = 0;
                browseNextIndex_ = 0;
                loadBrowsePageAsync(false);
            } else if (browseContentMode_ == BrowseContentMode::LetterItems) {
                browseContentMode_ = BrowseContentMode::Letters;
                browseLetter_.clear();
                populateLetterChoices();
            } else {
                popScreen(Screen::Home);
                if (screen_ == Screen::Home) homeRow_ = -1;
                browseItems_.clear();
            }
            return;
        }

        if (browseFilterFocused_ && browseHasFilterBar()) {
            const auto labels = browseFilterLabels();
            if (key == AKEYCODE_DPAD_LEFT) browseFilterSelection_ = std::max(0, browseFilterSelection_ - 1);
            else if (key == AKEYCODE_DPAD_RIGHT) browseFilterSelection_ = std::min(static_cast<int>(labels.size()) - 1, browseFilterSelection_ + 1);
            else if (key == AKEYCODE_DPAD_DOWN) browseFilterFocused_ = false;
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                browseFilterFocused_ = false;
                applyBrowseFilter(browseFilterSelection_);
            }
            return;
        }

        if (key == AKEYCODE_DPAD_UP && browseHasFilterBar() && isTopMediaGridSelection(browseSelection_)) {
            browseFilterFocused_ = true;
            return;
        }
        if (browseItems_.empty()) return;
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            const auto selected = browseItems_[static_cast<size_t>(browseSelection_)];
            if (selected.type == "Genre") {
                browseGenre_ = selected.name;
                browseContentMode_ = BrowseContentMode::GenreItems;
                browseItems_.clear();
                browseSelection_ = 0;
                browseNextIndex_ = 0;
                browseHasMore_ = false;
                loadBrowsePageAsync(false);
            } else if (selected.type == "Letter") {
                browseLetter_ = selected.name;
                browseContentMode_ = BrowseContentMode::LetterItems;
                browseItems_.clear();
                browseSelection_ = 0;
                browseNextIndex_ = 0;
                browseHasMore_ = false;
                loadBrowsePageAsync(false);
            } else if (isBrowsableContainer(selected)) {
                openBrowseContainer(selected, true);
            } else {
                openDetails(selected);
            }
            return;
        }
        moveGridSelection(key, browseItems_, browseSelection_);
        if (browseHasMore_ && !loading_ && browseSelection_ >= static_cast<int>(browseItems_.size()) - 12) {
            loadMoreBrowseAsync();
        }
    }

    void handleSearchKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            if (searchKeyboard_) {
                searchKeyboard_ = false;
            } else {
                popScreen(Screen::Home);
                if (screen_ == Screen::Home) homeRow_ = 0;
            }
            return;
        }
        if (searchKeyboard_) {
            if (key == AKEYCODE_ENTER) {
                searchKeyboard_ = false;
                searchSelection_ = 0;
                searchAsync();
            } else if (key == AKEYCODE_DPAD_LEFT) moveKeyboard(-1, 0);
            else if (key == AKEYCODE_DPAD_RIGHT) moveKeyboard(1, 0);
            else if (key == AKEYCODE_DPAD_UP) moveKeyboard(0, -1);
            else if (key == AKEYCODE_DPAD_DOWN) moveKeyboard(0, 1);
            else if (key == AKEYCODE_DPAD_CENTER) activateKeyboardKey(true);
            return;
        }

        if (key == AKEYCODE_SEARCH || (key == AKEYCODE_DPAD_UP && isTopMediaGridSelection(searchSelection_))) {
            searchKeyboard_ = true;
            keyboardRow_ = keyboardCol_ = 0;
            return;
        }
        if (searchResults_.empty()) return;
        constexpr int columns = mediaGridColumns();
        const int rows = static_cast<int>((searchResults_.size() + columns - 1) / columns);
        int row = searchSelection_ / columns;
        int col = searchSelection_ % columns;
        if (key == AKEYCODE_DPAD_LEFT) col = std::max(0, col - 1);
        else if (key == AKEYCODE_DPAD_RIGHT) col = std::min(columns - 1, col + 1);
        else if (key == AKEYCODE_DPAD_UP) row = std::max(0, row - 1);
        else if (key == AKEYCODE_DPAD_DOWN) row = std::min(rows - 1, row + 1);
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            openDetails(searchResults_[static_cast<size_t>(searchSelection_)]);
            return;
        }
        const int next = row * columns + col;
        if (next >= 0 && next < static_cast<int>(searchResults_.size())) searchSelection_ = next;
    }

    std::vector<std::string> detailActions() const {
        std::vector<std::string> actions;
        actions.emplace_back(detail_.type == "Series" ? "PLAY NEXT" : (detail_.positionTicks > 0 ? "RESUME" : "PLAY"));
        if (detail_.type == "Series") actions.emplace_back("EPISODES");
        actions.emplace_back(detail_.favorite ? "UNFAVORITE" : "FAVORITE");
        actions.emplace_back(detail_.played ? "MARK UNWATCHED" : "MARK WATCHED");
        if (!detail_.people.empty()) actions.emplace_back("CAST");
        actions.emplace_back("MORE");
        actions.emplace_back("BACK");
        return actions;
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
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) homeRow_ = -1;
            navIndex_ = 4;
            return;
        }
        if (key == AKEYCODE_DPAD_UP) settingsSelection_ = std::max(0, settingsSelection_ - 1);
        else if (key == AKEYCODE_DPAD_DOWN) settingsSelection_ = std::min(22, settingsSelection_ + 1);
        else if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_DPAD_RIGHT) {
            const int direction = key == AKEYCODE_DPAD_RIGHT ? 1 : -1;
            if (settingsSelection_ == 0) {
                static constexpr std::array<int, 6> choices{20, 40, 80, 120, 160, 200};
                auto it = std::find(choices.begin(), choices.end(), settings_.maxBitrateMbps);
                int index = it == choices.end() ? 3 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                settings_.maxBitrateMbps = choices[static_cast<size_t>(index)];
            } else if (settingsSelection_ == 1 || settingsSelection_ == 2) {
                static constexpr std::array<int, 6> choices{5, 10, 15, 20, 30, 60};
                int& value = settingsSelection_ == 1 ? settings_.seekBackSeconds : settings_.seekForwardSeconds;
                auto it = std::find(choices.begin(), choices.end(), value);
                int index = it == choices.end() ? 1 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                value = choices[static_cast<size_t>(index)];
            } else if (settingsSelection_ == 3) {
                settings_.zoomMode = std::clamp(settings_.zoomMode + direction, 0, 2);
                videoZoomMode_ = static_cast<VideoZoomMode>(settings_.zoomMode);
            } else if (settingsSelection_ == 4) {
                settings_.autoplayNext = !settings_.autoplayNext;
            } else if (settingsSelection_ == 5) {
                static constexpr std::array<int, 5> choices{2, 3, 4, 5, 6};
                auto it = std::find(choices.begin(), choices.end(), settings_.stillWatchingAfter);
                int index = it == choices.end() ? 1 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                settings_.stillWatchingAfter = choices[static_cast<size_t>(index)];
            } else if (settingsSelection_ == 6) {
                settings_.refreshRateSwitching = !settings_.refreshRateSwitching;
                if (!settings_.refreshRateSwitching) displayMode_.restore();
            } else if (settingsSelection_ == 7) {
                settings_.showWatchedIndicators = !settings_.showWatchedIndicators;
            } else if (settingsSelection_ == 8) {
                settings_.showClock = !settings_.showClock;
            } else if (settingsSelection_ == 9) {
                settings_.showBackdrops = !settings_.showBackdrops;
            } else if (settingsSelection_ == 10) {
                settings_.subtitleSize = std::clamp(settings_.subtitleSize + direction, 0, 2);
            } else if (settingsSelection_ == 11) {
                settings_.subtitleBackground = !settings_.subtitleBackground;
            } else if (settingsSelection_ == 12) {
                settings_.subtitlePosition = std::clamp(settings_.subtitlePosition + direction, 0, 2);
            } else if (settingsSelection_ == 13) {
                settings_.maxAudioChannels = settings_.maxAudioChannels <= 2 ? 8 : 2;
            } else if (settingsSelection_ == 14) {
                static constexpr std::array<int, 10> choices{0, 40, 41, 42, 50, 51, 52, 60, 61, 62};
                auto it = std::find(choices.begin(), choices.end(), settings_.avcLevelOverride);
                int index = it == choices.end() ? 0 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                settings_.avcLevelOverride = choices[static_cast<size_t>(index)];
            } else if (settingsSelection_ == 15) {
                static constexpr std::array<int, 9> choices{0, 120, 123, 150, 153, 156, 180, 183, 186};
                auto it = std::find(choices.begin(), choices.end(), settings_.hevcLevelOverride);
                int index = it == choices.end() ? 0 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                settings_.hevcLevelOverride = choices[static_cast<size_t>(index)];
            } else if (settingsSelection_ == 16) {
                settings_.hdrOverride = std::clamp(settings_.hdrOverride + direction, 0, 2);
            } else if (settingsSelection_ == 17) {
                settings_.uiTextSize = std::clamp(settings_.uiTextSize + direction, 0, 2);
            } else if (settingsSelection_ == 18) {
                static constexpr std::array<int, 4> choices{0, 2, 4, 6};
                auto it = std::find(choices.begin(), choices.end(), settings_.safeAreaPercent);
                int index = it == choices.end() ? 0 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                settings_.safeAreaPercent = choices[static_cast<size_t>(index)];
            } else if (settingsSelection_ == 19) {
                static constexpr std::array<int, 5> choices{0, 5, 10, 20, 30};
                auto it = std::find(choices.begin(), choices.end(), settings_.screensaverMinutes);
                int index = it == choices.end() ? 0 : static_cast<int>(std::distance(choices.begin(), it));
                index = std::clamp(index + direction, 0, static_cast<int>(choices.size()) - 1);
                settings_.screensaverMinutes = choices[static_cast<size_t>(index)];
                lastInteraction_ = std::chrono::steady_clock::now();
                screensaverActive_ = false;
            } else if (settingsSelection_ == 20) {
                cycleExternalPlayer(direction);
            }
            saveSession(session_);
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (settingsSelection_ == 21) openDiagnostics();
            else if (settingsSelection_ == 22) openProfiles();
        }
    }

    void handleDiagnosticsKey(int32_t key) {
        if (key == AKEYCODE_BACK || key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            popScreen(Screen::Settings);
        }
    }

    void handleDetailsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Home);
            return;
        }
        const auto actions = detailActions();
        if (detailsSimilarFocused_) {
            if (key == AKEYCODE_DPAD_UP) {
                detailsSimilarFocused_ = false;
            } else if (key == AKEYCODE_DPAD_LEFT && !detailsSimilar_.empty()) {
                detailsSimilarSelection_ = std::max(0, detailsSimilarSelection_ - 1);
            } else if (key == AKEYCODE_DPAD_RIGHT && !detailsSimilar_.empty()) {
                detailsSimilarSelection_ = std::min(static_cast<int>(detailsSimilar_.size()) - 1, detailsSimilarSelection_ + 1);
            } else if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && !detailsSimilar_.empty()) {
                openDetails(detailsSimilar_[static_cast<size_t>(detailsSimilarSelection_)]);
            }
            return;
        }
        if (key == AKEYCODE_DPAD_LEFT) {
            detailsButton_ = std::max(0, detailsButton_ - 1);
        } else if (key == AKEYCODE_DPAD_RIGHT) {
            detailsButton_ = std::min(static_cast<int>(actions.size()) - 1, detailsButton_ + 1);
        } else if (key == AKEYCODE_DPAD_DOWN && !detailsSimilar_.empty()) {
            detailsSimilarFocused_ = true;
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            const std::string& action = actions[static_cast<size_t>(detailsButton_)];
            if (action == "PLAY" || action == "RESUME" || action == "PLAY NEXT") beginPlayback();
            else if (action == "EPISODES") openSeasons();
            else if (action == "FAVORITE" || action == "UNFAVORITE") toggleFavoriteAsync();
            else if (action == "MARK WATCHED" || action == "MARK UNWATCHED") togglePlayedAsync();
            else if (action == "CAST") openCast();
            else if (action == "MORE") openItemMenu();
            else if (action == "BACK") popScreen(Screen::Home);
        }
    }

    void openCast() {
        if (detail_.people.empty()) return;
        pushScreen(Screen::Cast);
        castSelection_ = 0;
        error_.clear();
    }

    void handleCastKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Details);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (!detail_.people.empty()) openPersonItems(detail_.people[static_cast<size_t>(castSelection_)]);
            return;
        }
        moveGridSelectionByCount(key, static_cast<int>(detail_.people.size()), castSelection_);
    }

    void openPersonItems(const JellyfinPerson& person) {
        if (!session_.valid() || person.id.empty()) return;
        pushScreen(Screen::PersonItems);
        selectedPerson_ = person;
        personItems_.clear();
        personItemSelection_ = 0;
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string personId = person.id;
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, personId, generation] {
            auto result = api_.getItemsForPerson(session, personId, 60);
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (screen_ != Screen::PersonItems || selectedPerson_.id != personId) return;
            if (!result.ok) {
                error_ = "PERSON: " + result.error;
                return;
            }
            personItems_ = std::move(result.value);
            personItemSelection_ = 0;
        });
    }

    void handlePersonItemsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Cast);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (!personItems_.empty()) openDetails(personItems_[static_cast<size_t>(personItemSelection_)]);
            return;
        }
        moveGridSelection(key, personItems_, personItemSelection_);
    }

    std::vector<std::string> itemMenuActions() const {
        std::vector<std::string> actions;
        if (detail_.type == "Series") actions.emplace_back("PLAY ALL");
        if (selectedExternalPlayer().has_value()) actions.emplace_back("PLAY EXTERNAL");
        if (!playbackQueue_.empty()) actions.emplace_back("VIEW QUEUE");
        actions.emplace_back("REFRESH METADATA");
        if (detail_.canDelete) actions.emplace_back("DELETE MEDIA");
        actions.emplace_back("BACK");
        return actions;
    }

    void openItemMenu() {
        if (detail_.id.empty()) return;
        pushScreen(Screen::ItemMenu);
        itemMenuSelection_ = 0;
        deleteConfirmation_ = false;
        deleteConfirmationSelection_ = 1;
        error_.clear();
    }

    void handleItemMenuKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            deleteConfirmation_ = false;
            popScreen(Screen::Details);
            return;
        }
        if (deleteConfirmation_) {
            if (key == AKEYCODE_DPAD_UP || key == AKEYCODE_DPAD_LEFT) deleteConfirmationSelection_ = 0;
            else if (key == AKEYCODE_DPAD_DOWN || key == AKEYCODE_DPAD_RIGHT) deleteConfirmationSelection_ = 1;
            else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                if (deleteConfirmationSelection_ == 0) deleteCurrentItemAsync();
                else {
                    deleteConfirmation_ = false;
                    deleteConfirmationSelection_ = 1;
                }
            }
            return;
        }

        const auto actions = itemMenuActions();
        if (key == AKEYCODE_DPAD_UP) itemMenuSelection_ = std::max(0, itemMenuSelection_ - 1);
        else if (key == AKEYCODE_DPAD_DOWN) itemMenuSelection_ = std::min(static_cast<int>(actions.size()) - 1, itemMenuSelection_ + 1);
        else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            const std::string& action = actions[static_cast<size_t>(itemMenuSelection_)];
            if (action == "PLAY ALL") {
                popScreen(Screen::Details);
                beginSeriesPlayAll();
            } else if (action == "PLAY EXTERNAL") {
                popScreen(Screen::Details);
                launchExternalPlaybackAsync();
            } else if (action == "VIEW QUEUE") {
                popScreen(Screen::Details);
                openQueueOverlay();
            } else if (action == "REFRESH METADATA") refreshCurrentItemMetadataAsync();
            else if (action == "DELETE MEDIA") {
                deleteConfirmation_ = true;
                deleteConfirmationSelection_ = 1;
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
        const uint64_t generation = ++taskGeneration_;
        if (!tasks_.submit([this, session, selected, player = *player, generation]() mutable {
            JellyfinItem playable = selected;
            if (playable.type == "Series") {
                auto next = api_.getNextUpForSeries(session, playable.id);
                if (!next.ok) {
                    if (generation != taskGeneration_.load()) return;
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
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

            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (videoUrl.empty()) {
                error_ = "EXTERNAL PLAYER: NO STATIC STREAM";
                return;
            }
            pendingExternalLaunch_ = PendingExternalLaunch{
                .item = std::move(playable),
                .player = std::move(player),
                .url = videoUrl,
                .subtitleUrl = std::move(subtitleUrl),
            };
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
            detail_ = seriesDetail_;
            popScreen(Screen::Details);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (!seasonItems_.empty()) openEpisodes(seasonItems_[static_cast<size_t>(seasonSelection_)]);
            return;
        }
        moveGridSelection(key, seasonItems_, seasonSelection_);
    }

    void handleEpisodesKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Seasons);
            return;
        }
        if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (!episodeItems_.empty()) openDetails(episodeItems_[static_cast<size_t>(episodeSelection_)]);
            return;
        }
        moveGridSelection(key, episodeItems_, episodeSelection_);
    }

    void refreshPlaybackTelemetry(bool force = false) {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now - lastPlaybackTelemetryRead_ < 250ms) return;
        cachedPlaybackPositionMs_ = player_.positionMs();
        if (activePlaybackItem_.runtimeTicks > 0) {
            cachedPlaybackDurationMs_ = playbackPositionMsFromTicks(activePlaybackItem_.runtimeTicks);
        } else if (force || cachedPlaybackDurationMs_ <= 0) {
            if (force || lastPlaybackDurationProbe_ == std::chrono::steady_clock::time_point{}
                || now - lastPlaybackDurationProbe_ >= 2s) {
                const int duration = player_.durationMs();
                if (duration > 0) cachedPlaybackDurationMs_ = duration;
                lastPlaybackDurationProbe_ = now;
            }
        }
        lastPlaybackTelemetryRead_ = now;
    }

    std::vector<PlayerTrack> playerTracks(int type) const {
        auto tracks = player_.tracks();
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [type](const PlayerTrack& track) {
            if (type == 4) return track.type != 3 && track.type != 4;
            return track.type != type;
        }), tracks.end());
        return tracks;
    }

    std::string playerTrackLabel(int type, int selectedIndex) const {
        if (type == 2 && !activePlaybackItem_.audios.empty()) {
            const auto selected = std::find_if(
                activePlaybackItem_.audios.begin(),
                activePlaybackItem_.audios.end(),
                [&](const JellyfinAudioStream& audio) { return audio.index == selectedAudioServerIndex_; }
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
        if (type == 4 && subtitleLoadInProgress_) return "LOADING";
        if (type == 4 && selectedSubtitleServerIndex_ >= 0) {
            const auto selected = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == selectedSubtitleServerIndex_; }
            );
            if (selected != activePlaybackItem_.subtitles.end()) {
                std::string label = selected->language.empty() ? "ON" : selected->language;
                std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return label;
            }
        }
        if (type == 4 && !activeSubtitleCues_.empty()) {
            if (!activeSubtitleEnabled_) return "OFF";
            std::string label = activeSubtitleLanguage_.empty() ? "ON" : activeSubtitleLanguage_;
            std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            const auto subtitle = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [&](const JellyfinSubtitleStream& candidate) { return candidate.index == activeSubtitleServerIndex_; }
            );
            if (subtitle != activePlaybackItem_.subtitles.end() && activePlaybackItem_.subtitles.size() > 1) {
                label += " " + std::to_string(std::distance(activePlaybackItem_.subtitles.begin(), subtitle) + 1)
                    + "/" + std::to_string(activePlaybackItem_.subtitles.size());
            }
            return label;
        }
        if (type == 4 && selectedIndex < 0) return "OFF";
        const auto tracks = playerTracks(type);
        for (size_t i = 0; i < tracks.size(); ++i) {
            if (tracks[i].index != selectedIndex) continue;
            std::string label = (!tracks[i].language.empty() && tracks[i].language != "und")
                ? tracks[i].language
                : "TRACK";
            if (tracks.size() > 1) {
                label += " " + std::to_string(i + 1) + "/" + std::to_string(tracks.size());
            } else if (label == "TRACK") {
                label += " 1";
            }
            return label;
        }
        return type == 2 ? "DEFAULT" : "OFF";
    }

    void cycleAudioTrack() {
        const auto& tracks = activePlaybackItem_.audios;
        if (tracks.size() < 2) {
            error_ = "ONLY ONE AUDIO TRACK";
            return;
        }
        auto selected = std::find_if(tracks.begin(), tracks.end(), [&](const JellyfinAudioStream& audio) {
            return audio.index == selectedAudioServerIndex_;
        });
        const size_t next = selected == tracks.end()
            ? 0
            : (static_cast<size_t>(std::distance(tracks.begin(), selected)) + 1) % tracks.size();
        restartPlaybackAt(cachedPlaybackPositionMs_, tracks[next].index, selectedSubtitleServerIndex_);
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
            playbackSubtitleLanguagePreference_ = std::string{};
            return;
        }
        const auto selected = std::find_if(
            activePlaybackItem_.subtitles.begin(),
            activePlaybackItem_.subtitles.end(),
            [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == streamIndex; }
        );
        if (selected != activePlaybackItem_.subtitles.end() && !selected->language.empty()) {
            playbackSubtitleLanguagePreference_ = normalizeSubtitleLanguage(selected->language);
        } else {
            // An unlabelled subtitle can be selected for this item, but there is no stable
            // language key to carry to a different episode. Let Jellyfin choose again next time.
            playbackSubtitleLanguagePreference_.reset();
        }
    }

    int preferredSubtitlePosition() const {
        if (activePlaybackItem_.subtitles.empty()) return -1;
        auto preferred = std::find_if(
            activePlaybackItem_.subtitles.begin(),
            activePlaybackItem_.subtitles.end(),
            [](const JellyfinSubtitleStream& subtitle) { return subtitle.isDefault; }
        );
        if (preferred == activePlaybackItem_.subtitles.end()) {
            preferred = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [](const JellyfinSubtitleStream& subtitle) { return !subtitle.forced; }
            );
        }
        if (preferred == activePlaybackItem_.subtitles.end()) preferred = activePlaybackItem_.subtitles.begin();
        return static_cast<int>(std::distance(activePlaybackItem_.subtitles.begin(), preferred));
    }

    void loadSubtitleAsync(const JellyfinSubtitleStream& subtitle) {
        if (subtitleLoadInProgress_ || !session_.valid() || subtitle.index < 0) return;
        subtitleLoadInProgress_ = true;
        const JellyfinSession session = session_;
        const JellyfinItem item = activePlaybackItem_;
        const std::string dataPath = dataPath_;
        if (!tasks_.submit([this, session, item, subtitle, dataPath] {
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
            if (clean.empty()) {
                auto response = api_.downloadSubtitleSrt(session, item, subtitle.index);
                if (response.ok && !response.value.empty()) {
                    clean = sanitizeSubRip(std::move(response.value));
                    if (!cacheFile.empty() && !clean.empty()) {
                        std::error_code ec;
                        std::filesystem::create_directories(cacheFile.parent_path(), ec);
                        if (!ec) {
                            std::ofstream output(cacheFile, std::ios::binary | std::ios::trunc);
                            if (output) output.write(clean.data(), static_cast<std::streamsize>(clean.size()));
                        }
                    }
                }
            }
            std::vector<SubtitleCue> cues = parseSubRipCues(clean);
            if (cues.empty() && fromCache) {
                std::error_code ec;
                std::filesystem::remove(cacheFile, ec);
                auto response = api_.downloadSubtitleSrt(session, item, subtitle.index);
                if (response.ok) {
                    clean = sanitizeSubRip(std::move(response.value));
                    cues = parseSubRipCues(clean);
                }
            }
            const bool loaded = !cues.empty();
            std::scoped_lock lock(stateMutex_);
            if (screen_ == Screen::Player && activePlaybackItem_.id == item.id && loaded) {
                activeSubtitleCues_ = std::move(cues);
                activeSubtitleLanguage_ = subtitle.language.empty() ? "SUB" : subtitle.language;
                activeSubtitleServerIndex_ = subtitle.index;
                activeSubtitleEnabled_ = true;
                playerOverlayUntil_ = std::chrono::steady_clock::now() + 4s;
            }
            if (activePlaybackItem_.id == item.id) {
                subtitleLoadInProgress_ = false;
                if (!loaded) error_ = "SUBTITLES COULD NOT BE LOADED";
            }
        })) {
            subtitleLoadInProgress_ = false;
            error_ = "SUBTITLES COULD NOT BE STARTED";
        }
    }

    void cycleSubtitleTrack() {
        if (subtitleLoadInProgress_ || activePlaybackItem_.subtitles.empty()) {
            if (activePlaybackItem_.subtitles.empty()) error_ = "NO SUBTITLE TRACKS";
            return;
        }
        int nextIndex = -1;
        if (selectedSubtitleServerIndex_ < 0) {
            const int preferred = preferredSubtitlePosition();
            if (preferred >= 0) nextIndex = activePlaybackItem_.subtitles[static_cast<size_t>(preferred)].index;
        } else {
            const auto selected = std::find_if(
                activePlaybackItem_.subtitles.begin(),
                activePlaybackItem_.subtitles.end(),
                [&](const JellyfinSubtitleStream& subtitle) { return subtitle.index == selectedSubtitleServerIndex_; }
            );
            if (selected != activePlaybackItem_.subtitles.end() && std::next(selected) != activePlaybackItem_.subtitles.end()) {
                nextIndex = std::next(selected)->index;
            }
        }
        rememberPlaybackSubtitlePreference(nextIndex);
        restartPlaybackAt(cachedPlaybackPositionMs_, selectedAudioServerIndex_, nextIndex);
    }

    void restartPlaybackAt(int positionMs, int audioStreamIndex, int subtitleStreamIndex) {
        if (subtitleLoadInProgress_ || !session_.valid() || activePlaybackItem_.id.empty()) return;
        const int targetPositionMs = std::max(0, positionMs);
        const bool wasPaused = player_.status() == PlayerStatus::Paused;
        const JellyfinSession session = session_;
        JellyfinItem item = activePlaybackItem_;
        item.positionTicks = playbackTicksFromPositionMs(targetPositionMs);
        const PlaybackTarget previousTarget = activeTarget_;
        const bool shouldReportPrevious = playbackStartReported_ && !previousTarget.url.empty();
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);

        // A Jellyfin server-stream change is a real playback-session handoff. Resolve the
        // replacement only after closing/reporting the old session: asking Jellyfin for a
        // second transcode while the first one is still active has produced stalled HLS
        // sessions (and, on some servers, PlaybackInfo HTTP 500 responses).
        subtitleLoadInProgress_ = true;
        nextTransitionLoading_ = true;
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 10s;
        cachedPlaybackPositionMs_ = targetPositionMs;
        playbackStartReported_ = false;
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
            wasPaused
        ] {
            if (shouldReportPrevious) {
                api_.reportPlaybackStopped(session, item, previousTarget, item.positionTicks);
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
            std::scoped_lock lock(stateMutex_);
            subtitleLoadInProgress_ = false;
            nextTransitionLoading_ = false;
            if (screen_ != Screen::Player || activePlaybackItem_.id != item.id) return;
            if (!target.ok) {
                error_ = target.error;
                return;
            }
            pendingPlayback_ = std::move(target.value);
            pendingPlaybackItem_ = item;
            pendingStreamRestart_ = true;
            pendingRestartPaused_ = wasPaused;
            pendingAudioStreamIndex_ = audioStreamIndex;
        });
    }

    void clearTrickplayPreview() {
        if (trickplayPreview_.texture != 0 && trickplayPreview_.textureGeneration == renderer_.generation()) {
            renderer_.deleteTexture(trickplayPreview_.texture);
        }
        trickplayPreview_ = {};
        trickplayPreviewPositionMs_ = -1;
        trickplayPreviewUntil_ = {};
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

        trickplayPreviewPositionMs_ = std::max(0, positionMs);
        trickplayPreviewUntil_ = std::chrono::steady_clock::now() + 4s;
        if (trickplayPreview_.itemId == activePlaybackItem_.id
            && trickplayPreview_.tileIndex == frame.tileIndex
            && trickplayPreview_.state != ArtworkState::Failed) {
            return;
        }

        if (trickplayPreview_.texture != 0 && trickplayPreview_.textureGeneration == renderer_.generation()) {
            renderer_.deleteTexture(trickplayPreview_.texture);
        }
        trickplayPreview_ = {};
        trickplayPreview_.itemId = activePlaybackItem_.id;
        trickplayPreview_.tileIndex = frame.tileIndex;
        trickplayPreview_.state = ArtworkState::Loading;
        const JellyfinSession session = session_;
        const JellyfinItem item = activePlaybackItem_;
        const int tileIndex = frame.tileIndex;
        if (!tasks_.submit([this, session, item, tileIndex] {
            auto image = api_.downloadTrickplayTile(session, item, tileIndex);
            DecodedImage decoded;
            std::string decodeError;
            if (image.ok) decoded = imageDecoder_.decode(image.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            if (trickplayPreview_.itemId != item.id || trickplayPreview_.tileIndex != tileIndex) return;
            if (!image.ok || !decoded.valid()) {
                trickplayPreview_.state = ArtworkState::Failed;
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kTag,
                    "Trickplay tile %d unavailable: %s",
                    tileIndex,
                    image.ok ? decodeError.c_str() : image.error.c_str()
                );
                return;
            }
            trickplayPreview_.decoded = std::move(decoded);
            trickplayPreview_.state = ArtworkState::Ready;
        })) {
            trickplayPreview_.state = ArtworkState::Failed;
        }
    }

    bool drawTrickplayPreview() {
        if (std::chrono::steady_clock::now() >= trickplayPreviewUntil_
            || trickplayPreviewPositionMs_ < 0
            || trickplayPreview_.state != ArtworkState::Ready
            || trickplayPreview_.itemId != activePlaybackItem_.id
            || !activePlaybackItem_.trickplay.valid()) {
            return false;
        }
        const auto& info = activePlaybackItem_.trickplay;
        const TrickplayFrame frame = trickplayFrameForPosition(
            trickplayPreviewPositionMs_,
            info.intervalMs,
            info.thumbnailCount,
            info.tileWidth,
            info.tileHeight
        );
        if (!frame.valid() || frame.tileIndex != trickplayPreview_.tileIndex || !trickplayPreview_.decoded.valid()) return false;
        if (trickplayPreview_.texture == 0 || trickplayPreview_.textureGeneration != renderer_.generation()) {
            trickplayPreview_.texture = renderer_.createTexture(
                trickplayPreview_.decoded.width,
                trickplayPreview_.decoded.height,
                trickplayPreview_.decoded.rgba.data()
            );
            trickplayPreview_.textureGeneration = renderer_.generation();
        }
        if (trickplayPreview_.texture == 0) return false;

        constexpr float previewWidth = 420.0f;
        const float previewHeight = std::clamp(
            previewWidth * static_cast<float>(info.height) / static_cast<float>(info.width),
            180.0f,
            270.0f
        );
        const double progress = cachedPlaybackDurationMs_ > 0
            ? std::clamp(static_cast<double>(trickplayPreviewPositionMs_) / cachedPlaybackDurationMs_, 0.0, 1.0)
            : 0.5;
        const float centerX = 235.0f + static_cast<float>(1350.0 * progress);
        const float x = std::clamp(centerX - previewWidth * 0.5f, 80.0f, Renderer::logicalWidth() - 80.0f - previewWidth);
        constexpr float y = 235.0f;
        const float sourceWidth = static_cast<float>(trickplayPreview_.decoded.width);
        const float sourceHeight = static_cast<float>(trickplayPreview_.decoded.height);
        const float u0 = std::clamp((frame.cellX * info.width) / sourceWidth, 0.0f, 1.0f);
        const float v0 = std::clamp((frame.cellY * info.height) / sourceHeight, 0.0f, 1.0f);
        const float u1 = std::clamp(((frame.cellX + 1) * info.width) / sourceWidth, 0.0f, 1.0f);
        const float v1 = std::clamp(((frame.cellY + 1) * info.height) / sourceHeight, 0.0f, 1.0f);
        if (u1 <= u0 || v1 <= v0) return false;

        renderer_.rect(x - 7.0f, y - 7.0f, previewWidth + 14.0f, previewHeight + 58.0f, Color{0.0f, 0.0f, 0.0f, 0.90f});
        renderer_.outline(x - 7.0f, y - 7.0f, previewWidth + 14.0f, previewHeight + 58.0f, 4.0f, kFocus);
        renderer_.imageRegion(trickplayPreview_.texture, x, y, previewWidth, previewHeight, u0, v0, u1, v1);
        renderer_.text(x + 14.0f, y + previewHeight + 13.0f, 1.65f, formatPlaybackTime(trickplayPreviewPositionMs_), kText, previewWidth - 28.0f);
        return true;
    }

    void seekPlaybackTo(int positionMs) {
        const int targetMs = std::max(0, positionMs);
        if (seekStrategy(activeTarget_.transcoding) == SeekStrategy::InPlace) {
            player_.seekTo(targetMs);
        } else {
            restartPlaybackAt(targetMs, selectedAudioServerIndex_, selectedSubtitleServerIndex_);
        }
        cachedPlaybackPositionMs_ = targetMs;
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 3s;
    }

    const SubtitleCue* activeSubtitleCue() const {
        if (!activeSubtitleEnabled_ || activeSubtitleCues_.empty()) return nullptr;
        const int position = cachedPlaybackPositionMs_;
        auto it = std::upper_bound(
            activeSubtitleCues_.begin(),
            activeSubtitleCues_.end(),
            position,
            [](int value, const SubtitleCue& cue) { return value < cue.startMs; }
        );
        if (it == activeSubtitleCues_.begin()) return nullptr;
        --it;
        return position >= it->startMs && position < it->endMs ? &*it : nullptr;
    }

    void cyclePlaybackSpeed() {
        static constexpr std::array<float, 6> speeds{0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
        const float current = player_.playbackSpeed();
        size_t best = 0;
        for (size_t i = 1; i < speeds.size(); ++i) {
            if (std::abs(speeds[i] - current) < std::abs(speeds[best] - current)) best = i;
        }
        player_.setPlaybackSpeed(speeds[(best + 1) % speeds.size()]);
    }

    void cycleVideoZoom() {
        const int next = (static_cast<int>(videoZoomMode_) + 1) % 3;
        videoZoomMode_ = static_cast<VideoZoomMode>(next);
        settings_.zoomMode = next;
        saveSession(session_);
    }

    int currentChapterIndex() const {
        if (activePlaybackItem_.chapters.empty()) return -1;
        const int64_t positionTicks = static_cast<int64_t>(cachedPlaybackPositionMs_) * 10000;
        int current = 0;
        for (size_t i = 1; i < activePlaybackItem_.chapters.size(); ++i) {
            if (activePlaybackItem_.chapters[i].startTicks > positionTicks) break;
            current = static_cast<int>(i);
        }
        return current;
    }

    std::string chapterControlLabel() const {
        if (activePlaybackItem_.chapters.empty()) return "CHAPTER --";
        const int current = std::max(0, currentChapterIndex());
        return "CHAPTER " + std::to_string(current + 1) + "/" + std::to_string(activePlaybackItem_.chapters.size());
    }

    void cycleChapter() {
        if (activePlaybackItem_.chapters.empty()) return;
        const int current = std::max(0, currentChapterIndex());
        const int next = (current + 1) % static_cast<int>(activePlaybackItem_.chapters.size());
        const int targetMs = static_cast<int>(activePlaybackItem_.chapters[static_cast<size_t>(next)].startTicks / 10000);
        seekPlaybackTo(targetMs);
        reportProgressAsync(false);
    }

    std::optional<JellyfinMediaSegment> activeSkippableSegment() const {
        const int64_t positionTicks = static_cast<int64_t>(cachedPlaybackPositionMs_) * 10000;
        for (const auto& segment : activeMediaSegments_) {
            if (segment.endTicks - segment.startTicks < 30000000) continue; // under 3 seconds
            if (positionTicks >= segment.startTicks && positionTicks < segment.endTicks - 5000000) return segment;
        }
        return std::nullopt;
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
        switch (playerControlSelection_) {
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
        playerOverlayUntil_ = std::chrono::steady_clock::now() + (playerControlsActive_ ? 10s : 5s);
        if (playerControlsActive_) {
            if (key == AKEYCODE_BACK || key == AKEYCODE_DPAD_DOWN) {
                playerControlsActive_ = false;
            } else if (key == AKEYCODE_DPAD_LEFT) {
                playerControlSelection_ = std::max(0, playerControlSelection_ - 1);
            } else if (key == AKEYCODE_DPAD_RIGHT) {
                playerControlSelection_ = std::min(static_cast<int>(playerControlCount()) - 1, playerControlSelection_ + 1);
            } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
                activatePlayerControl();
            }
            return;
        }
        if (key == AKEYCODE_BACK) {
            stopPlayback();
        } else if (key == AKEYCODE_DPAD_DOWN) {
            openQueueOverlay();
        } else if (key == AKEYCODE_MEDIA_NEXT && playbackQueueIndex_ >= 0) {
            const int next = queueNextIndex(
                playbackQueueIndex_,
                static_cast<int>(playbackQueue_.size()),
                queueRepeatMode_,
                true
            );
            if (next >= 0) playQueuedIndexAsync(next);
        } else if (key == AKEYCODE_DPAD_UP) {
            playerControlsActive_ = true;
            playerControlSelection_ = 0;
            playerOverlayUntil_ = std::chrono::steady_clock::now() + 10s;
        } else if ((key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) && skipActiveMediaSegment()) {
            // A visible media-segment action owns OK, matching TV skip-button behavior.
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER || key == AKEYCODE_MEDIA_PLAY_PAUSE) {
            player_.togglePause();
            reportProgressAsync(true);
        } else if (key == AKEYCODE_DPAD_LEFT || key == AKEYCODE_MEDIA_REWIND) {
            const int targetMs = std::max(0, cachedPlaybackPositionMs_ - settings_.seekBackSeconds * 1000);
            requestTrickplayPreview(targetMs);
            seekPlaybackTo(targetMs);
            reportProgressAsync(false);
        } else if (key == AKEYCODE_DPAD_RIGHT || key == AKEYCODE_MEDIA_FAST_FORWARD) {
            const int targetMs = cachedPlaybackPositionMs_ + settings_.seekForwardSeconds * 1000;
            requestTrickplayPreview(targetMs);
            seekPlaybackTo(targetMs);
            reportProgressAsync(false);
        }
    }

    void handleMediaSessionCommand(const MediaSessionCommand& command) {
        if (screen_ != Screen::Player) return;
        switch (command.type) {
            case MediaSessionCommandType::Play:
                if (player_.status() == PlayerStatus::Paused) {
                    player_.togglePause();
                    reportProgressAsync(true);
                }
                break;
            case MediaSessionCommandType::Pause:
                if (player_.status() == PlayerStatus::Playing) {
                    player_.togglePause();
                    reportProgressAsync(true);
                }
                break;
            case MediaSessionCommandType::Stop:
                stopPlayback();
                break;
            case MediaSessionCommandType::SeekTo: {
                const int64_t maxPosition = cachedPlaybackDurationMs_ > 0
                    ? cachedPlaybackDurationMs_
                    : static_cast<int64_t>(std::numeric_limits<int>::max());
                const int positionMs = static_cast<int>(std::clamp<int64_t>(command.positionMs, 0, maxPosition));
                requestTrickplayPreview(positionMs);
                seekPlaybackTo(positionMs);
                reportProgressAsync(false);
                break;
            }
            case MediaSessionCommandType::Next:
                if (playbackQueueIndex_ >= 0) {
                    const int next = queueNextIndex(
                        playbackQueueIndex_,
                        static_cast<int>(playbackQueue_.size()),
                        queueRepeatMode_,
                        true
                    );
                    if (next >= 0) playQueuedIndexAsync(next);
                }
                break;
            case MediaSessionCommandType::Previous:
                if (playbackQueueIndex_ > 0) {
                    playQueuedIndexAsync(playbackQueueIndex_ - 1);
                } else {
                    requestTrickplayPreview(0);
                    seekPlaybackTo(0);
                    reportProgressAsync(false);
                }
                break;
        }
    }

    void openSettings() {
        refreshExternalPlayers();
        pushScreen(Screen::Settings);
        settingsSelection_ = 0;
        error_.clear();
    }

    void openDiagnostics() {
        if (!session_.valid()) return;
        pushScreen(Screen::Diagnostics);
        loading_ = true;
        error_.clear();
        serverInfo_ = {};
        const JellyfinSession session = session_;
        tasks_.submit([this, session] {
            auto result = api_.getServerInfo(session);
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
        ++taskGeneration_;
        session_ = {};
        home_ = {};
        artwork_.clear();
        homeArtwork_.clear();
        backdrops_.clear();
        loginFields_[1].clear();
        loginFields_[2].clear();
        homeSelection_.clear();
        loading_ = false;
        error_.clear();
    }

    void openProfiles() {
        if (savedSessions_.empty()) {
            startAddAccount();
            return;
        }
        pushScreen(Screen::Profiles);
        profilesSelection_ = std::clamp(profilesSelection_, 0, static_cast<int>(savedSessions_.size()));
        profileAction_ = 0;
        error_.clear();
    }

    void startAddAccount() {
        const std::string existingServer = session_.server;
        clearCurrentSessionUi();
        if (!existingServer.empty()) loginFields_[0] = existingServer;
        resetNavigation(Screen::Login);
        loginField_ = 0;
        saveSession(session_);
    }

    void switchSavedSession(size_t index) {
        if (index >= savedSessions_.size()) return;
        clearCurrentSessionUi();
        session_ = savedSessions_[index];
        session_.deviceId = deviceId_;
        loginFields_[0] = session_.server;
        loginFields_[1] = session_.username;
        resetNavigation(Screen::Home);
        homeRow_ = 0;
        saveSession(session_);
        loadHomeAsync();
    }

    void forgetSavedSession(size_t index) {
        if (index >= savedSessions_.size()) return;
        const JellyfinSession removed = savedSessions_[index];
        const bool removedCurrent = sameSessionIdentity(session_, removed);
        savedSessions_.erase(savedSessions_.begin() + static_cast<std::ptrdiff_t>(index));
        if (removedCurrent) {
            session_ = {};
            resetNavigation(Screen::Profiles);
        }
        profilesSelection_ = std::clamp(profilesSelection_, 0, static_cast<int>(savedSessions_.size()));
        profileAction_ = 0;
        saveSession(session_);
        if (savedSessions_.empty()) startAddAccount();
    }

    void openLibraries() {
        pushScreen(Screen::Libraries);
        librarySelection_ = std::clamp(librarySelection_, 0, std::max(0, static_cast<int>(home_.views.size()) - 1));
        error_.clear();
    }

    static constexpr int kBrowsePageSize = 60;

    void openLibrary(const JellyfinItem& library) {
        if (loading_ || library.id.empty()) return;
        browseStack_.clear();
        browseFilterFocused_ = false;
        browseFilterSelection_ = 0;
        browseContentMode_ = BrowseContentMode::All;
        browseGenre_.clear();
        browseLetter_.clear();
        openBrowseContainer(library, false);
    }

    void openLibraryByCollectionType(const std::string& collectionType) {
        const auto library = std::find_if(home_.views.begin(), home_.views.end(), [&](const JellyfinItem& view) {
            return view.collectionType == collectionType;
        });
        if (library == home_.views.end()) {
            error_ = collectionType == "movies" ? "NO MOVIE LIBRARY FOUND" : "NO SHOW LIBRARY FOUND";
            openLibraries();
            return;
        }
        openLibrary(*library);
    }

    bool browseHasFilterBar() const {
        return browseStack_.empty() && (activeLibrary_.collectionType == "movies"
            || activeLibrary_.collectionType == "tvshows" || activeLibrary_.collectionType == "mixed");
    }

    std::vector<std::string> browseFilterLabels() const {
        std::vector<std::string> labels{"ALL", "FAVORITES", "GENRES", "A-Z"};
        if (activeLibrary_.collectionType == "movies") labels.emplace_back("COLLECTIONS");
        return labels;
    }

    void populateLetterChoices() {
        browseItems_.clear();
        browseItems_.reserve(26);
        for (char c = 'A'; c <= 'Z'; ++c) {
            JellyfinItem item;
            item.id = std::string(1, c);
            item.name = item.id;
            item.type = "Letter";
            browseItems_.push_back(std::move(item));
        }
        browseSelection_ = 0;
        browseNextIndex_ = static_cast<int>(browseItems_.size());
        browseHasMore_ = false;
        loading_ = false;
        error_.clear();
    }

    void applyBrowseFilter(int selection) {
        if (loading_ || !browseHasFilterBar()) return;
        const auto labels = browseFilterLabels();
        if (labels.empty()) return;
        browseFilterSelection_ = std::clamp(selection, 0, static_cast<int>(labels.size()) - 1);
        browseItems_.clear();
        browseSelection_ = 0;
        browseNextIndex_ = 0;
        browseHasMore_ = false;
        browseGenre_.clear();
        browseLetter_.clear();
        switch (browseFilterSelection_) {
            case 1: browseContentMode_ = BrowseContentMode::Favorites; break;
            case 2: browseContentMode_ = BrowseContentMode::Genres; break;
            case 3:
                browseContentMode_ = BrowseContentMode::Letters;
                populateLetterChoices();
                return;
            case 4: browseContentMode_ = BrowseContentMode::Collections; break;
            default: browseContentMode_ = BrowseContentMode::All; break;
        }
        loadBrowsePageAsync(false);
    }

    void openBrowseContainer(const JellyfinItem& container, bool pushCurrent) {
        if (loading_ || container.id.empty()) return;
        if (pushCurrent && !activeLibrary_.id.empty()) {
            browseStack_.push_back(BrowseSnapshot{
                activeLibrary_,
                std::move(browseItems_),
                browseSelection_,
                browseNextIndex_,
                browseHasMore_,
            });
        }
        pushScreen(Screen::Browse);
        activeLibrary_ = container;
        browseItems_.clear();
        browseSelection_ = 0;
        browseNextIndex_ = 0;
        browseHasMore_ = false;
        loadBrowsePageAsync(false);
    }

    void loadMoreBrowseAsync() {
        if (loading_ || !browseHasMore_ || activeLibrary_.id.empty()) return;
        loadBrowsePageAsync(true);
    }

    void loadBrowsePageAsync(bool append) {
        if (loading_ || activeLibrary_.id.empty()) return;
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem container = activeLibrary_;
        const int startIndex = append ? browseNextIndex_ : 0;
        const uint64_t generation = ++taskGeneration_;
        const BrowseContentMode mode = browseContentMode_;
        const std::string genre = browseGenre_;
        const std::string letter = browseLetter_;
        const bool nested = !browseStack_.empty();
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
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            const int received = static_cast<int>(result.value.size());
            if (!append) {
                browseItems_ = std::move(result.value);
                browseSelection_ = 0;
            } else {
                browseItems_.insert(
                    browseItems_.end(),
                    std::make_move_iterator(result.value.begin()),
                    std::make_move_iterator(result.value.end())
                );
            }
            browseNextIndex_ = startIndex + received;
            browseHasMore_ = mode != BrowseContentMode::Genres && received == kBrowsePageSize;
            error_.clear();
        });
    }

    void openSeasons() {
        if (loading_ || detail_.id.empty() || detail_.type != "Series") return;
        seriesDetail_ = detail_;
        seasonItems_.clear();
        episodeItems_.clear();
        seasonSelection_ = 0;
        pushScreen(Screen::Seasons);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string seriesId = seriesDetail_.id;
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, seriesId, generation] {
            auto result = api_.getSeasons(session, seriesId);
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            seasonItems_ = std::move(result.value);
            seasonSelection_ = 0;
        });
    }

    void openEpisodes(const JellyfinItem& season) {
        if (loading_ || seriesDetail_.id.empty() || season.id.empty()) return;
        selectedSeason_ = season;
        episodeItems_.clear();
        episodeSelection_ = 0;
        pushScreen(Screen::Episodes);
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string seriesId = seriesDetail_.id;
        const std::string seasonId = season.id;
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, seriesId, seasonId, generation] {
            auto result = api_.getEpisodes(session, seriesId, seasonId);
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            episodeItems_ = std::move(result.value);
            episodeSelection_ = 0;
        });
    }

    void updateCachedUserData(const JellyfinItem& updated) {
        auto apply = [&](JellyfinItem& item) {
            if (item.id != updated.id) return;
            item.favorite = updated.favorite;
            item.played = updated.played;
            item.positionTicks = updated.positionTicks;
        };
        for (auto& row : home_.rows) for (auto& item : row.items) apply(item);
        for (auto& item : browseItems_) apply(item);
        for (auto& item : searchResults_) apply(item);
        for (auto& item : detailsSimilar_) apply(item);
        for (auto& item : seasonItems_) apply(item);
        for (auto& item : episodeItems_) apply(item);

        for (auto& row : home_.rows) {
            if (row.title == "FAVORITES") {
                const auto existing = std::find_if(row.items.begin(), row.items.end(), [&](const JellyfinItem& item) {
                    return item.id == updated.id;
                });
                if (updated.favorite && existing == row.items.end()) row.items.push_back(updated);
                else if (!updated.favorite && existing != row.items.end()) row.items.erase(existing);
            } else if (row.title == "CONTINUE WATCHING" && updated.played) {
                std::erase_if(row.items, [&](const JellyfinItem& item) { return item.id == updated.id; });
            }
        }
    }

    void toggleFavoriteAsync() {
        if (loading_ || detail_.id.empty()) return;
        const bool desired = !detail_.favorite;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        loading_ = true;
        error_.clear();
        tasks_.submit([this, session, item, desired] {
            auto result = api_.setFavorite(session, item, desired);
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            if (detail_.id == item.id) {
                detail_.favorite = desired;
                updateCachedUserData(detail_);
            }
        });
    }

    void togglePlayedAsync() {
        if (loading_ || detail_.id.empty()) return;
        const bool desired = !detail_.played;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        loading_ = true;
        error_.clear();
        tasks_.submit([this, session, item, desired] {
            auto result = api_.setPlayed(session, item, desired);
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            if (detail_.id == item.id) {
                detail_.played = desired;
                if (desired) detail_.positionTicks = 0;
                updateCachedUserData(detail_);
            }
        });
    }

    void removeCachedItem(const std::string& itemId) {
        if (itemId.empty()) return;
        auto remove = [&](auto& items) {
            std::erase_if(items, [&](const JellyfinItem& item) { return item.id == itemId; });
        };
        for (auto& row : home_.rows) remove(row.items);
        remove(browseItems_);
        remove(searchResults_);
        remove(detailsSimilar_);
        remove(seasonItems_);
        remove(episodeItems_);
        for (auto& snapshot : browseStack_) remove(snapshot.items);
    }

    void refreshCurrentItemMetadataAsync() {
        if (loading_ || detail_.id.empty()) return;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        loading_ = true;
        error_.clear();
        tasks_.submit([this, session, item] {
            auto result = api_.refreshMetadata(session, item);
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            if (screen_ == Screen::ItemMenu && detail_.id == item.id) {
                popScreen(Screen::Details);
                error_ = "METADATA REFRESH REQUESTED";
            }
        });
    }

    void deleteCurrentItemAsync() {
        if (loading_ || detail_.id.empty() || !detail_.canDelete) return;
        const JellyfinSession session = session_;
        const JellyfinItem item = detail_;
        loading_ = true;
        error_.clear();
        tasks_.submit([this, session, item] {
            auto result = api_.deleteItem(session, item);
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                deleteConfirmation_ = false;
                deleteConfirmationSelection_ = 1;
                return;
            }
            removeCachedItem(item.id);
            if (detail_.id == item.id) detail_ = {};
            deleteConfirmation_ = false;
            deleteConfirmationSelection_ = 1;
            if (screen_ == Screen::ItemMenu) {
                popScreen(Screen::Details);
                popScreen(Screen::Home);
            }
        });
    }

    void openSearch() {
        pushScreen(Screen::Search);
        searchKeyboard_ = true;
        keyboardRow_ = keyboardCol_ = 0;
        error_.clear();
    }

    void discoverServersAsync() {
        if (loading_) return;
        loading_ = true;
        error_.clear();
        discoveryStatus_ = "SEARCHING LOCAL NETWORK...";
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, generation] {
            auto servers = discoverJellyfinServers(1600);
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (servers.empty()) {
                discoveryStatus_.clear();
                error_ = "NO JELLYFIN SERVER FOUND ON THIS NETWORK";
                return;
            }
            loginFields_[0] = servers.front().address;
            discoveryStatus_ = "FOUND " + (servers.front().name.empty() ? std::string("JELLYFIN") : servers.front().name);
            if (servers.size() > 1) discoveryStatus_ += " + " + std::to_string(servers.size() - 1) + " MORE";
            loginField_ = 1;
            error_.clear();
        });
    }

    void loginAsync() {
        if (loading_) return;
        loading_ = true;
        error_.clear();
        const auto fields = loginFields_;
        const std::string deviceId = deviceId_;
        const uint64_t generation = ++taskGeneration_;

        tasks_.submit([this, fields, deviceId, generation] {
            auto result = api_.login(fields[0], fields[1], fields[2], deviceId);
            if (generation != taskGeneration_.load()) return;
            if (!result.ok) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                error_ = result.error;
                return;
            }
            {
                std::scoped_lock lock(stateMutex_);
                session_ = result.value;
                loginFields_[0] = session_.server;
                loginFields_[1] = session_.username;
                loginFields_[2].clear();
                resetNavigation(Screen::Home);
                loading_ = false;
                homeRow_ = 0;
                error_.clear();
                saveSession(session_);
            }
            loadHomeAsync();
        });
    }

    void quickConnectAsync() {
        if (loading_ || quickConnectActive_) return;
        if (loginFields_[0].empty()) {
            error_ = "ENTER THE JELLYFIN SERVER ADDRESS FIRST";
            loginField_ = 0;
            return;
        }

        const std::string server = loginFields_[0];
        const std::string deviceId = deviceId_;
        loading_ = true;
        quickConnectActive_ = true;
        quickConnectCode_ = "------";
        error_.clear();
        const uint64_t generation = ++taskGeneration_;

        tasks_.submit([this, server, deviceId, generation] {
            auto initiated = api_.initiateQuickConnect(server, deviceId);
            if (generation != taskGeneration_.load()) return;
            if (!initiated.ok) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                quickConnectActive_ = false;
                quickConnectCode_.clear();
                error_ = initiated.error;
                return;
            }

            const QuickConnectRequest request = initiated.value;
            {
                std::scoped_lock lock(stateMutex_);
                loginFields_[0] = request.server;
                quickConnectCode_ = request.code;
                loading_ = false;
            }

            for (int attempt = 0; attempt < 60; ++attempt) {
                std::this_thread::sleep_for(5s);
                if (generation != taskGeneration_.load()) return;

                auto state = api_.pollQuickConnect(request, deviceId);
                if (!state.ok) {
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    quickConnectActive_ = false;
                    quickConnectCode_.clear();
                    error_ = state.error;
                    return;
                }
                if (!state.value) continue;

                auto authenticated = api_.completeQuickConnect(request, deviceId);
                if (generation != taskGeneration_.load()) return;
                if (!authenticated.ok) {
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    quickConnectActive_ = false;
                    quickConnectCode_.clear();
                    error_ = authenticated.error;
                    return;
                }

                {
                    std::scoped_lock lock(stateMutex_);
                    session_ = authenticated.value;
                    loginFields_[0] = session_.server;
                    loginFields_[1] = session_.username;
                    loginFields_[2].clear();
                    quickConnectActive_ = false;
                    quickConnectCode_.clear();
                    loading_ = false;
                    resetNavigation(Screen::Home);
                    homeRow_ = 0;
                    error_.clear();
                    saveSession(session_);
                }
                loadHomeAsync();
                return;
            }

            if (generation == taskGeneration_.load()) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                quickConnectActive_ = false;
                quickConnectCode_.clear();
                error_ = "QUICK CONNECT TIMED OUT - TRY AGAIN";
            }
        });
    }

    void loadHomeAsync() {
        const JellyfinSession session = session_;
        if (!session.valid()) return;

        bool navFocused = false;
        std::string focusedRowTitle;
        std::unordered_map<std::string, std::string> selectedItemByRow;
        {
            std::scoped_lock lock(stateMutex_);
            navFocused = homeRow_ < 0;
            if (!navFocused && homeRow_ < static_cast<int>(home_.rows.size())) {
                focusedRowTitle = home_.rows[static_cast<size_t>(homeRow_)].title;
            }
            for (size_t row = 0; row < home_.rows.size() && row < homeSelection_.size(); ++row) {
                const auto& items = home_.rows[row].items;
                if (items.empty()) continue;
                const int selected = std::clamp(homeSelection_[row], 0, static_cast<int>(items.size()) - 1);
                selectedItemByRow[home_.rows[row].title] = items[static_cast<size_t>(selected)].id;
            }
            loading_ = true;
        }

        const uint64_t generation = ++taskGeneration_;
        const auto homeLoadStarted = std::chrono::steady_clock::now();
        tasks_.submit([this, session, generation, navFocused, focusedRowTitle, selectedItemByRow = std::move(selectedItemByRow), homeLoadStarted] {
            auto core = api_.loadHomeCore(session);
            if (generation != taskGeneration_.load()) return;
            if (!core.ok) {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                if (core.error.find("HTTP 401") != std::string::npos) {
                    const JellyfinSession expired = session_;
                    removeSavedSessionIdentity(expired);
                    session_.token.clear();
                    session_.userId.clear();
                    resetNavigation(Screen::Login);
                    error_ = "SESSION EXPIRED - LOG IN AGAIN";
                    saveSession(session_);
                } else {
                    error_ = core.error;
                }
                return;
            }

            std::vector<JellyfinItem> views = core.value.views;
            int coreRestoredRow = navFocused ? -1 : 0;
            {
                std::scoped_lock lock(stateMutex_);
                std::vector<int> restoredSelections(core.value.rows.size(), 0);
                for (size_t row = 0; row < core.value.rows.size(); ++row) {
                    const auto& section = core.value.rows[row];
                    if (!navFocused && section.title == focusedRowTitle) coreRestoredRow = static_cast<int>(row);
                    const auto saved = selectedItemByRow.find(section.title);
                    if (saved == selectedItemByRow.end()) continue;
                    const auto item = std::find_if(section.items.begin(), section.items.end(), [&](const JellyfinItem& candidate) {
                        return candidate.id == saved->second;
                    });
                    if (item != section.items.end()) restoredSelections[row] = static_cast<int>(std::distance(section.items.begin(), item));
                }

                loading_ = false;
                home_ = std::move(core.value);
                homeSelection_ = std::move(restoredSelections);
                homeRow_ = home_.rows.empty() ? -1 : std::clamp(coreRestoredRow, -1, static_cast<int>(home_.rows.size()) - 1);
                error_ = home_.warning;
                if (homeRow_ >= 0) prefetchHomeWindow(homeRow_, homeSelection_[static_cast<size_t>(homeRow_)]);
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
                    searchQuery_ = std::move(pendingSearchQuery_);
                    pendingSearchQuery_.clear();
                    searchKeyboard_ = false;
                    pushScreen(Screen::Search);
                    __android_log_print(ANDROID_LOG_INFO, kTag, "Opening ACTION_SEARCH query");
                    searchAsync();
                    return;
                }
            }

            tasks_.submit([this, session, generation, views = std::move(views), navFocused, focusedRowTitle, selectedItemByRow, coreRestoredRow, homeLoadStarted] {
                auto secondary = api_.loadHomeSecondary(session, views);
                if (generation != taskGeneration_.load()) return;
                std::scoped_lock lock(stateMutex_);
                if (!secondary.ok) {
                    if (!error_.empty()) error_ += " | ";
                    error_ += "SECONDARY HOME ROWS UNAVAILABLE";
                    return;
                }

                const size_t baseRowCount = home_.rows.size();
                for (auto& section : secondary.value.rows) {
                    int restoredSelection = 0;
                    const auto saved = selectedItemByRow.find(section.title);
                    if (saved != selectedItemByRow.end()) {
                        const auto item = std::find_if(section.items.begin(), section.items.end(), [&](const JellyfinItem& candidate) {
                            return candidate.id == saved->second;
                        });
                        if (item != section.items.end()) restoredSelection = static_cast<int>(std::distance(section.items.begin(), item));
                    }
                    if (!navFocused && section.title == focusedRowTitle && homeRow_ == coreRestoredRow) {
                        homeRow_ = static_cast<int>(home_.rows.size());
                    }
                    home_.rows.push_back(std::move(section));
                    homeSelection_.push_back(restoredSelection);
                }
                if (!secondary.value.warning.empty()) {
                    if (!home_.warning.empty()) home_.warning += " | ";
                    home_.warning += secondary.value.warning;
                    error_ = home_.warning;
                }
                if (homeRow_ >= static_cast<int>(baseRowCount) && homeRow_ < static_cast<int>(homeSelection_.size())) {
                    prefetchHomeWindow(homeRow_, homeSelection_[static_cast<size_t>(homeRow_)]);
                }
                const auto fullMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - homeLoadStarted).count();
                __android_log_print(ANDROID_LOG_INFO, kTag, "Home enrichment completed in %lld ms", static_cast<long long>(fullMs));
            });
        });
    }

    void searchAsync() {
        if (!session_.valid() || loading_) return;
        const JellyfinSession session = session_;
        const std::string query = searchQuery_;
        loading_ = true;
        error_.clear();
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, query, generation] {
            auto result = api_.search(session, query);
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!result.ok) {
                error_ = result.error;
                return;
            }
            searchResults_ = std::move(result.value);
            searchSelection_ = 0;
            error_.clear();
        });
    }

    void openDetails(const JellyfinItem& item) {
        pushScreen(Screen::Details);
        stillWatchingPrompt_ = false;
        detail_ = item;
        detailsButton_ = 0;
        detailsSimilar_.clear();
        detailsSimilarSelection_ = 0;
        detailsSimilarFocused_ = false;
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const std::string id = item.id;
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, id, generation] {
            auto result = api_.getItem(session, id);
            if (generation != taskGeneration_.load()) return;
            {
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
                if (!result.ok) {
                    error_ = "DETAILS: " + result.error;
                    return;
                }
                detail_ = std::move(result.value);
            }

            auto similar = api_.getSimilar(session, id, 18);
            if (generation != taskGeneration_.load() || !similar.ok) return;
            std::scoped_lock lock(stateMutex_);
            if (screen_ != Screen::Details || detail_.id != id) return;
            detailsSimilar_ = std::move(similar.value);
            detailsSimilarSelection_ = 0;
        });
    }

    void syncNextPlaybackFromQueue() {
        const int size = static_cast<int>(playbackQueue_.size());
        const int next = queueNextIndex(playbackQueueIndex_, size, queueRepeatMode_, false);
        if (next >= 0) nextPlaybackItem_ = playbackQueue_[static_cast<size_t>(next)];
        else nextPlaybackItem_.reset();
    }

    void shuffleRemainingQueue() {
        const int size = static_cast<int>(playbackQueue_.size());
        const int begin = queueShuffleBegin(playbackQueueIndex_, size);
        if (!queueCanShuffle(playbackQueueIndex_, size)) return;
        static thread_local std::mt19937 generator(std::random_device{}());
        std::shuffle(playbackQueue_.begin() + begin, playbackQueue_.end(), generator);
        queueSelection_ = queueDefaultSelection(playbackQueueIndex_, size);
        syncNextPlaybackFromQueue();
    }

    void openQueueOverlay() {
        if (playbackQueue_.empty()) {
            error_ = "QUEUE IS EMPTY";
            return;
        }
        queueOverlayActive_ = true;
        queueSelection_ = queueDefaultSelection(playbackQueueIndex_, static_cast<int>(playbackQueue_.size()));
        queueActionSelection_ = 0;
        error_.clear();
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 10s;
    }

    void moveQueuedItem(int from, int to) {
        const int size = static_cast<int>(playbackQueue_.size());
        if (from < 0 || from >= size || to < 0 || to >= size || from == to) return;
        JellyfinItem item = std::move(playbackQueue_[static_cast<size_t>(from)]);
        playbackQueue_.erase(playbackQueue_.begin() + from);
        playbackQueue_.insert(playbackQueue_.begin() + to, std::move(item));
        queueSelection_ = to;
        syncNextPlaybackFromQueue();
    }

    void playQueuedIndexAsync(int index, bool restartCurrent = false) {
        const int size = static_cast<int>(playbackQueue_.size());
        if (loading_ || index < 0 || index >= size || !session_.valid()) return;
        if (index == playbackQueueIndex_ && screen_ == Screen::Player && !restartCurrent) {
            queueOverlayActive_ = false;
            return;
        }

        const bool replacingPlayer = screen_ == Screen::Player && !activePlaybackItem_.id.empty();
        if (replacingPlayer) releaseActivePlayback(true);
        playbackQueueIndex_ = index;
        queueOverlayActive_ = false;
        loading_ = true;
        nextTransitionLoading_ = replacingPlayer;
        stillWatchingPrompt_ = false;
        error_.clear();
        const JellyfinSession session = session_;
        JellyfinItem queued = playbackQueue_[static_cast<size_t>(index)];
        if (restartCurrent) queued.positionTicks = 0;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const auto subtitlePreference = playbackSubtitleLanguagePreference_;
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, queued = std::move(queued), index, replacingPlayer, maxStreamingBitrate, maxAudioChannels, playbackOverrides, subtitlePreference, generation]() mutable {
            auto detailed = api_.getItem(session, queued.id);
            if (detailed.ok) queued = std::move(detailed.value);
            const int subtitleStreamIndex = subtitleIndexForPlaybackItem(queued, subtitlePreference);
            auto target = api_.resolvePlayback(
                session,
                queued,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                -1,
                subtitleStreamIndex
            );
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            nextTransitionLoading_ = false;
            if (index != playbackQueueIndex_) return;
            if (!target.ok) {
                if (replacingPlayer && screen_ == Screen::Player) popScreen(Screen::Details);
                error_ = "QUEUE: " + target.error;
                return;
            }
            playbackQueue_[static_cast<size_t>(index)] = queued;
            pendingPlayback_ = std::move(target.value);
            pendingPlaybackItem_ = std::move(queued);
        });
    }

    void handleQueueOverlayKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            queueOverlayActive_ = false;
            return;
        }
        const int size = static_cast<int>(playbackQueue_.size());
        if (size <= 0) {
            queueOverlayActive_ = false;
            return;
        }
        const int minimumSelection = std::clamp(playbackQueueIndex_, 0, size - 1);
        if (key == AKEYCODE_DPAD_UP) {
            queueSelection_ = std::max(minimumSelection, queueSelection_ - 1);
            return;
        }
        if (key == AKEYCODE_DPAD_DOWN) {
            queueSelection_ = std::min(size - 1, queueSelection_ + 1);
            return;
        }
        if (key == AKEYCODE_DPAD_LEFT) {
            queueActionSelection_ = std::max(0, queueActionSelection_ - 1);
            return;
        }
        if (key == AKEYCODE_DPAD_RIGHT) {
            queueActionSelection_ = std::min(6, queueActionSelection_ + 1);
            return;
        }
        if (key != AKEYCODE_DPAD_CENTER && key != AKEYCODE_ENTER) return;

        if (queueActionSelection_ == 0) {
            if (queueCanPlayNow(queueSelection_, playbackQueueIndex_, size)) playQueuedIndexAsync(queueSelection_);
        } else if (queueActionSelection_ == 1) {
            if (queueCanPlayNext(queueSelection_, playbackQueueIndex_, size)) {
                moveQueuedItem(queueSelection_, playbackQueueIndex_ + 1);
            }
        } else if (queueActionSelection_ == 2) {
            if (queueCanMoveUp(queueSelection_, playbackQueueIndex_, size)) moveQueuedItem(queueSelection_, queueSelection_ - 1);
        } else if (queueActionSelection_ == 3) {
            if (queueCanMoveDown(queueSelection_, playbackQueueIndex_, size)) moveQueuedItem(queueSelection_, queueSelection_ + 1);
        } else if (queueActionSelection_ == 4 && queueCanRemove(queueSelection_, playbackQueueIndex_, size)) {
            playbackQueue_.erase(playbackQueue_.begin() + queueSelection_);
            queueSelection_ = std::min(queueSelection_, static_cast<int>(playbackQueue_.size()) - 1);
            syncNextPlaybackFromQueue();
        } else if (queueActionSelection_ == 5) {
            shuffleRemainingQueue();
        } else if (queueActionSelection_ == 6) {
            queueRepeatMode_ = nextQueueRepeatMode(queueRepeatMode_);
            syncNextPlaybackFromQueue();
        }
    }

    void beginSeriesPlayAll() {
        if (loading_ || detail_.type != "Series" || detail_.id.empty() || !session_.valid()) return;
        loading_ = true;
        error_.clear();
        autoplayChainCount_ = 0;
        stillWatchingPrompt_ = false;
        playbackSubtitleLanguagePreference_.reset();
        const JellyfinSession session = session_;
        const JellyfinItem series = detail_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const uint64_t generation = ++taskGeneration_;
        tasks_.submit([this, session, series, maxStreamingBitrate, maxAudioChannels, playbackOverrides, generation] {
            auto episodes = api_.getSeriesEpisodes(session, series.id, 1000);
            if (!episodes.ok || episodes.value.empty()) {
                if (generation != taskGeneration_.load()) return;
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
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
                if (generation != taskGeneration_.load()) return;
                std::scoped_lock lock(stateMutex_);
                loading_ = false;
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
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!target.ok) {
                error_ = "PLAY ALL: " + target.error;
                return;
            }
            playbackQueue_ = std::move(episodes.value);
            playbackQueue_[0] = first;
            playbackQueueIndex_ = 0;
            queueRepeatMode_ = QueueRepeatMode::Off;
            queueSelection_ = queueDefaultSelection(0, static_cast<int>(playbackQueue_.size()));
            queueActionSelection_ = 0;
            queueOverlayActive_ = false;
            pendingPlayback_ = std::move(target.value);
            pendingPlaybackItem_ = std::move(first);
        });
    }

    void beginPlayback() {
        if (loading_ || detail_.id.empty()) return;
        const bool continuingPlaybackChain = stillWatchingPrompt_;
        const bool continuingQueuedPrompt = continuingPlaybackChain && !playbackQueue_.empty();
        if (!continuingPlaybackChain) playbackSubtitleLanguagePreference_.reset();
        if (continuingQueuedPrompt) {
            const auto queued = std::find_if(playbackQueue_.begin(), playbackQueue_.end(), [&](const JellyfinItem& item) {
                return item.id == detail_.id;
            });
            if (queued != playbackQueue_.end()) {
                playbackQueueIndex_ = static_cast<int>(std::distance(playbackQueue_.begin(), queued));
            } else {
                playbackQueue_.clear();
                playbackQueueIndex_ = -1;
                queueRepeatMode_ = QueueRepeatMode::Off;
            }
        } else {
            playbackQueue_.clear();
            playbackQueueIndex_ = -1;
            queueRepeatMode_ = QueueRepeatMode::Off;
        }
        autoplayChainCount_ = 0;
        stillWatchingPrompt_ = false;
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem selected = detail_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const auto subtitlePreference = playbackSubtitleLanguagePreference_;
        const uint64_t generation = ++taskGeneration_;

        tasks_.submit([this, session, selected, maxStreamingBitrate, maxAudioChannels, playbackOverrides, subtitlePreference, generation] {
            JellyfinItem playable = selected;
            if (selected.type == "Series") {
                auto next = api_.getNextUpForSeries(session, selected.id);
                if (!next.ok) {
                    if (generation != taskGeneration_.load()) return;
                    std::scoped_lock lock(stateMutex_);
                    loading_ = false;
                    error_ = next.error;
                    return;
                }
                playable = std::move(next.value);
                auto detailed = api_.getItem(session, playable.id);
                if (detailed.ok) playable = std::move(detailed.value);
            }

            const int subtitleStreamIndex = subtitleIndexForPlaybackItem(playable, subtitlePreference);
            auto target = api_.resolvePlayback(
                session,
                playable,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                -1,
                subtitleStreamIndex
            );
            if (generation != taskGeneration_.load()) return;
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            if (!target.ok) {
                error_ = target.error;
                return;
            }
            pendingPlayback_ = std::move(target.value);
            pendingPlaybackItem_ = std::move(playable);
        });
    }

    void requestMediaSegmentsAsync() {
        if (mediaSegmentsRequested_ || !session_.valid() || activePlaybackItem_.id.empty()) return;
        mediaSegmentsRequested_ = true;
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
            activeMediaSegments_ = std::move(result.value);
            __android_log_print(ANDROID_LOG_INFO, kTag, "Loaded %zu media segments", activeMediaSegments_.size());
        });
    }

    void requestNextEpisodeAsync() {
        if (playbackQueueIndex_ >= 0 && playbackQueueIndex_ < static_cast<int>(playbackQueue_.size())) {
            nextEpisodeRequested_ = true;
            syncNextPlaybackFromQueue();
            return;
        }
        if (nextEpisodeRequested_ || !session_.valid() || activePlaybackItem_.type != "Episode"
            || activePlaybackItem_.seriesId.empty() || activePlaybackItem_.id.empty()) {
            return;
        }
        nextEpisodeRequested_ = true;
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
            nextPlaybackItem_ = std::move(item);
        });
    }

    void releaseActivePlayback(bool reportStop) {
        if (player_.status() == PlayerStatus::Playing || player_.status() == PlayerStatus::Paused) {
            refreshPlaybackTelemetry(true);
        }
        const int64_t ticks = playbackTicksFromPositionMs(cachedPlaybackPositionMs_);
        const auto session = session_;
        const auto item = activePlaybackItem_;
        const auto target = activeTarget_;
        const bool shouldReport = reportStop && playbackStartReported_ && session.valid()
            && !item.id.empty() && !target.url.empty();
        if (!item.id.empty() && detail_.id == item.id) {
            detail_.positionTicks = ticks;
        }
        player_.stop();
        videoSurface_.release();
        displayMode_.restore();
        mediaSession_.clear();
        clearTrickplayPreview();
        playbackStartReported_ = false;
        playbackFallbackResolving_ = false;
        activeTarget_ = {};
        activePlaybackItem_ = {};
        cachedPlaybackPositionMs_ = 0;
        cachedPlaybackDurationMs_ = 0;
        lastPlaybackTelemetryRead_ = {};
        lastPlaybackDurationProbe_ = {};
        nextEpisodeRequested_ = false;
        nextPlaybackItem_.reset();
        subtitleLoadInProgress_ = false;
        activeSubtitleCues_.clear();
        activeSubtitleLanguage_.clear();
        activeSubtitleServerIndex_ = -1;
        activeSubtitleEnabled_ = false;
        mediaSegmentsRequested_ = false;
        activeMediaSegments_.clear();
        if (shouldReport) {
            tasks_.submit([this, session, item, target, ticks] {
                api_.reportPlaybackStopped(session, item, target, ticks);
            });
        }
    }

    void queueAutoplayNext(JellyfinItem nextItem) {
        if (playbackQueueIndex_ >= 0 && playbackQueueIndex_ + 1 < static_cast<int>(playbackQueue_.size())
            && playbackQueue_[static_cast<size_t>(playbackQueueIndex_ + 1)].id == nextItem.id) {
            ++playbackQueueIndex_;
        }
        releaseActivePlayback(true);
        ++autoplayChainCount_;
        loading_ = true;
        nextTransitionLoading_ = true;
        detail_ = nextItem;
        stillWatchingPrompt_ = false;
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 10s;
        const JellyfinSession session = session_;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const PlaybackOverrides playbackOverrides = playbackOverridesFor(settings_);
        const auto subtitlePreference = playbackSubtitleLanguagePreference_;
        tasks_.submit([this, session, maxStreamingBitrate, maxAudioChannels, playbackOverrides, subtitlePreference, nextItem = std::move(nextItem)]() mutable {
            auto detailed = api_.getItem(session, nextItem.id);
            if (detailed.ok) nextItem = std::move(detailed.value);
            const int subtitleStreamIndex = subtitleIndexForPlaybackItem(nextItem, subtitlePreference);
            auto target = api_.resolvePlayback(
                session,
                nextItem,
                maxStreamingBitrate,
                maxAudioChannels,
                playbackOverrides,
                -1,
                subtitleStreamIndex
            );
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            nextTransitionLoading_ = false;
            if (!target.ok) {
                popScreen(Screen::Details);
                error_ = "NEXT EPISODE: " + target.error;
                return;
            }
            if (playbackQueueIndex_ >= 0 && playbackQueueIndex_ < static_cast<int>(playbackQueue_.size())
                && playbackQueue_[static_cast<size_t>(playbackQueueIndex_)].id == nextItem.id) {
                playbackQueue_[static_cast<size_t>(playbackQueueIndex_)] = nextItem;
            }
            pendingPlayback_ = std::move(target.value);
            pendingPlaybackItem_ = std::move(nextItem);
        });
    }

    void showStillWatching(JellyfinItem nextItem) {
        releaseActivePlayback(true);
        autoplayChainCount_ = 0;
        detail_ = std::move(nextItem);
        detailsButton_ = 0;
        popScreen(Screen::Details);
        stillWatchingPrompt_ = true;
        error_.clear();
    }

    void startResolvedPlaybackTarget(const PlaybackTarget& target) {
        player_.startAsync(
            target.url,
            videoSurface_.surface(),
            initialPlayerSeekMs(target.transcoding, target.startTicks)
        );
    }

    bool retryPlaybackWithoutSubtitle() {
        if (!shouldRetryFailedSubtitleTranscode(
                activeTarget_.playMethod == PlaybackMethod::Transcode,
                selectedSubtitleServerIndex_
            )) {
            return false;
        }
        __android_log_print(
            ANDROID_LOG_WARN,
            kTag,
            "Subtitle-selected transcode failed; retrying item without subtitles (stream %d)",
            selectedSubtitleServerIndex_
        );
        restartPlaybackAt(cachedPlaybackPositionMs_, selectedAudioServerIndex_, kSubtitleOffIndex);
        return true;
    }

    bool retryPlaybackWithTranscodeFallback() {
        if (playbackFallbackAttempted_ || activeTarget_.transcoding || !session_.valid() || activePlaybackItem_.id.empty()) {
            return false;
        }

        const PlaybackTarget failedTarget = activeTarget_;
        JellyfinItem item = activePlaybackItem_;
        const JellyfinSession session = session_;
        const int64_t resumeTicks = playbackTicksFromPositionMs(cachedPlaybackPositionMs_);
        const bool shouldReportPrevious = playbackStartReported_ && !failedTarget.url.empty();
        item.positionTicks = resumeTicks;

        player_.stop();
        videoSurface_.release();
        playbackStartReported_ = false;
        playbackFallbackAttempted_ = true;
        cachedPlaybackPositionMs_ = playbackPositionMsFromTicks(resumeTicks);
        cachedPlaybackDurationMs_ = playbackPositionMsFromTicks(item.runtimeTicks);
        lastPlaybackTelemetryRead_ = {};
        lastPlaybackDurationProbe_ = {};

        // Some PlaybackInfo responses include a TranscodingUrl beside DirectPlay. Use
        // that immediately when available; it avoids a second round-trip to Jellyfin.
        if (!activeTarget_.fallbackTranscodeUrl.empty()) {
            if (shouldReportPrevious) {
                tasks_.submit([this, session, item, failedTarget, resumeTicks] {
                    api_.reportPlaybackStopped(session, item, failedTarget, resumeTicks);
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
            playerOverlayUntil_ = std::chrono::steady_clock::now() + 5s;
            startResolvedPlaybackTarget(activeTarget_);
            return true;
        }

        // Jellyfin commonly omits TranscodingUrl when it selected DirectPlay, even when
        // SupportsTranscoding=true. Re-negotiate asynchronously with direct paths disabled
        // instead of abandoning playback after an Android MediaPlayer prepare failure.
        PlaybackOverrides fallbackOverrides = playbackOverridesFor(settings_);
        fallbackOverrides.forceTranscode = true;
        const int maxStreamingBitrate = settings_.maxBitrateMbps * 1000000;
        const int maxAudioChannels = settings_.maxAudioChannels;
        const int audioStreamIndex = selectedAudioServerIndex_;
        const int subtitleStreamIndex = selectedSubtitleServerIndex_;
        loading_ = true;
        playbackFallbackResolving_ = true;
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 10s;
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
            subtitleStreamIndex
        ]() mutable {
            if (shouldReportPrevious) {
                api_.reportPlaybackStopped(session, item, failedTarget, resumeTicks);
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
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            playbackFallbackResolving_ = false;
            if (screen_ != Screen::Player || activePlaybackItem_.id != item.id) return;
            if (!target.ok) {
                error_ = "TRANSCODE FALLBACK: " + target.error;
                stopPlayback();
                return;
            }
            pendingPlayback_ = std::move(target.value);
            pendingPlaybackItem_ = std::move(item);
            pendingStreamRestart_ = true;
            pendingRestartPaused_ = false;
            pendingAudioStreamIndex_ = audioStreamIndex;
        });
        if (!submitted) {
            loading_ = false;
            playbackFallbackResolving_ = false;
            error_ = "TRANSCODE FALLBACK COULD NOT BE STARTED";
            return false;
        }
        return true;
    }

    void tick() {
        const auto mediaSessionCommand = mediaSession_.takeCommand();
        if (mediaSessionCommand) handleMediaSessionCommand(*mediaSessionCommand);

        std::optional<PlaybackTarget> target;
        std::optional<PendingExternalLaunch> externalLaunch;
        std::optional<PendingExternalLaunch> completedExternalPlayback;
        const auto externalResult = externalPlayer_.takeResult();
        JellyfinItem item;
        bool streamRestart = false;
        {
            std::scoped_lock lock(stateMutex_);
            if (externalResult && activeExternalPlayback_) {
                completedExternalPlayback = std::move(activeExternalPlayback_);
                activeExternalPlayback_.reset();
            }
            if (pendingExternalLaunch_) {
                externalLaunch = std::move(pendingExternalLaunch_);
                pendingExternalLaunch_.reset();
            }
            if (pendingPlayback_ && app_->window) {
                target = std::move(pendingPlayback_);
                pendingPlayback_.reset();
                streamRestart = pendingStreamRestart_;
                pendingStreamRestart_ = false;
                pauseAfterRestart_ = streamRestart && pendingRestartPaused_;
                pendingRestartPaused_ = false;
                item = pendingPlaybackItem_;
                activePlaybackItem_ = item;
                activeTarget_ = *target;
                std::ostringstream playbackSummary;
                playbackSummary << playbackMethodName(target->playMethod);
                if (!item.videoCodec.empty()) playbackSummary << " / " << item.videoCodec;
                if (item.videoWidth > 0 && item.videoHeight > 0) playbackSummary << " / " << item.videoWidth << 'X' << item.videoHeight;
                lastPlaybackSummary_ = playbackSummary.str();
                playbackStartReported_ = false;
                playbackFallbackAttempted_ = false;
                lastProgressReport_ = std::chrono::steady_clock::now();
                lastPlaybackTelemetryRead_ = {};
                lastPlaybackDurationProbe_ = {};
                cachedPlaybackPositionMs_ = playbackPositionMsFromTicks(target->startTicks);
                cachedPlaybackDurationMs_ = playbackPositionMsFromTicks(item.runtimeTicks);
                videoZoomMode_ = static_cast<VideoZoomMode>(settings_.zoomMode);
                playerControlsActive_ = false;
                playerControlSelection_ = 0;
                if (!streamRestart) {
                    nextEpisodeRequested_ = false;
                    nextPlaybackItem_.reset();
                    syncNextPlaybackFromQueue();
                }
                subtitleLoadInProgress_ = false;
                activeSubtitleCues_.clear();
                activeSubtitleLanguage_.clear();
                activeSubtitleServerIndex_ = -1;
                activeSubtitleEnabled_ = false;
                selectedAudioServerIndex_ = pendingAudioStreamIndex_;
                if (selectedAudioServerIndex_ < 0 && !item.audios.empty()) {
                    const auto preferred = std::find_if(item.audios.begin(), item.audios.end(), [](const JellyfinAudioStream& audio) {
                        return audio.isDefault;
                    });
                    selectedAudioServerIndex_ = preferred == item.audios.end() ? item.audios.front().index : preferred->index;
                }
                selectedSubtitleServerIndex_ = target->subtitleStreamIndex;
                pendingAudioStreamIndex_ = -1;
                if (!streamRestart) {
                    mediaSegmentsRequested_ = false;
                    activeMediaSegments_.clear();
                }
                nextTransitionLoading_ = false;
                if (!streamRestart) {
                    if (screen_ == Screen::Player) replaceScreen(Screen::Player);
                    else pushScreen(Screen::Player);
                } else {
                    replaceScreen(Screen::Player);
                }
            }
        }

        if (externalResult && completedExternalPlayback) {
            if (!externalResult->success) {
                std::scoped_lock lock(stateMutex_);
                error_ = "EXTERNAL PLAYER REPORTED PLAYBACK FAILURE";
            } else {
                std::optional<int64_t> positionTicks;
                if (externalResult->positionMs >= 0) {
                    positionTicks = static_cast<int64_t>(externalResult->positionMs) * 10000;
                } else if (externalResult->completionKnown && externalResult->completed
                    && completedExternalPlayback->item.runtimeTicks > 0) {
                    positionTicks = completedExternalPlayback->item.runtimeTicks;
                }
                if (positionTicks) {
                    const int64_t boundedTicks = completedExternalPlayback->item.runtimeTicks > 0
                        ? std::clamp<int64_t>(*positionTicks, 0, completedExternalPlayback->item.runtimeTicks)
                        : std::max<int64_t>(0, *positionTicks);
                    positionTicks = boundedTicks;
                    std::scoped_lock lock(stateMutex_);
                    if (detail_.id == completedExternalPlayback->item.id) detail_.positionTicks = boundedTicks;
                }
                const JellyfinSession reportSession = session_;
                const JellyfinItem reportItem = completedExternalPlayback->item;
                tasks_.submit([this, reportSession, reportItem, positionTicks] {
                    const auto reported = api_.reportExternalPlaybackStopped(reportSession, reportItem, positionTicks);
                    if (!reported.ok) {
                        __android_log_print(ANDROID_LOG_WARN, kTag, "External playback stop report failed: %s", reported.error.c_str());
                    }
                });
                std::scoped_lock lock(stateMutex_);
                error_.clear();
            }
        }

        if (externalLaunch) {
            std::string launchError;
            const std::string title = externalLaunch->item.seriesName.empty()
                ? externalLaunch->item.name
                : externalLaunch->item.seriesName + " - " + externalLaunch->item.name;
            const int positionMs = playbackPositionMsFromTicks(externalLaunch->item.positionTicks);
            if (!externalPlayer_.launch(
                    externalLaunch->player,
                    externalLaunch->url,
                    title,
                    positionMs,
                    externalLaunch->subtitleUrl,
                    launchError
                )) {
                std::scoped_lock lock(stateMutex_);
                error_ = launchError.empty() ? "EXTERNAL PLAYER COULD NOT BE LAUNCHED" : launchError;
            } else {
                std::scoped_lock lock(stateMutex_);
                error_.clear();
                lastPlaybackSummary_ = "EXTERNAL / " + externalLaunch->player.label;
                activeExternalPlayback_ = std::move(externalLaunch);
            }
            return;
        }

        if (target) {
            if (streamRestart) {
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
                return;
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
                playbackPositionMsFromTicks(target->startTicks)
            );
            playerOverlayUntil_ = std::chrono::steady_clock::now() + 5s;
            startResolvedPlaybackTarget(*target);
            return;
        }

        if (screen_ != Screen::Player) return;
        PlayerStatus status = player_.status();
        if (status == PlayerStatus::Preparing) {
            mediaSession_.updateState(MediaSessionState::Buffering, cachedPlaybackPositionMs_);
        }
        if (status == PlayerStatus::Playing && pauseAfterRestart_) {
            player_.togglePause();
            pauseAfterRestart_ = false;
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
        if (status == PlayerStatus::Playing || status == PlayerStatus::Paused) {
            refreshPlaybackTelemetry();
            mediaSession_.updateState(
                status == PlayerStatus::Playing ? MediaSessionState::Playing : MediaSessionState::Paused,
                cachedPlaybackPositionMs_
            );
            if (!mediaSegmentsRequested_) requestMediaSegmentsAsync();
            if (!playbackStartReported_) {
                playbackStartReported_ = true;
                const int64_t ticks = playbackTicksFromPositionMs(cachedPlaybackPositionMs_);
                const auto session = session_;
                const auto itemCopy = activePlaybackItem_;
                const auto targetCopy = activeTarget_;
                tasks_.submit([this, session, itemCopy, targetCopy, ticks] {
                    api_.reportPlaybackStart(session, itemCopy, targetCopy, ticks);
                });
            }
            const auto now = std::chrono::steady_clock::now();
            if (now - lastProgressReport_ >= 10s) {
                lastProgressReport_ = now;
                reportProgressAsync(false);
            }
            if (!nextEpisodeRequested_ && activePlaybackItem_.type == "Episode"
                && cachedPlaybackPositionMs_ >= 30000) {
                requestNextEpisodeAsync();
            }
            if (cachedPlaybackDurationMs_ > 1000 && cachedPlaybackPositionMs_ >= cachedPlaybackDurationMs_ - 1000) {
                if (playbackQueueIndex_ >= 0 && queueRepeatMode_ != QueueRepeatMode::Off) {
                    const int next = queueNextIndex(
                        playbackQueueIndex_,
                        static_cast<int>(playbackQueue_.size()),
                        queueRepeatMode_,
                        false
                    );
                    if (next >= 0) playQueuedIndexAsync(next, next == playbackQueueIndex_);
                    else stopPlayback();
                } else if (nextPlaybackItem_) {
                    JellyfinItem next = *nextPlaybackItem_;
                    if (settings_.autoplayNext && autoplayChainCount_ < settings_.stillWatchingAfter) {
                        queueAutoplayNext(std::move(next));
                    } else {
                        showStillWatching(std::move(next));
                    }
                } else {
                    stopPlayback();
                    autoplayChainCount_ = 0;
                }
            }
        }
    }

    void reportProgressAsync(bool immediate) {
        if (screen_ != Screen::Player || !activeTarget_.url.size() || !session_.valid() || !playbackStartReported_) return;
        if (!immediate && player_.status() == PlayerStatus::Preparing) return;
        const int64_t ticks = playbackTicksFromPositionMs(cachedPlaybackPositionMs_);
        const bool paused = player_.status() == PlayerStatus::Paused;
        const auto session = session_;
        const auto item = activePlaybackItem_;
        const auto target = activeTarget_;
        tasks_.submit([this, session, item, target, ticks, paused] {
            api_.reportPlaybackProgress(session, item, target, ticks, paused);
        });
    }

    void stopPlayback() {
        if (screen_ != Screen::Player && player_.status() == PlayerStatus::Idle) return;
        releaseActivePlayback(true);
        playbackSubtitleLanguagePreference_.reset();
        nextTransitionLoading_ = false;
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
        switch (screen_) {
            case Screen::Login: renderLogin(); break;
            case Screen::Profiles: renderProfiles(); break;
            case Screen::Home: renderHome(); break;
            case Screen::Libraries: renderLibraries(); break;
            case Screen::Browse: renderBrowse(); break;
            case Screen::Search: renderSearch(); break;
            case Screen::Settings: renderSettings(); break;
            case Screen::Diagnostics: renderDiagnostics(); break;
            case Screen::Details: renderDetails(); break;
            case Screen::Cast: renderCast(); break;
            case Screen::PersonItems: renderPersonItems(); break;
            case Screen::ItemMenu: renderItemMenu(); break;
            case Screen::Seasons: renderSeasons(); break;
            case Screen::Episodes: renderEpisodes(); break;
            case Screen::Player: renderPlayer(); break;
        }
        if (queueOverlayActive_) renderQueueOverlay();
        renderStatus();
        renderer_.endFrame();
    }

    std::string artworkKey(const JellyfinItem& item) const {
        return item.id + ":primary:" + item.imageTag;
    }

    std::string backdropKey(const JellyfinItem& item) const {
        return item.id + ":backdrop:" + item.backdropTag;
    }

    std::string homeArtworkKey(const JellyfinItem& item) const {
        const ArtworkReference artwork = homeArtworkReference(
            item.id,
            item.imageTag,
            item.seriesId,
            item.seriesPrimaryImageTag,
            item.type == "Episode",
            item.thumbTag,
            item.backdropTag
        );
        return artwork.itemId + ":home:v3:" + std::to_string(static_cast<int>(artwork.kind)) + ":" + artwork.tag;
    }

    std::string homeDiskCachePath(const std::string& key) const {
        if (dataPath_.empty()) return {};
        uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char value : key) {
            hash ^= value;
            hash *= 1099511628211ULL;
        }
        std::ostringstream name;
        name << std::hex << hash << ".img";
        return dataPath_ + "/home-image-cache/" + name.str();
    }

    void trimHomeDiskCacheLocked() {
        if (dataPath_.empty()) return;
        namespace fs = std::filesystem;
        const fs::path directory = fs::path(dataPath_) / "home-image-cache";
        std::error_code ec;
        if (!fs::exists(directory, ec)) return;

        struct CachedFile {
            fs::path path;
            uintmax_t size = 0;
            fs::file_time_type modified{};
        };
        std::vector<CachedFile> files;
        uintmax_t totalBytes = 0;
        for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            CachedFile file;
            file.path = it->path();
            file.size = it->file_size(ec);
            if (ec) { ec.clear(); continue; }
            file.modified = it->last_write_time(ec);
            if (ec) { ec.clear(); continue; }
            totalBytes += file.size;
            files.push_back(std::move(file));
        }

        constexpr uintmax_t kMaxDiskBytes = 48ULL * 1024ULL * 1024ULL;
        constexpr size_t kMaxDiskFiles = 256;
        if (totalBytes <= kMaxDiskBytes && files.size() <= kMaxDiskFiles) return;
        std::sort(files.begin(), files.end(), [](const CachedFile& left, const CachedFile& right) {
            return left.modified < right.modified;
        });
        size_t index = 0;
        while (index < files.size() && (totalBytes > kMaxDiskBytes || files.size() - index > kMaxDiskFiles)) {
            fs::remove(files[index].path, ec);
            if (!ec) totalBytes = files[index].size > totalBytes ? 0 : totalBytes - files[index].size;
            else ec.clear();
            ++index;
        }
    }

    std::optional<std::string> readHomeDiskCache(const std::string& key) {
        const std::string path = homeDiskCachePath(key);
        if (path.empty()) return std::nullopt;
        std::scoped_lock lock(homeDiskCacheMutex_);
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::nullopt;
        std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (bytes.empty()) return std::nullopt;
        std::error_code ec;
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ec);
        return bytes;
    }

    void writeHomeDiskCache(const std::string& key, const std::string& bytes) {
        if (bytes.empty()) return;
        const std::string path = homeDiskCachePath(key);
        if (path.empty()) return;
        std::scoped_lock lock(homeDiskCacheMutex_);
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        if (ec) return;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) return;
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        trimHomeDiskCacheLocked();
    }

    void eraseHomeDiskCache(const std::string& key) {
        const std::string path = homeDiskCachePath(key);
        if (path.empty()) return;
        std::scoped_lock lock(homeDiskCacheMutex_);
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    void trimHomeArtworkCache() {
        constexpr size_t kMaxHomeArtworkEntries = 48;
        while (homeArtwork_.size() >= kMaxHomeArtworkEntries) {
            auto victim = homeArtwork_.end();
            for (auto it = homeArtwork_.begin(); it != homeArtwork_.end(); ++it) {
                if (it->second.state == ArtworkState::Loading) continue;
                if (victim == homeArtwork_.end() || it->second.lastUse < victim->second.lastUse) victim = it;
            }
            if (victim == homeArtwork_.end()) break;
            if (victim->second.texture != 0 && victim->second.textureGeneration == renderer_.generation()) {
                renderer_.deleteTexture(victim->second.texture);
            }
            homeArtwork_.erase(victim);
        }
    }

    void requestHomeArtwork(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty()) return;
        const std::string key = homeArtworkKey(item);
        auto existing = homeArtwork_.find(key);
        if (existing != homeArtwork_.end()) {
            existing->second.lastUse = ++homeArtworkUseCounter_;
            return;
        }
        trimHomeArtworkCache();
        ArtworkEntry entry;
        entry.lastUse = ++homeArtworkUseCounter_;
        homeArtwork_.emplace(key, std::move(entry));

        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            bool fromDisk = false;
            std::string encoded;
            if (auto cached = readHomeDiskCache(key)) {
                encoded = std::move(*cached);
                fromDisk = true;
            } else {
                auto bytes = api_.downloadHomeImage(session, itemCopy, 480, 270);
                if (!bytes.ok) {
                    std::scoped_lock lock(stateMutex_);
                    auto it = homeArtwork_.find(key);
                    if (it != homeArtwork_.end()) it->second.state = ArtworkState::Failed;
                    return;
                }
                encoded = std::move(bytes.value);
            }

            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(encoded, decodeError);
            if (!decoded.valid() && fromDisk) {
                eraseHomeDiskCache(key);
                auto bytes = api_.downloadHomeImage(session, itemCopy, 480, 270);
                if (bytes.ok) {
                    encoded = std::move(bytes.value);
                    decodeError.clear();
                    decoded = imageDecoder_.decode(encoded, decodeError);
                    fromDisk = false;
                }
            }
            if (decoded.valid() && !fromDisk) writeHomeDiskCache(key, encoded);

            std::scoped_lock lock(stateMutex_);
            auto it = homeArtwork_.find(key);
            if (it == homeArtwork_.end()) return;
            if (!decoded.valid()) {
                it->second.state = ArtworkState::Failed;
                return;
            }
            it->second.decoded = std::move(decoded);
            it->second.state = ArtworkState::Ready;
        });
    }

    bool drawHomeArtwork(const JellyfinItem& item, float x, float y, float width, float height) {
        if (item.id.empty()) return false;
        const std::string key = homeArtworkKey(item);
        auto it = homeArtwork_.find(key);
        if (it == homeArtwork_.end()) {
            requestHomeArtwork(item);
            return false;
        }
        auto& entry = it->second;
        entry.lastUse = ++homeArtworkUseCounter_;
        if (entry.state != ArtworkState::Ready || !entry.decoded.valid()) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
        }
        if (entry.texture == 0) return false;
        renderer_.image(entry.texture, x, y, width, height);
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

    void requestArtwork(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty()) return;
        const std::string key = artworkKey(item);
        if (artwork_.contains(key)) return;
        artwork_.emplace(key, ArtworkEntry{});

        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            auto bytes = api_.downloadPrimaryImage(session, itemCopy, 360, 540);
            if (!bytes.ok) {
                std::scoped_lock lock(stateMutex_);
                auto it = artwork_.find(key);
                if (it != artwork_.end()) it->second.state = ArtworkState::Failed;
                return;
            }

            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(bytes.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            auto it = artwork_.find(key);
            if (it == artwork_.end()) return;
            if (!decoded.valid()) {
                it->second.state = ArtworkState::Failed;
                return;
            }
            it->second.decoded = std::move(decoded);
            it->second.state = ArtworkState::Ready;
        });
    }

    bool drawArtwork(const JellyfinItem& item, float x, float y, float width, float height, float alpha = 1.0f) {
        if (item.id.empty()) return false;
        const std::string key = artworkKey(item);
        auto it = artwork_.find(key);
        if (it == artwork_.end()) {
            requestArtwork(item);
            return false;
        }
        auto& entry = it->second;
        if (entry.state != ArtworkState::Ready || !entry.decoded.valid()) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
        }
        if (entry.texture == 0) return false;
        renderer_.image(entry.texture, x, y, width, height, alpha);
        return true;
    }

    void requestBackdrop(const JellyfinItem& item) {
        if (!session_.valid() || item.id.empty() || item.backdropTag.empty()) return;
        const std::string key = backdropKey(item);
        if (backdrops_.contains(key)) return;
        backdrops_.emplace(key, ArtworkEntry{});
        const JellyfinSession session = session_;
        const JellyfinItem itemCopy = item;
        tasks_.submit([this, session, itemCopy, key] {
            auto bytes = api_.downloadBackdropImage(session, itemCopy, 1280, 720);
            if (!bytes.ok) {
                std::scoped_lock lock(stateMutex_);
                auto it = backdrops_.find(key);
                if (it != backdrops_.end()) it->second.state = ArtworkState::Failed;
                return;
            }
            std::string decodeError;
            DecodedImage decoded = imageDecoder_.decode(bytes.value, decodeError);
            std::scoped_lock lock(stateMutex_);
            auto it = backdrops_.find(key);
            if (it == backdrops_.end()) return;
            if (!decoded.valid()) {
                it->second.state = ArtworkState::Failed;
                return;
            }
            it->second.decoded = std::move(decoded);
            it->second.state = ArtworkState::Ready;
        });
    }

    bool drawBackdrop(const JellyfinItem& item, float alpha = 0.28f) {
        if (!settings_.showBackdrops || item.id.empty() || item.backdropTag.empty()) return false;
        const std::string key = backdropKey(item);
        auto it = backdrops_.find(key);
        if (it == backdrops_.end()) {
            requestBackdrop(item);
            return false;
        }
        auto& entry = it->second;
        if (entry.state != ArtworkState::Ready || !entry.decoded.valid()) return false;
        if (entry.textureGeneration != renderer_.generation() || entry.texture == 0) {
            entry.texture = renderer_.createTexture(entry.decoded.width, entry.decoded.height, entry.decoded.rgba.data());
            entry.textureGeneration = renderer_.generation();
        }
        if (entry.texture == 0) return false;
        renderer_.image(entry.texture, 0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), alpha);
        return true;
    }

    void renderHeader(const std::string& title) {
        renderer_.text(72, 48, 5.6f, "SLOPPATV", kText);
        renderer_.text(74, 122, 2.7f, title, kMuted);
        if (settings_.showClock) {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_r(&now, &local);
            std::ostringstream clock;
            clock << std::put_time(&local, "%H:%M");
            renderer_.text(1675, 61, 2.4f, clock.str(), kMuted, 170);
        }
    }

    void renderLogin() {
        renderHeader("NATIVE JELLYFIN TV");
        renderer_.text(1180, 65, 2.1f, "C++ / NDK / GLES3", kMuted);

        if (quickConnectActive_) {
            renderer_.text(120, 215, 2.2f, "QUICK CONNECT", kMuted);
            renderer_.text(120, 290, 6.5f, quickConnectCode_, kText);
            renderer_.text(120, 400, 2.1f, "OPEN JELLYFIN ON ANOTHER DEVICE", kMuted);
            renderer_.text(120, 445, 2.1f, "SETTINGS > QUICK CONNECT, THEN ENTER THIS CODE", kMuted);
            renderer_.text(120, 520, 1.9f, loading_ ? "STARTING QUICK CONNECT..." : "WAITING FOR AUTHORIZATION...", kFocus);
            renderer_.text(120, 590, 1.7f, "PRESS BACK TO CANCEL", kMuted);
            return;
        }

        static constexpr std::array<const char*, 3> labels{"SERVER", "USERNAME", "PASSWORD"};
        for (int i = 0; i < 3; ++i) {
            const float y = 210.0f + static_cast<float>(i) * 100.0f;
            renderer_.text(120, y - 27, 1.9f, labels[static_cast<size_t>(i)], kMuted);
            renderer_.rect(120, y, 1050, 68, kPanel);
            std::string value = loginFields_[static_cast<size_t>(i)];
            if (i == 2 && !value.empty()) value.assign(value.size(), '*');
            if (value.empty()) value = i == 0 ? "HTTPS://YOUR-JELLYFIN-SERVER" : "";
            renderer_.text(145, y + 22, 2.4f, value, value.empty() ? kMuted : kText, 990);
            if (!loginKeyboard_ && loginField_ == i) renderer_.outline(116, y - 4, 1058, 76, 4, kFocus);
        }

        renderer_.rect(120, 520, 245, 68, loginField_ == 3 && !loginKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(160, 542, 2.35f, "LOG IN", kText);
        renderer_.rect(385, 520, 340, 68, loginField_ == 4 && !loginKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(425, 542, 2.0f, "QUICK CONNECT", kText);
        renderer_.rect(745, 520, 285, 68, loginField_ == 5 && !loginKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(790, 542, 2.05f, "DISCOVER", kText);
        if (!savedSessions_.empty()) {
            renderer_.rect(1050, 520, 390, 68, loginField_ == 6 && !loginKeyboard_ ? kFocus : kPanelAlt);
            renderer_.text(1090, 542, 1.9f, "SAVED USERS (" + std::to_string(savedSessions_.size()) + ")", kText, 320);
        }
        if (!loginKeyboard_) {
            const std::string hint = !discoveryStatus_.empty() ? discoveryStatus_ : "DISCOVER FINDS JELLYFIN ON YOUR LAN";
            renderer_.text(120, 620, 1.65f, hint, discoveryStatus_.empty() ? kMuted : kFocus, 1300);
        }
        if (loginKeyboard_) renderKeyboard(610);
    }

    void renderProfiles() {
        renderHeader("USERS & SERVERS");
        renderer_.text(105, 175, 2.0f, "SAVED AUTHENTICATED SESSIONS", kMuted);
        const int totalRows = static_cast<int>(savedSessions_.size()) + 1;
        constexpr int visibleRows = 6;
        const int maxFirst = std::max(0, totalRows - visibleRows);
        const int first = std::clamp(profilesSelection_ - visibleRows + 1, 0, maxFirst);
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int index = first + slot;
            if (index >= totalRows) break;
            const float y = 225.0f + static_cast<float>(slot) * 118.0f;
            const bool focused = index == profilesSelection_;
            renderer_.rect(105, y, 1580, 102, focused ? kPanelAlt : kPanel);
            if (index == static_cast<int>(savedSessions_.size())) {
                renderer_.text(145, y + 32, 2.3f, "ADD ANOTHER ACCOUNT", kText, 780);
                if (focused) renderer_.outline(101, y - 4, 1588, 110, 5, kFocus);
                continue;
            }
            const auto& saved = savedSessions_[static_cast<size_t>(index)];
            renderer_.text(145, y + 20, 2.2f, saved.username.empty() ? "USER" : saved.username, kText, 560);
            renderer_.text(145, y + 62, 1.45f, saved.server, kMuted, 850);
            const bool useFocused = focused && profileAction_ == 0;
            const bool forgetFocused = focused && profileAction_ == 1;
            renderer_.rect(1110, y + 14, 220, 72, useFocused ? kFocus : kPanelAlt);
            renderer_.text(1162, y + 39, 1.8f, "USE", kText, 120);
            renderer_.rect(1350, y + 14, 285, 72, forgetFocused ? kError : kPanelAlt);
            renderer_.text(1392, y + 39, 1.8f, "FORGET", kText, 195);
            if (focused) renderer_.outline(101, y - 4, 1588, 110, 5, profileAction_ == 1 ? kError : kFocus);
        }
        renderer_.text(105, 960, 1.75f, "UP / DOWN CHOOSES ACCOUNT. LEFT / RIGHT CHOOSES USE OR FORGET.", kMuted, 1660);
    }

    void renderKeyboard(float top) {
        const auto& rows = keyboardRows();
        constexpr float startX = 210.0f;
        constexpr float keyH = 70.0f;
        constexpr float gap = 12.0f;
        for (size_t row = 0; row < rows.size(); ++row) {
            const float y = top + static_cast<float>(row) * (keyH + gap);
            const auto& keys = rows[row];
            const float keyW = row == rows.size() - 1 ? 285.0f : 135.0f;
            for (size_t col = 0; col < keys.size(); ++col) {
                const float x = startX + static_cast<float>(col) * (keyW + gap);
                const bool selected = static_cast<int>(row) == keyboardRow_ && static_cast<int>(col) == keyboardCol_;
                renderer_.rect(x, y, keyW, keyH, selected ? kFocus : kPanelAlt);
                const auto& label = keys[col].label;
                const float width = renderer_.textWidth(2.2f, label);
                renderer_.text(x + (keyW - width) / 2.0f, y + 23, 2.2f, label, kText);
            }
        }
    }

    void renderHome() {
        renderHeader("HOME");
        const std::array<std::string, 5> nav{"HOME", "MOVIES", "SHOWS", "SEARCH", "SETTINGS"};
        const std::array<float, 5> widths{145.0f, 190.0f, 180.0f, 180.0f, 220.0f};
        float x = 835.0f;
        for (int i = 0; i < 5; ++i) {
            const float width = widths[static_cast<size_t>(i)];
            renderer_.rect(x, 54, width, 66, homeRow_ < 0 && navIndex_ == i ? kFocus : kPanel);
            renderer_.text(x + 24, 77, 2.15f, nav[static_cast<size_t>(i)], kText);
            x += width + 16.0f;
        }
        if (home_.rows.empty()) {
            renderer_.text(90, 260, 2.5f, loading_ ? "LOADING HOME..." : "NO VIDEO HOME SECTIONS", kMuted);
            return;
        }

        const int firstRow = homeRow_ < 0 ? 0 : std::max(0, homeRow_ - 1);
        for (int slot = 0; slot < 2; ++slot) {
            const int row = firstRow + slot;
            if (row >= static_cast<int>(home_.rows.size())) break;
            const auto& section = home_.rows[static_cast<size_t>(row)];
            renderHomeRow(section.title, section.items, row, 170.0f + static_cast<float>(slot) * 410.0f);
        }
    }

    void renderHomeRow(const std::string& title, const std::vector<JellyfinItem>& items, int row, float top) {
        renderer_.text(70, top, 2.85f, title, homeRow_ == row ? kText : kMuted);
        if (items.empty()) {
            renderer_.text(88, top + 95, 2.0f, loading_ ? "LOADING..." : "NOTHING HERE", kMuted);
            return;
        }

        const int selected = std::clamp(homeSelection_[static_cast<size_t>(row)], 0, static_cast<int>(items.size()) - 1);
        constexpr int visible = 3;
        constexpr float cardW = 560.0f;
        constexpr float cardH = 280.0f;
        constexpr float gap = 40.0f;
        const int maxStart = std::max(0, static_cast<int>(items.size()) - visible);
        const int start = std::clamp(selected - 1, 0, maxStart);

        for (int slot = 0; slot < visible; ++slot) {
            const int index = start + slot;
            if (index >= static_cast<int>(items.size())) break;
            const float x = 70.0f + static_cast<float>(slot) * (cardW + gap);
            const float y = top + 52.0f;
            const auto& item = items[static_cast<size_t>(index)];
            const bool focused = homeRow_ == row && index == selected;

            renderer_.rect(x, y, cardW, cardH, kPanel);
            const bool hasArtwork = drawHomeArtwork(item, x, y, cardW, cardH);
            if (!hasArtwork) renderer_.rect(x + 1, y + 1, cardW - 2, cardH - 2, kPanelAlt);

            renderer_.rect(x, y + cardH - 92.0f, cardW, 92.0f, Color{0.0f, 0.0f, 0.0f, 0.80f});
            renderer_.text(x + 20, y + cardH - 75.0f, 2.15f, item.name, kText, cardW - 40.0f);
            std::string secondary = episodeLabel(item);
            if (secondary.empty() && item.productionYear > 0) secondary = std::to_string(item.productionYear);
            if (!secondary.empty()) renderer_.text(x + 20, y + cardH - 34.0f, 1.4f, secondary, kMuted, cardW - 40.0f);

            if (item.positionTicks > 0 && item.runtimeTicks > 0) {
                const double progress = std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
                renderer_.rect(x, y + cardH - 5.0f, cardW, 5.0f, kPanelAlt);
                renderer_.rect(x, y + cardH - 5.0f, static_cast<float>(cardW * progress), 5.0f, kFocus);
            }
            if (settings_.showWatchedIndicators && item.played) {
                renderer_.rect(x + cardW - 116.0f, y + 12.0f, 104.0f, 34.0f, Color{0.0f, 0.0f, 0.0f, 0.75f});
                renderer_.text(x + cardW - 106.0f, y + 21.0f, 1.0f, "WATCHED", kText, 86.0f);
            }
            if (focused) renderer_.outline(x - 5, y - 5, cardW + 10, cardH + 10, 5, kFocus);
        }
    }

    void renderLibraries() {
        renderHeader("LIBRARIES");
        if (home_.views.empty()) {
            renderer_.text(100, 250, 2.5f, loading_ ? "LOADING LIBRARIES..." : "NO LIBRARIES", kMuted);
            return;
        }
        const int selected = std::clamp(librarySelection_, 0, static_cast<int>(home_.views.size()) - 1);
        const int start = std::max(0, selected - 2);
        constexpr int visible = 4;
        constexpr float cardW = 405.0f;
        constexpr float cardH = 260.0f;
        constexpr float gap = 35.0f;
        for (int slot = 0; slot < visible; ++slot) {
            const int index = start + slot;
            if (index >= static_cast<int>(home_.views.size())) break;
            const float x = 90.0f + static_cast<float>(slot) * (cardW + gap);
            const float y = 280.0f;
            const bool focused = index == selected;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& view = home_.views[static_cast<size_t>(index)];
            const bool hasArtwork = drawArtwork(view, x, y, 175.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 195.0f : 22.0f);
            const float textWidth = cardW - (hasArtwork ? 215.0f : 44.0f);
            renderer_.text(textX, y + 32, 1.7f, view.type, kMuted, textWidth);
            renderer_.text(textX, y + 92, 2.25f, view.name, kText, textWidth);
        }
        renderer_.text(90, 590, 2.0f, "SELECT A LIBRARY TO BROWSE", kMuted);
    }

    void renderBrowse() {
        std::string heading = activeLibrary_.name.empty() ? "LIBRARY" : activeLibrary_.name;
        if (browseContentMode_ == BrowseContentMode::Favorites) heading += " - FAVORITES";
        else if (browseContentMode_ == BrowseContentMode::Genres) heading += " - GENRES";
        else if (browseContentMode_ == BrowseContentMode::GenreItems && !browseGenre_.empty()) heading += " - " + browseGenre_;
        else if (browseContentMode_ == BrowseContentMode::Letters) heading += " - A-Z";
        else if (browseContentMode_ == BrowseContentMode::LetterItems && !browseLetter_.empty()) heading += " - " + browseLetter_;
        else if (browseContentMode_ == BrowseContentMode::Collections) heading = "COLLECTIONS";
        renderHeader(heading);

        if (browseHasFilterBar()) {
            const auto labels = browseFilterLabels();
            float x = 90.0f;
            for (size_t index = 0; index < labels.size(); ++index) {
                const float width = labels[index] == "COLLECTIONS" ? 280.0f : 225.0f;
                const bool focused = browseFilterFocused_ && static_cast<int>(index) == browseFilterSelection_;
                const bool active = !browseFilterFocused_ && static_cast<int>(index) == browseFilterSelection_;
                renderer_.rect(x, 150, width, 66, focused ? kFocus : (active ? kPanelAlt : kPanel));
                renderer_.text(x + 20, 171, 1.65f, labels[index], kText, width - 40.0f);
                x += width + 14.0f;
            }
        }

        if (browseItems_.empty()) {
            renderer_.text(100, 285, 2.5f, loading_ ? "LOADING..." : "NO ITEMS", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = mediaCardWidth();
        constexpr float cardH = 230.0f;
        constexpr float xGap = 35.0f;
        constexpr float yGap = 32.0f;
        const int selectedRow = browseSelection_ / columns;
        const int firstRow = std::max(0, selectedRow - 1);
        for (int index = firstRow * columns; index < static_cast<int>(browseItems_.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 3) break;
            const float x = 90.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 245.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = !browseFilterFocused_ && index == browseSelection_;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& item = browseItems_[static_cast<size_t>(index)];
            const bool synthetic = item.type == "Genre" || item.type == "Letter";
            const bool hasArtwork = synthetic ? false : drawArtwork(item, x, y, 165.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 185.0f : 20.0f);
            const float textWidth = cardW - (hasArtwork ? 205.0f : 40.0f);
            renderer_.text(textX, y + 22, 1.55f, item.type, kMuted, textWidth);
            renderer_.text(textX, y + 65, mediaTitleScale(), item.name, kText, textWidth);
            const auto secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(textX, y + 145, 1.3f, secondary, kMuted, textWidth);
        }
    }

    void renderSearch() {
        renderHeader("SEARCH");
        renderer_.rect(100, 160, 1320, 82, kPanel);
        renderer_.text(130, 188, 2.8f, searchQuery_.empty() ? "TYPE A TITLE" : searchQuery_, searchQuery_.empty() ? kMuted : kText, 1250);
        renderer_.rect(1450, 160, 320, 82, searchKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(1505, 188, 2.4f, searchKeyboard_ ? "KEYBOARD" : "EDIT", kText);

        if (searchKeyboard_) {
            renderKeyboard(285);
            renderer_.text(105, 755, 2.0f, "DONE RUNS THE SEARCH", kMuted);
            return;
        }

        if (searchResults_.empty()) {
            renderer_.text(120, 330, 2.5f, loading_ ? "SEARCHING..." : "NO RESULTS", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = mediaCardWidth();
        constexpr float cardH = 215.0f;
        constexpr float xGap = 35.0f;
        constexpr float yGap = 30.0f;
        const int selectedRow = searchSelection_ / columns;
        const int firstRow = std::max(0, selectedRow - 1);
        for (int index = firstRow * columns; index < static_cast<int>(searchResults_.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 3) break;
            const float x = 90.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 285.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = index == searchSelection_;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& item = searchResults_[static_cast<size_t>(index)];
            const bool hasArtwork = drawArtwork(item, x, y, 155.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 175.0f : 20.0f);
            const float textWidth = cardW - (hasArtwork ? 195.0f : 40.0f);
            renderer_.text(textX, y + 20, 1.5f, item.type, kMuted, textWidth);
            renderer_.text(textX, y + 60, mediaTitleScale(), item.name, kText, textWidth);
            const auto secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(textX, y + 136, 1.25f, secondary, kMuted, textWidth);
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
            if (sourceWidth > 0 && sourceHeight > 0 && videoZoomMode_ != VideoZoomMode::Stretch) {
                const float widthScale = Renderer::logicalWidth() / static_cast<float>(sourceWidth);
                const float heightScale = Renderer::logicalHeight() / static_cast<float>(sourceHeight);
                const float scale = videoZoomMode_ == VideoZoomMode::Fit
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
        const int remainingMs = cachedPlaybackDurationMs_ > 0
            ? std::max(0, cachedPlaybackDurationMs_ - cachedPlaybackPositionMs_)
            : 0;
        const bool showNextUp = nextPlaybackItem_.has_value() && remainingMs > 0 && remainingMs <= 30000;
        const auto skipSegment = activeSkippableSegment();
        const bool showOverlay = status == PlayerStatus::Preparing
            || status == PlayerStatus::Paused
            || nextTransitionLoading_
            || playbackFallbackResolving_
            || showNextUp
            || now < playerOverlayUntil_;
        if (const SubtitleCue* cue = activeSubtitleCue()) {
            const std::string subtitle = wrapText(cue->text, 62, 3);
            const float subtitleY = (showOverlay ? 555.0f : 840.0f) - static_cast<float>(settings_.subtitlePosition) * 115.0f;
            const float textScale = subtitleTextScale(settings_.subtitleSize);
            if (settings_.subtitleBackground) {
                renderer_.rect(190, subtitleY - 22.0f, 1540, 132.0f, Color{0.0f, 0.0f, 0.0f, 0.74f});
            }
            renderer_.text(230, subtitleY + 12.0f, textScale, subtitle, kText, 1460.0f);
        }
        if (skipSegment) {
            renderer_.rect(1470, 670, 360, 78, Color{0.0f, 0.0f, 0.0f, 0.82f});
            renderer_.outline(1466, 666, 368, 86, 4, kFocus);
            renderer_.text(1510, 698, 1.75f, mediaSegmentSkipLabel(*skipSegment), kText, 280);
            renderer_.text(1510, 728, 1.0f, "PRESS OK", kMuted, 280);
        }
        if (!showOverlay) return;

        renderer_.rect(0, 0, Renderer::logicalWidth(), 165, Color{0.0f, 0.0f, 0.0f, 0.74f});
        renderer_.rect(0, 755, Renderer::logicalWidth(), 325, Color{0.0f, 0.0f, 0.0f, 0.80f});
        if (showNextUp && nextPlaybackItem_) {
            renderer_.rect(1260, 175, 590, 190, Color{0.0f, 0.0f, 0.0f, 0.82f});
            renderer_.text(1290, 205, 1.6f, "NEXT UP IN " + std::to_string(std::max(0, remainingMs / 1000)) + "S", kFocus, 520);
            renderer_.text(1290, 250, 2.0f, nextPlaybackItem_->name, kText, 520);
            const std::string nextLabel = episodeLabel(*nextPlaybackItem_);
            if (!nextLabel.empty()) renderer_.text(1290, 310, 1.45f, nextLabel, kMuted, 520);
        }
        const std::string heading = activePlaybackItem_.seriesName.empty()
            ? activePlaybackItem_.name
            : activePlaybackItem_.seriesName;
        renderer_.text(82, 48, 3.9f, heading.empty() ? "PLAYBACK" : heading, kText, 1540);
        const std::string secondary = episodeLabel(activePlaybackItem_);
        if (!secondary.empty() && secondary != heading) renderer_.text(86, 110, 2.15f, secondary, kMuted, 1540);

        const int position = cachedPlaybackPositionMs_;
        const int duration = cachedPlaybackDurationMs_;
        renderer_.text(80, 825, 2.6f,
            playbackFallbackResolving_ ? "RETRYING TRANSCODE" :
            (nextTransitionLoading_ ? "LOADING NEXT EPISODE" :
            (status == PlayerStatus::Paused ? "PAUSED" : (status == PlayerStatus::Preparing ? "LOADING" : "PLAYING"))),
            kText);
        renderer_.text(80, 905, 2.35f, formatPlaybackTime(position), kText);
        renderer_.text(1640, 905, 2.35f, formatPlaybackTime(duration), kText);
        renderer_.rect(235, 928, 1350, 14, kPanelAlt);
        if (duration > 0) {
            const double progress = std::clamp(static_cast<double>(position) / static_cast<double>(duration), 0.0, 1.0);
            renderer_.rect(235, 928, static_cast<float>(1350.0 * progress), 14, kFocus);
        }
        drawTrickplayPreview();
        if (playerControlsActive_) {
            const std::array<std::string, 3> controls{
                status == PlayerStatus::Paused ? "PLAY" : "PAUSE",
                "AUDIO " + playerTrackLabel(2, player_.selectedAudioTrack()),
                "SUBS " + playerTrackLabel(4, player_.selectedSubtitleTrack()),
            };
            constexpr float startX = 205.0f;
            constexpr float gap = 28.0f;
            constexpr float width = 485.0f;
            for (size_t i = 0; i < controls.size(); ++i) {
                const float x = startX + static_cast<float>(i) * (width + gap);
                const bool selected = static_cast<int>(i) == playerControlSelection_;
                renderer_.rect(x, 970, width, 84, selected ? kFocus : kPanelAlt);
                renderer_.text(x + 24, 999, 1.95f, controls[i], kText, width - 48.0f);
            }
        } else {
            const std::string queueHint = playbackQueue_.empty() ? "" : "   DOWN QUEUE";
            renderer_.text(
                80,
                992,
                1.7f,
                "LEFT -" + std::to_string(settings_.seekBackSeconds) + "S   OK PLAY/PAUSE   RIGHT +" + std::to_string(settings_.seekForwardSeconds) + "S   UP OPTIONS" + queueHint + "   BACK EXIT",
                kMuted,
                1700
            );
        }
    }

    void renderQueueOverlay() {
        if (playbackQueue_.empty()) return;
        const int size = static_cast<int>(playbackQueue_.size());
        const int current = std::clamp(playbackQueueIndex_, 0, size - 1);
        queueSelection_ = std::clamp(queueSelection_, current, size - 1);

        renderer_.rect(45, 45, 1830, 990, Color{0.01f, 0.012f, 0.018f, 0.96f});
        renderer_.outline(45, 45, 1830, 990, 4, kFocus);
        renderer_.text(85, 85, 3.6f, "PLAYBACK QUEUE", kText, 1200);
        renderer_.text(1390, 95, 1.7f,
            std::to_string(size - current) + " REMAINING", kMuted, 390);

        constexpr int visibleRows = 6;
        const int first = std::clamp(queueSelection_ - 2, current, std::max(current, size - visibleRows));
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int index = first + slot;
            if (index >= size) break;
            const float y = 165.0f + static_cast<float>(slot) * 105.0f;
            const bool selected = index == queueSelection_;
            const bool isCurrent = index == current;
            renderer_.rect(90, y, 1640, 88, selected ? kPanelAlt : kPanel);
            if (selected) renderer_.outline(86, y - 4, 1648, 96, 4, kFocus);
            const auto& item = playbackQueue_[static_cast<size_t>(index)];
            const std::string marker = isCurrent ? "CURRENT" : (index == current + 1 ? "NEXT" : std::to_string(index - current + 1));
            renderer_.text(120, y + 28, 1.55f, marker, isCurrent ? kFocus : kMuted, 150);
            renderer_.text(285, y + 24, 2.15f, item.name, kText, 860);
            const std::string secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(1160, y + 29, 1.45f, secondary, kMuted, 520);
        }

        const std::array<std::string, 7> actions{
            "PLAY NOW",
            "PLAY NEXT",
            "MOVE UP",
            "MOVE DOWN",
            "REMOVE",
            "SHUFFLE",
            std::string("REPEAT ") + queueRepeatModeName(queueRepeatMode_),
        };
        auto enabled = [&](int action) {
            if (action == 0) return queueCanPlayNow(queueSelection_, playbackQueueIndex_, size);
            if (action == 1) return queueCanPlayNext(queueSelection_, playbackQueueIndex_, size);
            if (action == 2) return queueCanMoveUp(queueSelection_, playbackQueueIndex_, size);
            if (action == 3) return queueCanMoveDown(queueSelection_, playbackQueueIndex_, size);
            if (action == 4) return queueCanRemove(queueSelection_, playbackQueueIndex_, size);
            if (action == 5) return queueCanShuffle(playbackQueueIndex_, size);
            return true;
        };
        constexpr float actionWidth = 225.0f;
        constexpr float actionGap = 15.0f;
        for (size_t i = 0; i < actions.size(); ++i) {
            const float x = 100.0f + static_cast<float>(i) * (actionWidth + actionGap);
            const bool focused = queueActionSelection_ == static_cast<int>(i);
            const bool available = enabled(static_cast<int>(i));
            renderer_.rect(x, 855, actionWidth, 82, focused ? (available ? kFocus : kPanelAlt) : kPanel);
            if (focused) renderer_.outline(x - 4, 851, actionWidth + 8, 90, 4, available ? kFocus : kMuted);
            renderer_.text(x + 14, 884, 1.42f, actions[i], available ? kText : kMuted, actionWidth - 28.0f);
        }
        renderer_.text(100, 982, 1.45f, "UP/DOWN SELECTS. LEFT/RIGHT CHOOSES ACTION. OK APPLIES. BACK CLOSES QUEUE.", kMuted, 1700);
    }

    void renderScreensaver() {
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(), Color{0.008f, 0.009f, 0.012f, 1.0f});
        const int64_t elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        static constexpr std::array<std::array<float, 2>, 8> positions{{
            {{180.0f, 180.0f}},
            {{1120.0f, 180.0f}},
            {{180.0f, 690.0f}},
            {{1120.0f, 690.0f}},
            {{650.0f, 265.0f}},
            {{650.0f, 640.0f}},
            {{350.0f, 430.0f}},
            {{950.0f, 430.0f}},
        }};
        const auto& position = positions[static_cast<size_t>(screensaverPositionSlot(elapsedSeconds))];

        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        char clock[16]{};
        std::strftime(clock, sizeof(clock), "%H:%M", &local);

        renderer_.rect(position[0] - 42.0f, position[1] - 42.0f, 610.0f, 250.0f, Color{0.035f, 0.038f, 0.050f, 0.92f});
        renderer_.outline(position[0] - 42.0f, position[1] - 42.0f, 610.0f, 250.0f, 3.0f, Color{0.56f, 0.38f, 0.98f, 0.72f});
        renderer_.text(position[0], position[1], 3.2f, "SLOPPATV", kText, 520.0f);
        renderer_.text(position[0], position[1] + 78.0f, 5.8f, clock, kText, 520.0f);
        renderer_.text(670, 1015, 1.35f, "PRESS ANY BUTTON TO RETURN", kMuted, 620.0f);
    }

    void renderSettings() {
        renderHeader("SETTINGS");
        const std::array<std::string, 23> labels{
            "MAX STREAMING BITRATE",
            "SKIP BACK",
            "SKIP AHEAD",
            "DEFAULT VIDEO ZOOM",
            "AUTOPLAY NEXT EPISODE",
            "STILL WATCHING AFTER",
            "MATCH VIDEO REFRESH RATE",
            "WATCHED INDICATORS",
            "CLOCK",
            "BACKDROPS",
            "SUBTITLE SIZE",
            "SUBTITLE BACKGROUND",
            "SUBTITLE POSITION",
            "MAX AUDIO CHANNELS",
            "AVC / H.264 MAX LEVEL",
            "HEVC / H.265 MAX LEVEL",
            "HDR PLAYBACK",
            "UI TEXT SIZE",
            "OVERSCAN SAFE AREA",
            "IN-APP SCREENSAVER",
            "EXTERNAL PLAYER",
            "DIAGNOSTICS",
            "SWITCH USER",
        };
        const std::array<std::string, 23> values{
            std::to_string(settings_.maxBitrateMbps) + " MBIT/S",
            std::to_string(settings_.seekBackSeconds) + " SECONDS",
            std::to_string(settings_.seekForwardSeconds) + " SECONDS",
            videoZoomName(static_cast<VideoZoomMode>(settings_.zoomMode)),
            settings_.autoplayNext ? "ON" : "OFF",
            std::to_string(settings_.stillWatchingAfter) + " AUTOPLAYS",
            settings_.refreshRateSwitching ? "ON" : "OFF",
            settings_.showWatchedIndicators ? "ON" : "OFF",
            settings_.showClock ? "ON" : "OFF",
            settings_.showBackdrops ? "ON" : "OFF",
            subtitleSizeName(settings_.subtitleSize),
            settings_.subtitleBackground ? "ON" : "OFF",
            subtitlePositionName(settings_.subtitlePosition),
            std::to_string(settings_.maxAudioChannels) + " CHANNELS",
            avcLevelName(settings_.avcLevelOverride),
            hevcLevelName(settings_.hevcLevelOverride),
            hdrOverrideName(settings_.hdrOverride),
            uiTextSizeName(settings_.uiTextSize),
            settings_.safeAreaPercent == 0 ? "OFF" : std::to_string(settings_.safeAreaPercent) + "% PER EDGE",
            screensaverName(settings_.screensaverMinutes),
            externalPlayerLabel(),
            "DEVICE / SERVER / PLAYBACK",
            session_.username.empty() ? "CURRENT USER" : session_.username,
        };
        constexpr int visibleRows = 7;
        const int first = std::clamp(settingsSelection_ - visibleRows + 1, 0, static_cast<int>(labels.size()) - visibleRows);
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int i = first + slot;
            const float y = 155.0f + static_cast<float>(slot) * 118.0f;
            const bool focused = i == settingsSelection_;
            renderer_.rect(100, y, 1580, 104, focused ? kPanelAlt : kPanel);
            if (focused) renderer_.outline(96, y - 4, 1588, 112, 5, kFocus);
            renderer_.text(140, y + 31, 2.3f, labels[static_cast<size_t>(i)], kText, 760);
            renderer_.text(965, y + 31, 2.2f, values[static_cast<size_t>(i)], i == 22 ? kMuted : kFocus, 650);
        }
        renderer_.text(105, 1000, 1.75f, "LEFT / RIGHT CHANGES VALUES. OK OPENS ACTIONS. SETTINGS SAVE IMMEDIATELY.", kMuted, 1700);
    }

    void renderDiagnostics() {
        renderHeader("DIAGNOSTICS");
        const auto& codecs = api_.deviceCodecSupport();
        const auto videoCodecs = codecs.jellyfinVideoCodecs();
        const auto audioCodecs = codecs.jellyfinAudioCodecs();
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
            {"AUDIO DECODERS", audioCodecs.empty() ? "NONE DETECTED" : joinGenres(audioCodecs, audioCodecs.size())},
            {"HEVC MAX", codecs.maxHevcWidth > 0 ? std::to_string(codecs.maxHevcWidth) + "X" + std::to_string(codecs.maxHevcHeight) : "UNKNOWN"},
            {"HDR DISPLAY", hdr.empty() ? "SDR / NONE DETECTED" : joinGenres(hdr, hdr.size())},
            {"LAST PLAYBACK", lastPlaybackSummary_.empty() ? "NOT YET PLAYED THIS SESSION" : lastPlaybackSummary_},
        };
        for (size_t i = 0; i < rows.size(); ++i) {
            const float y = 180.0f + static_cast<float>(i) * 86.0f;
            renderer_.text(105, y, 1.8f, rows[i].first, kMuted, 460);
            renderer_.text(585, y, 1.8f, rows[i].second, kText, 1210);
        }
        renderer_.text(105, 965, 1.75f, "BACK OR OK RETURNS TO SETTINGS", kMuted);
    }

    void renderItemMenu() {
        if (settings_.showBackdrops) drawBackdrop(detail_);
        renderHeader("ITEM OPTIONS");
        renderer_.text(120, 205, 4.0f, detail_.name.empty() ? "ITEM" : detail_.name, kText, 1580);
        renderer_.text(120, 275, 1.7f, detail_.type.empty() ? "MEDIA" : detail_.type, kMuted, 700);

        if (deleteConfirmation_) {
            renderer_.rect(120, 355, 1540, 180, kPanel);
            renderer_.text(155, 385, 2.25f, "DELETE THIS MEDIA PERMANENTLY?", kError, 1450);
            renderer_.text(
                155,
                445,
                1.55f,
                "THIS ASKS JELLYFIN TO DELETE THE ITEM AND ITS MEDIA FILES. THIS CANNOT BE UNDONE.",
                kMuted,
                1420
            );

            const std::array<std::string, 2> actions{"DELETE PERMANENTLY", "CANCEL"};
            for (int i = 0; i < 2; ++i) {
                const float y = 585.0f + static_cast<float>(i) * 110.0f;
                const bool focused = deleteConfirmationSelection_ == i;
                const Color panel = i == 0 ? kError : kPanelAlt;
                renderer_.rect(120, y, 700, 82, focused ? panel : kPanel);
                if (focused) renderer_.outline(116, y - 4, 708, 90, 4, i == 0 ? kError : kFocus);
                renderer_.text(155, y + 27, 1.8f, actions[static_cast<size_t>(i)], kText, 630);
            }
            renderer_.text(120, 955, 1.5f, "CANCEL IS SELECTED BY DEFAULT. BACK ALSO CANCELS.", kMuted);
            return;
        }

        const auto actions = itemMenuActions();
        for (size_t i = 0; i < actions.size(); ++i) {
            const float y = 365.0f + static_cast<float>(i) * 112.0f;
            const bool focused = itemMenuSelection_ == static_cast<int>(i);
            const bool destructive = actions[i] == "DELETE MEDIA";
            renderer_.rect(120, y, 1050, 86, focused ? (destructive ? kError : kPanelAlt) : kPanel);
            if (focused) renderer_.outline(116, y - 4, 1058, 94, 4, destructive ? kError : kFocus);
            renderer_.text(155, y + 29, 1.9f, actions[i], kText, 970);
        }
        if (!detail_.canDelete) {
            renderer_.text(120, 850, 1.45f, "DELETE IS HIDDEN BECAUSE JELLYFIN REPORTS CANDELETE = FALSE.", kMuted, 1450);
        }
        renderer_.text(120, 955, 1.5f, "OK SELECTS. BACK RETURNS TO DETAILS.", kMuted);
    }

    void renderMediaGrid(const std::string& title, const std::vector<JellyfinItem>& items, int selection) {
        renderHeader(title);
        if (items.empty()) {
            renderer_.text(100, 250, 2.5f, loading_ ? "LOADING..." : "NO ITEMS", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = mediaCardWidth();
        constexpr float cardH = 235.0f;
        constexpr float xGap = 35.0f;
        constexpr float yGap = 32.0f;
        const int selectedRow = selection / columns;
        const int firstRow = std::max(0, selectedRow - 1);
        for (int index = firstRow * columns; index < static_cast<int>(items.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 3) break;
            const float x = 90.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 220.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = index == selection;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& item = items[static_cast<size_t>(index)];
            const bool hasArtwork = drawArtwork(item, x, y, 165.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 185.0f : 20.0f);
            const float textWidth = cardW - (hasArtwork ? 205.0f : 40.0f);
            renderer_.text(textX, y + 22, 1.5f, item.type, kMuted, textWidth);
            renderer_.text(textX, y + 62, mediaTitleScale(), item.name, kText, textWidth);
            const auto secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(textX, y + 142, 1.3f, secondary, kMuted, textWidth);
            if (item.favorite) renderer_.text(textX, y + 185, 1.2f, "FAVORITE", kFocus, textWidth);
            else if (settings_.showWatchedIndicators && item.played) renderer_.text(textX, y + 185, 1.2f, "WATCHED", kMuted, textWidth);
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
            renderer_.text(100, 260, 2.5f, "NO CAST DATA", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = 390.0f;
        constexpr float cardH = 215.0f;
        constexpr float xGap = 35.0f;
        constexpr float yGap = 25.0f;
        const int selectedRow = castSelection_ / columns;
        const int firstRow = std::max(0, selectedRow - 2);
        for (int index = firstRow * columns; index < static_cast<int>(detail_.people.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 4) break;
            const float x = 80.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 190.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = index == castSelection_;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& person = detail_.people[static_cast<size_t>(index)];
            const JellyfinItem artworkItem = personArtworkItem(person);
            const bool hasArtwork = drawArtwork(artworkItem, x, y, 145.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 5, kFocus);
            const float textX = x + (hasArtwork ? 165.0f : 20.0f);
            const float textWidth = cardW - (hasArtwork ? 185.0f : 40.0f);
            renderer_.text(textX, y + 42.0f, 2.05f, person.name, kText, textWidth);
            if (!person.role.empty()) renderer_.text(textX, y + 118.0f, 1.35f, person.role, kMuted, textWidth);
        }
        renderer_.text(85, 1010, 1.6f, "OK OPENS TITLES FEATURING THIS PERSON. BACK RETURNS TO DETAILS.", kMuted, 1700);
    }

    void renderPersonItems() {
        const std::string heading = selectedPerson_.name.empty() ? "PERSON" : "FEATURING " + selectedPerson_.name;
        renderMediaGrid(heading, personItems_, personItemSelection_);
    }

    void renderSeasons() {
        renderMediaGrid(seriesDetail_.name.empty() ? "SEASONS" : seriesDetail_.name + " - SEASONS", seasonItems_, seasonSelection_);
    }

    void renderEpisodes() {
        const std::string heading = selectedSeason_.name.empty() ? "EPISODES" : seriesDetail_.name + " - " + selectedSeason_.name;
        renderMediaGrid(heading, episodeItems_, episodeSelection_);
    }

    void renderDetails() {
        drawBackdrop(detail_);
        renderHeader("DETAILS");
        const bool hasSimilar = !detailsSimilar_.empty();
        const bool episodeArtwork = detail_.type == "Episode";
        const float artworkWidth = episodeArtwork ? 420.0f : 380.0f;
        const float artworkHeight = episodeArtwork ? 236.0f : 570.0f;
        const bool hasArtwork = drawArtwork(detail_, 90, 185, artworkWidth, artworkHeight);
        const float contentX = hasArtwork ? 90.0f + artworkWidth + 55.0f : 90.0f;
        const float contentWidth = hasArtwork ? 1805.0f - contentX : 1710.0f;
        if (stillWatchingPrompt_) {
            renderer_.text(contentX, 150, 2.5f, "STILL WATCHING?", kFocus, contentWidth);
        }
        renderer_.text(contentX, 185, 5.5f, detail_.name.empty() ? "LOADING..." : detail_.name, kText, contentWidth);
        const std::string secondary = episodeLabel(detail_);
        if (!secondary.empty()) renderer_.text(contentX + 5.0f, 272, 2.55f, secondary, kMuted, contentWidth);

        std::string metadata;
        if (detail_.productionYear > 0) metadata += std::to_string(detail_.productionYear);
        if (!detail_.officialRating.empty()) metadata += (metadata.empty() ? "" : "   ") + detail_.officialRating;
        if (detail_.runtimeTicks > 0) metadata += (metadata.empty() ? "" : "   ") + formatPlaybackTime(static_cast<int>(detail_.runtimeTicks / 10000));
        if (detail_.communityRating >= 0.0f) {
            std::ostringstream rating;
            rating << std::fixed << std::setprecision(1) << detail_.communityRating << "/10";
            metadata += (metadata.empty() ? "" : "   ") + rating.str();
        }
        if (!metadata.empty()) renderer_.text(contentX + 5.0f, 320, 1.95f, metadata, kText, contentWidth);
        const std::string genres = joinGenres(detail_.genres);
        if (!genres.empty()) renderer_.text(contentX + 5.0f, 365, 1.75f, genres, kMuted, contentWidth);
        renderer_.text(
            contentX + 5.0f,
            genres.empty() ? 365.0f : 410.0f,
            2.25f,
            wrapText(detail_.overview, hasArtwork ? 70 : 92, hasSimilar ? 4 : 8),
            kMuted,
            contentWidth
        );
        const std::string cast = joinGenres(detail_.cast, 5);
        if (!cast.empty()) renderer_.text(contentX + 5.0f, hasSimilar ? 575.0f : 665.0f, 1.6f, "CAST  " + cast, kMuted, contentWidth);
        const float badgeY = hasSimilar ? 620.0f : 710.0f;
        if (detail_.favorite) renderer_.text(contentX + 5.0f, badgeY, 1.9f, "FAVORITE", kFocus);
        if (settings_.showWatchedIndicators && detail_.played) renderer_.text(contentX + 205.0f, badgeY, 1.9f, "WATCHED", kMuted);

        const auto actions = detailActions();
        const float available = std::min(1690.0f - contentX, 1320.0f);
        const float gap = 14.0f;
        const float buttonWidth = std::max(170.0f, (available - gap * static_cast<float>(actions.size() - 1)) / static_cast<float>(actions.size()));
        const float actionY = hasSimilar ? 675.0f : 825.0f;
        for (size_t i = 0; i < actions.size(); ++i) {
            const float x = contentX + 5.0f + static_cast<float>(i) * (buttonWidth + gap);
            const bool focused = !detailsSimilarFocused_ && detailsButton_ == static_cast<int>(i);
            renderer_.rect(x, actionY, buttonWidth, 86, focused ? kFocus : kPanelAlt);
            renderer_.text(x + 18, actionY + 31, 1.8f, actions[i], kText, buttonWidth - 36.0f);
        }

        if (detail_.positionTicks > 0 && detail_.runtimeTicks > 0) {
            const double fraction = std::clamp(static_cast<double>(detail_.positionTicks) / static_cast<double>(detail_.runtimeTicks), 0.0, 1.0);
            const float progressY = hasSimilar ? 775.0f : 950.0f;
            renderer_.rect(contentX + 5.0f, progressY, 720, 10, kPanelAlt);
            renderer_.rect(contentX + 5.0f, progressY, static_cast<float>(720.0 * fraction), 10, kFocus);
        }

        if (hasSimilar) {
            renderer_.text(90, 810, 2.1f, "MORE LIKE THIS", detailsSimilarFocused_ ? kFocus : kText);
            constexpr int visible = 4;
            const int maxStart = std::max(0, static_cast<int>(detailsSimilar_.size()) - visible);
            const int start = std::clamp(detailsSimilarSelection_ - 1, 0, maxStart);
            constexpr float cardWidth = 420.0f;
            constexpr float cardHeight = 200.0f;
            constexpr float cardGap = 25.0f;
            for (int slot = 0; slot < visible; ++slot) {
                const int index = start + slot;
                if (index >= static_cast<int>(detailsSimilar_.size())) break;
                const auto& similar = detailsSimilar_[static_cast<size_t>(index)];
                const float x = 90.0f + static_cast<float>(slot) * (cardWidth + cardGap);
                const float y = 845.0f;
                renderer_.rect(x, y, cardWidth, cardHeight, kPanel);
                drawHomeArtwork(similar, x, y, cardWidth, cardHeight);
                renderer_.rect(x, y + 145.0f, cardWidth, 55.0f, Color{0.0f, 0.0f, 0.0f, 0.76f});
                renderer_.text(x + 14.0f, y + 163.0f, 1.5f, similar.name, kText, cardWidth - 28.0f);
                if (detailsSimilarFocused_ && index == detailsSimilarSelection_) {
                    renderer_.outline(x - 4.0f, y - 4.0f, cardWidth + 8.0f, cardHeight + 8.0f, 4.0f, kFocus);
                }
            }
        }
    }

    void renderStatus() {
        if (loading_) {
            renderer_.rect(1490, 985, 350, 55, kPanelAlt);
            renderer_.text(1545, 1004, 1.9f, "LOADING...", kText);
        }
        if (!error_.empty()) {
            renderer_.rect(70, 995, 1360, 55, kError);
            renderer_.text(90, 1014, 1.7f, error_, kText, 1320);
        }
    }

    static bool sameSessionIdentity(const JellyfinSession& left, const JellyfinSession& right) {
        return !left.server.empty() && left.server == right.server && !left.userId.empty() && left.userId == right.userId;
    }

    void rememberSession(const JellyfinSession& session) {
        if (!session.valid()) return;
        const auto existing = std::find_if(savedSessions_.begin(), savedSessions_.end(), [&](const JellyfinSession& saved) {
            return sameSessionIdentity(saved, session);
        });
        JellyfinSession saved = session;
        saved.deviceId = deviceId_;
        if (existing == savedSessions_.end()) savedSessions_.insert(savedSessions_.begin(), std::move(saved));
        else {
            *existing = std::move(saved);
            std::rotate(savedSessions_.begin(), existing, std::next(existing));
        }
        constexpr size_t kMaxSavedSessions = 16;
        if (savedSessions_.size() > kMaxSavedSessions) savedSessions_.resize(kMaxSavedSessions);
    }

    void removeSavedSessionIdentity(const JellyfinSession& session) {
        std::erase_if(savedSessions_, [&](const JellyfinSession& saved) {
            return sameSessionIdentity(saved, session);
        });
    }

    void loadSession() {
        deviceId_ = generateDeviceId();
        if (dataPath_.empty()) return;
        std::ifstream input(dataPath_ + "/session.json");
        if (!input) return;
        try {
            json data;
            input >> data;
            deviceId_ = data.value("deviceId", deviceId_);
            savedSessions_.clear();
            if (data.contains("savedSessions") && data["savedSessions"].is_array()) {
                for (const auto& saved : data["savedSessions"]) {
                    if (!saved.is_object()) continue;
                    JellyfinSession candidate;
                    candidate.deviceId = deviceId_;
                    candidate.server = saved.value("server", std::string{});
                    candidate.username = saved.value("username", std::string{});
                    candidate.userId = saved.value("userId", std::string{});
                    candidate.token = saved.value("token", std::string{});
                    if (candidate.valid()) savedSessions_.push_back(std::move(candidate));
                }
            }
            session_.deviceId = deviceId_;
            session_.server = data.value("server", std::string{});
            session_.username = data.value("username", std::string{});
            session_.userId = data.value("userId", std::string{});
            session_.token = data.value("token", std::string{});
            if (session_.valid()) rememberSession(session_);
            if (data.contains("settings") && data["settings"].is_object()) {
                const auto& saved = data["settings"];
                settings_.maxBitrateMbps = std::clamp(saved.value("maxBitrateMbps", settings_.maxBitrateMbps), 20, 200);
                settings_.seekBackSeconds = std::clamp(saved.value("seekBackSeconds", settings_.seekBackSeconds), 5, 60);
                settings_.seekForwardSeconds = std::clamp(saved.value("seekForwardSeconds", settings_.seekForwardSeconds), 5, 60);
                settings_.zoomMode = std::clamp(saved.value("zoomMode", settings_.zoomMode), 0, 2);
                settings_.autoplayNext = saved.value("autoplayNext", settings_.autoplayNext);
                settings_.stillWatchingAfter = std::clamp(saved.value("stillWatchingAfter", settings_.stillWatchingAfter), 2, 6);
                settings_.refreshRateSwitching = saved.value("refreshRateSwitching", settings_.refreshRateSwitching);
                settings_.showWatchedIndicators = saved.value("showWatchedIndicators", settings_.showWatchedIndicators);
                settings_.showClock = saved.value("showClock", settings_.showClock);
                settings_.showBackdrops = saved.value("showBackdrops", settings_.showBackdrops);
                settings_.subtitleSize = std::clamp(saved.value("subtitleSize", settings_.subtitleSize), 0, 2);
                settings_.subtitleBackground = saved.value("subtitleBackground", settings_.subtitleBackground);
                settings_.subtitlePosition = std::clamp(saved.value("subtitlePosition", settings_.subtitlePosition), 0, 2);
                const int savedMaxAudioChannels = saved.value("maxAudioChannels", settings_.maxAudioChannels);
                settings_.maxAudioChannels = savedMaxAudioChannels <= 2 ? 2 : 8;
                settings_.avcLevelOverride = saved.value("avcLevelOverride", settings_.avcLevelOverride);
                settings_.hevcLevelOverride = saved.value("hevcLevelOverride", settings_.hevcLevelOverride);
                settings_.hdrOverride = std::clamp(saved.value("hdrOverride", settings_.hdrOverride), 0, 2);
                settings_.uiTextSize = std::clamp(saved.value("uiTextSize", settings_.uiTextSize), 0, 2);
                const int savedSafeArea = saved.value("safeAreaPercent", settings_.safeAreaPercent);
                settings_.safeAreaPercent = savedSafeArea <= 0 ? 0 : (savedSafeArea <= 2 ? 2 : (savedSafeArea <= 4 ? 4 : 6));
                settings_.screensaverMinutes = normalizedScreensaverMinutes(saved.value("screensaverMinutes", settings_.screensaverMinutes));
                settings_.externalPlayerComponent = saved.value("externalPlayerComponent", std::string{});
                videoZoomMode_ = static_cast<VideoZoomMode>(settings_.zoomMode);
            }
            loginFields_[0] = session_.server;
            loginFields_[1] = session_.username;
        } catch (const std::exception& e) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to read session: %s", e.what());
        }
    }

    void saveSession(const JellyfinSession& session) {
        if (dataPath_.empty()) return;
        try {
            if (session.valid()) rememberSession(session);
            json saved = json::array();
            for (const auto& candidate : savedSessions_) {
                if (!candidate.valid()) continue;
                saved.push_back({
                    {"server", candidate.server},
                    {"username", candidate.username},
                    {"userId", candidate.userId},
                    {"token", candidate.token},
                });
            }
            json data = {
                {"deviceId", deviceId_},
                {"server", session.server},
                {"username", session.username},
                {"userId", session.userId},
                {"token", session.token},
                {"savedSessions", std::move(saved)},
                {"settings", {
                    {"maxBitrateMbps", settings_.maxBitrateMbps},
                    {"seekBackSeconds", settings_.seekBackSeconds},
                    {"seekForwardSeconds", settings_.seekForwardSeconds},
                    {"zoomMode", settings_.zoomMode},
                    {"autoplayNext", settings_.autoplayNext},
                    {"stillWatchingAfter", settings_.stillWatchingAfter},
                    {"refreshRateSwitching", settings_.refreshRateSwitching},
                    {"showWatchedIndicators", settings_.showWatchedIndicators},
                    {"showClock", settings_.showClock},
                    {"showBackdrops", settings_.showBackdrops},
                    {"subtitleSize", settings_.subtitleSize},
                    {"subtitleBackground", settings_.subtitleBackground},
                    {"subtitlePosition", settings_.subtitlePosition},
                    {"maxAudioChannels", settings_.maxAudioChannels},
                    {"avcLevelOverride", settings_.avcLevelOverride},
                    {"hevcLevelOverride", settings_.hevcLevelOverride},
                    {"hdrOverride", settings_.hdrOverride},
                    {"uiTextSize", settings_.uiTextSize},
                    {"safeAreaPercent", settings_.safeAreaPercent},
                    {"screensaverMinutes", settings_.screensaverMinutes},
                    {"externalPlayerComponent", settings_.externalPlayerComponent},
                }},
            };
            std::ofstream output(dataPath_ + "/session.json", std::ios::trunc);
            output << data.dump(2);
        } catch (const std::exception& e) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to save session: %s", e.what());
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
    std::atomic<uint64_t> taskGeneration_{0};
    std::string dataPath_;
    std::string deviceId_;

    Screen screen_ = Screen::Login;
    NavigationStack<Screen> navigation_{Screen::Login};
    bool loading_ = false;
    AppSettings settings_;
    std::vector<ExternalPlayerApp> externalPlayers_;
    int settingsSelection_ = 0;
    std::string error_;

    JellyfinSession session_;
    std::string pendingDeepLinkItemId_;
    std::string pendingSearchQuery_;
    std::vector<JellyfinSession> savedSessions_;
    int profilesSelection_ = 0;
    int profileAction_ = 0;
    JellyfinServerInfo serverInfo_;
    JellyfinHomeData home_;
    std::unordered_map<std::string, ArtworkEntry> artwork_;
    std::unordered_map<std::string, ArtworkEntry> homeArtwork_;
    uint64_t homeArtworkUseCounter_ = 0;
    std::mutex homeDiskCacheMutex_;
    std::unordered_map<std::string, ArtworkEntry> backdrops_;
    std::vector<int> homeSelection_;
    int homeRow_ = 0;
    int navIndex_ = 0;
    int librarySelection_ = 0;
    JellyfinItem activeLibrary_;
    std::vector<JellyfinItem> browseItems_;
    int browseSelection_ = 0;
    int browseNextIndex_ = 0;
    bool browseHasMore_ = false;
    std::vector<BrowseSnapshot> browseStack_;
    BrowseContentMode browseContentMode_ = BrowseContentMode::All;
    bool browseFilterFocused_ = false;
    int browseFilterSelection_ = 0;
    std::string browseGenre_;
    std::string browseLetter_;

    std::array<std::string, 3> loginFields_{};
    int loginField_ = 0;
    bool loginKeyboard_ = false;
    bool quickConnectActive_ = false;
    std::string quickConnectCode_;
    std::string discoveryStatus_;

    std::string searchQuery_;
    std::vector<JellyfinItem> searchResults_;
    int searchSelection_ = 0;
    bool searchKeyboard_ = true;

    int keyboardRow_ = 0;
    int keyboardCol_ = 0;

    JellyfinItem detail_;
    int detailsButton_ = 0;
    int itemMenuSelection_ = 0;
    bool deleteConfirmation_ = false;
    int deleteConfirmationSelection_ = 1;
    std::vector<JellyfinItem> detailsSimilar_;
    int detailsSimilarSelection_ = 0;
    bool detailsSimilarFocused_ = false;
    int castSelection_ = 0;
    JellyfinPerson selectedPerson_;
    std::vector<JellyfinItem> personItems_;
    int personItemSelection_ = 0;
    JellyfinItem seriesDetail_;
    std::vector<JellyfinItem> seasonItems_;
    int seasonSelection_ = 0;
    JellyfinItem selectedSeason_;
    std::vector<JellyfinItem> episodeItems_;
    int episodeSelection_ = 0;

    std::vector<JellyfinItem> playbackQueue_;
    int playbackQueueIndex_ = -1;
    int queueSelection_ = 0;
    int queueActionSelection_ = 0;
    QueueRepeatMode queueRepeatMode_ = QueueRepeatMode::Off;
    bool queueOverlayActive_ = false;

    std::optional<PendingExternalLaunch> pendingExternalLaunch_;
    std::optional<PendingExternalLaunch> activeExternalPlayback_;
    std::optional<PlaybackTarget> pendingPlayback_;
    JellyfinItem pendingPlaybackItem_;
    bool pendingStreamRestart_ = false;
    bool pendingRestartPaused_ = false;
    bool pauseAfterRestart_ = false;
    int pendingAudioStreamIndex_ = -1;
    PlaybackTarget activeTarget_;
    JellyfinItem activePlaybackItem_;
    std::optional<JellyfinItem> nextPlaybackItem_;
    std::vector<JellyfinMediaSegment> activeMediaSegments_;
    bool mediaSegmentsRequested_ = false;
    bool nextEpisodeRequested_ = false;
    bool nextTransitionLoading_ = false;
    int autoplayChainCount_ = 0;
    bool playbackStartReported_ = false;
    bool playbackFallbackAttempted_ = false;
    bool playbackFallbackResolving_ = false;
    VideoZoomMode videoZoomMode_ = VideoZoomMode::Fit;
    bool playerControlsActive_ = false;
    int playerControlSelection_ = 0;
    bool subtitleLoadInProgress_ = false;
    std::vector<SubtitleCue> activeSubtitleCues_;
    std::string activeSubtitleLanguage_;
    int activeSubtitleServerIndex_ = -1;
    int selectedAudioServerIndex_ = -1;
    int selectedSubtitleServerIndex_ = -1;
    std::optional<std::string> playbackSubtitleLanguagePreference_;
    bool activeSubtitleEnabled_ = false;
    TrickplayPreviewEntry trickplayPreview_;
    int trickplayPreviewPositionMs_ = -1;
    std::chrono::steady_clock::time_point trickplayPreviewUntil_{};
    std::chrono::steady_clock::time_point lastProgressReport_{};
    std::chrono::steady_clock::time_point lastPlaybackTelemetryRead_{};
    std::chrono::steady_clock::time_point lastPlaybackDurationProbe_{};
    int cachedPlaybackPositionMs_ = 0;
    int cachedPlaybackDurationMs_ = 0;
    std::chrono::steady_clock::time_point playerOverlayUntil_{};
    std::chrono::steady_clock::time_point renderBurstUntil_{};
    std::chrono::steady_clock::time_point lastInteraction_ = std::chrono::steady_clock::now();
    bool screensaverActive_ = false;
    bool stillWatchingPrompt_ = false;
    std::string lastPlaybackSummary_;
};
}  // namespace

void android_main(android_app* app) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "Starting sloppaTV native activity");
    SloppaApp sloppa(app);
    app->userData = &sloppa;
    app->onAppCmd = SloppaApp::handleAppCommand;
    app->onInputEvent = SloppaApp::handleInput;
    sloppa.run();
}
