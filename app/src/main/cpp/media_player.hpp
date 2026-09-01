#pragma once

#include <jni.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class PlayerStatus {
    Idle,
    Preparing,
    Playing,
    Paused,
    Error,
};

struct PlayerTrack {
    int index = -1;
    int type = 0;
    std::string language;
};

struct ExternalSubtitleTrack {
    std::string path;
    std::string language;
};

class NativeMediaPlayer {
public:
    explicit NativeMediaPlayer(JavaVM* vm) : vm_(vm) {}
    ~NativeMediaPlayer();

    void startAsync(
        const std::string& url,
        jobject surface,
        int64_t startPositionMs,
        std::vector<ExternalSubtitleTrack> externalSubtitles = {}
    );
    void stop();
    void togglePause();
    void seekBy(int deltaMs);
    void seekTo(int positionMs);
    bool setPlaybackSpeed(float speed);
    bool selectTrack(int trackIndex);
    bool deselectTrack(int trackIndex);
    bool addExternalSubtitle(const std::string& path, const std::string& language, bool select);

    [[nodiscard]] PlayerStatus status() const;
    [[nodiscard]] std::string error() const;
    [[nodiscard]] int positionMs() const;
    [[nodiscard]] int durationMs() const;
    [[nodiscard]] int videoWidth() const;
    [[nodiscard]] int videoHeight() const;
    [[nodiscard]] float playbackSpeed() const;
    [[nodiscard]] std::vector<PlayerTrack> tracks() const;
    [[nodiscard]] int selectedAudioTrack() const;
    [[nodiscard]] int selectedSubtitleTrack() const;

private:
    void worker(
        std::string url,
        jobject surface,
        int64_t startPositionMs,
        std::vector<ExternalSubtitleTrack> externalSubtitles
    );
    void joinWorkerIfDone();

    JavaVM* vm_ = nullptr;
    mutable std::mutex mutex_;
    jobject player_ = nullptr;
    PlayerStatus status_ = PlayerStatus::Idle;
    std::string error_;
    int videoWidth_ = 0;
    int videoHeight_ = 0;
    float playbackSpeed_ = 1.0f;
    std::vector<PlayerTrack> tracks_;
    int selectedAudioTrack_ = -1;
    int selectedSubtitleTrack_ = -1;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
};
