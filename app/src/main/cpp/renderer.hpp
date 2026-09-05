#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <jni.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct Color {
    float r;
    float g;
    float b;
    float a;
};

class Renderer {
public:
    Renderer() = default;
    Renderer(JavaVM* vm, jobject activity);
    ~Renderer();

    bool init(ANativeWindow* window);
    bool detachWindow();
    bool attachWindow(ANativeWindow* window);
    void shutdown();
    [[nodiscard]] bool contextReady() const { return display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT && config_ != nullptr; }
    [[nodiscard]] bool ready() const { return contextReady() && surface_ != EGL_NO_SURFACE; }

    void beginFrame();
    void endFrame();
    void setUiTransform(float safeAreaFraction, float textScale);

    void rect(float x, float y, float w, float h, Color color);
    void triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color);
    void roundedRect(float x, float y, float w, float h, float radius, Color color);
    void roundedOutline(float x, float y, float w, float h, float radius, float thickness, Color color);
    void verticalGradient(float x, float y, float w, float h, Color top, Color bottom);
    void horizontalGradient(float x, float y, float w, float h, Color left, Color right);
    void outline(float x, float y, float w, float h, float thickness, Color color);
    void text(float x, float y, float scale, const std::string& value, Color color, float maxWidth = 0.0f);
    void outlinedText(
        float x,
        float y,
        float scale,
        const std::string& value,
        Color fill,
        Color outline,
        float maxWidth = 0.0f
    );
    void textCentered(float x, float y, float w, float h, float scale, const std::string& value, Color color);
    void textVerticallyCentered(float x, float y, float h, float scale, const std::string& value, Color color, float maxWidth = 0.0f);
    float textWidth(float scale, const std::string& value) const;
    GLuint createTexture(int width, int height, const uint8_t* rgbaPixels);
    void deleteTexture(GLuint texture);
    void image(GLuint texture, float x, float y, float w, float h, float alpha = 1.0f);
    void imageRegion(
        GLuint texture,
        float x,
        float y,
        float w,
        float h,
        float u0,
        float v0,
        float u1,
        float v1,
        float alpha = 1.0f
    );
    void roundedImageRegion(
        GLuint texture,
        float x,
        float y,
        float w,
        float h,
        float radius,
        float u0,
        float v0,
        float u1,
        float v1,
        float alpha = 1.0f
    );
    bool externalImage(
        GLuint texture,
        float x,
        float y,
        float w,
        float h,
        const std::array<float, 16>& transform,
        float alpha = 1.0f
    );
    [[nodiscard]] uint64_t generation() const { return generation_; }

    static constexpr float logicalWidth() { return 1920.0f; }
    static constexpr float logicalHeight() { return 1080.0f; }

private:
    struct Vertex {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;
    };

    struct TextureVertex {
        float x;
        float y;
        float u;
        float v;
        float localX;
        float localY;
    };

    void flush();
    bool ensureExternalProgram();
    bool loadFontAtlas();
    void imageRegionTint(
        GLuint texture,
        float x,
        float y,
        float w,
        float h,
        float u0,
        float v0,
        float u1,
        float v1,
        Color tint,
        float alpha,
        float radius = 0.0f
    );
    GLuint compileShader(GLenum type, const char* source);
    void textWithAtlas(
        GLuint atlasTexture,
        float x,
        float y,
        float scale,
        const std::string& value,
        Color color,
        float maxWidth
    );
    std::array<uint8_t, 7> glyph(char c) const;

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig config_ = nullptr;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    int surfaceWidth_ = 0;
    int surfaceHeight_ = 0;

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLint resolutionLocation_ = -1;
    GLuint textureProgram_ = 0;
    GLuint textureVao_ = 0;
    GLuint textureVbo_ = 0;
    GLint textureResolutionLocation_ = -1;
    GLint textureAlphaLocation_ = -1;
    GLint textureTintLocation_ = -1;
    GLint textureRectSizeLocation_ = -1;
    GLint textureRadiusLocation_ = -1;
    GLuint externalProgram_ = 0;
    GLuint externalVao_ = 0;
    GLuint externalVbo_ = 0;
    GLint externalResolutionLocation_ = -1;
    GLint externalAlphaLocation_ = -1;
    GLint externalTransformLocation_ = -1;
    bool externalProgramFailed_ = false;
    GLuint fontTexture_ = 0;
    GLuint fontOutlineTexture_ = 0;
    bool fontAtlasAttempted_ = false;
    std::array<float, 95> fontAdvances_{};
    bool fontAdvancesReady_ = false;
    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    uint64_t generation_ = 0;
    float uiOffsetX_ = 0.0f;
    float uiOffsetY_ = 0.0f;
    float uiScale_ = 1.0f;
    float textScale_ = 1.0f;
    std::vector<Vertex> vertices_;
};
