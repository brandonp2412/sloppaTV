#include "launch_intent.hpp"

#include "jni_env.hpp"

#include <android_native_app_glue.h>
#include <jni.h>

#include <string>

namespace {
bool clearPendingException(JNIEnv* env) {
    if (!env || !env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass objectClass(JNIEnv* env, jobject object) {
    if (!env || !object) return nullptr;
    jclass value = env->GetObjectClass(object);
    if (!clearPendingException(env)) return value;
    if (value) env->DeleteLocalRef(value);
    return nullptr;
}

jmethodID method(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jmethodID value = env->GetMethodID(clazz, name, signature);
    return clearPendingException(env) ? nullptr : value;
}
} // namespace

LaunchRequest readLaunchRequest(android_app* app) {
    if (!app || !app->activity || !app->activity->vm || !app->activity->clazz) return {};
    ScopedJniEnv scoped(app->activity->vm);
    JNIEnv* env = scoped.get();
    if (!env) return {};

    jobject activity = app->activity->clazz;
    jclass activityClass = objectClass(env, activity);
    if (!activityClass) return {};
    jmethodID getIntent = method(env, activityClass, "getIntent", "()Landroid/content/Intent;");
    if (!getIntent) {
        env->DeleteLocalRef(activityClass);
        return {};
    }

    jobject intent = env->CallObjectMethod(activity, getIntent);
    if (clearPendingException(env) || !intent) {
        if (intent) env->DeleteLocalRef(intent);
        env->DeleteLocalRef(activityClass);
        return {};
    }

    jclass intentClass = objectClass(env, intent);
    jmethodID getAction = method(env, intentClass, "getAction", "()Ljava/lang/String;");
    jmethodID getDataString = method(env, intentClass, "getDataString", "()Ljava/lang/String;");
    jmethodID getStringExtra = method(env, intentClass, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!intentClass || !getAction || !getDataString || !getStringExtra) {
        if (intentClass) env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
        env->DeleteLocalRef(activityClass);
        return {};
    }

    auto actionValue = static_cast<jstring>(env->CallObjectMethod(intent, getAction));
    if (clearPendingException(env)) {
        if (actionValue) env->DeleteLocalRef(actionValue);
        actionValue = nullptr;
    }

    auto dataValue = static_cast<jstring>(env->CallObjectMethod(intent, getDataString));
    if (clearPendingException(env)) {
        if (dataValue) env->DeleteLocalRef(dataValue);
        dataValue = nullptr;
    }

    const std::string action = jniString(env, actionValue);
    const std::string data = jniString(env, dataValue);
    std::string query;
    if (action == "android.intent.action.SEARCH") {
        jstring queryKey = jniNewString(env, "query");
        if (clearPendingException(env)) {
            if (queryKey) env->DeleteLocalRef(queryKey);
            queryKey = nullptr;
        }
        auto queryValue =
            queryKey ? static_cast<jstring>(env->CallObjectMethod(intent, getStringExtra, queryKey)) : nullptr;
        if (clearPendingException(env)) {
            if (queryValue) env->DeleteLocalRef(queryValue);
            queryValue = nullptr;
        }
        query = jniString(env, queryValue);
        if (queryValue) env->DeleteLocalRef(queryValue);
        if (queryKey) env->DeleteLocalRef(queryKey);
    }
    if (actionValue) env->DeleteLocalRef(actionValue);
    if (dataValue) env->DeleteLocalRef(dataValue);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(activityClass);
    return launchRequestFromIntentParts(action, data, query);
}
