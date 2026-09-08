#pragma once

#include <jni.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

enum class PlayerStatus {
    Idle,
    Preparing,
    Playing,
    Paused,
    Error,
};

struct MpvSymbols;

class NativeMediaPlayer {
public:
    NativeMediaPlayer(JavaVM* vm, jobject activity, const char* dataPath);
    ~NativeMediaPlayer();

    void startAsync(
        const std::string& url,
        jobject surface,
        int64_t startPositionMs,
        int bufferPreset = 0,
        int embeddedAudioOrdinal = -1,
        int embeddedSubtitleStreamIndex = -1,
        int embeddedSubtitleOrdinal = -1,
        const std::string& externalSubtitleUrl = {}
    );
    void stop();
    void togglePause();
    void pause();
    void play();
    void seekTo(int positionMs);
    bool selectEmbeddedAudioOrdinal(int ordinal);
    bool selectEmbeddedSubtitleStream(int streamIndex, int ordinal = -1);
    bool disableSubtitles();

    [[nodiscard]] PlayerStatus status() const;
    [[nodiscard]] std::string error() const;
    [[nodiscard]] int positionMs() const;
    [[nodiscard]] int durationMs() const;
    [[nodiscard]] bool seekable() const;
    [[nodiscard]] int videoWidth() const;
    [[nodiscard]] int videoHeight() const;
    void subtitleText(std::string& output) const;

private:
    bool loadLibrariesLocked(std::string& error);
    bool initializeLocked(JNIEnv* env, jobject surface, int bufferPreset, std::string& error);
    bool ensureAndroidCaBundleLocked(std::string& error);
    void releaseLocked(JNIEnv* env);
    bool commandLocked(const char* const* args, const char* operation, std::string* error = nullptr) const;
    bool setOptionLocked(const char* name, const std::string& value, bool required, std::string* error = nullptr) const;
    bool setStringPropertyLocked(const char* name, const std::string& value) const;
    bool setFlagPropertyLocked(const char* name, bool value) const;
    bool setIntPropertyLocked(const char* name, int64_t value) const;
    bool getFlagPropertyLocked(const char* name, bool& value) const;
    bool getIntPropertyLocked(const char* name, int64_t& value) const;
    bool getDoublePropertyLocked(const char* name, double& value) const;
    std::string getStringPropertyLocked(const char* name) const;
    bool selectTrackOrdinalLocked(const char* type, const char* selectionProperty, int ordinal) const;
    bool selectTrackStreamIndexLocked(const char* type, const char* selectionProperty, int streamIndex, int fallbackOrdinal) const;
    void applyPendingTracksLocked() const;
    void logPlaybackTelemetryLocked() const;

    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    jobject appContext_ = nullptr;
    std::string filesDir_;
    std::string caBundlePath_;
    mutable std::mutex mutex_;
    void* mpvLibrary_ = nullptr;
    void* avcodecLibrary_ = nullptr;
    void* mpv_ = nullptr;
    MpvSymbols* symbols_ = nullptr;
    jobject surface_ = nullptr;
    mutable std::string error_;
    mutable PlayerStatus cachedStatus_ = PlayerStatus::Idle;
    mutable int cachedVideoWidth_ = 0;
    mutable int cachedVideoHeight_ = 0;
    mutable std::string cachedSubtitleText_;
    mutable std::chrono::steady_clock::time_point lastSnapshotPoll_{};
    mutable int pendingAudioOrdinal_ = -1;
    mutable int pendingSubtitleStreamIndex_ = -1;
    mutable int pendingSubtitleOrdinal_ = -1;
    mutable bool pendingSubtitleOff_ = false;
    mutable bool telemetryLogged_ = false;
};
