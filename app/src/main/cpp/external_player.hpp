#pragma once

#include <jni.h>

#include <string>
#include <vector>

struct ExternalPlayerApp {
    std::string componentName;
    std::string packageName;
    std::string label;
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
    ) const;

private:
    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
};
