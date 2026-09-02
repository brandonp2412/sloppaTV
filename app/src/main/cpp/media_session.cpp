#include "media_session.hpp"

#include <android/log.h>

#include <algorithm>
#include <cstdlib>

namespace {
constexpr const char* kTag = "sloppaTV/media-session";

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

bool clearException(JNIEnv* env, const char* operation) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_WARN, kTag, "JNI exception during %s", operation);
    env->ExceptionClear();
    return true;
}

int playbackStateValue(MediaSessionState state) {
    switch (state) {
        case MediaSessionState::Stopped: return 1;   // PlaybackState.STATE_STOPPED
        case MediaSessionState::Paused: return 2;    // PlaybackState.STATE_PAUSED
        case MediaSessionState::Playing: return 3;   // PlaybackState.STATE_PLAYING
        case MediaSessionState::Buffering: return 6; // PlaybackState.STATE_BUFFERING
    }
    return 1;
}
}  // namespace

NativeMediaSession::NativeMediaSession(JavaVM* vm, jobject activity) : vm_(vm) {
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    activity_ = env->NewGlobalRef(activity);
}

NativeMediaSession::~NativeMediaSession() {
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

    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    if (!sessionClass || clearException(env, "MediaSession class lookup")) return false;
    jmethodID ctor = env->GetMethodID(sessionClass, "<init>", "(Landroid/content/Context;Ljava/lang/String;)V");
    jmethodID setFlags = env->GetMethodID(sessionClass, "setFlags", "(I)V");
    if (!ctor || !setFlags || clearException(env, "MediaSession method lookup")) {
        env->DeleteLocalRef(sessionClass);
        return false;
    }

    jstring tag = env->NewStringUTF("sloppaTV");
    jobject localSession = tag ? env->NewObject(sessionClass, ctor, activity_, tag) : nullptr;
    if (tag) env->DeleteLocalRef(tag);
    if (!localSession || clearException(env, "MediaSession construction")) {
        env->DeleteLocalRef(sessionClass);
        return false;
    }

    // Hardware media keys already arrive through NativeActivity input. Do not advertise
    // transport callbacks until a real callback bridge exists.
    env->CallVoidMethod(localSession, setFlags, 0);
    if (!clearException(env, "MediaSession initialization")) {
        session_ = env->NewGlobalRef(localSession);
    }
    env->DeleteLocalRef(localSession);
    env->DeleteLocalRef(sessionClass);
    if (session_) __android_log_print(ANDROID_LOG_INFO, kTag, "Android media session created for playback");
    return session_ != nullptr;
}

void NativeMediaSession::updateMetadata(
    const std::string& title,
    const std::string& subtitle,
    int64_t durationMs
) {
    if (!ensureSession()) return;
    durationMs = std::max<int64_t>(0, durationMs);
    if (title == title_ && subtitle == subtitle_ && durationMs == durationMs_) return;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    jclass builderClass = env->FindClass("android/media/MediaMetadata$Builder");
    jclass metadataClass = env->FindClass("android/media/MediaMetadata");
    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    if (!builderClass || !metadataClass || !sessionClass || clearException(env, "metadata class lookup")) return;

    jmethodID ctor = env->GetMethodID(builderClass, "<init>", "()V");
    jmethodID putString = env->GetMethodID(
        builderClass,
        "putString",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/media/MediaMetadata$Builder;"
    );
    jmethodID putLong = env->GetMethodID(
        builderClass,
        "putLong",
        "(Ljava/lang/String;J)Landroid/media/MediaMetadata$Builder;"
    );
    jmethodID build = env->GetMethodID(builderClass, "build", "()Landroid/media/MediaMetadata;");
    jmethodID setMetadata = env->GetMethodID(sessionClass, "setMetadata", "(Landroid/media/MediaMetadata;)V");
    if (!ctor || !putString || !putLong || !build || !setMetadata || clearException(env, "metadata method lookup")) {
        env->DeleteLocalRef(builderClass);
        env->DeleteLocalRef(metadataClass);
        env->DeleteLocalRef(sessionClass);
        return;
    }

    jobject builder = env->NewObject(builderClass, ctor);
    auto putStringValue = [&](const char* key, const std::string& value) {
        if (!builder || value.empty()) return;
        jstring jKey = env->NewStringUTF(key);
        jstring jValue = env->NewStringUTF(value.c_str());
        if (jKey && jValue) env->CallObjectMethod(builder, putString, jKey, jValue);
        if (jKey) env->DeleteLocalRef(jKey);
        if (jValue) env->DeleteLocalRef(jValue);
    };
    putStringValue("android.media.metadata.TITLE", title);
    putStringValue("android.media.metadata.DISPLAY_TITLE", title);
    putStringValue("android.media.metadata.DISPLAY_SUBTITLE", subtitle);
    if (builder && durationMs > 0) {
        jstring key = env->NewStringUTF("android.media.metadata.DURATION");
        if (key) {
            env->CallObjectMethod(builder, putLong, key, static_cast<jlong>(durationMs));
            env->DeleteLocalRef(key);
        }
    }
    jobject metadata = builder ? env->CallObjectMethod(builder, build) : nullptr;
    if (metadata) env->CallVoidMethod(session_, setMetadata, metadata);
    const bool failed = clearException(env, "media metadata update");
    if (!failed) {
        title_ = title;
        subtitle_ = subtitle;
        durationMs_ = durationMs;
    }
    if (metadata) env->DeleteLocalRef(metadata);
    if (builder) env->DeleteLocalRef(builder);
    env->DeleteLocalRef(builderClass);
    env->DeleteLocalRef(metadataClass);
    env->DeleteLocalRef(sessionClass);
}

void NativeMediaSession::updateState(MediaSessionState state, int64_t positionMs) {
    if (state == MediaSessionState::Stopped && !session_) return;
    if (!ensureSession()) return;
    positionMs = std::max<int64_t>(0, positionMs);
    if (state == state_ && lastPositionMs_ >= 0 && std::abs(positionMs - lastPositionMs_) < 5000) return;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    jclass builderClass = env->FindClass("android/media/session/PlaybackState$Builder");
    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    jclass clockClass = env->FindClass("android/os/SystemClock");
    if (!builderClass || !sessionClass || !clockClass || clearException(env, "playback-state class lookup")) return;

    jmethodID ctor = env->GetMethodID(builderClass, "<init>", "()V");
    jmethodID setState = env->GetMethodID(builderClass, "setState", "(IJFJ)Landroid/media/session/PlaybackState$Builder;");
    jmethodID setActions = env->GetMethodID(builderClass, "setActions", "(J)Landroid/media/session/PlaybackState$Builder;");
    jmethodID build = env->GetMethodID(builderClass, "build", "()Landroid/media/session/PlaybackState;");
    jmethodID setPlaybackState = env->GetMethodID(sessionClass, "setPlaybackState", "(Landroid/media/session/PlaybackState;)V");
    jmethodID setActive = env->GetMethodID(sessionClass, "setActive", "(Z)V");
    jmethodID elapsedRealtime = env->GetStaticMethodID(clockClass, "elapsedRealtime", "()J");
    if (!ctor || !setState || !setActions || !build || !setPlaybackState || !setActive || !elapsedRealtime
        || clearException(env, "playback-state method lookup")) {
        env->DeleteLocalRef(builderClass);
        env->DeleteLocalRef(sessionClass);
        env->DeleteLocalRef(clockClass);
        return;
    }

    jobject builder = env->NewObject(builderClass, ctor);
    const jlong now = env->CallStaticLongMethod(clockClass, elapsedRealtime);
    const jfloat speed = state == MediaSessionState::Playing ? 1.0f : 0.0f;
    if (builder) {
        env->CallObjectMethod(
            builder,
            setState,
            static_cast<jint>(playbackStateValue(state)),
            static_cast<jlong>(positionMs),
            speed,
            now
        );
        // Do not advertise system transport actions until callbacks are actually wired.
        env->CallObjectMethod(builder, setActions, static_cast<jlong>(0));
    }
    jobject playbackState = builder ? env->CallObjectMethod(builder, build) : nullptr;
    if (playbackState) env->CallVoidMethod(session_, setPlaybackState, playbackState);
    env->CallVoidMethod(session_, setActive, state == MediaSessionState::Stopped ? JNI_FALSE : JNI_TRUE);
    const bool failed = clearException(env, "playback-state update");
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

void NativeMediaSession::clear() {
    title_.clear();
    subtitle_.clear();
    durationMs_ = -1;
    state_ = MediaSessionState::Stopped;
    lastPositionMs_ = -1;
    if (!session_) return;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    jclass sessionClass = env->FindClass("android/media/session/MediaSession");
    if (sessionClass) {
        jmethodID setActive = env->GetMethodID(sessionClass, "setActive", "(Z)V");
        jmethodID release = env->GetMethodID(sessionClass, "release", "()V");
        if (setActive) env->CallVoidMethod(session_, setActive, JNI_FALSE);
        if (release) env->CallVoidMethod(session_, release);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(sessionClass);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteGlobalRef(session_);
    session_ = nullptr;
    __android_log_print(ANDROID_LOG_INFO, kTag, "Android media session released");
}
