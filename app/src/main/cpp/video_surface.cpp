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
} // namespace

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
    const bool textureClassLookupFailed = clearException(env, "FindClass(SurfaceTexture)", error);
    if (textureClassLookupFailed || !localTextureClass) {
        if (error.empty()) error = "Unable to find Android SurfaceTexture";
        goto fail;
    }
    surfaceTextureClass_ = static_cast<jclass>(env->NewGlobalRef(localTextureClass));
    if (clearException(env, "Retain SurfaceTexture class", error) || !surfaceTextureClass_) {
        env->DeleteLocalRef(localTextureClass);
        if (error.empty()) error = "Unable to retain SurfaceTexture class";
        goto fail;
    }
    {
        jmethodID ctor = env->GetMethodID(localTextureClass, "<init>", "(I)V");
        bool methodLookupFailed = clearException(env, "SurfaceTexture constructor lookup", error);
        if (!methodLookupFailed) {
            updateTexImageMethod_ = env->GetMethodID(localTextureClass, "updateTexImage", "()V");
            methodLookupFailed = clearException(env, "SurfaceTexture update method lookup", error);
        }
        if (!methodLookupFailed) {
            getTransformMatrixMethod_ = env->GetMethodID(localTextureClass, "getTransformMatrix", "([F)V");
            methodLookupFailed = clearException(env, "SurfaceTexture transform method lookup", error);
        }
        if (methodLookupFailed || !ctor || !updateTexImageMethod_ || !getTransformMatrixMethod_) {
            env->DeleteLocalRef(localTextureClass);
            if (error.empty()) error = "Unable to find required SurfaceTexture methods";
            goto fail;
        }
        jobject localTexture = env->NewObject(localTextureClass, ctor, static_cast<jint>(texture_));
        const bool textureCreateFailed = clearException(env, "SurfaceTexture constructor", error);
        if (textureCreateFailed || !localTexture) {
            env->DeleteLocalRef(localTextureClass);
            if (localTexture) env->DeleteLocalRef(localTexture);
            if (error.empty()) error = "Unable to create SurfaceTexture";
            goto fail;
        }
        surfaceTexture_ = env->NewGlobalRef(localTexture);
        const bool textureRetainFailed = clearException(env, "Retain SurfaceTexture", error);
        env->DeleteLocalRef(localTexture);
        if (textureRetainFailed || !surfaceTexture_) {
            env->DeleteLocalRef(localTextureClass);
            if (error.empty()) error = "Unable to retain SurfaceTexture";
            goto fail;
        }
    }
    env->DeleteLocalRef(localTextureClass);

    {
        jfloatArray localTransform = env->NewFloatArray(16);
        const bool transformCreateFailed = clearException(env, "SurfaceTexture transform buffer", error);
        if (transformCreateFailed || !localTransform) {
            if (error.empty()) error = "Unable to allocate SurfaceTexture transform buffer";
            goto fail;
        }
        transformArray_ = static_cast<jfloatArray>(env->NewGlobalRef(localTransform));
        const bool transformRetainFailed = clearException(env, "Retain SurfaceTexture transform buffer", error);
        env->DeleteLocalRef(localTransform);
        if (transformRetainFailed || !transformArray_) {
            if (error.empty()) error = "Unable to retain SurfaceTexture transform buffer";
            goto fail;
        }
    }

    {
        jclass surfaceClass = env->FindClass("android/view/Surface");
        const bool surfaceClassLookupFailed = clearException(env, "FindClass(Surface)", error);
        if (surfaceClassLookupFailed || !surfaceClass) {
            if (error.empty()) error = "Unable to find Android Surface";
            goto fail;
        }
        jmethodID ctor = env->GetMethodID(surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
        const bool surfaceMethodLookupFailed = clearException(env, "Surface constructor lookup", error);
        if (surfaceMethodLookupFailed || !ctor) {
            env->DeleteLocalRef(surfaceClass);
            if (error.empty()) error = "Unable to find Android Surface constructor";
            goto fail;
        }
        jobject localSurface = env->NewObject(surfaceClass, ctor, surfaceTexture_);
        const bool surfaceCreateFailed = clearException(env, "Surface constructor", error);
        if (surfaceCreateFailed || !localSurface) {
            if (localSurface) env->DeleteLocalRef(localSurface);
            env->DeleteLocalRef(surfaceClass);
            if (error.empty()) error = "Unable to create video Surface";
            goto fail;
        }
        surface_ = env->NewGlobalRef(localSurface);
        const bool surfaceRetainFailed = clearException(env, "Retain Surface", error);
        env->DeleteLocalRef(localSurface);
        env->DeleteLocalRef(surfaceClass);
        if (surfaceRetainFailed || !surface_) {
            if (error.empty()) error = "Unable to retain video Surface";
            goto fail;
        }
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
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                if (surfaceClass) env->DeleteLocalRef(surfaceClass);
                surfaceClass = nullptr;
            }
            if (surfaceClass) {
                jmethodID method = env->GetMethodID(surfaceClass, "release", "()V");
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    method = nullptr;
                }
                if (method) {
                    env->CallVoidMethod(surface_, method);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                }
                env->DeleteLocalRef(surfaceClass);
            }
            env->DeleteGlobalRef(surface_);
            surface_ = nullptr;
        }
        if (surfaceTexture_) {
            jclass textureClass = surfaceTextureClass_;
            jmethodID method = textureClass ? env->GetMethodID(textureClass, "release", "()V") : nullptr;
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                method = nullptr;
            }
            if (method) {
                env->CallVoidMethod(surfaceTexture_, method);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
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
