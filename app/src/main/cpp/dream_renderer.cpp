#include "renderer.hpp"
#include "ui_theme.hpp"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <mutex>
#include <thread>

namespace {
constexpr const char* kTag = "sloppaTV/dream";
constexpr Color kText = material_tv::onSurface;
constexpr Color kMuted = material_tv::onSurfaceVariant;

std::mutex gMutex;
std::condition_variable gCv;
std::thread gThread;
bool gStopRequested = false;

void renderDreamFrame(Renderer& renderer, int positionIndex, bool clock24Hour) {
    static constexpr std::array<std::array<float, 2>, 6> positions{{
        {150.0f, 165.0f},
        {1110.0f, 165.0f},
        {150.0f, 650.0f},
        {1110.0f, 650.0f},
        {630.0f, 280.0f},
        {630.0f, 585.0f},
    }};
    const auto& position = positions[static_cast<size_t>(positionIndex % static_cast<int>(positions.size()))];

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char clock[24]{};
    std::strftime(clock, sizeof(clock), clock24Hour ? "%H:%M" : "%I:%M %p", &local);
    std::string clockText(clock);
    if (!clock24Hour && !clockText.empty() && clockText.front() == '0') clockText.erase(clockText.begin());

    renderer.beginFrame();
    renderer.setUiTransform(0.0f, 1.0f);
    renderer.roundedRect(position[0] - 42.0f, position[1] - 42.0f, 610.0f, 250.0f,
        material_tv::cornerLarge, material_tv::surfaceContainer);
    renderer.text(position[0], position[1], material_tv::type::title, "sloppaTV", material_tv::primary, 520.0f);
    renderer.text(position[0], position[1] + 78.0f, 5.8f, clockText, kText, 520.0f);
    renderer.text(position[0], position[1] + 170.0f, material_tv::type::supporting, "Your Jellyfin library", kMuted, 520.0f);
    renderer.endFrame();
}

void dreamLoop(ANativeWindow* window, bool clock24Hour) {
    Renderer renderer;
    if (!renderer.init(window)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Unable to initialize dream renderer");
        ANativeWindow_release(window);
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag, "System dream renderer started");
    int positionIndex = 0;
    while (true) {
        renderDreamFrame(renderer, positionIndex++, clock24Hour);
        std::unique_lock lock(gMutex);
        if (gCv.wait_for(lock, std::chrono::seconds(30), [] { return gStopRequested; })) break;
    }
    renderer.shutdown();
    ANativeWindow_release(window);
    __android_log_print(ANDROID_LOG_INFO, kTag, "System dream renderer stopped");
}

void stopDreamThread() {
    std::thread thread;
    {
        std::scoped_lock lock(gMutex);
        if (!gThread.joinable()) {
            gStopRequested = false;
            return;
        }
        gStopRequested = true;
        thread = std::move(gThread);
    }
    gCv.notify_all();
    thread.join();
    {
        std::scoped_lock lock(gMutex);
        gStopRequested = false;
    }
}
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaDreamService_nativeStartDream(JNIEnv* env, jclass, jobject surface, jboolean clock24Hour) {
    if (!env || !surface) return;
    stopDreamThread();
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Dream Surface did not provide an ANativeWindow");
        return;
    }
    {
        std::scoped_lock lock(gMutex);
        gStopRequested = false;
        gThread = std::thread(dreamLoop, window, clock24Hour == JNI_TRUE);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaDreamService_nativeStopDream(JNIEnv*, jclass) {
    stopDreamThread();
}
