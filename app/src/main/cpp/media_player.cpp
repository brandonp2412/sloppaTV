#include "media_player.hpp"
#include "media_player_policy.hpp"

#include <android/log.h>

#include <algorithm>

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

bool clearException(JNIEnv* env, const char* label, std::string& error) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_ERROR, kTag, "MediaPlayer exception at %s", label);
    env->ExceptionDescribe();
    env->ExceptionClear();
    error = std::string("MediaPlayer failed at ") + label;
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

void seekDecoderSafely(JNIEnv* env, jobject player, jmethodID seekMethod, int64_t positionMs) {
    if (!env || !player || !seekMethod) return;
    env->CallVoidMethod(player, seekMethod, static_cast<jint>(clampSeekPositionMs(positionMs)));
}

void releasePlayerAsync(JavaVM* vm, jobject player) {
    if (!vm || !player) return;
    std::thread([vm, player] {
        ScopedEnv scoped(vm);
        JNIEnv* env = scoped.get();
        if (!env) return;
        __android_log_print(ANDROID_LOG_INFO, kTag, "Async MediaPlayer teardown started");
        jclass playerClass = env->FindClass("android/media/MediaPlayer");
        if (playerClass) {
            jmethodID releaseMethod = env->GetMethodID(playerClass, "release", "()V");
            // Do not call MediaPlayer.stop() first. A wedged NuPlayer HLS pipeline can
            // block indefinitely inside NuPlayerDriver::stop/reset. release() is the
            // terminal ownership operation and is sufficient for abandoning this player.
            if (releaseMethod) env->CallVoidMethod(player, releaseMethod);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(playerClass);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        env->DeleteGlobalRef(player);
        __android_log_print(ANDROID_LOG_INFO, kTag, "Async MediaPlayer teardown finished");
    }).detach();
}
}  // namespace

NativeMediaPlayer::~NativeMediaPlayer() {
    stop();
}

void NativeMediaPlayer::joinWorkerIfDone() {
    bool shouldJoin = false;
    {
        std::scoped_lock lock(mutex_);
        shouldJoin = worker_.joinable() && status_ != PlayerStatus::Preparing;
    }
    if (shouldJoin && worker_.joinable()) worker_.join();
}

void NativeMediaPlayer::startAsync(
    const std::string& url,
    jobject surface,
    int64_t startPositionMs,
    std::vector<ExternalSubtitleTrack> externalSubtitles
) {
    stop();
    if (!surface || url.empty()) {
        std::scoped_lock lock(mutex_);
        status_ = PlayerStatus::Error;
        error_ = "Missing playback surface or URL";
        return;
    }

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        std::scoped_lock lock(mutex_);
        status_ = PlayerStatus::Error;
        error_ = "Unable to retain playback surface";
        return;
    }
    jobject retainedSurface = env->NewGlobalRef(surface);
    if (!retainedSurface) {
        std::scoped_lock lock(mutex_);
        status_ = PlayerStatus::Error;
        error_ = "Unable to retain playback surface";
        return;
    }

    cancel_.store(false);
    {
        std::scoped_lock lock(mutex_);
        status_ = PlayerStatus::Preparing;
        error_.clear();
    }
    worker_ = std::thread(
        &NativeMediaPlayer::worker,
        this,
        url,
        retainedSurface,
        startPositionMs,
        std::move(externalSubtitles)
    );
}

void NativeMediaPlayer::worker(
    std::string url,
    jobject surface,
    int64_t startPositionMs,
    std::vector<ExternalSubtitleTrack> externalSubtitles
) {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    std::string error;
    jobject localPlayer = nullptr;
    jobject globalPlayer = nullptr;
    int preparedVideoWidth = 0;
    int preparedVideoHeight = 0;
    std::vector<PlayerTrack> parsedTracks;
    int selectedAudio = -1;
    int selectedSubtitle = -1;

    if (!env) {
        error = "Unable to attach player thread to JVM";
        goto fail;
    }

    {
        jclass playerClass = env->FindClass("android/media/MediaPlayer");
        if (!playerClass || clearException(env, "FindClass", error)) goto fail;
        jmethodID ctor = env->GetMethodID(playerClass, "<init>", "()V");
        jmethodID setDataSource = env->GetMethodID(playerClass, "setDataSource", "(Ljava/lang/String;)V");
        jmethodID setSurface = env->GetMethodID(playerClass, "setSurface", "(Landroid/view/Surface;)V");
        jmethodID setAudioStreamType = env->GetMethodID(playerClass, "setAudioStreamType", "(I)V");
        jmethodID prepare = env->GetMethodID(playerClass, "prepare", "()V");
        jmethodID start = env->GetMethodID(playerClass, "start", "()V");
        jmethodID seekTo = env->GetMethodID(playerClass, "seekTo", "(I)V");
        jmethodID getVideoWidth = env->GetMethodID(playerClass, "getVideoWidth", "()I");
        jmethodID getVideoHeight = env->GetMethodID(playerClass, "getVideoHeight", "()I");
        jmethodID getTrackInfo = env->GetMethodID(playerClass, "getTrackInfo", "()[Landroid/media/MediaPlayer$TrackInfo;");
        jmethodID getSelectedTrack = env->GetMethodID(playerClass, "getSelectedTrack", "(I)I");
        jmethodID addTimedTextSource = env->GetMethodID(
            playerClass,
            "addTimedTextSource",
            "(Ljava/lang/String;Ljava/lang/String;)V"
        );
        if (clearException(env, "method lookup", error) || !ctor || !setDataSource || !setSurface || !prepare || !start) {
            env->DeleteLocalRef(playerClass);
            goto fail;
        }

        localPlayer = env->NewObject(playerClass, ctor);
        if (!localPlayer || clearException(env, "constructor", error)) {
            env->DeleteLocalRef(playerClass);
            goto fail;
        }

        jstring jUrl = env->NewStringUTF(url.c_str());
        env->CallVoidMethod(localPlayer, setDataSource, jUrl);
        env->DeleteLocalRef(jUrl);
        if (clearException(env, "setDataSource", error)) {
            env->DeleteLocalRef(playerClass);
            goto fail;
        }

        env->CallVoidMethod(localPlayer, setSurface, surface);
        if (clearException(env, "setSurface", error)) {
            env->DeleteLocalRef(playerClass);
            goto fail;
        }

        if (setAudioStreamType) env->CallVoidMethod(localPlayer, setAudioStreamType, 3 /* STREAM_MUSIC */);
        env->CallVoidMethod(localPlayer, prepare);
        if (clearException(env, "prepare", error)) {
            env->DeleteLocalRef(playerClass);
            goto fail;
        }
        if (cancel_.load()) {
            env->DeleteLocalRef(playerClass);
            goto cancelled;
        }

        if (startPositionMs > 0 && seekTo) {
            seekDecoderSafely(env, localPlayer, seekTo, startPositionMs);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        env->CallVoidMethod(localPlayer, start);
        if (clearException(env, "start", error)) {
            env->DeleteLocalRef(playerClass);
            goto fail;
        }

        // Playback is now running. Track and subtitle discovery is useful metadata,
        // but it must not extend time-to-first-frame.

        if (addTimedTextSource) {
            for (const auto& subtitle : externalSubtitles) {
                if (subtitle.path.empty()) continue;
                jstring path = env->NewStringUTF(subtitle.path.c_str());
                jstring mime = env->NewStringUTF("application/x-subrip");
                env->CallVoidMethod(localPlayer, addTimedTextSource, path, mime);
                env->DeleteLocalRef(path);
                env->DeleteLocalRef(mime);
                if (env->ExceptionCheck()) {
                    __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to attach timed-text subtitle %s", subtitle.path.c_str());
                    env->ExceptionClear();
                }
            }
        }

        if (getVideoWidth) preparedVideoWidth = env->CallIntMethod(localPlayer, getVideoWidth);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (getVideoHeight) preparedVideoHeight = env->CallIntMethod(localPlayer, getVideoHeight);
        if (env->ExceptionCheck()) env->ExceptionClear();

        if (getTrackInfo) {
            auto trackArray = static_cast<jobjectArray>(env->CallObjectMethod(localPlayer, getTrackInfo));
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                trackArray = nullptr;
            }
            if (trackArray) {
                jclass trackInfoClass = env->FindClass("android/media/MediaPlayer$TrackInfo");
                jmethodID getTrackType = trackInfoClass ? env->GetMethodID(trackInfoClass, "getTrackType", "()I") : nullptr;
                jmethodID getLanguage = trackInfoClass ? env->GetMethodID(trackInfoClass, "getLanguage", "()Ljava/lang/String;") : nullptr;
                if (env->ExceptionCheck()) env->ExceptionClear();
                const jsize count = env->GetArrayLength(trackArray);
                parsedTracks.reserve(static_cast<size_t>(count));
                for (jsize index = 0; index < count; ++index) {
                    jobject info = env->GetObjectArrayElement(trackArray, index);
                    if (!info) continue;
                    PlayerTrack track;
                    track.index = static_cast<int>(index);
                    track.type = getTrackType ? env->CallIntMethod(info, getTrackType) : 0;
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (getLanguage) {
                        auto language = static_cast<jstring>(env->CallObjectMethod(info, getLanguage));
                        if (!env->ExceptionCheck() && language) {
                            track.language = fromJString(env, language);
                            env->DeleteLocalRef(language);
                        } else if (env->ExceptionCheck()) {
                            env->ExceptionClear();
                        }
                    }
                    parsedTracks.push_back(std::move(track));
                    env->DeleteLocalRef(info);
                }
                if (trackInfoClass) env->DeleteLocalRef(trackInfoClass);
                env->DeleteLocalRef(trackArray);
            }
        }
        if (!externalSubtitles.empty()) {
            std::vector<size_t> timedTextTracks;
            for (size_t index = 0; index < parsedTracks.size(); ++index) {
                if (parsedTracks[index].type == 3 || parsedTracks[index].type == 4) timedTextTracks.push_back(index);
            }
            if (timedTextTracks.size() >= externalSubtitles.size()) {
                const size_t offset = timedTextTracks.size() - externalSubtitles.size();
                for (size_t index = 0; index < externalSubtitles.size(); ++index) {
                    if (!externalSubtitles[index].language.empty()) {
                        parsedTracks[timedTextTracks[offset + index]].language = externalSubtitles[index].language;
                    }
                }
            }
        }
        if (getSelectedTrack) {
            selectedAudio = env->CallIntMethod(localPlayer, getSelectedTrack, 2 /* MEDIA_TRACK_TYPE_AUDIO */);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                selectedAudio = -1;
            }
            selectedSubtitle = env->CallIntMethod(localPlayer, getSelectedTrack, 4 /* MEDIA_TRACK_TYPE_SUBTITLE */);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                selectedSubtitle = -1;
            }
            if (selectedSubtitle < 0) {
                selectedSubtitle = env->CallIntMethod(localPlayer, getSelectedTrack, 3 /* MEDIA_TRACK_TYPE_TIMEDTEXT */);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    selectedSubtitle = -1;
                }
            }
        }

        globalPlayer = env->NewGlobalRef(localPlayer);
        env->DeleteLocalRef(playerClass);
        if (!globalPlayer) {
            error = "Unable to retain MediaPlayer";
            goto fail;
        }
    }

    {
        std::scoped_lock lock(mutex_);
        player_ = globalPlayer;
        globalPlayer = nullptr;
        status_ = PlayerStatus::Playing;
        videoWidth_ = preparedVideoWidth;
        videoHeight_ = preparedVideoHeight;
        playbackSpeed_ = 1.0f;
        tracks_ = std::move(parsedTracks);
        selectedAudioTrack_ = selectedAudio;
        selectedSubtitleTrack_ = selectedSubtitle;
        error_.clear();
    }
    if (localPlayer) env->DeleteLocalRef(localPlayer);
    if (surface) env->DeleteGlobalRef(surface);
    __android_log_print(ANDROID_LOG_INFO, kTag, "Playback started");
    return;

cancelled:
    error.clear();
fail:
    if (env && localPlayer) {
        jclass playerClass = env->FindClass("android/media/MediaPlayer");
        if (playerClass) {
            jmethodID release = env->GetMethodID(playerClass, "release", "()V");
            if (release) env->CallVoidMethod(localPlayer, release);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(playerClass);
        }
        env->DeleteLocalRef(localPlayer);
    }
    if (env && globalPlayer) env->DeleteGlobalRef(globalPlayer);
    if (env && surface) env->DeleteGlobalRef(surface);
    {
        std::scoped_lock lock(mutex_);
        if (cancel_.load()) {
            status_ = PlayerStatus::Idle;
            error_.clear();
        } else {
            status_ = PlayerStatus::Error;
            error_ = error.empty() ? "Unable to start playback" : error;
        }
    }
}

void NativeMediaPlayer::stop() {
    cancel_.store(true);
    if (worker_.joinable()) worker_.join();

    jobject playerToRelease = nullptr;
    {
        std::scoped_lock lock(mutex_);
        playerToRelease = player_;
        player_ = nullptr;
        status_ = PlayerStatus::Idle;
        videoWidth_ = 0;
        videoHeight_ = 0;
        playbackSpeed_ = 1.0f;
        tracks_.clear();
        selectedAudioTrack_ = -1;
        selectedSubtitleTrack_ = -1;
        error_.clear();
    }
    // Framework MediaPlayer.stop()/release() can block for many seconds when NuPlayer
    // is wedged in HLS/decoder teardown. Never perform that binder cleanup on the native
    // activity/input thread; ownership of the retained global ref moves to this worker.
    releasePlayerAsync(vm_, playerToRelease);
}

void NativeMediaPlayer::togglePause() {
    joinWorkerIfDone();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;

    std::scoped_lock lock(mutex_);
    if (!player_ || (status_ != PlayerStatus::Playing && status_ != PlayerStatus::Paused)) return;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return;
    if (status_ == PlayerStatus::Playing) {
        jmethodID pause = env->GetMethodID(playerClass, "pause", "()V");
        if (pause) env->CallVoidMethod(player_, pause);
        if (!env->ExceptionCheck()) status_ = PlayerStatus::Paused;
    } else {
        jmethodID start = env->GetMethodID(playerClass, "start", "()V");
        if (start) env->CallVoidMethod(player_, start);
        if (!env->ExceptionCheck()) status_ = PlayerStatus::Playing;
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
}

void NativeMediaPlayer::seekBy(int deltaMs) {
    joinWorkerIfDone();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;

    std::scoped_lock lock(mutex_);
    if (!player_) return;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return;
    jmethodID getCurrent = env->GetMethodID(playerClass, "getCurrentPosition", "()I");
    jmethodID seek = env->GetMethodID(playerClass, "seekTo", "(I)V");
    if (getCurrent && seek) {
        const jint current = env->CallIntMethod(player_, getCurrent);
        if (!env->ExceptionCheck()) {
            const int64_t next = std::max<int64_t>(0, static_cast<int64_t>(current) + deltaMs);
            seekDecoderSafely(env, player_, seek, next);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
}

void NativeMediaPlayer::seekTo(int positionMs) {
    joinWorkerIfDone();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;

    std::scoped_lock lock(mutex_);
    if (!player_) return;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return;
    jmethodID seek = env->GetMethodID(playerClass, "seekTo", "(I)V");
    if (seek) {
        seekDecoderSafely(env, player_, seek, positionMs);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
}

bool NativeMediaPlayer::setPlaybackSpeed(float speed) {
    joinWorkerIfDone();
    speed = std::clamp(speed, 0.25f, 2.0f);
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return false;

    std::scoped_lock lock(mutex_);
    if (!player_) return false;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    jclass paramsClass = env->FindClass("android/media/PlaybackParams");
    if (!playerClass || !paramsClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (playerClass) env->DeleteLocalRef(playerClass);
        if (paramsClass) env->DeleteLocalRef(paramsClass);
        return false;
    }
    jmethodID getParams = env->GetMethodID(playerClass, "getPlaybackParams", "()Landroid/media/PlaybackParams;");
    jmethodID setParams = env->GetMethodID(playerClass, "setPlaybackParams", "(Landroid/media/PlaybackParams;)V");
    jmethodID setSpeed = env->GetMethodID(paramsClass, "setSpeed", "(F)Landroid/media/PlaybackParams;");
    if (!getParams || !setParams || !setSpeed || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(paramsClass);
        return false;
    }
    jobject params = env->CallObjectMethod(player_, getParams);
    if (env->ExceptionCheck() || !params) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(paramsClass);
        return false;
    }
    env->CallObjectMethod(params, setSpeed, speed);
    if (!env->ExceptionCheck()) env->CallVoidMethod(player_, setParams, params);
    const bool ok = !env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (ok) playbackSpeed_ = speed;
    env->DeleteLocalRef(params);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(paramsClass);
    return ok;
}

bool NativeMediaPlayer::selectTrack(int trackIndex) {
    joinWorkerIfDone();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return false;
    std::scoped_lock lock(mutex_);
    if (!player_) return false;

    const auto it = std::find_if(tracks_.begin(), tracks_.end(), [trackIndex](const PlayerTrack& track) {
        return track.index == trackIndex;
    });
    if (it == tracks_.end()) return false;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return false;
    jmethodID method = env->GetMethodID(playerClass, "selectTrack", "(I)V");
    if (method) env->CallVoidMethod(player_, method, static_cast<jint>(trackIndex));
    const bool ok = method && !env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (ok) {
        if (it->type == 2) selectedAudioTrack_ = trackIndex;
        else if (it->type == 3 || it->type == 4) selectedSubtitleTrack_ = trackIndex;
    }
    env->DeleteLocalRef(playerClass);
    return ok;
}

bool NativeMediaPlayer::deselectTrack(int trackIndex) {
    joinWorkerIfDone();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return false;
    std::scoped_lock lock(mutex_);
    if (!player_) return false;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return false;
    jmethodID method = env->GetMethodID(playerClass, "deselectTrack", "(I)V");
    if (method) env->CallVoidMethod(player_, method, static_cast<jint>(trackIndex));
    const bool ok = method && !env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (ok) {
        if (selectedAudioTrack_ == trackIndex) selectedAudioTrack_ = -1;
        if (selectedSubtitleTrack_ == trackIndex) selectedSubtitleTrack_ = -1;
    }
    env->DeleteLocalRef(playerClass);
    return ok;
}

bool NativeMediaPlayer::addExternalSubtitle(const std::string& path, const std::string& language, bool select) {
    joinWorkerIfDone();
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || path.empty()) return false;
    std::scoped_lock lock(mutex_);
    if (!player_ || (status_ != PlayerStatus::Playing && status_ != PlayerStatus::Paused)) return false;

    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return false;
    jmethodID addTimedTextSource = env->GetMethodID(
        playerClass,
        "addTimedTextSource",
        "(Ljava/lang/String;Ljava/lang/String;)V"
    );
    jmethodID getTrackInfo = env->GetMethodID(playerClass, "getTrackInfo", "()[Landroid/media/MediaPlayer$TrackInfo;");
    jmethodID selectTrackMethod = env->GetMethodID(playerClass, "selectTrack", "(I)V");
    if (!addTimedTextSource || !getTrackInfo || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(playerClass);
        return false;
    }

    jstring jPath = env->NewStringUTF(path.c_str());
    jstring mime = env->NewStringUTF("application/x-subrip");
    env->CallVoidMethod(player_, addTimedTextSource, jPath, mime);
    env->DeleteLocalRef(jPath);
    env->DeleteLocalRef(mime);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(playerClass);
        return false;
    }

    auto trackArray = static_cast<jobjectArray>(env->CallObjectMethod(player_, getTrackInfo));
    if (env->ExceptionCheck() || !trackArray) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(playerClass);
        return false;
    }
    jclass trackInfoClass = env->FindClass("android/media/MediaPlayer$TrackInfo");
    jmethodID getTrackType = trackInfoClass ? env->GetMethodID(trackInfoClass, "getTrackType", "()I") : nullptr;
    jmethodID getLanguage = trackInfoClass ? env->GetMethodID(trackInfoClass, "getLanguage", "()Ljava/lang/String;") : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();

    std::vector<PlayerTrack> refreshed;
    int newSubtitleIndex = -1;
    const jsize count = env->GetArrayLength(trackArray);
    refreshed.reserve(static_cast<size_t>(count));
    for (jsize index = 0; index < count; ++index) {
        jobject info = env->GetObjectArrayElement(trackArray, index);
        if (!info) continue;
        PlayerTrack track;
        track.index = static_cast<int>(index);
        track.type = getTrackType ? env->CallIntMethod(info, getTrackType) : 0;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (getLanguage) {
            auto jLanguage = static_cast<jstring>(env->CallObjectMethod(info, getLanguage));
            if (!env->ExceptionCheck() && jLanguage) {
                track.language = fromJString(env, jLanguage);
                env->DeleteLocalRef(jLanguage);
            } else if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
        if (track.type == 3 || track.type == 4) newSubtitleIndex = track.index;
        refreshed.push_back(std::move(track));
        env->DeleteLocalRef(info);
    }
    if (newSubtitleIndex >= 0 && !language.empty()) {
        auto it = std::find_if(refreshed.begin(), refreshed.end(), [newSubtitleIndex](const PlayerTrack& track) {
            return track.index == newSubtitleIndex;
        });
        if (it != refreshed.end()) it->language = language;
    }
    tracks_ = std::move(refreshed);

    bool ok = newSubtitleIndex >= 0;
    if (ok && select && selectTrackMethod) {
        env->CallVoidMethod(player_, selectTrackMethod, static_cast<jint>(newSubtitleIndex));
        ok = !env->ExceptionCheck();
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (ok) selectedSubtitleTrack_ = newSubtitleIndex;
    }
    if (trackInfoClass) env->DeleteLocalRef(trackInfoClass);
    env->DeleteLocalRef(trackArray);
    env->DeleteLocalRef(playerClass);
    return ok;
}

PlayerStatus NativeMediaPlayer::status() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

std::string NativeMediaPlayer::error() const {
    std::scoped_lock lock(mutex_);
    return error_;
}

int NativeMediaPlayer::positionMs() const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return 0;
    std::scoped_lock lock(mutex_);
    if (!player_) return 0;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return 0;
    jmethodID method = env->GetMethodID(playerClass, "getCurrentPosition", "()I");
    const jint value = method ? env->CallIntMethod(player_, method) : 0;
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
    return value;
}

int NativeMediaPlayer::durationMs() const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return 0;
    std::scoped_lock lock(mutex_);
    if (!player_) return 0;
    jclass playerClass = env->FindClass("android/media/MediaPlayer");
    if (!playerClass) return 0;
    jmethodID method = env->GetMethodID(playerClass, "getDuration", "()I");
    const jint value = method ? env->CallIntMethod(player_, method) : 0;
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
    return value;
}

int NativeMediaPlayer::videoWidth() const {
    std::scoped_lock lock(mutex_);
    return videoWidth_;
}

int NativeMediaPlayer::videoHeight() const {
    std::scoped_lock lock(mutex_);
    return videoHeight_;
}

float NativeMediaPlayer::playbackSpeed() const {
    std::scoped_lock lock(mutex_);
    return playbackSpeed_;
}

std::vector<PlayerTrack> NativeMediaPlayer::tracks() const {
    std::scoped_lock lock(mutex_);
    return tracks_;
}

int NativeMediaPlayer::selectedAudioTrack() const {
    std::scoped_lock lock(mutex_);
    return selectedAudioTrack_;
}

int NativeMediaPlayer::selectedSubtitleTrack() const {
    std::scoped_lock lock(mutex_);
    return selectedSubtitleTrack_;
}
