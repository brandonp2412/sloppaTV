#include "launch_intent.hpp"

#include "jni_env.hpp"

#include <android_native_app_glue.h>
#include <jni.h>

#include <string>

namespace {
std::string jniString(JNIEnv* env, jstring value) {
    if (!env || !value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}
}

LaunchRequest readLaunchRequest(android_app* app) {
    if (!app || !app->activity || !app->activity->vm || !app->activity->clazz) return {};
    ScopedJniEnv scoped(app->activity->vm);
    JNIEnv* env = scoped.get();
    if (!env) return {};

    jobject activity = app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    if (!activityClass) return {};
    jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    if (!getIntent || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        return {};
    }

    jobject intent = env->CallObjectMethod(activity, getIntent);
    if (!intent || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        return {};
    }

    jclass intentClass = env->GetObjectClass(intent);
    jmethodID getAction = intentClass ? env->GetMethodID(intentClass, "getAction", "()Ljava/lang/String;") : nullptr;
    jmethodID getDataString = intentClass ? env->GetMethodID(intentClass, "getDataString", "()Ljava/lang/String;") : nullptr;
    jmethodID getStringExtra = intentClass ? env->GetMethodID(intentClass, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;") : nullptr;
    if (!getAction || !getDataString || !getStringExtra || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (intentClass) env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
        env->DeleteLocalRef(activityClass);
        return {};
    }

    auto actionValue = static_cast<jstring>(env->CallObjectMethod(intent, getAction));
    auto dataValue = static_cast<jstring>(env->CallObjectMethod(intent, getDataString));
    const std::string action = jniString(env, actionValue);
    const std::string data = jniString(env, dataValue);
    std::string query;
    if (action == "android.intent.action.SEARCH") {
        jstring queryKey = env->NewStringUTF("query");
        auto queryValue = queryKey
            ? static_cast<jstring>(env->CallObjectMethod(intent, getStringExtra, queryKey))
            : nullptr;
        query = jniString(env, queryValue);
        if (queryValue) env->DeleteLocalRef(queryValue);
        if (queryKey) env->DeleteLocalRef(queryKey);
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    if (actionValue) env->DeleteLocalRef(actionValue);
    if (dataValue) env->DeleteLocalRef(dataValue);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(activityClass);
    return launchRequestFromIntentParts(action, data, query);
}
