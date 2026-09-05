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

class NativeMediaPlayer {
public:
    NativeMediaPlayer(JavaVM* vm, jobject activity);
    ~NativeMediaPlayer();

    void startAsync(
        const std::string& url,
        jobject surface,
        int64_t startPositionMs,
        int bufferPreset = 0,
        int embeddedAudioOrdinal = -1
    );
    void stop();
    void togglePause();
    void pause();
    void play();
    void seekBy(int deltaMs);
    void seekTo(int positionMs);
    bool selectEmbeddedAudioOrdinal(int ordinal);

    [[nodiscard]] PlayerStatus status() const;
    [[nodiscard]] std::string error() const;
    [[nodiscard]] int positionMs() const;
    [[nodiscard]] int durationMs() const;
    [[nodiscard]] bool seekable() const;
    [[nodiscard]] int videoWidth() const;
    [[nodiscard]] int videoHeight() const;

private:
    void invokeTransportCommand(const char* methodName, const char* operation);

    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    mutable std::mutex mutex_;
    jobject player_ = nullptr;
    std::string error_;
    mutable PlayerStatus cachedStatus_ = PlayerStatus::Idle;
    mutable int cachedVideoWidth_ = 0;
    mutable int cachedVideoHeight_ = 0;
    mutable std::chrono::steady_clock::time_point lastSnapshotPoll_{};
};
