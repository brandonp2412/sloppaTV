#include "renderer.hpp"

#include <android/log.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {
constexpr const char* kTag = "sloppaTV/render";

constexpr const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform vec2 uResolution;
out vec4 vColor;
void main() {
    vec2 p = vec2(
        (aPosition.x / uResolution.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uResolution.y) * 2.0
    );
    gl_Position = vec4(p, 0.0, 1.0);
    vColor = aColor;
}
)";

constexpr const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 outColor;
void main() {
    outColor = vColor;
}
)";

constexpr const char* kTextureVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
uniform vec2 uResolution;
out vec2 vTexCoord;
void main() {
    vec2 p = vec2(
        (aPosition.x / uResolution.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uResolution.y) * 2.0
    );
    gl_Position = vec4(p, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

constexpr const char* kTextureFragmentShader = R"(#version 300 es
precision mediump float;
in vec2 vTexCoord;
uniform sampler2D uTexture;
uniform float uAlpha;
out vec4 outColor;
void main() {
    vec4 sampled = texture(uTexture, vTexCoord);
    outColor = vec4(sampled.rgb, sampled.a * uAlpha);
}
)";

constexpr const char* kExternalVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
uniform vec2 uResolution;
uniform mat4 uTransform;
out vec2 vTexCoord;
void main() {
    vec2 p = vec2(
        (aPosition.x / uResolution.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uResolution.y) * 2.0
    );
    gl_Position = vec4(p, 0.0, 1.0);
    vec2 sourceCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
    vec4 transformed = uTransform * vec4(sourceCoord, 0.0, 1.0);
    vTexCoord = transformed.xy;
}
)";

constexpr const char* kExternalFragmentShader = R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
in vec2 vTexCoord;
uniform samplerExternalOES uTexture;
uniform float uAlpha;
out vec4 outColor;
void main() {
    vec4 sampled = texture(uTexture, vTexCoord);
    outColor = vec4(sampled.rgb, sampled.a * uAlpha);
}
)";
}  // namespace

Renderer::~Renderer() {
    shutdown();
}

GLuint Renderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char buffer[1024]{};
        glGetShaderInfoLog(shader, sizeof(buffer), nullptr, buffer);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Shader compile failed: %s", buffer);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Renderer::init(ANativeWindow* window) {
    shutdown();
    if (!window) return false;

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, nullptr, nullptr)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "eglInitialize failed");
        shutdown();
        return false;
    }

    constexpr EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (!eglChooseConfig(display_, configAttribs, &config, 1, &configCount) || configCount < 1) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "eglChooseConfig failed");
        shutdown();
        return false;
    }

    EGLint format = 0;
    eglGetConfigAttrib(display_, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    constexpr EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (surface_ == EGL_NO_SURFACE || context_ == EGL_NO_CONTEXT || !eglMakeCurrent(display_, surface_, surface_, context_)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Unable to create EGL surface/context");
        shutdown();
        return false;
    }
    eglSwapInterval(display_, 1);

    eglQuerySurface(display_, surface_, EGL_WIDTH, &surfaceWidth_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &surfaceHeight_);
    glViewport(0, 0, surfaceWidth_, surfaceHeight_);

    const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        shutdown();
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glLinkProgram(program_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char buffer[1024]{};
        glGetProgramInfoLog(program_, sizeof(buffer), nullptr, buffer);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Program link failed: %s", buffer);
        shutdown();
        return false;
    }

    resolutionLocation_ = glGetUniformLocation(program_, "uResolution");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    const GLuint textureVertex = compileShader(GL_VERTEX_SHADER, kTextureVertexShader);
    const GLuint textureFragment = compileShader(GL_FRAGMENT_SHADER, kTextureFragmentShader);
    if (!textureVertex || !textureFragment) {
        if (textureVertex) glDeleteShader(textureVertex);
        if (textureFragment) glDeleteShader(textureFragment);
        shutdown();
        return false;
    }
    textureProgram_ = glCreateProgram();
    glAttachShader(textureProgram_, textureVertex);
    glAttachShader(textureProgram_, textureFragment);
    glLinkProgram(textureProgram_);
    glDeleteShader(textureVertex);
    glDeleteShader(textureFragment);
    GLint textureLinked = GL_FALSE;
    glGetProgramiv(textureProgram_, GL_LINK_STATUS, &textureLinked);
    if (textureLinked != GL_TRUE) {
        char buffer[1024]{};
        glGetProgramInfoLog(textureProgram_, sizeof(buffer), nullptr, buffer);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Texture program link failed: %s", buffer);
        shutdown();
        return false;
    }

    textureResolutionLocation_ = glGetUniformLocation(textureProgram_, "uResolution");
    textureAlphaLocation_ = glGetUniformLocation(textureProgram_, "uAlpha");
    glGenVertexArrays(1, &textureVao_);
    glGenBuffers(1, &textureVbo_);
    glBindVertexArray(textureVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textureVbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), reinterpret_cast<void*>(offsetof(TextureVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), reinterpret_cast<void*>(offsetof(TextureVertex, u)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vertices_.reserve(32768);
    ++generation_;
    __android_log_print(ANDROID_LOG_INFO, kTag, "Renderer initialized at %dx%d (generation %llu)", surfaceWidth_, surfaceHeight_, static_cast<unsigned long long>(generation_));
    return true;
}

void Renderer::shutdown() {
    if (display_ != EGL_NO_DISPLAY) {
        if (context_ != EGL_NO_CONTEXT && eglGetCurrentContext() == context_) {
            if (vbo_) glDeleteBuffers(1, &vbo_);
            if (vao_) glDeleteVertexArrays(1, &vao_);
            if (program_) glDeleteProgram(program_);
            if (textureVbo_) glDeleteBuffers(1, &textureVbo_);
            if (textureVao_) glDeleteVertexArrays(1, &textureVao_);
            if (textureProgram_) glDeleteProgram(textureProgram_);
            if (externalVbo_) glDeleteBuffers(1, &externalVbo_);
            if (externalVao_) glDeleteVertexArrays(1, &externalVao_);
            if (externalProgram_) glDeleteProgram(externalProgram_);
        }
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
    }
    display_ = EGL_NO_DISPLAY;
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    program_ = 0;
    vao_ = 0;
    vbo_ = 0;
    resolutionLocation_ = -1;
    textureProgram_ = 0;
    textureVao_ = 0;
    textureVbo_ = 0;
    textureResolutionLocation_ = -1;
    textureAlphaLocation_ = -1;
    externalProgram_ = 0;
    externalVao_ = 0;
    externalVbo_ = 0;
    externalResolutionLocation_ = -1;
    externalAlphaLocation_ = -1;
    externalTransformLocation_ = -1;
    externalProgramFailed_ = false;
    surfaceWidth_ = 0;
    surfaceHeight_ = 0;
    vertices_.clear();
}

void Renderer::beginFrame() {
    if (!ready()) return;
    glViewport(0, 0, surfaceWidth_, surfaceHeight_);
    glClearColor(0.043f, 0.047f, 0.059f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    vertices_.clear();
}

void Renderer::endFrame() {
    if (!ready()) return;
    flush();
    eglSwapBuffers(display_, surface_);
}

void Renderer::flush() {
    if (vertices_.empty()) return;
    glUseProgram(program_);
    glUniform2f(resolutionLocation_, logicalWidth(), logicalHeight());
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
    vertices_.clear();
}

void Renderer::rect(float x, float y, float w, float h, Color c) {
    const Vertex v0{x, y, c.r, c.g, c.b, c.a};
    const Vertex v1{x + w, y, c.r, c.g, c.b, c.a};
    const Vertex v2{x + w, y + h, c.r, c.g, c.b, c.a};
    const Vertex v3{x, y + h, c.r, c.g, c.b, c.a};
    vertices_.insert(vertices_.end(), {v0, v1, v2, v0, v2, v3});
}

void Renderer::outline(float x, float y, float w, float h, float thickness, Color c) {
    rect(x, y, w, thickness, c);
    rect(x, y + h - thickness, w, thickness, c);
    rect(x, y, thickness, h, c);
    rect(x + w - thickness, y, thickness, h, c);
}

GLuint Renderer::createTexture(int width, int height, const uint8_t* rgbaPixels) {
    if (!ready() || width <= 0 || height <= 0 || !rgbaPixels) return 0;
    flush();
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void Renderer::deleteTexture(GLuint texture) {
    if (!ready() || texture == 0) return;
    flush();
    glDeleteTextures(1, &texture);
}

void Renderer::image(GLuint texture, float x, float y, float w, float h, float alpha) {
    if (!ready() || texture == 0 || w <= 0.0f || h <= 0.0f) return;
    flush();
    const TextureVertex vertices[] = {
        {x, y, 0.0f, 0.0f},
        {x + w, y, 1.0f, 0.0f},
        {x + w, y + h, 1.0f, 1.0f},
        {x, y, 0.0f, 0.0f},
        {x + w, y + h, 1.0f, 1.0f},
        {x, y + h, 0.0f, 1.0f},
    };
    glUseProgram(textureProgram_);
    glUniform2f(textureResolutionLocation_, logicalWidth(), logicalHeight());
    glUniform1f(textureAlphaLocation_, std::clamp(alpha, 0.0f, 1.0f));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(textureVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textureVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool Renderer::ensureExternalProgram() {
    if (externalProgram_ != 0) return true;
    if (externalProgramFailed_ || !ready()) return false;

    const GLuint vertex = compileShader(GL_VERTEX_SHADER, kExternalVertexShader);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kExternalFragmentShader);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        externalProgramFailed_ = true;
        __android_log_print(ANDROID_LOG_ERROR, kTag, "External video shader unavailable");
        return false;
    }

    externalProgram_ = glCreateProgram();
    glAttachShader(externalProgram_, vertex);
    glAttachShader(externalProgram_, fragment);
    glLinkProgram(externalProgram_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(externalProgram_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char buffer[1024]{};
        glGetProgramInfoLog(externalProgram_, sizeof(buffer), nullptr, buffer);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "External video program link failed: %s", buffer);
        glDeleteProgram(externalProgram_);
        externalProgram_ = 0;
        externalProgramFailed_ = true;
        return false;
    }

    externalResolutionLocation_ = glGetUniformLocation(externalProgram_, "uResolution");
    externalAlphaLocation_ = glGetUniformLocation(externalProgram_, "uAlpha");
    externalTransformLocation_ = glGetUniformLocation(externalProgram_, "uTransform");
    glGenVertexArrays(1, &externalVao_);
    glGenBuffers(1, &externalVbo_);
    glBindVertexArray(externalVao_);
    glBindBuffer(GL_ARRAY_BUFFER, externalVbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), reinterpret_cast<void*>(offsetof(TextureVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), reinterpret_cast<void*>(offsetof(TextureVertex, u)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return true;
}

bool Renderer::externalImage(
    GLuint texture,
    float x,
    float y,
    float w,
    float h,
    const std::array<float, 16>& transform,
    float alpha
) {
    if (!ready() || texture == 0 || w <= 0.0f || h <= 0.0f || !ensureExternalProgram()) return false;
    flush();
    const TextureVertex vertices[] = {
        {x, y, 0.0f, 0.0f},
        {x + w, y, 1.0f, 0.0f},
        {x + w, y + h, 1.0f, 1.0f},
        {x, y, 0.0f, 0.0f},
        {x + w, y + h, 1.0f, 1.0f},
        {x, y + h, 0.0f, 1.0f},
    };
    glUseProgram(externalProgram_);
    glUniform2f(externalResolutionLocation_, logicalWidth(), logicalHeight());
    glUniform1f(externalAlphaLocation_, std::clamp(alpha, 0.0f, 1.0f));
    glUniformMatrix4fv(externalTransformLocation_, 1, GL_FALSE, transform.data());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
    glBindVertexArray(externalVao_);
    glBindBuffer(GL_ARRAY_BUFFER, externalVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    return true;
}

float Renderer::textWidth(float scale, const std::string& value) const {
    size_t longest = 0;
    size_t current = 0;
    for (const char c : value) {
        if (c == '\n') {
            longest = std::max(longest, current);
            current = 0;
        } else {
            ++current;
        }
    }
    longest = std::max(longest, current);
    return static_cast<float>(longest) * 6.0f * scale;
}

void Renderer::text(float x, float y, float scale, const std::string& value, Color color, float maxWidth) {
    const float originX = x;
    const float charWidth = 6.0f * scale;
    const float lineHeight = 9.0f * scale;

    for (char raw : value) {
        if (raw == '\r') continue;
        if (raw == '\n') {
            x = originX;
            y += lineHeight;
            continue;
        }
        if (maxWidth > 0.0f && x + charWidth > originX + maxWidth) {
            x = originX;
            y += lineHeight;
        }

        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const auto rows = glyph(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[static_cast<size_t>(row)] & (1u << (4 - col))) != 0) {
                    rect(x + static_cast<float>(col) * scale, y + static_cast<float>(row) * scale, scale, scale, color);
                }
            }
        }
        x += charWidth;
    }
}

std::array<uint8_t, 7> Renderer::glyph(char c) const {
    switch (c) {
        case 'A': return {14,17,17,31,17,17,17};
        case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14};
        case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31};
        case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,15};
        case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {31,4,4,4,4,4,31};
        case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17};
        case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17};
        case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14};
        case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13};
        case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};
        case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14};
        case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10};
        case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14};
        case '.': return {0,0,0,0,0,12,12};
        case ',': return {0,0,0,0,0,12,8};
        case ':': return {0,12,12,0,12,12,0};
        case ';': return {0,12,12,0,12,8,0};
        case '!': return {4,4,4,4,4,0,4};
        case '?': return {14,17,1,2,4,0,4};
        case '-': return {0,0,0,31,0,0,0};
        case '_': return {0,0,0,0,0,0,31};
        case '/': return {1,2,2,4,8,8,16};
        case '\\': return {16,8,8,4,2,2,1};
        case '@': return {14,17,23,21,23,16,14};
        case '(': return {2,4,8,8,8,4,2};
        case ')': return {8,4,2,2,2,4,8};
        case '[': return {14,8,8,8,8,8,14};
        case ']': return {14,2,2,2,2,2,14};
        case '+': return {0,4,4,31,4,4,0};
        case '=': return {0,0,31,0,31,0,0};
        case '%': return {17,2,4,8,16,17,0};
        case '#': return {10,31,10,10,31,10,0};
        case '*': return {0,21,14,31,14,21,0};
        case '\'': return {4,4,2,0,0,0,0};
        case '"': return {10,10,0,0,0,0,0};
        case '<': return {1,2,4,8,4,2,1};
        case '>': return {16,8,4,2,4,8,16};
        case '|': return {4,4,4,4,4,4,4};
        case ' ': return {0,0,0,0,0,0,0};
        default: return {31,17,1,2,4,0,4};
    }
}
