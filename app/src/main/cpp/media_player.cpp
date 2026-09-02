#include "media_player.hpp"
#include "media_player_policy.hpp"

#include <android/log.h>

#include <algorithm>
#include <limits>

namespace {
constexpr const char* kTag = "sloppaTV/player";

class ScopedEnv {
public:
    explicit ScopedEnv(JavaVM* vm) : vm_(vm) {
        if (!vm_) return;
        const jint result = vm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED && vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) attached_ = true;
    }
    ~ScopedEnv() { if (attached_ && vm_) vm_->DetachCurrentThread(); }
    JNIEnv* get() const { return env_; }
private:
    JavaVM* vm_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};

bool clearException(JNIEnv* env, const char* operation, std::string* error = nullptr) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_WARN, kTag, "Media3 bridge exception during %s", operation);
    env->ExceptionClear();
    if (error) *error = std::string("Media3 bridge failed during ") + operation;
    return true;
}

std::string fromJString(JNIEnv* env, jstring value) {
    if (!env || !value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

jclass objectClass(JNIEnv* env, jobject object) {
    if (!env || !object) return nullptr;
    jclass result = env->GetObjectClass(object);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return result;
}

int boundedInt(jlong value) {
    return static_cast<int>(std::clamp<jlong>(value, 0, std::numeric_limits<int>::max()));
}
}  // namespace

NativeMediaPlayer::NativeMediaPlayer(JavaVM* vm, jobject activity) : vm_(vm) {
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (env) activity_ = env->NewGlobalRef(activity);
}

NativeMediaPlayer::~NativeMediaPlayer() {
    stop();
    if (!activity_) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (env) env->DeleteGlobalRef(activity_);
    activity_ = nullptr;
}

void NativeMediaPlayer::startAsync(
    const std::string& url,
    jobject surface,
    int64_t startPositionMs,
    int bufferPreset,
    int embeddedAudioOrdinal,
    std::vector<ExternalSubtitleTrack> externalSubtitles
) {
    stop();
    if (!activity_ || !surface || url.empty()) {
        std::scoped_lock lock(mutex_);
        error_ = "Missing playback surface or URL";
        return;
    }

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        std::scoped_lock lock(mutex_);
        error_ = "Unable to attach playback thread to JVM";
        return;
    }

    jclass activityClass = objectClass(env, activity_);
    jmethodID createBridge = activityClass
        ? env->GetMethodID(activityClass, "createPlayerBridge", "()Lnz/presley/sloppatv/SloppaPlayerBridge;")
        : nullptr;
    jobject localPlayer = createBridge ? env->CallObjectMethod(activity_, createBridge) : nullptr;
    std::string error;
    if (!localPlayer || clearException(env, "player bridge construction", &error)) {
        if (activityClass) env->DeleteLocalRef(activityClass);
        if (localPlayer) env->DeleteLocalRef(localPlayer);
        std::scoped_lock lock(mutex_);
        error_ = error.empty() ? "Unable to create Media3 player bridge" : error;
        return;
    }

    const PlaybackBufferDurations bufferDurations = playbackBufferDurations(bufferPreset);
    jclass playerClass = objectClass(env, localPlayer);
    jmethodID start = playerClass
        ? env->GetMethodID(
            playerClass,
            "start",
            "(Ljava/lang/String;Landroid/view/Surface;JIIIIIILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"
        )
        : nullptr;
    if (!start || clearException(env, "player bridge lookup", &error)) {
        if (playerClass) env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(activityClass);
        std::scoped_lock lock(mutex_);
        error_ = error.empty() ? "Unable to initialize Media3 player bridge" : error;
        return;
    }

    const ExternalSubtitleTrack* subtitle = externalSubtitles.empty() ? nullptr : &externalSubtitles.front();
    jstring jUrl = env->NewStringUTF(url.c_str());
    const jint embeddedSubtitleOrdinal = subtitle ? static_cast<jint>(subtitle->embeddedOrdinal) : static_cast<jint>(-1);
    jstring jSubtitleUrl = subtitle && !subtitle->path.empty() ? env->NewStringUTF(subtitle->path.c_str()) : nullptr;
    jstring jSubtitleCodec = subtitle && !subtitle->codec.empty() ? env->NewStringUTF(subtitle->codec.c_str()) : nullptr;
    jstring jSubtitleLanguage = subtitle && !subtitle->language.empty() ? env->NewStringUTF(subtitle->language.c_str()) : nullptr;
    env->CallVoidMethod(
        localPlayer,
        start,
        jUrl,
        surface,
        static_cast<jlong>(std::max<int64_t>(0, startPositionMs)),
        static_cast<jint>(bufferDurations.minBufferMs),
        static_cast<jint>(bufferDurations.maxBufferMs),
        static_cast<jint>(bufferDurations.bufferForPlaybackMs),
        static_cast<jint>(bufferDurations.bufferForPlaybackAfterRebufferMs),
        static_cast<jint>(embeddedAudioOrdinal),
        embeddedSubtitleOrdinal,
        jSubtitleUrl,
        jSubtitleCodec,
        jSubtitleLanguage
    );
    if (jUrl) env->DeleteLocalRef(jUrl);
    if (jSubtitleUrl) env->DeleteLocalRef(jSubtitleUrl);
    if (jSubtitleCodec) env->DeleteLocalRef(jSubtitleCodec);
    if (jSubtitleLanguage) env->DeleteLocalRef(jSubtitleLanguage);
    if (clearException(env, "playback start", &error)) {
        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(activityClass);
        std::scoped_lock lock(mutex_);
        error_ = error;
        return;
    }

    jobject globalPlayer = env->NewGlobalRef(localPlayer);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(localPlayer);
    env->DeleteLocalRef(activityClass);
    if (!globalPlayer) {
        std::scoped_lock lock(mutex_);
        error_ = "Unable to retain Media3 player bridge";
        return;
    }
    {
        std::scoped_lock lock(mutex_);
        player_ = globalPlayer;
        error_.clear();
        cachedStatus_ = PlayerStatus::Preparing;
        cachedVideoWidth_ = 0;
        cachedVideoHeight_ = 0;
        cachedPlaybackSpeed_ = 1.0f;
        lastSnapshotPoll_ = {};
    }
    __android_log_print(ANDROID_LOG_INFO, kTag, "Media3 playback requested (buffer preset %d)", bufferPreset);
}

void NativeMediaPlayer::stop() {
    jobject playerToRelease = nullptr;
    {
        std::scoped_lock lock(mutex_);
        playerToRelease = player_;
        player_ = nullptr;
        error_.clear();
        cachedStatus_ = PlayerStatus::Idle;
        cachedVideoWidth_ = 0;
        cachedVideoHeight_ = 0;
        cachedPlaybackSpeed_ = 1.0f;
        lastSnapshotPoll_ = {};
    }
    if (!playerToRelease) return;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    jclass playerClass = objectClass(env, playerToRelease);
    jmethodID release = playerClass ? env->GetMethodID(playerClass, "release", "()V") : nullptr;
    if (release) env->CallVoidMethod(playerToRelease, release);
    clearException(env, "player release");
    if (playerClass) env->DeleteLocalRef(playerClass);
    env->DeleteGlobalRef(playerToRelease);
    __android_log_print(ANDROID_LOG_INFO, kTag, "Media3 player detached for asynchronous release");
}

void NativeMediaPlayer::togglePause() {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    std::scoped_lock lock(mutex_);
    if (!player_) return;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "togglePause", "()V") : nullptr;
    if (method) env->CallVoidMethod(player_, method);
    lastSnapshotPoll_ = {};
    clearException(env, "toggle pause");
    if (playerClass) env->DeleteLocalRef(playerClass);
}

void NativeMediaPlayer::seekBy(int deltaMs) {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    std::scoped_lock lock(mutex_);
    if (!player_) return;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "seekBy", "(J)V") : nullptr;
    if (method) env->CallVoidMethod(player_, method, static_cast<jlong>(deltaMs));
    clearException(env, "relative seek");
    if (playerClass) env->DeleteLocalRef(playerClass);
}

void NativeMediaPlayer::seekTo(int positionMs) {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    std::scoped_lock lock(mutex_);
    if (!player_) return;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "seekTo", "(J)V") : nullptr;
    if (method) env->CallVoidMethod(player_, method, static_cast<jlong>(clampSeekPositionMs(positionMs)));
    clearException(env, "absolute seek");
    if (playerClass) env->DeleteLocalRef(playerClass);
}

bool NativeMediaPlayer::selectEmbeddedAudioOrdinal(int ordinal) {
    if (ordinal < 0) return false;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return false;
    std::scoped_lock lock(mutex_);
    if (!player_) return false;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "selectEmbeddedAudioOrdinal", "(I)V") : nullptr;
    if (method) env->CallVoidMethod(player_, method, static_cast<jint>(ordinal));
    const bool ok = method && !clearException(env, "embedded audio selection");
    if (playerClass) env->DeleteLocalRef(playerClass);
    return ok;
}

bool NativeMediaPlayer::setPlaybackSpeed(float speed) {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return false;
    std::scoped_lock lock(mutex_);
    if (!player_) return false;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "setPlaybackSpeed", "(F)V") : nullptr;
    const float bounded = std::clamp(speed, 0.25f, 2.0f);
    if (method) env->CallVoidMethod(player_, method, static_cast<jfloat>(bounded));
    cachedPlaybackSpeed_ = bounded;
    const bool ok = method && !clearException(env, "playback speed");
    if (playerClass) env->DeleteLocalRef(playerClass);
    return ok;
}

bool NativeMediaPlayer::selectTrack(int) { return false; }
bool NativeMediaPlayer::deselectTrack(int) { return false; }
bool NativeMediaPlayer::addExternalSubtitle(const std::string&, const std::string&, bool) { return false; }

PlayerStatus NativeMediaPlayer::status() const {
    const auto now = std::chrono::steady_clock::now();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return PlayerStatus::Error;
    std::scoped_lock lock(mutex_);
    if (!player_) return error_.empty() ? PlayerStatus::Idle : PlayerStatus::Error;
    if (lastSnapshotPoll_ != std::chrono::steady_clock::time_point{}
        && now - lastSnapshotPoll_ < std::chrono::milliseconds(50)) {
        return cachedStatus_;
    }

    jclass playerClass = objectClass(env, player_);
    jmethodID stateMethod = playerClass ? env->GetMethodID(playerClass, "getState", "()I") : nullptr;
    jmethodID widthMethod = playerClass ? env->GetMethodID(playerClass, "getVideoWidth", "()I") : nullptr;
    jmethodID heightMethod = playerClass ? env->GetMethodID(playerClass, "getVideoHeight", "()I") : nullptr;
    jmethodID speedMethod = playerClass ? env->GetMethodID(playerClass, "getPlaybackSpeed", "()F") : nullptr;
    const jint state = stateMethod ? env->CallIntMethod(player_, stateMethod) : 4;
    if (!env->ExceptionCheck() && widthMethod) cachedVideoWidth_ = std::max(0, static_cast<int>(env->CallIntMethod(player_, widthMethod)));
    if (!env->ExceptionCheck() && heightMethod) cachedVideoHeight_ = std::max(0, static_cast<int>(env->CallIntMethod(player_, heightMethod)));
    if (!env->ExceptionCheck() && speedMethod) cachedPlaybackSpeed_ = env->CallFloatMethod(player_, speedMethod);
    clearException(env, "state snapshot read");
    if (playerClass) env->DeleteLocalRef(playerClass);

    switch (state) {
        case 0: cachedStatus_ = PlayerStatus::Idle; break;
        case 1: cachedStatus_ = PlayerStatus::Preparing; break;
        case 2: cachedStatus_ = PlayerStatus::Playing; break;
        case 3: cachedStatus_ = PlayerStatus::Paused; break;
        default: cachedStatus_ = PlayerStatus::Error; break;
    }
    lastSnapshotPoll_ = now;
    return cachedStatus_;
}

std::string NativeMediaPlayer::error() const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return error_;
    std::scoped_lock lock(mutex_);
    if (!player_) return error_;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "getError", "()Ljava/lang/String;") : nullptr;
    auto value = method ? static_cast<jstring>(env->CallObjectMethod(player_, method)) : nullptr;
    std::string result = value ? fromJString(env, value) : error_;
    if (value) env->DeleteLocalRef(value);
    clearException(env, "error read");
    if (playerClass) env->DeleteLocalRef(playerClass);
    return result;
}

int NativeMediaPlayer::positionMs() const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return 0;
    std::scoped_lock lock(mutex_);
    if (!player_) return 0;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "getPositionMs", "()J") : nullptr;
    const jlong value = method ? env->CallLongMethod(player_, method) : 0;
    clearException(env, "position read");
    if (playerClass) env->DeleteLocalRef(playerClass);
    return boundedInt(value);
}

int NativeMediaPlayer::durationMs() const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return 0;
    std::scoped_lock lock(mutex_);
    if (!player_) return 0;
    jclass playerClass = objectClass(env, player_);
    jmethodID method = playerClass ? env->GetMethodID(playerClass, "getDurationMs", "()J") : nullptr;
    const jlong value = method ? env->CallLongMethod(player_, method) : 0;
    clearException(env, "duration read");
    if (playerClass) env->DeleteLocalRef(playerClass);
    return boundedInt(value);
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

float NativeMediaPlayer::playbackSpeed() const {
    (void)status();
    std::scoped_lock lock(mutex_);
    return cachedPlaybackSpeed_;
}

std::vector<PlayerTrack> NativeMediaPlayer::tracks() const { return {}; }
int NativeMediaPlayer::selectedAudioTrack() const { return -1; }
int NativeMediaPlayer::selectedSubtitleTrack() const { return -1; }
