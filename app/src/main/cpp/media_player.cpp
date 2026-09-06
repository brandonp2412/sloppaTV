#include "media_player.hpp"
#include "jni_env.hpp"
#include "media_player_policy.hpp"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr const char* kTag = "sloppaTV/mpv";
constexpr int kMpvFormatString = 1;
constexpr int kMpvFormatFlag = 3;
constexpr int kMpvFormatInt64 = 4;
constexpr int kMpvFormatDouble = 5;
constexpr int kMpvEventLogMessage = 2;
constexpr int kMpvEventEndFile = 7;

struct MpvEvent {
    int eventId;
    int error;
    uint64_t replyUserdata;
    void* data;
};

struct MpvEventLogMessage {
    const char* prefix;
    const char* level;
    const char* text;
    int logLevel;
};

struct MpvEventEndFile {
    int reason;
    int error;
};

using ScopedEnv = ScopedJniEnv;

using AvJniSetJavaVm = int (*)(void*, void*);
using AvJniSetAndroidAppCtx = int (*)(void*, void*);

template <typename T>
T loadSymbol(void* library, const char* name) {
    return reinterpret_cast<T>(dlsym(library, name));
}

int boundedMs(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) return 0;
    const double milliseconds = seconds * 1000.0;
    if (milliseconds >= static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(milliseconds);
}

std::string trackProperty(int index, const char* property) {
    return "track-list/" + std::to_string(index) + "/" + property;
}

std::string redactSensitiveQuery(std::string text) {
    for (const char* key : {"api_key=", "ApiKey="}) {
        size_t offset = 0;
        while ((offset = text.find(key, offset)) != std::string::npos) {
            const size_t valueStart = offset + std::char_traits<char>::length(key);
            size_t valueEnd = text.find_first_of("& \t\r\n", valueStart);
            if (valueEnd == std::string::npos) valueEnd = text.size();
            text.replace(valueStart, valueEnd - valueStart, "<redacted>");
            offset = valueStart + 10;
        }
    }
    return text;
}
}

struct MpvSymbols {
    using Create = void* (*)();
    using Initialize = int (*)(void*);
    using TerminateDestroy = void (*)(void*);
    using SetOptionString = int (*)(void*, const char*, const char*);
    using SetOption = int (*)(void*, const char*, int, void*);
    using SetProperty = int (*)(void*, const char*, int, void*);
    using GetProperty = int (*)(void*, const char*, int, void*);
    using Command = int (*)(void*, const char* const*);
    using ErrorString = const char* (*)(int);
    using MpvFree = void (*)(void*);
    using RequestLogMessages = int (*)(void*, const char*);
    using WaitEvent = MpvEvent* (*)(void*, double);

    Create create = nullptr;
    Initialize initialize = nullptr;
    TerminateDestroy terminateDestroy = nullptr;
    SetOptionString setOptionString = nullptr;
    SetOption setOption = nullptr;
    SetProperty setProperty = nullptr;
    GetProperty getProperty = nullptr;
    Command command = nullptr;
    ErrorString errorString = nullptr;
    MpvFree free = nullptr;
    RequestLogMessages requestLogMessages = nullptr;
    WaitEvent waitEvent = nullptr;

    [[nodiscard]] bool complete() const {
        return create && initialize && terminateDestroy && setOptionString && setOption
            && setProperty && getProperty && command && errorString && free && requestLogMessages && waitEvent;
    }
};

NativeMediaPlayer::NativeMediaPlayer(JavaVM* vm, jobject activity, const char* dataPath) : vm_(vm) {
    if (dataPath && *dataPath) filesDir_ = std::string(dataPath) + "/files";
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;

    activity_ = env->NewGlobalRef(activity);
    jclass activityClass = env->GetObjectClass(activity);
    if (!activityClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (activityClass) env->DeleteLocalRef(activityClass);
        return;
    }
    jmethodID getApplicationContext = env->GetMethodID(
        activityClass,
        "getApplicationContext",
        "()Landroid/content/Context;"
    );
    if (!getApplicationContext || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        return;
    }
    jobject context = env->CallObjectMethod(activity, getApplicationContext);
    if (!env->ExceptionCheck() && context) appContext_ = env->NewGlobalRef(context);
    else if (env->ExceptionCheck()) env->ExceptionClear();
    if (context) env->DeleteLocalRef(context);
    env->DeleteLocalRef(activityClass);
}

NativeMediaPlayer::~NativeMediaPlayer() {
    stop();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (env && appContext_) env->DeleteGlobalRef(appContext_);
    if (env && activity_) env->DeleteGlobalRef(activity_);
    appContext_ = nullptr;
    activity_ = nullptr;
}

bool NativeMediaPlayer::ensureAndroidCaBundleLocked(std::string& error) {
    if (filesDir_.empty()) {
        error = "Android app files directory is unavailable";
        return false;
    }

    const std::string outputPath = filesDir_ + "/android-ca-bundle.pem";
    struct stat existing{};
    if (!caBundlePath_.empty() && stat(outputPath.c_str(), &existing) == 0 && existing.st_size > 0) return true;

    const char* sourceDir = "/system/etc/security/cacerts";
    DIR* directory = opendir(sourceDir);
    if (!directory) {
        sourceDir = "/apex/com.android.conscrypt/cacerts";
        directory = opendir(sourceDir);
    }
    if (!directory) {
        error = "Android system CA store is unavailable";
        return false;
    }

    std::vector<std::string> names;
    while (dirent* entry = readdir(directory)) {
        if (entry->d_name[0] == '.') continue;
        names.emplace_back(entry->d_name);
    }
    closedir(directory);
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        error = "Android system CA store is empty";
        return false;
    }

    mkdir(filesDir_.c_str(), 0700);
    const std::string temporaryPath = outputPath + ".tmp";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Unable to create embedded mpv CA bundle";
        return false;
    }

    size_t certificates = 0;
    for (const auto& name : names) {
        std::ifstream input(std::string(sourceDir) + "/" + name, std::ios::binary);
        if (!input) continue;
        output << input.rdbuf() << '\n';
        ++certificates;
    }
    output.close();
    if (certificates == 0 || !output) {
        std::remove(temporaryPath.c_str());
        error = "Unable to read Android system CA certificates";
        return false;
    }
    if (std::rename(temporaryPath.c_str(), outputPath.c_str()) != 0) {
        std::remove(temporaryPath.c_str());
        error = "Unable to install embedded mpv CA bundle";
        return false;
    }

    caBundlePath_ = outputPath;
    __android_log_print(ANDROID_LOG_INFO, kTag, "Prepared Android CA bundle with %zu certificates", certificates);
    return true;
}

bool NativeMediaPlayer::loadLibrariesLocked(std::string& error) {
    if (mpvLibrary_ && avcodecLibrary_ && symbols_) return true;

    mpvLibrary_ = dlopen("libmpv.so", RTLD_NOW | RTLD_LOCAL);
    if (!mpvLibrary_) {
        const char* message = dlerror();
        error = std::string("Unable to load embedded libmpv: ") + (message ? message : "unknown error");
        return false;
    }
    avcodecLibrary_ = dlopen("libavcodec.so", RTLD_NOW | RTLD_LOCAL);
    if (!avcodecLibrary_) {
        const char* message = dlerror();
        error = std::string("Unable to load embedded FFmpeg: ") + (message ? message : "unknown error");
        dlclose(mpvLibrary_);
        mpvLibrary_ = nullptr;
        return false;
    }

    auto symbols = std::make_unique<MpvSymbols>();
    symbols->create = loadSymbol<MpvSymbols::Create>(mpvLibrary_, "mpv_create");
    symbols->initialize = loadSymbol<MpvSymbols::Initialize>(mpvLibrary_, "mpv_initialize");
    symbols->terminateDestroy = loadSymbol<MpvSymbols::TerminateDestroy>(mpvLibrary_, "mpv_terminate_destroy");
    symbols->setOptionString = loadSymbol<MpvSymbols::SetOptionString>(mpvLibrary_, "mpv_set_option_string");
    symbols->setOption = loadSymbol<MpvSymbols::SetOption>(mpvLibrary_, "mpv_set_option");
    symbols->setProperty = loadSymbol<MpvSymbols::SetProperty>(mpvLibrary_, "mpv_set_property");
    symbols->getProperty = loadSymbol<MpvSymbols::GetProperty>(mpvLibrary_, "mpv_get_property");
    symbols->command = loadSymbol<MpvSymbols::Command>(mpvLibrary_, "mpv_command");
    symbols->errorString = loadSymbol<MpvSymbols::ErrorString>(mpvLibrary_, "mpv_error_string");
    symbols->free = loadSymbol<MpvSymbols::MpvFree>(mpvLibrary_, "mpv_free");
    symbols->requestLogMessages = loadSymbol<MpvSymbols::RequestLogMessages>(mpvLibrary_, "mpv_request_log_messages");
    symbols->waitEvent = loadSymbol<MpvSymbols::WaitEvent>(mpvLibrary_, "mpv_wait_event");
    if (!symbols->complete()) {
        error = "Embedded libmpv is missing required client API symbols";
        dlclose(avcodecLibrary_);
        dlclose(mpvLibrary_);
        avcodecLibrary_ = nullptr;
        mpvLibrary_ = nullptr;
        return false;
    }

    symbols_ = symbols.release();
    return true;
}

bool NativeMediaPlayer::setOptionLocked(
    const char* name,
    const std::string& value,
    bool required,
    std::string* error
) const {
    if (!symbols_ || !mpv_) return false;
    const int result = symbols_->setOptionString(mpv_, name, value.c_str());
    if (result >= 0) return true;
    const char* message = symbols_->errorString(result);
    __android_log_print(
        required ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN,
        kTag,
        "mpv option %s=%s failed: %s",
        name,
        value.c_str(),
        message ? message : "unknown"
    );
    if (required && error) {
        *error = std::string("mpv rejected required option ") + name + ": " + (message ? message : "unknown");
    }
    return !required;
}

bool NativeMediaPlayer::initializeLocked(JNIEnv* env, jobject surface, int bufferPreset, std::string& error) {
    if (!env || !surface || !activity_ || !appContext_) {
        error = "Missing Android playback context or surface";
        return false;
    }
    if (!loadLibrariesLocked(error)) return false;

    auto setJavaVm = loadSymbol<AvJniSetJavaVm>(avcodecLibrary_, "av_jni_set_java_vm");
    auto setAndroidAppCtx = loadSymbol<AvJniSetAndroidAppCtx>(avcodecLibrary_, "av_jni_set_android_app_ctx");
    if (!setJavaVm || !setAndroidAppCtx) {
        error = "Embedded FFmpeg is missing Android JNI MediaCodec hooks";
        return false;
    }
    if (setJavaVm(vm_, nullptr) < 0 || setAndroidAppCtx(appContext_, nullptr) < 0) {
        error = "Unable to register Android runtime with embedded FFmpeg";
        return false;
    }

    mpv_ = symbols_->create();
    if (!mpv_) {
        error = "Unable to create embedded libmpv context";
        return false;
    }
    symbols_->requestLogMessages(mpv_, "warn");
    if (!ensureAndroidCaBundleLocked(error)) return false;

    const PlaybackBufferDurations durations = playbackBufferDurations(bufferPreset);
    const std::string cacheBytes = bufferPreset == 2 ? "128MiB" : (bufferPreset == 1 ? "96MiB" : "64MiB");
    const std::string cacheSecs = durations.maxBufferMs > 0
        ? std::to_string(std::max(30, durations.maxBufferMs / 1000))
        : "120";

    const struct {
        const char* name;
        const char* value;
        bool required;
    } options[] = {
        {"config", "no", true},
        {"profile", "fast", false},
        {"vo", "mediacodec_embed", true},
        {"hwdec", "mediacodec,no", true},
        {"hwdec-codecs", "all", true},
        {"vd-lavc-dr", "auto", false},
        {"vd-lavc-film-grain", "auto", false},
        {"force-window", "no", true},
        {"idle", "once", true},
        {"keep-open", "yes", false},
        {"input-default-bindings", "no", false},
        {"input-cursor", "no", false},
        {"osc", "no", false},
        {"osd-level", "0", false},
        {"tls-ca-file", caBundlePath_.c_str(), true},
        {"tls-verify", "yes", true},
        {"cache", "auto", false},
        {"cache-pause", "yes", false},
        {"cache-pause-wait", "2", false},
        {"demuxer-max-bytes", cacheBytes.c_str(), false},
        {"demuxer-max-back-bytes", "16MiB", false},
        {"demuxer-readahead-secs", cacheSecs.c_str(), false},
        {"demuxer-lavf-o", "http_persistent=0,reconnect=1,reconnect_on_network_error=1,reconnect_streamed=1,reconnect_delay_max=5,reconnect_max_retries=5,reconnect_delay_total_max=20", false},
        {"stream-lavf-o", "reconnect=1,reconnect_on_network_error=1,reconnect_on_http_error=5xx,reconnect_streamed=1,reconnect_delay_max=5,reconnect_max_retries=5,reconnect_delay_total_max=20", false},
        {"framedrop", "vo", false},
        {"video-sync", "audio", false},
        {"audio-display", "no", false},
        {"msg-level", "all=warn", false},
    };
    for (const auto& option : options) {
        if (!setOptionLocked(option.name, option.value, option.required, &error)) return false;
    }

    const int initializeResult = symbols_->initialize(mpv_);
    if (initializeResult < 0) {
        const char* message = symbols_->errorString(initializeResult);
        error = std::string("Unable to initialize embedded libmpv: ") + (message ? message : "unknown");
        return false;
    }

    surface_ = env->NewGlobalRef(surface);
    if (!surface_) {
        error = "Unable to retain playback Surface for libmpv";
        return false;
    }
    int64_t wid = reinterpret_cast<intptr_t>(surface_);
    const int surfaceResult = symbols_->setOption(mpv_, "wid", kMpvFormatInt64, &wid);
    if (surfaceResult < 0) {
        const char* message = symbols_->errorString(surfaceResult);
        error = std::string("Unable to bind libmpv to Sloppa Surface: ") + (message ? message : "unknown");
        return false;
    }

    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow) {
        const int width = ANativeWindow_getWidth(nativeWindow);
        const int height = ANativeWindow_getHeight(nativeWindow);
        if (width > 0 && height > 0) {
            setStringPropertyLocked("android-surface-size", std::to_string(width) + "x" + std::to_string(height));
        }
        ANativeWindow_release(nativeWindow);
    }
    if (!setOptionLocked("force-window", "yes", true, &error)) return false;

    __android_log_print(ANDROID_LOG_INFO, kTag, "Embedded libmpv initialized lazily for playback");
    return true;
}

void NativeMediaPlayer::releaseLocked(JNIEnv* env) {
    if (symbols_ && mpv_) {
        int64_t noSurface = 0;
        symbols_->setOption(mpv_, "wid", kMpvFormatInt64, &noSurface);
        symbols_->terminateDestroy(mpv_);
        mpv_ = nullptr;
    }
    if (surface_ && env) {
        env->DeleteGlobalRef(surface_);
        surface_ = nullptr;
    }
    delete symbols_;
    symbols_ = nullptr;
    if (avcodecLibrary_) {
        dlclose(avcodecLibrary_);
        avcodecLibrary_ = nullptr;
    }
    if (mpvLibrary_) {
        dlclose(mpvLibrary_);
        mpvLibrary_ = nullptr;
    }
    pendingAudioOrdinal_ = -1;
    pendingSubtitleStreamIndex_ = -1;
    pendingSubtitleOrdinal_ = -1;
    pendingSubtitleOff_ = false;
    telemetryLogged_ = false;
    cachedStatus_ = PlayerStatus::Idle;
    cachedVideoWidth_ = 0;
    cachedVideoHeight_ = 0;
    cachedSubtitleText_.clear();
    lastSnapshotPoll_ = {};
}

void NativeMediaPlayer::startAsync(
    const std::string& url,
    jobject surface,
    int64_t startPositionMs,
    int bufferPreset,
    int embeddedAudioOrdinal,
    int embeddedSubtitleStreamIndex,
    int embeddedSubtitleOrdinal,
    const std::string& externalSubtitleUrl
) {
    stop();
    if (!surface || url.empty()) {
        std::scoped_lock lock(mutex_);
        error_ = "Missing playback surface or URL";
        cachedStatus_ = PlayerStatus::Error;
        return;
    }

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        std::scoped_lock lock(mutex_);
        error_ = "Unable to attach playback thread to JVM";
        cachedStatus_ = PlayerStatus::Error;
        return;
    }

    std::scoped_lock lock(mutex_);
    std::string error;
    if (!initializeLocked(env, surface, bufferPreset, error)) {
        error_ = error;
        cachedStatus_ = PlayerStatus::Error;
        releaseLocked(env);
        error_ = error;
        cachedStatus_ = PlayerStatus::Error;
        return;
    }

    pendingAudioOrdinal_ = embeddedAudioOrdinal;
    pendingSubtitleStreamIndex_ = embeddedSubtitleStreamIndex;
    pendingSubtitleOrdinal_ = embeddedSubtitleOrdinal;
    pendingSubtitleOff_ = embeddedSubtitleStreamIndex < 0 && externalSubtitleUrl.empty();

    const double startSeconds = static_cast<double>(std::max<int64_t>(0, startPositionMs)) / 1000.0;
    std::string loadOptions = "pause=no";
    if (startSeconds > 0.0) loadOptions += ",start=" + std::to_string(startSeconds);
    const char* loadCommand[] = {"loadfile", url.c_str(), "replace", "-1", loadOptions.c_str(), nullptr};
    if (!commandLocked(loadCommand, "loadfile", &error)) {
        error_ = error;
        cachedStatus_ = PlayerStatus::Error;
        return;
    }
    if (!externalSubtitleUrl.empty()) {
        const char* subCommand[] = {"sub-add", externalSubtitleUrl.c_str(), "select", nullptr};
        if (!commandLocked(subCommand, "external subtitle add", &error)) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "%s", error.c_str());
        }
    }

    error_.clear();
    cachedStatus_ = PlayerStatus::Preparing;
    lastSnapshotPoll_ = {};
    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "Embedded playback requested start=%lldms audioOrdinal=%d subtitleStream=%d",
        static_cast<long long>(std::max<int64_t>(0, startPositionMs)),
        embeddedAudioOrdinal,
        embeddedSubtitleStreamIndex
    );
}

void NativeMediaPlayer::stop() {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    std::scoped_lock lock(mutex_);
    releaseLocked(env);
    error_.clear();
    __android_log_print(ANDROID_LOG_INFO, kTag, "Embedded libmpv playback resources released");
}

bool NativeMediaPlayer::commandLocked(const char* const* args, const char* operation, std::string* error) const {
    if (!symbols_ || !mpv_) return false;
    const int result = symbols_->command(mpv_, args);
    if (result >= 0) return true;
    const char* message = symbols_->errorString(result);
    __android_log_print(ANDROID_LOG_WARN, kTag, "mpv command %s failed: %s", operation, message ? message : "unknown");
    if (error) *error = std::string("mpv ") + operation + " failed: " + (message ? message : "unknown");
    return false;
}

bool NativeMediaPlayer::setStringPropertyLocked(const char* name, const std::string& value) const {
    if (!symbols_ || !mpv_) return false;
    const char* raw = value.c_str();
    return symbols_->setProperty(mpv_, name, kMpvFormatString, &raw) >= 0;
}

bool NativeMediaPlayer::setFlagPropertyLocked(const char* name, bool value) const {
    if (!symbols_ || !mpv_) return false;
    int raw = value ? 1 : 0;
    return symbols_->setProperty(mpv_, name, kMpvFormatFlag, &raw) >= 0;
}

bool NativeMediaPlayer::setIntPropertyLocked(const char* name, int64_t value) const {
    if (!symbols_ || !mpv_) return false;
    return symbols_->setProperty(mpv_, name, kMpvFormatInt64, &value) >= 0;
}

bool NativeMediaPlayer::getFlagPropertyLocked(const char* name, bool& value) const {
    if (!symbols_ || !mpv_) return false;
    int raw = 0;
    if (symbols_->getProperty(mpv_, name, kMpvFormatFlag, &raw) < 0) return false;
    value = raw != 0;
    return true;
}

bool NativeMediaPlayer::getIntPropertyLocked(const char* name, int64_t& value) const {
    if (!symbols_ || !mpv_) return false;
    return symbols_->getProperty(mpv_, name, kMpvFormatInt64, &value) >= 0;
}

bool NativeMediaPlayer::getDoublePropertyLocked(const char* name, double& value) const {
    if (!symbols_ || !mpv_) return false;
    return symbols_->getProperty(mpv_, name, kMpvFormatDouble, &value) >= 0;
}

std::string NativeMediaPlayer::getStringPropertyLocked(const char* name) const {
    if (!symbols_ || !mpv_) return {};
    char* raw = nullptr;
    if (symbols_->getProperty(mpv_, name, kMpvFormatString, &raw) < 0 || !raw) return {};
    std::string result(raw);
    symbols_->free(raw);
    return result;
}

void NativeMediaPlayer::togglePause() {
    std::scoped_lock lock(mutex_);
    bool paused = false;
    if (getFlagPropertyLocked("pause", paused)) setFlagPropertyLocked("pause", !paused);
    lastSnapshotPoll_ = {};
}

void NativeMediaPlayer::pause() {
    std::scoped_lock lock(mutex_);
    setFlagPropertyLocked("pause", true);
    lastSnapshotPoll_ = {};
}

void NativeMediaPlayer::play() {
    std::scoped_lock lock(mutex_);
    setFlagPropertyLocked("pause", false);
    lastSnapshotPoll_ = {};
}

void NativeMediaPlayer::seekBy(int deltaMs) {
    std::scoped_lock lock(mutex_);
    if (!mpv_) return;
    const double seconds = static_cast<double>(deltaMs) / 1000.0;
    const std::string amount = std::to_string(seconds);
    const char* args[] = {"seek", amount.c_str(), "relative+exact", nullptr};
    commandLocked(args, "relative seek");
    lastSnapshotPoll_ = {};
}

void NativeMediaPlayer::seekTo(int positionMs) {
    std::scoped_lock lock(mutex_);
    if (!mpv_) return;
    const double seconds = static_cast<double>(clampSeekPositionMs(positionMs)) / 1000.0;
    const std::string amount = std::to_string(seconds);
    const char* args[] = {"seek", amount.c_str(), "absolute+exact", nullptr};
    commandLocked(args, "absolute seek");
    lastSnapshotPoll_ = {};
}

bool NativeMediaPlayer::selectTrackOrdinalLocked(const char* type, const char* selectionProperty, int ordinal) const {
    if (ordinal < 0) return false;
    int64_t count = 0;
    if (!getIntPropertyLocked("track-list/count", count)) return false;
    int seen = 0;
    for (int64_t index = 0; index < count; ++index) {
        const std::string typeName = getStringPropertyLocked(trackProperty(static_cast<int>(index), "type").c_str());
        if (typeName != type) continue;
        if (seen++ != ordinal) continue;
        int64_t id = -1;
        if (!getIntPropertyLocked(trackProperty(static_cast<int>(index), "id").c_str(), id)) return false;
        return setIntPropertyLocked(selectionProperty, id);
    }
    return false;
}

bool NativeMediaPlayer::selectTrackStreamIndexLocked(
    const char* type,
    const char* selectionProperty,
    int streamIndex,
    int fallbackOrdinal
) const {
    if (streamIndex < 0) return false;
    int64_t count = 0;
    if (!getIntPropertyLocked("track-list/count", count)) return false;
    for (int64_t index = 0; index < count; ++index) {
        const int slot = static_cast<int>(index);
        if (getStringPropertyLocked(trackProperty(slot, "type").c_str()) != type) continue;
        int64_t ffIndex = -1;
        if (!getIntPropertyLocked(trackProperty(slot, "ff-index").c_str(), ffIndex) || ffIndex != streamIndex) continue;
        int64_t id = -1;
        if (!getIntPropertyLocked(trackProperty(slot, "id").c_str(), id)) return false;
        return setIntPropertyLocked(selectionProperty, id);
    }
    return selectTrackOrdinalLocked(type, selectionProperty, fallbackOrdinal);
}

bool NativeMediaPlayer::selectEmbeddedAudioOrdinal(int ordinal) {
    if (ordinal < 0) return false;
    std::scoped_lock lock(mutex_);
    if (selectTrackOrdinalLocked("audio", "aid", ordinal)) {
        pendingAudioOrdinal_ = -1;
        __android_log_print(ANDROID_LOG_INFO, kTag, "Selected embedded audio ordinal %d", ordinal);
        return true;
    }
    pendingAudioOrdinal_ = ordinal;
    return mpv_ != nullptr;
}

bool NativeMediaPlayer::selectEmbeddedSubtitleStream(int streamIndex, int ordinal) {
    if (streamIndex < 0) return disableSubtitles();
    std::scoped_lock lock(mutex_);
    pendingSubtitleOff_ = false;
    if (selectTrackStreamIndexLocked("sub", "sid", streamIndex, ordinal)) {
        pendingSubtitleStreamIndex_ = -1;
        pendingSubtitleOrdinal_ = -1;
        __android_log_print(ANDROID_LOG_INFO, kTag, "Selected embedded subtitle stream %d", streamIndex);
        return true;
    }
    pendingSubtitleStreamIndex_ = streamIndex;
    pendingSubtitleOrdinal_ = ordinal;
    return mpv_ != nullptr;
}

bool NativeMediaPlayer::disableSubtitles() {
    std::scoped_lock lock(mutex_);
    if (!mpv_) return false;
    pendingSubtitleStreamIndex_ = -1;
    pendingSubtitleOrdinal_ = -1;
    pendingSubtitleOff_ = true;
    return setStringPropertyLocked("sid", "no");
}

bool NativeMediaPlayer::addExternalSubtitle(const std::string& url, bool select) {
    if (url.empty()) return false;
    std::scoped_lock lock(mutex_);
    if (!mpv_) return false;
    const char* args[] = {"sub-add", url.c_str(), select ? "select" : "auto", nullptr};
    return commandLocked(args, "external subtitle add");
}

void NativeMediaPlayer::applyPendingTracksLocked() const {
    if (!mpv_) return;
    int64_t count = 0;
    if (!getIntPropertyLocked("track-list/count", count) || count <= 0) return;

    if (pendingAudioOrdinal_ >= 0 && selectTrackOrdinalLocked("audio", "aid", pendingAudioOrdinal_)) {
        __android_log_print(ANDROID_LOG_INFO, kTag, "Applied deferred audio ordinal %d", pendingAudioOrdinal_);
        pendingAudioOrdinal_ = -1;
    }
    if (pendingSubtitleOff_) {
        if (setStringPropertyLocked("sid", "no")) pendingSubtitleOff_ = false;
    } else if (pendingSubtitleStreamIndex_ >= 0
        && selectTrackStreamIndexLocked("sub", "sid", pendingSubtitleStreamIndex_, pendingSubtitleOrdinal_)) {
        __android_log_print(ANDROID_LOG_INFO, kTag, "Applied deferred subtitle stream %d", pendingSubtitleStreamIndex_);
        pendingSubtitleStreamIndex_ = -1;
        pendingSubtitleOrdinal_ = -1;
    }
}

void NativeMediaPlayer::logPlaybackTelemetryLocked() const {
    if (telemetryLogged_ || !mpv_) return;
    const std::string hwdec = getStringPropertyLocked("hwdec-current");
    const std::string video = getStringPropertyLocked("video-codec");
    if (video.empty()) return;
    const std::string audio = getStringPropertyLocked("audio-codec");
    double fps = 0.0;
    getDoublePropertyLocked("container-fps", fps);
    int64_t dropped = 0;
    getIntPropertyLocked("decoder-frame-drop-count", dropped);
    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "Playback telemetry hwdec=%s video=%s audio=%s fps=%.3f dropped=%lld",
        hwdec.empty() ? "none" : hwdec.c_str(),
        video.c_str(),
        audio.empty() ? "unknown" : audio.c_str(),
        fps,
        static_cast<long long>(dropped)
    );
    telemetryLogged_ = true;
}

PlayerStatus NativeMediaPlayer::status() const {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);
    if (!mpv_) return error_.empty() ? PlayerStatus::Idle : PlayerStatus::Error;

    while (true) {
        MpvEvent* event = symbols_->waitEvent(mpv_, 0.0);
        if (!event || event->eventId == 0) break;
        if (event->eventId == kMpvEventLogMessage && event->data) {
            const auto* message = static_cast<const MpvEventLogMessage*>(event->data);
            const std::string safeText = redactSensitiveQuery(message->text ? message->text : "");
            __android_log_print(
                ANDROID_LOG_INFO,
                kTag,
                "core[%s/%s] %s",
                message->prefix ? message->prefix : "?",
                message->level ? message->level : "?",
                safeText.c_str()
            );
        } else if (event->eventId == kMpvEventEndFile && event->data) {
            const auto* end = static_cast<const MpvEventEndFile*>(event->data);
            if (end->error < 0) {
                const char* message = symbols_->errorString(end->error);
                error_ = std::string("mpv playback ended with error: ") + (message ? message : "unknown");
                cachedStatus_ = PlayerStatus::Error;
                __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", error_.c_str());
                return cachedStatus_;
            }
        }
    }
    if (lastSnapshotPoll_ != std::chrono::steady_clock::time_point{}
        && now - lastSnapshotPoll_ < std::chrono::milliseconds(50)) {
        return cachedStatus_;
    }

    applyPendingTracksLocked();
    bool eof = false;
    bool paused = false;
    bool idle = false;
    const bool haveEof = getFlagPropertyLocked("eof-reached", eof);
    const bool havePaused = getFlagPropertyLocked("pause", paused);
    const bool haveIdle = getFlagPropertyLocked("idle-active", idle);
    double duration = 0.0;
    const bool haveDuration = getDoublePropertyLocked("duration", duration) && duration > 0.0;
    double position = 0.0;
    const bool havePosition = getDoublePropertyLocked("time-pos", position) && position >= 0.0;

    int64_t width = 0;
    int64_t height = 0;
    if (getIntPropertyLocked("video-params/w", width)) cachedVideoWidth_ = std::max<int64_t>(0, width);
    if (getIntPropertyLocked("video-params/h", height)) cachedVideoHeight_ = std::max<int64_t>(0, height);
    cachedSubtitleText_ = getStringPropertyLocked("sub-text");

    if (haveEof && eof) {
        cachedStatus_ = PlayerStatus::Paused;
    } else if (haveIdle && idle && !haveDuration) {
        cachedStatus_ = PlayerStatus::Preparing;
    } else if (!havePosition && !haveDuration) {
        cachedStatus_ = PlayerStatus::Preparing;
    } else if (havePaused && paused) {
        cachedStatus_ = PlayerStatus::Paused;
    } else {
        cachedStatus_ = PlayerStatus::Playing;
    }
    if (cachedStatus_ == PlayerStatus::Playing || cachedStatus_ == PlayerStatus::Paused) {
        logPlaybackTelemetryLocked();
    }
    lastSnapshotPoll_ = now;
    return cachedStatus_;
}

std::string NativeMediaPlayer::error() const {
    std::scoped_lock lock(mutex_);
    return error_;
}

int NativeMediaPlayer::positionMs() const {
    std::scoped_lock lock(mutex_);
    double value = 0.0;
    return getDoublePropertyLocked("time-pos", value) ? boundedMs(value) : 0;
}

int NativeMediaPlayer::durationMs() const {
    std::scoped_lock lock(mutex_);
    double value = 0.0;
    return getDoublePropertyLocked("duration", value) ? boundedMs(value) : 0;
}

bool NativeMediaPlayer::seekable() const {
    std::scoped_lock lock(mutex_);
    bool value = false;
    return getFlagPropertyLocked("seekable", value) && value;
}

int NativeMediaPlayer::videoWidth() const {
    (void)status();
    std::scoped_lock lock(mutex_);
    return cachedVideoWidth_;
}

int NativeMediaPlayer::videoHeight() const {
    (void)status();
    std::scoped_lock lock(mutex_);
    return cachedVideoHeight_;
}

std::string NativeMediaPlayer::hardwareDecoder() const {
    std::scoped_lock lock(mutex_);
    return getStringPropertyLocked("hwdec-current");
}

std::string NativeMediaPlayer::videoCodec() const {
    std::scoped_lock lock(mutex_);
    return getStringPropertyLocked("video-codec");
}

std::string NativeMediaPlayer::audioCodec() const {
    std::scoped_lock lock(mutex_);
    return getStringPropertyLocked("audio-codec");
}

std::string NativeMediaPlayer::subtitleText() const {
    (void)status();
    std::scoped_lock lock(mutex_);
    return cachedSubtitleText_;
}

double NativeMediaPlayer::containerFps() const {
    std::scoped_lock lock(mutex_);
    double value = 0.0;
    return getDoublePropertyLocked("container-fps", value) ? value : 0.0;
}

int64_t NativeMediaPlayer::droppedFrames() const {
    std::scoped_lock lock(mutex_);
    int64_t value = 0;
    return getIntPropertyLocked("decoder-frame-drop-count", value) ? value : 0;
}
