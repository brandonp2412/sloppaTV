#include "media_session.hpp"
#include "jni_env.hpp"

#include <android/log.h>

#include <algorithm>
#include <cstdlib>

namespace {
constexpr const char* kTag = "sloppaTV/media-session";
constexpr int64_t kTransportActions =
    1LL | 2LL | 4LL | 16LL | 32LL | 256LL; // STOP, PAUSE, PLAY, PREVIOUS, NEXT, SEEK_TO
std::mutex gInstanceMutex;
NativeMediaSession* gInstance = nullptr;

using ScopedEnv = ScopedJniEnv;

bool clearException(JNIEnv* env, const char* operation) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_WARN, kTag, "JNI exception during %s", operation);
    env->ExceptionClear();
    return true;
}

bool setPlaybackKeepScreenOn(JNIEnv* env, jobject activity, bool enabled) {
    if (!env || !activity) return false;
    jclass activityClass = env->GetObjectClass(activity);
    if (clearException(env, "playback keep-screen-on class lookup") || !activityClass) {
        if (activityClass) env->DeleteLocalRef(activityClass);
        return false;
    }
    jmethodID method = env->GetMethodID(activityClass, "setPlaybackKeepScreenOn", "(Z)V");
    if (clearException(env, "playback keep-screen-on method lookup") || !method) {
        env->DeleteLocalRef(activityClass);
        return false;
    }
    env->CallVoidMethod(activity, method, enabled ? JNI_TRUE : JNI_FALSE);
    const bool failed = clearException(env, "playback keep-screen-on update");
    env->DeleteLocalRef(activityClass);
    return !failed;
}

int playbackStateValue(MediaSessionState state) {
    switch (state) {
    case MediaSessionState::Stopped:
        return 1; // PlaybackState.STATE_STOPPED
    case MediaSessionState::Paused:
        return 2; // PlaybackState.STATE_PAUSED
    case MediaSessionState::Playing:
        return 3; // PlaybackState.STATE_PLAYING
    case MediaSessionState::Buffering:
        return 6; // PlaybackState.STATE_BUFFERING
    }
    return 1;
}
} // namespace

NativeMediaSession::NativeMediaSession(JavaVM* vm, jobject activity) : vm_(vm) {
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    activity_ = env->NewGlobalRef(activity);
    if (clearException(env, "MediaSession activity retention") || !activity_) {
        if (activity_) env->DeleteGlobalRef(activity_);
        activity_ = nullptr;
        return;
    }
    std::scoped_lock lock(gInstanceMutex);
    gInstance = this;
}

NativeMediaSession::~NativeMediaSession() {
    {
        std::scoped_lock lock(gInstanceMutex);
        if (gInstance == this) gInstance = nullptr;
    }
    clear();
    if (!activity_) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    env->DeleteGlobalRef(activity_);
    activity_ = nullptr;
}

bool NativeMediaSession::ensureSession() {
    if (session_) return true;
    if (!activity_) return false;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return false;

    // Android's MediaSession constructor needs the Activity/main looper on some TV
    // firmware (including the Google TV Streamer). Let the Java bridge create and
    // configure it on the UI thread, then keep only a global JNI reference here.
    jclass activityClass = env->GetObjectClass(activity_);
    if (clearException(env, "MediaSession activity class lookup") || !activityClass) {
        if (activityClass) env->DeleteLocalRef(activityClass);
        return false;
    }

    jmethodID createSession =
        env->GetMethodID(activityClass, "createMediaSessionBridge", "()Landroid/media/session/MediaSession;");
    if (clearException(env, "MediaSession bridge method lookup") || !createSession) {
        env->DeleteLocalRef(activityClass);
        return false;
    }

    jobject localSession = env->CallObjectMethod(activity_, createSession);
    if (clearException(env, "MediaSession bridge construction") || !localSession) {
        if (localSession) env->DeleteLocalRef(localSession);
        env->DeleteLocalRef(activityClass);
        return false;
    }

    session_ = env->NewGlobalRef(localSession);
    const bool retainFailed = clearException(env, "MediaSession bridge retention");
    env->DeleteLocalRef(localSession);
    env->DeleteLocalRef(activityClass);
    if (retainFailed || !session_) {
        if (session_) env->DeleteGlobalRef(session_);
        session_ = nullptr;
        return false;
    }
    __android_log_print(ANDROID_LOG_INFO, kTag, "Android media session created for playback");
    return true;
}

void NativeMediaSession::updateMetadata(const std::string& title, const std::string& subtitle, int64_t durationMs) {
    if (!ensureSession()) return;
    durationMs = std::max<int64_t>(0, durationMs);
    if (title == title_ && subtitle == subtitle_ && durationMs == durationMs_) return;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    jclass builderClass = env->FindClass("android/media/MediaMetadata$Builder");
    if (clearException(env, "metadata builder class lookup") || !builderClass) {
        if (builderClass) env->DeleteLocalRef(builderClass);
        return;
    }
    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    if (clearException(env, "metadata session class lookup") || !sessionClass) {
        if (sessionClass) env->DeleteLocalRef(sessionClass);
        env->DeleteLocalRef(builderClass);
        return;
    }

    const auto getMethod = [&](jclass clazz, const char* name, const char* signature) {
        jmethodID method = env->GetMethodID(clazz, name, signature);
        return clearException(env, "metadata method lookup") ? static_cast<jmethodID>(nullptr) : method;
    };
    jmethodID ctor = getMethod(builderClass, "<init>", "()V");
    jmethodID putString = getMethod(builderClass, "putString",
                                    "(Ljava/lang/String;Ljava/lang/String;)Landroid/media/MediaMetadata$Builder;");
    jmethodID putLong =
        getMethod(builderClass, "putLong", "(Ljava/lang/String;J)Landroid/media/MediaMetadata$Builder;");
    jmethodID build = getMethod(builderClass, "build", "()Landroid/media/MediaMetadata;");
    jmethodID setMetadata = getMethod(sessionClass, "setMetadata", "(Landroid/media/MediaMetadata;)V");
    if (!ctor || !putString || !putLong || !build || !setMetadata) {
        env->DeleteLocalRef(builderClass);
        env->DeleteLocalRef(sessionClass);
        return;
    }

    jobject builder = env->NewObject(builderClass, ctor);
    if (clearException(env, "media metadata builder construction") || !builder) {
        if (builder) env->DeleteLocalRef(builder);
        env->DeleteLocalRef(builderClass);
        env->DeleteLocalRef(sessionClass);
        return;
    }

    auto putStringValue = [&](const char* key, const std::string& value) {
        if (value.empty()) return true;
        jstring jKey = jniNewString(env, key);
        if (clearException(env, "media metadata key creation") || !jKey) {
            if (jKey) env->DeleteLocalRef(jKey);
            return false;
        }
        jstring jValue = jniNewString(env, value);
        if (clearException(env, "media metadata value creation") || !jValue) {
            if (jValue) env->DeleteLocalRef(jValue);
            env->DeleteLocalRef(jKey);
            return false;
        }
        env->CallObjectMethod(builder, putString, jKey, jValue);
        const bool failed = clearException(env, "media metadata string update");
        env->DeleteLocalRef(jKey);
        env->DeleteLocalRef(jValue);
        return !failed;
    };
    bool failed = !putStringValue("android.media.metadata.TITLE", title) ||
                  !putStringValue("android.media.metadata.DISPLAY_TITLE", title) ||
                  !putStringValue("android.media.metadata.DISPLAY_SUBTITLE", subtitle);
    if (!failed && durationMs > 0) {
        jstring key = jniNewString(env, "android.media.metadata.DURATION");
        failed = clearException(env, "media metadata duration key creation") || !key;
        if (!failed) {
            env->CallObjectMethod(builder, putLong, key, static_cast<jlong>(durationMs));
            failed = clearException(env, "media metadata duration update");
        }
        if (key) env->DeleteLocalRef(key);
    }

    jobject metadata = nullptr;
    if (!failed) {
        metadata = env->CallObjectMethod(builder, build);
        failed = clearException(env, "media metadata build") || !metadata;
    }
    if (!failed) {
        env->CallVoidMethod(session_, setMetadata, metadata);
        failed = clearException(env, "media metadata update");
    }
    if (!failed) {
        title_ = title;
        subtitle_ = subtitle;
        durationMs_ = durationMs;
    }
    if (metadata) env->DeleteLocalRef(metadata);
    env->DeleteLocalRef(builder);
    env->DeleteLocalRef(builderClass);
    env->DeleteLocalRef(sessionClass);
}

void NativeMediaSession::updateState(MediaSessionState state, int64_t positionMs) {
    const bool keepScreenOn = mediaSessionNeedsScreenOn(state);
    if (shouldUpdateKeepScreenOn(keepScreenOn_, keepScreenOn)) {
        ScopedEnv scoped(vm_);
        JNIEnv* env = scoped.get();
        const bool updated = env && setPlaybackKeepScreenOn(env, activity_, keepScreenOn);
        keepScreenOn_ = keepScreenOnAfterAttempt(keepScreenOn_, keepScreenOn, updated);
    }
    if (state == MediaSessionState::Stopped && !session_) return;
    if (!ensureSession()) return;
    positionMs = std::max<int64_t>(0, positionMs);
    if (state == state_ && lastPositionMs_ >= 0 && std::abs(positionMs - lastPositionMs_) < 5000) return;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    jclass builderClass = env->FindClass("android/media/session/PlaybackState$Builder");
    if (clearException(env, "playback-state builder class lookup") || !builderClass) {
        if (builderClass) env->DeleteLocalRef(builderClass);
        return;
    }
    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    if (clearException(env, "playback-state session class lookup") || !sessionClass) {
        if (sessionClass) env->DeleteLocalRef(sessionClass);
        env->DeleteLocalRef(builderClass);
        return;
    }
    jclass clockClass = env->FindClass("android/os/SystemClock");
    if (clearException(env, "playback-state clock class lookup") || !clockClass) {
        if (clockClass) env->DeleteLocalRef(clockClass);
        env->DeleteLocalRef(builderClass);
        env->DeleteLocalRef(sessionClass);
        return;
    }

    const auto getMethod = [&](jclass clazz, const char* name, const char* signature) {
        jmethodID method = env->GetMethodID(clazz, name, signature);
        return clearException(env, "playback-state method lookup") ? static_cast<jmethodID>(nullptr) : method;
    };
    const auto getStaticMethod = [&](jclass clazz, const char* name, const char* signature) {
        jmethodID method = env->GetStaticMethodID(clazz, name, signature);
        return clearException(env, "playback-state static method lookup") ? static_cast<jmethodID>(nullptr) : method;
    };
    jmethodID ctor = getMethod(builderClass, "<init>", "()V");
    jmethodID setState = getMethod(builderClass, "setState", "(IJFJ)Landroid/media/session/PlaybackState$Builder;");
    jmethodID setActions = getMethod(builderClass, "setActions", "(J)Landroid/media/session/PlaybackState$Builder;");
    jmethodID build = getMethod(builderClass, "build", "()Landroid/media/session/PlaybackState;");
    jmethodID setPlaybackState =
        getMethod(sessionClass, "setPlaybackState", "(Landroid/media/session/PlaybackState;)V");
    jmethodID setActive = getMethod(sessionClass, "setActive", "(Z)V");
    jmethodID elapsedRealtime = getStaticMethod(clockClass, "elapsedRealtime", "()J");
    if (!ctor || !setState || !setActions || !build || !setPlaybackState || !setActive || !elapsedRealtime) {
        env->DeleteLocalRef(builderClass);
        env->DeleteLocalRef(sessionClass);
        env->DeleteLocalRef(clockClass);
        return;
    }

    jobject builder = env->NewObject(builderClass, ctor);
    bool failed = clearException(env, "playback-state builder construction") || !builder;
    jlong now = 0;
    if (!failed) {
        now = env->CallStaticLongMethod(clockClass, elapsedRealtime);
        failed = clearException(env, "playback-state clock read");
    }
    const jfloat speed = state == MediaSessionState::Playing ? 1.0f : 0.0f;
    if (!failed) {
        env->CallObjectMethod(builder, setState, static_cast<jint>(playbackStateValue(state)),
                              static_cast<jlong>(positionMs), speed, now);
        failed = clearException(env, "playback-state state update");
    }
    if (!failed) {
        env->CallObjectMethod(builder, setActions, static_cast<jlong>(kTransportActions));
        failed = clearException(env, "playback-state action update");
    }

    jobject playbackState = nullptr;
    if (!failed) {
        playbackState = env->CallObjectMethod(builder, build);
        failed = clearException(env, "playback-state build") || !playbackState;
    }
    if (!failed) {
        env->CallVoidMethod(session_, setPlaybackState, playbackState);
        failed = clearException(env, "playback-state session update");
    }
    if (!failed) {
        env->CallVoidMethod(session_, setActive, state == MediaSessionState::Stopped ? JNI_FALSE : JNI_TRUE);
        failed = clearException(env, "playback-state active update");
    }
    if (!failed) {
        state_ = state;
        lastPositionMs_ = positionMs;
    }
    if (playbackState) env->DeleteLocalRef(playbackState);
    if (builder) env->DeleteLocalRef(builder);
    env->DeleteLocalRef(builderClass);
    env->DeleteLocalRef(sessionClass);
    env->DeleteLocalRef(clockClass);
}

std::optional<MediaSessionCommand> NativeMediaSession::takeCommand() {
    std::scoped_lock lock(commandMutex_);
    auto command = pendingCommand_;
    pendingCommand_.reset();
    return command;
}

void NativeMediaSession::handlePlatformCommand(int command, int64_t positionMs) {
    std::optional<MediaSessionCommandType> type;
    switch (command) {
    case 1:
        type = MediaSessionCommandType::Play;
        break;
    case 2:
        type = MediaSessionCommandType::Pause;
        break;
    case 3:
        type = MediaSessionCommandType::Stop;
        break;
    case 4:
        type = MediaSessionCommandType::SeekTo;
        break;
    case 5:
        type = MediaSessionCommandType::Next;
        break;
    case 6:
        type = MediaSessionCommandType::Previous;
        break;
    default:
        break;
    }
    if (!type) return;
    {
        std::scoped_lock lock(commandMutex_);
        pendingCommand_ = MediaSessionCommand{
            .type = *type,
            .positionMs = std::max<int64_t>(0, positionMs),
        };
    }
    __android_log_print(ANDROID_LOG_INFO, kTag, "Received platform transport command %d at %lld ms", command,
                        static_cast<long long>(positionMs));
}

void NativeMediaSession::clear() {
    title_.clear();
    subtitle_.clear();
    durationMs_ = -1;
    state_ = MediaSessionState::Stopped;
    lastPositionMs_ = -1;
    {
        std::scoped_lock lock(commandMutex_);
        pendingCommand_.reset();
    }

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (env && keepScreenOn_.value_or(false)) setPlaybackKeepScreenOn(env, activity_, false);
    keepScreenOn_ = false;
    if (!session_ || !env) return;
    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    if (!clearException(env, "MediaSession release class lookup") && sessionClass) {
        jmethodID setActive = env->GetMethodID(sessionClass, "setActive", "(Z)V");
        const bool setActiveLookupFailed = clearException(env, "MediaSession release setActive lookup");
        jmethodID release = env->GetMethodID(sessionClass, "release", "()V");
        const bool releaseLookupFailed = clearException(env, "MediaSession release method lookup");
        if (!setActiveLookupFailed && setActive) {
            env->CallVoidMethod(session_, setActive, JNI_FALSE);
            clearException(env, "MediaSession release deactivate");
        }
        if (!releaseLookupFailed && release) {
            env->CallVoidMethod(session_, release);
            clearException(env, "MediaSession release");
        }
        env->DeleteLocalRef(sessionClass);
    } else if (sessionClass) {
        env->DeleteLocalRef(sessionClass);
    }
    env->DeleteGlobalRef(session_);
    session_ = nullptr;
    __android_log_print(ANDROID_LOG_INFO, kTag, "Android media session released");
}

extern "C" JNIEXPORT void JNICALL Java_app_sloppatv_SloppaNativeActivity_nativeOnMediaSessionCommand(JNIEnv*, jclass,
                                                                                                     jint command,
                                                                                                     jlong positionMs) {
    std::scoped_lock lock(gInstanceMutex);
    if (gInstance) gInstance->handlePlatformCommand(command, positionMs);
}
