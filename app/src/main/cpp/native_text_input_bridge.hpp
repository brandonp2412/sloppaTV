#pragma once

#include "jni_env.hpp"
#include "system_text_input.hpp"
#include "system_text_input_controller.hpp"

#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include <string>
#include <utility>

template <typename CompletionQueue> class NativeTextInputBridge {
public:
    NativeTextInputBridge(android_app* app, SystemTextInputController& controller, CompletionQueue& completions)
        : app_(app), controller_(controller), completions_(completions) {}

    [[nodiscard]] bool active() const { return controller_.active(); }

    [[nodiscard]] int mode() const { return controller_.mode(); }

    void queue(SystemTextInputPhase phase, int mode, std::string value) {
        completions_.push(systemTextInputEvent(phase, mode, std::move(value)));
        wake();
    }

    bool show(const std::string& initial, const std::string& hint, int mode, bool password = false) {
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return false;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return false;
        jobject activity = app_->activity->clazz;
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID method = activityClass ? env->GetMethodID(activityClass, "showTextInput",
                                                            "(Ljava/lang/String;Ljava/lang/String;IZ)Z")
                                         : nullptr;
        jstring jInitial = env->NewStringUTF(initial.c_str());
        jstring jHint = env->NewStringUTF(hint.c_str());
        jboolean shown = JNI_FALSE;
        if (method && jInitial && jHint) {
            shown = env->CallBooleanMethod(activity, method, jInitial, jHint, static_cast<jint>(mode),
                                           password ? JNI_TRUE : JNI_FALSE);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            shown = JNI_FALSE;
        }
        if (jHint) env->DeleteLocalRef(jHint);
        if (jInitial) env->DeleteLocalRef(jInitial);
        if (activityClass) env->DeleteLocalRef(activityClass);
        if (shown == JNI_TRUE) controller_.begin(mode, initial);
        return shown == JNI_TRUE;
    }

    void hide() {
        controller_.hide();
        if (!app_ || !app_->activity || !app_->activity->vm || !app_->activity->clazz) return;
        ScopedJniEnv scoped(app_->activity->vm);
        JNIEnv* env = scoped.get();
        if (!env) return;
        jobject activity = app_->activity->clazz;
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID method = activityClass ? env->GetMethodID(activityClass, "hideTextInput", "()V") : nullptr;
        if (method) env->CallVoidMethod(activity, method);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (activityClass) env->DeleteLocalRef(activityClass);
    }

private:
    void wake() const {
        if (app_ && app_->looper) ALooper_wake(app_->looper);
    }

    android_app* app_ = nullptr;
    SystemTextInputController& controller_;
    CompletionQueue& completions_;
};
