#pragma once

#include <jni.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
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
    std::string codec;
    std::string language;
    // >= 0 selects an embedded container subtitle by its ordinal among embedded
    // subtitle streams. path remains empty for this mode.
    int embeddedOrdinal = -1;
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
        int embeddedAudioOrdinal = -1,
        std::vector<ExternalSubtitleTrack> externalSubtitles = {}
    );
    void stop();
    void togglePause();
    void pause();
    void play();
    void seekBy(int deltaMs);
    void seekTo(int positionMs);
    bool selectEmbeddedAudioOrdinal(int ordinal);
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
    void invokeTransportCommand(const char* methodName, const char* operation);

    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    mutable std::mutex mutex_;
    jobject player_ = nullptr;
    std::string error_;
    mutable PlayerStatus cachedStatus_ = PlayerStatus::Idle;
    mutable int cachedVideoWidth_ = 0;
    mutable int cachedVideoHeight_ = 0;
    mutable float cachedPlaybackSpeed_ = 1.0f;
    mutable std::chrono::steady_clock::time_point lastSnapshotPoll_{};
};
