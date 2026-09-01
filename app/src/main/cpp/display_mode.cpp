#include "display_mode.hpp"

#include <android/api-level.h>
#include <android/log.h>
#include <dlfcn.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kTag = "sloppaTV/display";

using SetFrameRateFn = int32_t (*)(ANativeWindow*, float, int8_t);
using SetFrameRateWithStrategyFn = int32_t (*)(ANativeWindow*, float, int8_t, int8_t);

struct FrameRateApi {
    SetFrameRateFn setFrameRate = nullptr;
    SetFrameRateWithStrategyFn setFrameRateWithStrategy = nullptr;
};

FrameRateApi frameRateApi() {
    static const FrameRateApi api = [] {
        FrameRateApi result;
        void* handle = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (!handle) return result;
        result.setFrameRate = reinterpret_cast<SetFrameRateFn>(dlsym(handle, "ANativeWindow_setFrameRate"));
        result.setFrameRateWithStrategy = reinterpret_cast<SetFrameRateWithStrategyFn>(
            dlsym(handle, "ANativeWindow_setFrameRateWithChangeStrategy")
        );
        return result;
    }();
    return api;
}

int32_t applyFrameRate(ANativeWindow* window, float frameRate, bool allowNonSeamless) {
    if (!window) return -1;
    const auto api = frameRateApi();
    const int sdk = android_get_device_api_level();
    if (sdk >= 31 && api.setFrameRateWithStrategy) {
        return api.setFrameRateWithStrategy(
            window,
            frameRate,
            frameRate > 0.0f
                ? ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE
                : ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT,
            allowNonSeamless ? static_cast<int8_t>(1) : static_cast<int8_t>(0)
        );
    }
    if (sdk >= 30 && api.setFrameRate) {
        return api.setFrameRate(
            window,
            frameRate,
            frameRate > 0.0f
                ? ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE
                : ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT
        );
    }
    return -1;
}
}  // namespace

DisplayModeController::~DisplayModeController() {
    restore();
}

bool DisplayModeController::matchVideo(ANativeWindow* window, float frameRate) {
    if (!window || !std::isfinite(frameRate) || frameRate <= 1.0f) return false;
    std::scoped_lock lock(mutex_);

    if (window_ != window) {
        if (window_) {
            applyFrameRate(window_, 0.0f, false);
            ANativeWindow_release(window_);
        }
        window_ = window;
        ANativeWindow_acquire(window_);
    }

    const int32_t result = applyFrameRate(window_, frameRate, true);
    if (result == 0) {
        __android_log_print(ANDROID_LOG_INFO, kTag, "Requested fixed-source %.3f fps on app window", frameRate);
        return true;
    }

    __android_log_print(ANDROID_LOG_WARN, kTag, "Unable to request %.3f fps (result=%d)", frameRate, result);
    return false;
}

void DisplayModeController::restore() {
    std::scoped_lock lock(mutex_);
    if (!window_) return;
    const int32_t result = applyFrameRate(window_, 0.0f, false);
    __android_log_print(
        result == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kTag,
        "Cleared app-window frame-rate preference (result=%d)",
        result
    );
    ANativeWindow_release(window_);
    window_ = nullptr;
}
