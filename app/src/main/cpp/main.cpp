#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "discovery.hpp"
#include "display_mode.hpp"
#include "image_decoder.hpp"
#include "jellyfin.hpp"
#include "media_player.hpp"
#include "media_player_policy.hpp"
#include "navigation_stack.hpp"
#include "ui_policy.hpp"
#include "renderer.hpp"
#include "task_runner.hpp"
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
    Home,
    Libraries,
    Browse,
    Search,
    Settings,
    Diagnostics,
    Details,
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
};

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

class SloppaApp {
public:
    explicit SloppaApp(android_app* app)
        : app_(app),
          api_(app->activity->vm, app->activity->clazz),
          player_(app->activity->vm),
          imageDecoder_(app->activity->vm),
          videoSurface_(app->activity->vm),
          tasks_(4, [app] {
              if (app && app->looper) ALooper_wake(app->looper);
          }) {
        dataPath_ = app->activity->internalDataPath ? app->activity->internalDataPath : "";
        loadSession();
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
            bool burstActive = std::chrono::steady_clock::now() < renderBurstUntil_;
            {
                std::scoped_lock lock(stateMutex_);
                if (screen_ == Screen::Player || burstActive) timeoutMs = 0;
                else if (loading_ || quickConnectActive_) timeoutMs = 100;
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
            {
                std::scoped_lock lock(stateMutex_);
                playerScreen = screen_ == Screen::Player;
            }
            burstActive = std::chrono::steady_clock::now() < renderBurstUntil_;
            if (renderer_.ready() && (playerScreen || shouldRender || burstActive)) render();
        }
    }

private:
    void onAppCommand(int32_t command) {
        std::scoped_lock lock(stateMutex_);
        switch (command) {
            case APP_CMD_INIT_WINDOW:
                if (app_->window) renderer_.init(app_->window);
                break;
            case APP_CMD_TERM_WINDOW:
                if (screen_ == Screen::Player) stopPlayback();
                renderer_.shutdown();
                break;
            case APP_CMD_LOST_FOCUS:
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
        renderBurstUntil_ = std::chrono::steady_clock::now() + 150ms;
        std::scoped_lock lock(stateMutex_);

        if (screen_ == Screen::Player) {
            handlePlayerKey(key);
            return 1;
        }

        const char typed = keyCodeToChar(key, meta);
        if (typed != 0 && handleTypedCharacter(typed)) return 1;
        if (key == AKEYCODE_DEL && handleBackspace()) return 1;

        switch (screen_) {
            case Screen::Login: handleLoginKey(key); break;
            case Screen::Home: handleHomeKey(key); break;
            case Screen::Libraries: handleLibrariesKey(key); break;
            case Screen::Browse: handleBrowseKey(key); break;
            case Screen::Search: handleSearchKey(key); break;
            case Screen::Settings: handleSettingsKey(key); break;
            case Screen::Diagnostics: handleDiagnosticsKey(key); break;
            case Screen::Details: handleDetailsKey(key); break;
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
            if (key == AKEYCODE_DPAD_LEFT) loginField_ = loginField_ <= 3 ? 5 : loginField_ - 1;
            else loginField_ = loginField_ >= 5 ? 3 : loginField_ + 1;
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (loginField_ < 3) {
                loginKeyboard_ = true;
                keyboardRow_ = keyboardCol_ = 0;
            } else if (loginField_ == 3) {
                loginAsync();
            } else if (loginField_ == 4) {
                quickConnectAsync();
            } else {
                discoverServersAsync();
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
        actions.emplace_back("BACK");
        return actions;
    }

    void handleSettingsKey(int32_t key) {
        if (key == AKEYCODE_BACK) {
            popScreen(Screen::Home);
            if (screen_ == Screen::Home) homeRow_ = -1;
            navIndex_ = 4;
            return;
        }
        if (key == AKEYCODE_DPAD_UP) settingsSelection_ = std::max(0, settingsSelection_ - 1);
        else if (key == AKEYCODE_DPAD_DOWN) settingsSelection_ = std::min(11, settingsSelection_ + 1);
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
            }
            saveSession(session_);
        } else if (key == AKEYCODE_DPAD_CENTER || key == AKEYCODE_ENTER) {
            if (settingsSelection_ == 10) openDiagnostics();
            else if (settingsSelection_ == 11) logout();
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
            int action = detailsButton_;
            if (action == 0) {
                beginPlayback();
                return;
            }
            if (detail_.type == "Series") {
                if (action == 1) {
                    openSeasons();
                    return;
                }
                --action;
            }
            if (action == 1) toggleFavoriteAsync();
            else if (action == 2) togglePlayedAsync();
            else if (action == 3) popScreen(Screen::Home);
        }
    }

    void moveGridSelection(int32_t key, const std::vector<JellyfinItem>& items, int& selection) {
        if (items.empty()) return;
        constexpr int columns = mediaGridColumns();
        const int rows = static_cast<int>((items.size() + columns - 1) / columns);
        int row = selection / columns;
        int col = selection % columns;
        if (key == AKEYCODE_DPAD_LEFT) col = std::max(0, col - 1);
        else if (key == AKEYCODE_DPAD_RIGHT) col = std::min(columns - 1, col + 1);
        else if (key == AKEYCODE_DPAD_UP) row = std::max(0, row - 1);
        else if (key == AKEYCODE_DPAD_DOWN) row = std::min(rows - 1, row + 1);
        const int next = row * columns + col;
        if (next >= 0 && next < static_cast<int>(items.size())) selection = next;
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
            pendingSubtitleStreamIndex_ = subtitleStreamIndex;
        });
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
            seekPlaybackTo(cachedPlaybackPositionMs_ - settings_.seekBackSeconds * 1000);
            reportProgressAsync(false);
        } else if (key == AKEYCODE_DPAD_RIGHT || key == AKEYCODE_MEDIA_FAST_FORWARD) {
            seekPlaybackTo(cachedPlaybackPositionMs_ + settings_.seekForwardSeconds * 1000);
            reportProgressAsync(false);
        }
    }

    void openSettings() {
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

    void logout() {
        ++taskGeneration_;
        session_ = {};
        home_ = {};
        artwork_.clear();
        homeArtwork_.clear();
        backdrops_.clear();
        loginFields_[1].clear();
        loginFields_[2].clear();
        homeSelection_.clear();
        resetNavigation(Screen::Login);
        loginField_ = 0;
        loading_ = false;
        error_.clear();
        saveSession(session_);
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

    void beginPlayback() {
        if (loading_ || detail_.id.empty()) return;
        autoplayChainCount_ = 0;
        stillWatchingPrompt_ = false;
        loading_ = true;
        error_.clear();
        const JellyfinSession session = session_;
        const JellyfinItem selected = detail_;
        const uint64_t generation = ++taskGeneration_;

        tasks_.submit([this, session, selected, generation] {
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

            auto target = api_.resolvePlayback(session, playable, settings_.maxBitrateMbps * 1000000);
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
        playbackStartReported_ = false;
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
        releaseActivePlayback(true);
        ++autoplayChainCount_;
        loading_ = true;
        nextTransitionLoading_ = true;
        detail_ = nextItem;
        stillWatchingPrompt_ = false;
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 10s;
        const JellyfinSession session = session_;
        tasks_.submit([this, session, nextItem = std::move(nextItem)]() mutable {
            auto target = api_.resolvePlayback(session, nextItem, settings_.maxBitrateMbps * 1000000);
            std::scoped_lock lock(stateMutex_);
            loading_ = false;
            nextTransitionLoading_ = false;
            if (!target.ok) {
                popScreen(Screen::Details);
                error_ = "NEXT EPISODE: " + target.error;
                return;
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

    bool retryPlaybackWithTranscodeFallback() {
        if (playbackFallbackAttempted_ || activeTarget_.transcoding || activeTarget_.fallbackTranscodeUrl.empty()) return false;

        const auto failedTarget = activeTarget_;
        const auto item = activePlaybackItem_;
        const auto session = session_;
        const int64_t resumeTicks = playbackTicksFromPositionMs(cachedPlaybackPositionMs_);
        if (playbackStartReported_ && session.valid() && !item.id.empty()) {
            tasks_.submit([this, session, item, failedTarget, resumeTicks] {
                api_.reportPlaybackStopped(session, item, failedTarget, resumeTicks);
            });
        }

        player_.stop();
        videoSurface_.release();
        playbackStartReported_ = false;
        playbackFallbackAttempted_ = true;
        cachedPlaybackPositionMs_ = playbackPositionMsFromTicks(resumeTicks);
        cachedPlaybackDurationMs_ = playbackPositionMsFromTicks(item.runtimeTicks);
        lastPlaybackTelemetryRead_ = {};
        lastPlaybackDurationProbe_ = {};

        activeTarget_.url = std::move(activeTarget_.fallbackTranscodeUrl);
        activeTarget_.fallbackTranscodeUrl.clear();
        activeTarget_.transcoding = true;
        if (resumeTicks > 0) activeTarget_.startTicks = resumeTicks;

        std::string surfaceError;
        if (!renderer_.ready() || !videoSurface_.create(surfaceError)) {
            error_ = surfaceError.empty() ? "VIDEO FALLBACK SURFACE IS NOT AVAILABLE" : surfaceError;
            return false;
        }
        __android_log_print(ANDROID_LOG_WARN, kTag, "Direct play failed; retrying Jellyfin transcode fallback");
        playerOverlayUntil_ = std::chrono::steady_clock::now() + 5s;
        startResolvedPlaybackTarget(activeTarget_);
        return true;
    }

    void tick() {
        std::optional<PlaybackTarget> target;
        JellyfinItem item;
        bool streamRestart = false;
        {
            std::scoped_lock lock(stateMutex_);
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
                playbackSummary << (target->transcoding ? "TRANSCODE" : "DIRECT PLAY");
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
                selectedSubtitleServerIndex_ = pendingSubtitleStreamIndex_;
                pendingAudioStreamIndex_ = -1;
                pendingSubtitleStreamIndex_ = -1;
                if (!streamRestart) {
                    mediaSegmentsRequested_ = false;
                    activeMediaSegments_.clear();
                }
                nextTransitionLoading_ = false;
                if (!streamRestart) pushScreen(Screen::Player);
                else replaceScreen(Screen::Player);
            }
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
            playerOverlayUntil_ = std::chrono::steady_clock::now() + 5s;
            startResolvedPlaybackTarget(*target);
            return;
        }

        if (screen_ != Screen::Player) return;
        PlayerStatus status = player_.status();
        if (status == PlayerStatus::Playing && pauseAfterRestart_) {
            player_.togglePause();
            pauseAfterRestart_ = false;
            status = player_.status();
        }
        if (status == PlayerStatus::Error) {
            std::scoped_lock lock(stateMutex_);
            const std::string playerError = player_.error();
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
                if (nextPlaybackItem_) {
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
        nextTransitionLoading_ = false;
        if (screen_ == Screen::Player) popScreen(Screen::Details);
        if (app_->window && !renderer_.ready()) renderer_.init(app_->window);
        loadHomeAsync();
    }

    void render() {
        std::scoped_lock lock(stateMutex_);
        renderer_.beginFrame();
        switch (screen_) {
            case Screen::Login: renderLogin(); break;
            case Screen::Home: renderHome(); break;
            case Screen::Libraries: renderLibraries(); break;
            case Screen::Browse: renderBrowse(); break;
            case Screen::Search: renderSearch(); break;
            case Screen::Settings: renderSettings(); break;
            case Screen::Diagnostics: renderDiagnostics(); break;
            case Screen::Details: renderDetails(); break;
            case Screen::Seasons: renderSeasons(); break;
            case Screen::Episodes: renderEpisodes(); break;
            case Screen::Player: renderPlayer(); break;
        }
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
        renderer_.text(80, 55, 5.0f, "SLOPPATV", kText);
        renderer_.text(80, 120, 2.3f, title, kMuted);
        if (settings_.showClock) {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_r(&now, &local);
            std::ostringstream clock;
            clock << std::put_time(&local, "%H:%M");
            renderer_.text(1690, 65, 2.1f, clock.str(), kMuted, 150);
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

        renderer_.rect(120, 520, 280, 68, loginField_ == 3 && !loginKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(165, 542, 2.5f, "LOG IN", kText);
        renderer_.rect(420, 520, 390, 68, loginField_ == 4 && !loginKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(460, 542, 2.2f, "QUICK CONNECT", kText);
        renderer_.rect(830, 520, 330, 68, loginField_ == 5 && !loginKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(880, 542, 2.2f, "DISCOVER", kText);
        if (!loginKeyboard_) {
            const std::string hint = !discoveryStatus_.empty() ? discoveryStatus_ : "DISCOVER FINDS JELLYFIN ON YOUR LAN";
            renderer_.text(120, 620, 1.65f, hint, discoveryStatus_.empty() ? kMuted : kFocus, 1300);
        }
        if (loginKeyboard_) renderKeyboard(610);
    }

    void renderKeyboard(float top) {
        const auto& rows = keyboardRows();
        constexpr float startX = 210.0f;
        constexpr float keyH = 64.0f;
        constexpr float gap = 10.0f;
        for (size_t row = 0; row < rows.size(); ++row) {
            const float y = top + static_cast<float>(row) * (keyH + gap);
            const auto& keys = rows[row];
            const float keyW = row == rows.size() - 1 ? 285.0f : 135.0f;
            for (size_t col = 0; col < keys.size(); ++col) {
                const float x = startX + static_cast<float>(col) * (keyW + gap);
                const bool selected = static_cast<int>(row) == keyboardRow_ && static_cast<int>(col) == keyboardCol_;
                renderer_.rect(x, y, keyW, keyH, selected ? kFocus : kPanelAlt);
                const auto& label = keys[col].label;
                const float width = renderer_.textWidth(2.0f, label);
                renderer_.text(x + (keyW - width) / 2.0f, y + 22, 2.0f, label, kText);
            }
        }
    }

    void renderHome() {
        renderHeader("HOME");
        const std::array<std::string, 5> nav{"HOME", "MOVIES", "SHOWS", "SEARCH", "SETTINGS"};
        const std::array<float, 5> widths{130.0f, 165.0f, 155.0f, 155.0f, 190.0f};
        float x = 955.0f;
        for (int i = 0; i < 5; ++i) {
            const float width = widths[static_cast<size_t>(i)];
            renderer_.rect(x, 60, width, 54, homeRow_ < 0 && navIndex_ == i ? kFocus : kPanel);
            renderer_.text(x + 24, 79, 1.9f, nav[static_cast<size_t>(i)], kText);
            x += width + 18.0f;
        }
        if (home_.rows.empty()) {
            renderer_.text(90, 260, 2.5f, loading_ ? "LOADING HOME..." : "NO VIDEO HOME SECTIONS", kMuted);
            return;
        }

        const int firstRow = homeRow_ < 0 ? 0 : std::max(0, homeRow_ - 1);
        for (int slot = 0; slot < 3; ++slot) {
            const int row = firstRow + slot;
            if (row >= static_cast<int>(home_.rows.size())) break;
            const auto& section = home_.rows[static_cast<size_t>(row)];
            renderHomeRow(section.title, section.items, row, 165.0f + static_cast<float>(slot) * 295.0f);
        }
    }

    void renderHomeRow(const std::string& title, const std::vector<JellyfinItem>& items, int row, float top) {
        renderer_.text(78, top, 2.45f, title, homeRow_ == row ? kText : kMuted);
        if (items.empty()) {
            renderer_.text(88, top + 95, 2.0f, loading_ ? "LOADING..." : "NOTHING HERE", kMuted);
            return;
        }

        const int selected = std::clamp(homeSelection_[static_cast<size_t>(row)], 0, static_cast<int>(items.size()) - 1);
        constexpr int visible = 4;
        constexpr float cardW = 420.0f;
        constexpr float cardH = 210.0f;
        constexpr float gap = 30.0f;
        const int maxStart = std::max(0, static_cast<int>(items.size()) - visible);
        const int start = std::clamp(selected - 2, 0, maxStart);

        for (int slot = 0; slot < visible; ++slot) {
            const int index = start + slot;
            if (index >= static_cast<int>(items.size())) break;
            const float x = 78.0f + static_cast<float>(slot) * (cardW + gap);
            const float y = top + 43.0f;
            const auto& item = items[static_cast<size_t>(index)];
            const bool focused = homeRow_ == row && index == selected;

            renderer_.rect(x, y, cardW, cardH, kPanel);
            const bool hasArtwork = drawHomeArtwork(item, x, y, cardW, cardH);
            if (!hasArtwork) renderer_.rect(x + 1, y + 1, cardW - 2, cardH - 2, kPanelAlt);

            renderer_.rect(x, y + cardH - 72.0f, cardW, 72.0f, Color{0.0f, 0.0f, 0.0f, 0.78f});
            renderer_.text(x + 16, y + cardH - 59.0f, 1.7f, item.name, kText, cardW - 32.0f);
            std::string secondary = episodeLabel(item);
            if (secondary.empty() && item.productionYear > 0) secondary = std::to_string(item.productionYear);
            if (!secondary.empty()) renderer_.text(x + 16, y + cardH - 27.0f, 1.15f, secondary, kMuted, cardW - 32.0f);

            if (item.positionTicks > 0 && item.runtimeTicks > 0) {
                const double progress = std::clamp(static_cast<double>(item.positionTicks) / static_cast<double>(item.runtimeTicks), 0.0, 1.0);
                renderer_.rect(x, y + cardH - 5.0f, cardW, 5.0f, kPanelAlt);
                renderer_.rect(x, y + cardH - 5.0f, static_cast<float>(cardW * progress), 5.0f, kFocus);
            }
            if (settings_.showWatchedIndicators && item.played) {
                renderer_.rect(x + cardW - 92.0f, y + 10.0f, 82.0f, 27.0f, Color{0.0f, 0.0f, 0.0f, 0.72f});
                renderer_.text(x + cardW - 84.0f, y + 18.0f, 0.85f, "WATCHED", kText, 68.0f);
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
        const int start = std::max(0, selected - 4);
        constexpr int visible = 6;
        constexpr float cardW = 275.0f;
        constexpr float cardH = 190.0f;
        constexpr float gap = 25.0f;
        for (int slot = 0; slot < visible; ++slot) {
            const int index = start + slot;
            if (index >= static_cast<int>(home_.views.size())) break;
            const float x = 90.0f + static_cast<float>(slot) * (cardW + gap);
            const float y = 280.0f;
            const bool focused = index == selected;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& view = home_.views[static_cast<size_t>(index)];
            const bool hasArtwork = drawArtwork(view, x, y, 125.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 142.0f : 18.0f);
            const float textWidth = cardW - (hasArtwork ? 158.0f : 36.0f);
            renderer_.text(textX, y + 25, 1.45f, view.type, kMuted, textWidth);
            renderer_.text(textX, y + 78, 1.9f, view.name, kText, textWidth);
        }
        renderer_.text(90, 540, 1.8f, "SELECT A LIBRARY TO BROWSE", kMuted);
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
                const float width = labels[index] == "COLLECTIONS" ? 250.0f : 205.0f;
                const bool focused = browseFilterFocused_ && static_cast<int>(index) == browseFilterSelection_;
                const bool active = !browseFilterFocused_ && static_cast<int>(index) == browseFilterSelection_;
                renderer_.rect(x, 150, width, 58, focused ? kFocus : (active ? kPanelAlt : kPanel));
                renderer_.text(x + 18, 169, 1.45f, labels[index], kText, width - 36.0f);
                x += width + 12.0f;
            }
        }

        if (browseItems_.empty()) {
            renderer_.text(100, 285, 2.5f, loading_ ? "LOADING..." : "NO ITEMS", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = mediaCardWidth();
        constexpr float cardH = 170.0f;
        constexpr float xGap = 45.0f;
        constexpr float yGap = 48.0f;
        const int selectedRow = browseSelection_ / columns;
        const int firstRow = std::max(0, selectedRow - 2);
        for (int index = firstRow * columns; index < static_cast<int>(browseItems_.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 4) break;
            const float x = 90.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 245.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = !browseFilterFocused_ && index == browseSelection_;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& item = browseItems_[static_cast<size_t>(index)];
            const bool synthetic = item.type == "Genre" || item.type == "Letter";
            const bool hasArtwork = synthetic ? false : drawArtwork(item, x, y, 115.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 132.0f : 16.0f);
            const float textWidth = cardW - (hasArtwork ? 148.0f : 32.0f);
            renderer_.text(textX, y + 16, 1.4f, item.type, kMuted, textWidth);
            renderer_.text(textX, y + 52, mediaTitleScale(), item.name, kText, textWidth);
            const auto secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(textX, y + 109, 1.15f, secondary, kMuted, textWidth);
        }
    }

    void renderSearch() {
        renderHeader("SEARCH");
        renderer_.rect(120, 165, 1280, 70, kPanel);
        renderer_.text(145, 188, 2.5f, searchQuery_.empty() ? "TYPE A TITLE" : searchQuery_, searchQuery_.empty() ? kMuted : kText, 1220);
        renderer_.rect(1430, 165, 300, 70, searchKeyboard_ ? kFocus : kPanelAlt);
        renderer_.text(1490, 188, 2.2f, searchKeyboard_ ? "KEYBOARD" : "EDIT", kText);

        if (searchKeyboard_) {
            renderKeyboard(280);
            renderer_.text(120, 730, 1.8f, "DONE RUNS THE SEARCH", kMuted);
            return;
        }

        if (searchResults_.empty()) {
            renderer_.text(120, 330, 2.5f, loading_ ? "SEARCHING..." : "NO RESULTS", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = mediaCardWidth();
        constexpr float cardH = 170.0f;
        constexpr float xGap = 45.0f;
        constexpr float yGap = 48.0f;
        const int selectedRow = searchSelection_ / columns;
        const int firstRow = std::max(0, selectedRow - 2);
        for (int index = firstRow * columns; index < static_cast<int>(searchResults_.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 4) break;
            const float x = 90.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 300.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = index == searchSelection_;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& item = searchResults_[static_cast<size_t>(index)];
            const bool hasArtwork = drawArtwork(item, x, y, 115.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 132.0f : 16.0f);
            const float textWidth = cardW - (hasArtwork ? 148.0f : 32.0f);
            renderer_.text(textX, y + 16, 1.4f, item.type, kMuted, textWidth);
            renderer_.text(textX, y + 52, mediaTitleScale(), item.name, kText, textWidth);
            const auto secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(textX, y + 109, 1.15f, secondary, kMuted, textWidth);
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
            || showNextUp
            || now < playerOverlayUntil_;
        if (const SubtitleCue* cue = activeSubtitleCue()) {
            const std::string subtitle = wrapText(cue->text, 72, 3);
            const float subtitleY = showOverlay ? 585.0f : 865.0f;
            renderer_.rect(250, subtitleY - 18.0f, 1420, 105.0f, Color{0.0f, 0.0f, 0.0f, 0.72f});
            renderer_.text(285, subtitleY + 10.0f, 1.9f, subtitle, kText, 1350.0f);
        }
        if (skipSegment) {
            renderer_.rect(1470, 670, 360, 78, Color{0.0f, 0.0f, 0.0f, 0.82f});
            renderer_.outline(1466, 666, 368, 86, 4, kFocus);
            renderer_.text(1510, 698, 1.75f, mediaSegmentSkipLabel(*skipSegment), kText, 280);
            renderer_.text(1510, 728, 1.0f, "PRESS OK", kMuted, 280);
        }
        if (!showOverlay) return;

        renderer_.rect(0, 0, Renderer::logicalWidth(), 145, Color{0.0f, 0.0f, 0.0f, 0.72f});
        renderer_.rect(0, 790, Renderer::logicalWidth(), 290, Color{0.0f, 0.0f, 0.0f, 0.78f});
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
        renderer_.text(90, 55, 3.2f, heading.empty() ? "PLAYBACK" : heading, kText, 1500);
        const std::string secondary = episodeLabel(activePlaybackItem_);
        if (!secondary.empty() && secondary != heading) renderer_.text(95, 105, 1.8f, secondary, kMuted, 1500);

        const int position = cachedPlaybackPositionMs_;
        const int duration = cachedPlaybackDurationMs_;
        renderer_.text(90, 855, 2.2f,
            nextTransitionLoading_ ? "LOADING NEXT EPISODE" :
            (status == PlayerStatus::Paused ? "PAUSED" : (status == PlayerStatus::Preparing ? "LOADING" : "PLAYING")),
            kText);
        renderer_.text(90, 925, 2.0f, formatPlaybackTime(position), kText);
        renderer_.text(1650, 925, 2.0f, formatPlaybackTime(duration), kText);
        renderer_.rect(250, 940, 1340, 12, kPanelAlt);
        if (duration > 0) {
            const double progress = std::clamp(static_cast<double>(position) / static_cast<double>(duration), 0.0, 1.0);
            renderer_.rect(250, 940, static_cast<float>(1340.0 * progress), 12, kFocus);
        }
        if (playerControlsActive_) {
            const std::array<std::string, 3> controls{
                status == PlayerStatus::Paused ? "PLAY" : "PAUSE",
                "AUDIO " + playerTrackLabel(2, player_.selectedAudioTrack()),
                "SUBS " + playerTrackLabel(4, player_.selectedSubtitleTrack()),
            };
            constexpr float startX = 245.0f;
            constexpr float gap = 25.0f;
            constexpr float width = 460.0f;
            for (size_t i = 0; i < controls.size(); ++i) {
                const float x = startX + static_cast<float>(i) * (width + gap);
                const bool selected = static_cast<int>(i) == playerControlSelection_;
                renderer_.rect(x, 985, width, 68, selected ? kFocus : kPanelAlt);
                renderer_.text(x + 22, 1007, 1.65f, controls[i], kText, width - 44.0f);
            }
        } else {
            renderer_.text(
                90,
                1000,
                1.5f,
                "LEFT -" + std::to_string(settings_.seekBackSeconds) + "S   OK PLAY/PAUSE   RIGHT +" + std::to_string(settings_.seekForwardSeconds) + "S   UP OPTIONS   BACK EXIT",
                kMuted,
                1700
            );
        }
    }

    void renderSettings() {
        renderHeader("SETTINGS");
        const std::array<std::string, 12> labels{
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
            "DIAGNOSTICS",
            "LOG OUT",
        };
        const std::array<std::string, 12> values{
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
            "DEVICE / SERVER / PLAYBACK",
            session_.username.empty() ? "CURRENT USER" : session_.username,
        };
        constexpr int visibleRows = 8;
        const int first = std::clamp(settingsSelection_ - visibleRows + 1, 0, static_cast<int>(labels.size()) - visibleRows);
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int i = first + slot;
            const float y = 125.0f + static_cast<float>(slot) * 101.0f;
            const bool focused = i == settingsSelection_;
            renderer_.rect(120, y, 1380, 92, focused ? kPanelAlt : kPanel);
            if (focused) renderer_.outline(116, y - 4, 1388, 100, 4, kFocus);
            renderer_.text(155, y + 25, 2.0f, labels[static_cast<size_t>(i)], kText, 650);
            renderer_.text(900, y + 25, 2.0f, values[static_cast<size_t>(i)], i == 11 ? kMuted : kFocus, 520);
        }
        renderer_.text(120, 955, 1.6f, "LEFT / RIGHT CHANGES VALUES. OK OPENS ACTIONS. SETTINGS SAVE IMMEDIATELY.", kMuted);
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
            const float y = 180.0f + static_cast<float>(i) * 82.0f;
            renderer_.text(120, y, 1.55f, rows[i].first, kMuted, 430);
            renderer_.text(560, y, 1.55f, rows[i].second, kText, 1240);
        }
        renderer_.text(120, 955, 1.5f, "BACK OR OK RETURNS TO SETTINGS", kMuted);
    }

    void renderMediaGrid(const std::string& title, const std::vector<JellyfinItem>& items, int selection) {
        renderHeader(title);
        if (items.empty()) {
            renderer_.text(100, 250, 2.5f, loading_ ? "LOADING..." : "NO ITEMS", kMuted);
            return;
        }
        constexpr int columns = mediaGridColumns();
        constexpr float cardW = mediaCardWidth();
        constexpr float cardH = 195.0f;
        constexpr float xGap = 45.0f;
        constexpr float yGap = 30.0f;
        const int selectedRow = selection / columns;
        const int firstRow = std::max(0, selectedRow - 2);
        for (int index = firstRow * columns; index < static_cast<int>(items.size()); ++index) {
            const int row = index / columns - firstRow;
            const int col = index % columns;
            if (row >= 4) break;
            const float x = 90.0f + static_cast<float>(col) * (cardW + xGap);
            const float y = 220.0f + static_cast<float>(row) * (cardH + yGap);
            const bool focused = index == selection;
            renderer_.rect(x, y, cardW, cardH, focused ? kPanelAlt : kPanel);
            const auto& item = items[static_cast<size_t>(index)];
            const bool hasArtwork = drawArtwork(item, x, y, 110.0f, cardH);
            if (focused) renderer_.outline(x - 4, y - 4, cardW + 8, cardH + 8, 4, kFocus);
            const float textX = x + (hasArtwork ? 124.0f : 16.0f);
            const float textWidth = cardW - (hasArtwork ? 138.0f : 32.0f);
            renderer_.text(textX, y + 16, 1.25f, item.type, kMuted, textWidth);
            renderer_.text(textX, y + 48, mediaTitleScale(), item.name, kText, textWidth);
            const auto secondary = episodeLabel(item);
            if (!secondary.empty()) renderer_.text(textX, y + 112, 1.1f, secondary, kMuted, textWidth);
            if (item.favorite) renderer_.text(textX, y + 145, 1.0f, "FAVORITE", kFocus, textWidth);
            else if (settings_.showWatchedIndicators && item.played) renderer_.text(textX, y + 145, 1.0f, "WATCHED", kMuted, textWidth);
        }
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
        const bool hasArtwork = drawArtwork(detail_, 110, 205, 300, 450);
        const float contentX = hasArtwork ? 465.0f : 110.0f;
        const float contentWidth = hasArtwork ? 1330.0f : 1680.0f;
        if (stillWatchingPrompt_) {
            renderer_.text(contentX, 165, 2.2f, "STILL WATCHING?", kFocus, contentWidth);
        }
        renderer_.text(contentX, 205, 5.0f, detail_.name.empty() ? "LOADING..." : detail_.name, kText, contentWidth);
        const std::string secondary = episodeLabel(detail_);
        if (!secondary.empty()) renderer_.text(contentX + 5.0f, 285, 2.3f, secondary, kMuted, contentWidth);

        std::string metadata;
        if (detail_.productionYear > 0) metadata += std::to_string(detail_.productionYear);
        if (!detail_.officialRating.empty()) metadata += (metadata.empty() ? "" : "   ") + detail_.officialRating;
        if (detail_.runtimeTicks > 0) metadata += (metadata.empty() ? "" : "   ") + formatPlaybackTime(static_cast<int>(detail_.runtimeTicks / 10000));
        if (detail_.communityRating >= 0.0f) {
            std::ostringstream rating;
            rating << std::fixed << std::setprecision(1) << detail_.communityRating << "/10";
            metadata += (metadata.empty() ? "" : "   ") + rating.str();
        }
        if (!metadata.empty()) renderer_.text(contentX + 5.0f, 330, 1.75f, metadata, kText, contentWidth);
        const std::string genres = joinGenres(detail_.genres);
        if (!genres.empty()) renderer_.text(contentX + 5.0f, 375, 1.5f, genres, kMuted, contentWidth);
        renderer_.text(
            contentX + 5.0f,
            genres.empty() ? 370.0f : 420.0f,
            2.0f,
            wrapText(detail_.overview, hasArtwork ? 82 : 105, hasSimilar ? 5 : 9),
            kMuted,
            contentWidth
        );
        const std::string cast = joinGenres(detail_.cast, 5);
        if (!cast.empty()) renderer_.text(contentX + 5.0f, hasSimilar ? 590.0f : 650.0f, 1.35f, "CAST  " + cast, kMuted, contentWidth);
        const float badgeY = hasSimilar ? 630.0f : 700.0f;
        if (detail_.favorite) renderer_.text(contentX + 5.0f, badgeY, 1.7f, "FAVORITE", kFocus);
        if (settings_.showWatchedIndicators && detail_.played) renderer_.text(contentX + 180.0f, badgeY, 1.7f, "WATCHED", kMuted);

        const auto actions = detailActions();
        const float available = std::min(1690.0f - contentX, 1320.0f);
        const float gap = 14.0f;
        const float buttonWidth = std::max(170.0f, (available - gap * static_cast<float>(actions.size() - 1)) / static_cast<float>(actions.size()));
        const float actionY = hasSimilar ? 680.0f : 835.0f;
        for (size_t i = 0; i < actions.size(); ++i) {
            const float x = contentX + 5.0f + static_cast<float>(i) * (buttonWidth + gap);
            const bool focused = !detailsSimilarFocused_ && detailsButton_ == static_cast<int>(i);
            renderer_.rect(x, actionY, buttonWidth, 74, focused ? kFocus : kPanelAlt);
            renderer_.text(x + 18, actionY + 26, 1.55f, actions[i], kText, buttonWidth - 36.0f);
        }

        if (detail_.positionTicks > 0 && detail_.runtimeTicks > 0) {
            const double fraction = std::clamp(static_cast<double>(detail_.positionTicks) / static_cast<double>(detail_.runtimeTicks), 0.0, 1.0);
            const float progressY = hasSimilar ? 775.0f : 950.0f;
            renderer_.rect(contentX + 5.0f, progressY, 720, 10, kPanelAlt);
            renderer_.rect(contentX + 5.0f, progressY, static_cast<float>(720.0 * fraction), 10, kFocus);
        }

        if (hasSimilar) {
            renderer_.text(90, 810, 1.8f, "MORE LIKE THIS", detailsSimilarFocused_ ? kFocus : kText);
            constexpr int visible = 5;
            const int maxStart = std::max(0, static_cast<int>(detailsSimilar_.size()) - visible);
            const int start = std::clamp(detailsSimilarSelection_ - 2, 0, maxStart);
            constexpr float cardWidth = 330.0f;
            constexpr float cardHeight = 170.0f;
            constexpr float cardGap = 22.0f;
            for (int slot = 0; slot < visible; ++slot) {
                const int index = start + slot;
                if (index >= static_cast<int>(detailsSimilar_.size())) break;
                const auto& similar = detailsSimilar_[static_cast<size_t>(index)];
                const float x = 90.0f + static_cast<float>(slot) * (cardWidth + cardGap);
                const float y = 845.0f;
                renderer_.rect(x, y, cardWidth, cardHeight, kPanel);
                drawHomeArtwork(similar, x, y, cardWidth, cardHeight);
                renderer_.rect(x, y + 126.0f, cardWidth, 44.0f, Color{0.0f, 0.0f, 0.0f, 0.72f});
                renderer_.text(x + 12.0f, y + 141.0f, 1.2f, similar.name, kText, cardWidth - 24.0f);
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

    void loadSession() {
        deviceId_ = generateDeviceId();
        if (dataPath_.empty()) return;
        std::ifstream input(dataPath_ + "/session.json");
        if (!input) return;
        try {
            json data;
            input >> data;
            deviceId_ = data.value("deviceId", deviceId_);
            session_.deviceId = deviceId_;
            session_.server = data.value("server", std::string{});
            session_.username = data.value("username", std::string{});
            session_.userId = data.value("userId", std::string{});
            session_.token = data.value("token", std::string{});
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
            json data = {
                {"deviceId", deviceId_},
                {"server", session.server},
                {"username", session.username},
                {"userId", session.userId},
                {"token", session.token},
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
    int settingsSelection_ = 0;
    std::string error_;

    JellyfinSession session_;
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
    std::vector<JellyfinItem> detailsSimilar_;
    int detailsSimilarSelection_ = 0;
    bool detailsSimilarFocused_ = false;
    JellyfinItem seriesDetail_;
    std::vector<JellyfinItem> seasonItems_;
    int seasonSelection_ = 0;
    JellyfinItem selectedSeason_;
    std::vector<JellyfinItem> episodeItems_;
    int episodeSelection_ = 0;

    std::optional<PlaybackTarget> pendingPlayback_;
    JellyfinItem pendingPlaybackItem_;
    bool pendingStreamRestart_ = false;
    bool pendingRestartPaused_ = false;
    bool pauseAfterRestart_ = false;
    int pendingAudioStreamIndex_ = -1;
    int pendingSubtitleStreamIndex_ = -1;
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
    VideoZoomMode videoZoomMode_ = VideoZoomMode::Fit;
    bool playerControlsActive_ = false;
    int playerControlSelection_ = 0;
    bool subtitleLoadInProgress_ = false;
    std::vector<SubtitleCue> activeSubtitleCues_;
    std::string activeSubtitleLanguage_;
    int activeSubtitleServerIndex_ = -1;
    int selectedAudioServerIndex_ = -1;
    int selectedSubtitleServerIndex_ = -1;
    bool activeSubtitleEnabled_ = false;
    std::chrono::steady_clock::time_point lastProgressReport_{};
    std::chrono::steady_clock::time_point lastPlaybackTelemetryRead_{};
    std::chrono::steady_clock::time_point lastPlaybackDurationProbe_{};
    int cachedPlaybackPositionMs_ = 0;
    int cachedPlaybackDurationMs_ = 0;
    std::chrono::steady_clock::time_point playerOverlayUntil_{};
    std::chrono::steady_clock::time_point renderBurstUntil_{};
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
