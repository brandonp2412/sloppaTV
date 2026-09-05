#include "video_surface.hpp"
#include "jni_env.hpp"

#include <GLES2/gl2ext.h>
#include <android/log.h>

namespace {
constexpr const char* kTag = "sloppaTV/video-surface";

using ScopedEnv = ScopedJniEnv;

bool clearException(JNIEnv* env, const char* where, std::string& error) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_ERROR, kTag, "Java exception at %s", where);
    env->ExceptionDescribe();
    env->ExceptionClear();
    error = std::string("Video surface failed at ") + where;
    return true;
}
}

VideoSurface::~VideoSurface() {
    release();
}

bool VideoSurface::create(std::string& error) {
    release();
    error.clear();

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        error = "Unable to attach video surface thread to JVM";
        return false;
    }

    glGenTextures(1, &texture_);
    if (texture_ == 0) {
        error = "Unable to allocate external video texture";
        return false;
    }
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    jclass localTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    if (!localTextureClass || clearException(env, "FindClass(SurfaceTexture)", error)) goto fail;
    surfaceTextureClass_ = static_cast<jclass>(env->NewGlobalRef(localTextureClass));
    if (!surfaceTextureClass_) {
        env->DeleteLocalRef(localTextureClass);
        error = "Unable to retain SurfaceTexture class";
        goto fail;
    }
    {
        jmethodID ctor = env->GetMethodID(localTextureClass, "<init>", "(I)V");
        updateTexImageMethod_ = env->GetMethodID(localTextureClass, "updateTexImage", "()V");
        getTransformMatrixMethod_ = env->GetMethodID(localTextureClass, "getTransformMatrix", "([F)V");
        if (!ctor || !updateTexImageMethod_ || !getTransformMatrixMethod_ || clearException(env, "SurfaceTexture method lookup", error)) {
            env->DeleteLocalRef(localTextureClass);
            goto fail;
        }
        jobject localTexture = env->NewObject(localTextureClass, ctor, static_cast<jint>(texture_));
        if (!localTexture || clearException(env, "SurfaceTexture constructor", error)) {
            env->DeleteLocalRef(localTextureClass);
            goto fail;
        }
        surfaceTexture_ = env->NewGlobalRef(localTexture);
        env->DeleteLocalRef(localTexture);
    }
    env->DeleteLocalRef(localTextureClass);
    if (!surfaceTexture_) {
        error = "Unable to retain SurfaceTexture";
        goto fail;
    }

    {
        jfloatArray localTransform = env->NewFloatArray(16);
        if (!localTransform || clearException(env, "SurfaceTexture transform buffer", error)) goto fail;
        transformArray_ = static_cast<jfloatArray>(env->NewGlobalRef(localTransform));
        env->DeleteLocalRef(localTransform);
        if (!transformArray_) {
            error = "Unable to retain SurfaceTexture transform buffer";
            goto fail;
        }
    }

    {
        jclass surfaceClass = env->FindClass("android/view/Surface");
        if (!surfaceClass || clearException(env, "FindClass(Surface)", error)) goto fail;
        jmethodID ctor = env->GetMethodID(surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
        if (!ctor || clearException(env, "Surface constructor lookup", error)) {
            env->DeleteLocalRef(surfaceClass);
            goto fail;
        }
        jobject localSurface = env->NewObject(surfaceClass, ctor, surfaceTexture_);
        if (!localSurface || clearException(env, "Surface constructor", error)) {
            env->DeleteLocalRef(surfaceClass);
            goto fail;
        }
        surface_ = env->NewGlobalRef(localSurface);
        env->DeleteLocalRef(localSurface);
        env->DeleteLocalRef(surfaceClass);
    }
    if (!surface_) {
        error = "Unable to retain video Surface";
        goto fail;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag, "Created external video texture %u", texture_);
    return true;

fail:
    release();
    return false;
}

void VideoSurface::release() {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (env) {
        if (surface_) {
            jclass surfaceClass = env->FindClass("android/view/Surface");
            if (surfaceClass) {
                jmethodID method = env->GetMethodID(surfaceClass, "release", "()V");
                if (method) env->CallVoidMethod(surface_, method);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(surfaceClass);
            } else if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            env->DeleteGlobalRef(surface_);
            surface_ = nullptr;
        }
        if (surfaceTexture_) {
            jclass textureClass = surfaceTextureClass_;
            jmethodID method = textureClass ? env->GetMethodID(textureClass, "release", "()V") : nullptr;
            if (method) env->CallVoidMethod(surfaceTexture_, method);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteGlobalRef(surfaceTexture_);
            surfaceTexture_ = nullptr;
        }
        if (transformArray_) {
            env->DeleteGlobalRef(transformArray_);
            transformArray_ = nullptr;
        }
        if (surfaceTextureClass_) {
            env->DeleteGlobalRef(surfaceTextureClass_);
            surfaceTextureClass_ = nullptr;
        }
    }
    updateTexImageMethod_ = nullptr;
    getTransformMatrixMethod_ = nullptr;
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
}

bool VideoSurface::update(std::string& error) {
    if (!ready()) return false;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        error = "Unable to attach video surface thread to JVM";
        return false;
    }

    if (!updateTexImageMethod_ || !getTransformMatrixMethod_ || !transformArray_) {
        error = "Video surface JNI cache is incomplete";
        return false;
    }

    env->CallVoidMethod(surfaceTexture_, updateTexImageMethod_);
    if (clearException(env, "SurfaceTexture.updateTexImage", error)) return false;

    env->CallVoidMethod(surfaceTexture_, getTransformMatrixMethod_, transformArray_);
    if (!clearException(env, "SurfaceTexture.getTransformMatrix", error)) {
        env->GetFloatArrayRegion(transformArray_, 0, 16, transform_.data());
        clearException(env, "SurfaceTexture transform read", error);
    }
    return error.empty();
}
