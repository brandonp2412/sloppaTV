#pragma once

#include <jni.h>

class ScopedJniEnv {
public:
    explicit ScopedJniEnv(JavaVM* vm) : vm_(vm) {
        if (!vm_) return;
        const jint result = vm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED && vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) attached_ = true;
    }

    ~ScopedJniEnv() {
        if (attached_ && vm_) vm_->DetachCurrentThread();
    }

    ScopedJniEnv(const ScopedJniEnv&) = delete;
    ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

    [[nodiscard]] JNIEnv* get() const { return env_; }

private:
    JavaVM* vm_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};
