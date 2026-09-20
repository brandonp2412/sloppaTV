#pragma once

#include "unicode_text.hpp"

#include <jni.h>

#include <limits>
#include <string>
#include <string_view>
#include <vector>

inline std::string jniString(JNIEnv* env, jstring value) {
    if (!env || !value) return {};
    const jsize length = env->GetStringLength(value);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return {};
    }
    const jchar* chars = env->GetStringChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    std::u16string utf16;
    utf16.reserve(static_cast<size_t>(length));
    for (jsize index = 0; index < length; ++index) utf16.push_back(static_cast<char16_t>(chars[index]));
    env->ReleaseStringChars(value, chars);
    return utf16ToUtf8(utf16);
}

inline jstring jniNewString(JNIEnv* env, std::string_view value) {
    if (!env) return nullptr;
    const std::u16string utf16 = utf8ToUtf16(value);
    if (utf16.size() > static_cast<size_t>(std::numeric_limits<jsize>::max())) return nullptr;
    std::vector<jchar> chars;
    chars.reserve(utf16.size());
    for (char16_t character : utf16) chars.push_back(static_cast<jchar>(character));
    static constexpr jchar empty = 0;
    return env->NewString(chars.empty() ? &empty : chars.data(), static_cast<jsize>(chars.size()));
}

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
