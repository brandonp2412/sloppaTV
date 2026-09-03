#include "renderer.hpp"

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
constexpr Color kText{0.95f, 0.96f, 0.99f, 1.0f};
constexpr Color kMuted{0.61f, 0.64f, 0.72f, 1.0f};

std::mutex gMutex;
std::condition_variable gCv;
std::thread gThread;
bool gStopRequested = false;

void renderDreamFrame(Renderer& renderer, int positionIndex) {
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
    char clock[16]{};
    std::strftime(clock, sizeof(clock), "%H:%M", &local);

    renderer.beginFrame();
    renderer.setUiTransform(0.0f, 1.0f);
    renderer.rect(position[0] - 42.0f, position[1] - 42.0f, 610.0f, 250.0f, Color{0.012f, 0.014f, 0.020f, 0.96f});
    renderer.outline(position[0] - 42.0f, position[1] - 42.0f, 610.0f, 250.0f, 3.0f, Color{0.56f, 0.38f, 0.98f, 0.65f});
    renderer.text(position[0], position[1], 3.2f, "SLOPPATV", kText, 520.0f);
    renderer.text(position[0], position[1] + 78.0f, 5.8f, clock, kText, 520.0f);
    renderer.text(position[0], position[1] + 170.0f, 1.35f, "JELLYFIN TV", kMuted, 520.0f);
    renderer.endFrame();
}

void dreamLoop(ANativeWindow* window) {
    Renderer renderer;
    if (!renderer.init(window)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Unable to initialize dream renderer");
        ANativeWindow_release(window);
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag, "System dream renderer started");
    int positionIndex = 0;
    while (true) {
        renderDreamFrame(renderer, positionIndex++);
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
}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaDreamService_nativeStartDream(JNIEnv* env, jclass, jobject surface) {
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
        gThread = std::thread(dreamLoop, window);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaDreamService_nativeStopDream(JNIEnv*, jclass) {
    stopDreamThread();
}
