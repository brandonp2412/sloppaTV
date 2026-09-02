#pragma once

#include <jni.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

enum class MediaSessionState {
    Stopped,
    Buffering,
    Playing,
    Paused,
};

enum class MediaSessionCommandType {
    Play,
    Pause,
    Stop,
    SeekTo,
    Next,
    Previous,
};

struct MediaSessionCommand {
    MediaSessionCommandType type = MediaSessionCommandType::Play;
    int64_t positionMs = 0;
};

class NativeMediaSession {
public:
    NativeMediaSession(JavaVM* vm, jobject activity);
    ~NativeMediaSession();

    NativeMediaSession(const NativeMediaSession&) = delete;
    NativeMediaSession& operator=(const NativeMediaSession&) = delete;

    void updateMetadata(
        const std::string& title,
        const std::string& subtitle,
        int64_t durationMs
    );
    void updateState(MediaSessionState state, int64_t positionMs);
    void clear();
    [[nodiscard]] std::optional<MediaSessionCommand> takeCommand();
    void handlePlatformCommand(int command, int64_t positionMs);

    [[nodiscard]] bool ready() const { return session_ != nullptr; }

private:
    bool ensureSession();

    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    jobject session_ = nullptr;
    std::string title_;
    std::string subtitle_;
    int64_t durationMs_ = -1;
    MediaSessionState state_ = MediaSessionState::Stopped;
    int64_t lastPositionMs_ = -1;
    std::mutex commandMutex_;
    std::optional<MediaSessionCommand> pendingCommand_;
};
