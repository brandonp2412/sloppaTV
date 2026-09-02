#pragma once

#include "external_player_policy.hpp"

#include <jni.h>

#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct ExternalPlayerApp {
    std::string componentName;
    std::string packageName;
    std::string label;
};

struct ExternalPlayerResult {
    bool success = false;
    bool completionKnown = false;
    bool completed = false;
    int64_t positionMs = -1;
};

class NativeExternalPlayer {
public:
    NativeExternalPlayer(JavaVM* vm, jobject activity);
    ~NativeExternalPlayer();

    NativeExternalPlayer(const NativeExternalPlayer&) = delete;
    NativeExternalPlayer& operator=(const NativeExternalPlayer&) = delete;

    [[nodiscard]] std::vector<ExternalPlayerApp> availablePlayers() const;
    [[nodiscard]] bool launch(
        const ExternalPlayerApp& app,
        const std::string& url,
        const std::string& title,
        int positionMs,
        const std::string& subtitleUrl,
        std::string& error
    );
    [[nodiscard]] std::optional<ExternalPlayerResult> takeResult();
    void handleActivityResult(JNIEnv* env, int requestCode, int resultCode, jobject dataIntent);

private:
    static constexpr int kRequestCode = 0x534c;

    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    std::mutex resultMutex_;
    ExternalPlayerKind activeKind_ = ExternalPlayerKind::Generic;
    std::optional<ExternalPlayerResult> pendingResult_;
};
