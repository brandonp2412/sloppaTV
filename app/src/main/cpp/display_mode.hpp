#pragma once

#include <android/native_window.h>

#include <mutex>

class DisplayModeController {
public:
    DisplayModeController() = default;
    ~DisplayModeController();

    DisplayModeController(const DisplayModeController&) = delete;
    DisplayModeController& operator=(const DisplayModeController&) = delete;

    bool matchVideo(ANativeWindow* window, float frameRate);
    void restore();

private:
    std::mutex mutex_;
    ANativeWindow* window_ = nullptr;
};
