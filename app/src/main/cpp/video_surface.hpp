#pragma once

#include <GLES3/gl3.h>
#include <jni.h>

#include <array>
#include <string>

class VideoSurface {
public:
    explicit VideoSurface(JavaVM* vm) : vm_(vm) {}
    ~VideoSurface();

    VideoSurface(const VideoSurface&) = delete;
    VideoSurface& operator=(const VideoSurface&) = delete;

    bool create(std::string& error);
    void release();
    bool update(std::string& error);

    [[nodiscard]] bool ready() const { return surfaceTexture_ != nullptr && surface_ != nullptr && texture_ != 0; }
    [[nodiscard]] jobject surface() const { return surface_; }
    [[nodiscard]] GLuint texture() const { return texture_; }
    [[nodiscard]] const std::array<float, 16>& transform() const { return transform_; }

private:
    JavaVM* vm_ = nullptr;
    jobject surfaceTexture_ = nullptr;
    jobject surface_ = nullptr;
    jclass surfaceTextureClass_ = nullptr;
    jmethodID updateTexImageMethod_ = nullptr;
    jmethodID getTransformMatrixMethod_ = nullptr;
    jfloatArray transformArray_ = nullptr;
    GLuint texture_ = 0;
    std::array<float, 16> transform_{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
};
