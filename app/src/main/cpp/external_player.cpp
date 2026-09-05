#include "external_player.hpp"

#include "external_player_policy.hpp"
#include "jni_env.hpp"

#include <android/log.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace {
constexpr const char* kTag = "sloppaTV/external";
constexpr const char* kActionView = "android.intent.action.VIEW";
constexpr const char* kVideoMime = "video/*";
constexpr const char* kSampleVideoUrl = "http://jellyfin.local/query.mp4";
std::mutex gInstanceMutex;
NativeExternalPlayer* gInstance = nullptr;

using ScopedEnv = ScopedJniEnv;

bool clearException(JNIEnv* env, const char* operation, std::string* error = nullptr) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_WARN, kTag, "JNI exception during %s", operation);
    env->ExceptionClear();
    if (error) *error = std::string("External player failed during ") + operation;
    return true;
}

jobject createVideoIntent(JNIEnv* env, const std::string& url) {
    jclass intentClass = env->FindClass("android/content/Intent");
    jclass uriClass = env->FindClass("android/net/Uri");
    if (!intentClass || !uriClass || clearException(env, "intent class lookup")) return nullptr;

    jmethodID intentCtor = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;)V");
    jmethodID uriParse = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jmethodID setDataAndType = env->GetMethodID(
        intentClass,
        "setDataAndType",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;"
    );
    if (!intentCtor || !uriParse || !setDataAndType || clearException(env, "intent method lookup")) {
        env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(uriClass);
        return nullptr;
    }

    jstring action = env->NewStringUTF(kActionView);
    jobject intent = action ? env->NewObject(intentClass, intentCtor, action) : nullptr;
    if (action) env->DeleteLocalRef(action);
    jstring jUrl = env->NewStringUTF(url.c_str());
    jobject uri = jUrl ? env->CallStaticObjectMethod(uriClass, uriParse, jUrl) : nullptr;
    if (jUrl) env->DeleteLocalRef(jUrl);
    jstring mime = env->NewStringUTF(kVideoMime);
    if (intent && uri && mime) env->CallObjectMethod(intent, setDataAndType, uri, mime);
    if (mime) env->DeleteLocalRef(mime);
    if (uri) env->DeleteLocalRef(uri);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(uriClass);
    if (clearException(env, "video intent construction")) {
        if (intent) env->DeleteLocalRef(intent);
        return nullptr;
    }
    return intent;
}

void putStringExtra(JNIEnv* env, jobject intent, const char* key, const std::string& value) {
    if (!env || !intent || !key || value.empty()) return;
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID method = intentClass
        ? env->GetMethodID(intentClass, "putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;")
        : nullptr;
    if (method) {
        jstring jKey = env->NewStringUTF(key);
        jstring jValue = env->NewStringUTF(value.c_str());
        if (jKey && jValue) env->CallObjectMethod(intent, method, jKey, jValue);
        if (jKey) env->DeleteLocalRef(jKey);
        if (jValue) env->DeleteLocalRef(jValue);
    }
    if (intentClass) env->DeleteLocalRef(intentClass);
    clearException(env, "string intent extra");
}

void putIntExtra(JNIEnv* env, jobject intent, const char* key, int value) {
    if (!env || !intent || !key) return;
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID method = intentClass
        ? env->GetMethodID(intentClass, "putExtra", "(Ljava/lang/String;I)Landroid/content/Intent;")
        : nullptr;
    if (method) {
        jstring jKey = env->NewStringUTF(key);
        if (jKey) env->CallObjectMethod(intent, method, jKey, static_cast<jint>(value));
        if (jKey) env->DeleteLocalRef(jKey);
    }
    if (intentClass) env->DeleteLocalRef(intentClass);
    clearException(env, "integer intent extra");
}

bool hasExtra(JNIEnv* env, jobject intent, const char* key) {
    if (!env || !intent || !key) return false;
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID method = intentClass
        ? env->GetMethodID(intentClass, "hasExtra", "(Ljava/lang/String;)Z")
        : nullptr;
    jstring jKey = env->NewStringUTF(key);
    const bool result = method && jKey && env->CallBooleanMethod(intent, method, jKey) == JNI_TRUE;
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
    clearException(env, "has intent extra");
    return result;
}

int getIntExtra(JNIEnv* env, jobject intent, const char* key, int fallback = -1) {
    if (!env || !intent || !key) return fallback;
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID method = intentClass
        ? env->GetMethodID(intentClass, "getIntExtra", "(Ljava/lang/String;I)I")
        : nullptr;
    jstring jKey = env->NewStringUTF(key);
    const int result = method && jKey ? env->CallIntMethod(intent, method, jKey, static_cast<jint>(fallback)) : fallback;
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
    clearException(env, "integer result extra");
    return result;
}

int64_t getLongExtra(JNIEnv* env, jobject intent, const char* key, int64_t fallback = -1) {
    if (!env || !intent || !key) return fallback;
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID method = intentClass
        ? env->GetMethodID(intentClass, "getLongExtra", "(Ljava/lang/String;J)J")
        : nullptr;
    jstring jKey = env->NewStringUTF(key);
    const int64_t result = method && jKey ? env->CallLongMethod(intent, method, jKey, static_cast<jlong>(fallback)) : fallback;
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
    clearException(env, "long result extra");
    return result;
}

void putBoolExtra(JNIEnv* env, jobject intent, const char* key, bool value) {
    if (!env || !intent || !key) return;
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID method = intentClass
        ? env->GetMethodID(intentClass, "putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;")
        : nullptr;
    if (method) {
        jstring jKey = env->NewStringUTF(key);
        if (jKey) env->CallObjectMethod(intent, method, jKey, static_cast<jboolean>(value));
        if (jKey) env->DeleteLocalRef(jKey);
    }
    if (intentClass) env->DeleteLocalRef(intentClass);
    clearException(env, "boolean intent extra");
}
}

NativeExternalPlayer::NativeExternalPlayer(JavaVM* vm, jobject activity) : vm_(vm) {
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    activity_ = env->NewGlobalRef(activity);
    std::scoped_lock lock(gInstanceMutex);
    gInstance = this;
}

NativeExternalPlayer::~NativeExternalPlayer() {
    {
        std::scoped_lock lock(gInstanceMutex);
        if (gInstance == this) gInstance = nullptr;
    }
    if (!activity_) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    env->DeleteGlobalRef(activity_);
    activity_ = nullptr;
}

std::vector<ExternalPlayerApp> NativeExternalPlayer::availablePlayers() const {
    std::vector<ExternalPlayerApp> result;
    if (!activity_) return result;

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return result;

    jclass activityClass = env->GetObjectClass(activity_);
    if (!activityClass) return result;
    jmethodID getPackageManager = env->GetMethodID(activityClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jmethodID getPackageName = env->GetMethodID(activityClass, "getPackageName", "()Ljava/lang/String;");
    jobject packageManager = getPackageManager ? env->CallObjectMethod(activity_, getPackageManager) : nullptr;
    jstring ownPackageValue = getPackageName ? static_cast<jstring>(env->CallObjectMethod(activity_, getPackageName)) : nullptr;
    const std::string ownPackage = jniString(env, ownPackageValue);
    if (ownPackageValue) env->DeleteLocalRef(ownPackageValue);
    if (!packageManager || clearException(env, "package manager lookup")) {
        if (packageManager) env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }

    jobject intent = createVideoIntent(env, kSampleVideoUrl);
    jclass packageManagerClass = env->GetObjectClass(packageManager);
    jmethodID queryIntentActivities = packageManagerClass
        ? env->GetMethodID(packageManagerClass, "queryIntentActivities", "(Landroid/content/Intent;I)Ljava/util/List;")
        : nullptr;
    jobject list = intent && queryIntentActivities
        ? env->CallObjectMethod(packageManager, queryIntentActivities, intent, static_cast<jint>(0))
        : nullptr;
    if (intent) env->DeleteLocalRef(intent);
    if (!list || clearException(env, "external player query")) {
        if (list) env->DeleteLocalRef(list);
        if (packageManagerClass) env->DeleteLocalRef(packageManagerClass);
        env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }

    jclass listClass = env->GetObjectClass(list);
    jmethodID sizeMethod = listClass ? env->GetMethodID(listClass, "size", "()I") : nullptr;
    jmethodID getMethod = listClass ? env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;") : nullptr;
    const jint size = sizeMethod ? env->CallIntMethod(list, sizeMethod) : 0;
    std::unordered_set<std::string> seen;
    for (jint i = 0; i < size; ++i) {
        jobject resolveInfo = getMethod ? env->CallObjectMethod(list, getMethod, i) : nullptr;
        if (!resolveInfo) continue;
        jclass resolveInfoClass = env->GetObjectClass(resolveInfo);
        jfieldID priorityField = resolveInfoClass ? env->GetFieldID(resolveInfoClass, "priority", "I") : nullptr;
        jfieldID activityInfoField = resolveInfoClass
            ? env->GetFieldID(resolveInfoClass, "activityInfo", "Landroid/content/pm/ActivityInfo;")
            : nullptr;
        const jint priority = priorityField ? env->GetIntField(resolveInfo, priorityField) : 0;
        jobject activityInfo = activityInfoField ? env->GetObjectField(resolveInfo, activityInfoField) : nullptr;
        if (priority >= 0 && activityInfo) {
            jclass activityInfoClass = env->GetObjectClass(activityInfo);
            jfieldID packageField = activityInfoClass ? env->GetFieldID(activityInfoClass, "packageName", "Ljava/lang/String;") : nullptr;
            jfieldID nameField = activityInfoClass ? env->GetFieldID(activityInfoClass, "name", "Ljava/lang/String;") : nullptr;
            jstring packageValue = packageField ? static_cast<jstring>(env->GetObjectField(activityInfo, packageField)) : nullptr;
            jstring nameValue = nameField ? static_cast<jstring>(env->GetObjectField(activityInfo, nameField)) : nullptr;
            const std::string packageName = jniString(env, packageValue);
            const std::string activityName = jniString(env, nameValue);
            if (packageValue) env->DeleteLocalRef(packageValue);
            if (nameValue) env->DeleteLocalRef(nameValue);

            if (!packageName.empty() && packageName != ownPackage && !activityName.empty()) {
                const std::string component = packageName + "/" + activityName;
                if (seen.insert(component).second) {
                    std::string label = packageName;
                    jmethodID loadLabel = resolveInfoClass
                        ? env->GetMethodID(resolveInfoClass, "loadLabel", "(Landroid/content/pm/PackageManager;)Ljava/lang/CharSequence;")
                        : nullptr;
                    jobject labelValue = loadLabel ? env->CallObjectMethod(resolveInfo, loadLabel, packageManager) : nullptr;
                    if (labelValue && !clearException(env, "external player label")) {
                        jclass labelClass = env->GetObjectClass(labelValue);
                        jmethodID toString = labelClass ? env->GetMethodID(labelClass, "toString", "()Ljava/lang/String;") : nullptr;
                        jstring labelString = toString ? static_cast<jstring>(env->CallObjectMethod(labelValue, toString)) : nullptr;
                        const std::string parsedLabel = jniString(env, labelString);
                        if (!parsedLabel.empty()) label = parsedLabel;
                        if (labelString) env->DeleteLocalRef(labelString);
                        if (labelClass) env->DeleteLocalRef(labelClass);
                    }
                    if (labelValue) env->DeleteLocalRef(labelValue);
                    result.push_back({component, packageName, label});
                }
            }
            if (activityInfoClass) env->DeleteLocalRef(activityInfoClass);
        }
        if (activityInfo) env->DeleteLocalRef(activityInfo);
        if (resolveInfoClass) env->DeleteLocalRef(resolveInfoClass);
        env->DeleteLocalRef(resolveInfo);
        if (clearException(env, "external player result parsing")) break;
    }

    if (listClass) env->DeleteLocalRef(listClass);
    env->DeleteLocalRef(list);
    if (packageManagerClass) env->DeleteLocalRef(packageManagerClass);
    env->DeleteLocalRef(packageManager);
    env->DeleteLocalRef(activityClass);

    std::sort(result.begin(), result.end(), [](const ExternalPlayerApp& left, const ExternalPlayerApp& right) {
        if (left.label != right.label) return left.label < right.label;
        return left.packageName < right.packageName;
    });
    __android_log_print(ANDROID_LOG_INFO, kTag, "Detected %zu external video player activities", result.size());
    return result;
}

bool NativeExternalPlayer::launch(
    const ExternalPlayerApp& app,
    const std::string& url,
    const std::string& title,
    int positionMs,
    const std::string& subtitleUrl,
    std::string& error
) {
    if (!activity_ || app.componentName.empty() || app.packageName.empty() || url.empty()) {
        error = "External player launch is incomplete";
        return false;
    }

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        error = "Unable to attach external-player thread to JVM";
        return false;
    }

    jobject intent = createVideoIntent(env, url);
    if (!intent) {
        error = "Unable to create external-player intent";
        return false;
    }
    jclass intentClass = env->GetObjectClass(intent);
    jclass componentClass = env->FindClass("android/content/ComponentName");
    jmethodID unflatten = componentClass
        ? env->GetStaticMethodID(componentClass, "unflattenFromString", "(Ljava/lang/String;)Landroid/content/ComponentName;")
        : nullptr;
    jmethodID setComponent = intentClass
        ? env->GetMethodID(intentClass, "setComponent", "(Landroid/content/ComponentName;)Landroid/content/Intent;")
        : nullptr;
    jstring componentValue = env->NewStringUTF(app.componentName.c_str());
    jobject component = componentValue && unflatten
        ? env->CallStaticObjectMethod(componentClass, unflatten, componentValue)
        : nullptr;
    if (componentValue) env->DeleteLocalRef(componentValue);
    if (!component || !setComponent || clearException(env, "external player component", &error)) {
        if (component) env->DeleteLocalRef(component);
        if (componentClass) env->DeleteLocalRef(componentClass);
        if (intentClass) env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
        if (error.empty()) error = "Configured external player is unavailable";
        return false;
    }
    env->CallObjectMethod(intent, setComponent, component);
    env->DeleteLocalRef(component);
    if (componentClass) env->DeleteLocalRef(componentClass);

    const int safePosition = std::max(0, positionMs);
    switch (externalPlayerKindForPackage(app.packageName)) {
        case ExternalPlayerKind::Vlc:
            putStringExtra(env, intent, "title", title);
            putIntExtra(env, intent, "position", safePosition);
            putStringExtra(env, intent, "subtitles_location", subtitleUrl);
            break;
        case ExternalPlayerKind::MxPlayer:
            putStringExtra(env, intent, "title", title);
            putIntExtra(env, intent, "position", safePosition);
            putBoolExtra(env, intent, "return_result", true);
            break;
        case ExternalPlayerKind::Mpv:
            putStringExtra(env, intent, "media-title", title);
            putIntExtra(env, intent, "position", safePosition);
            break;
        case ExternalPlayerKind::Vimu:
            putStringExtra(env, intent, "forcename", title);
            putIntExtra(env, intent, "startfrom", safePosition);
            putStringExtra(env, intent, "forcedsrt", subtitleUrl);
            break;
        case ExternalPlayerKind::Generic:
            putStringExtra(env, intent, "title", title);
            putIntExtra(env, intent, "position", safePosition);
            break;
    }

    jclass activityClass = env->GetObjectClass(activity_);
    jmethodID startActivityForResult = activityClass
        ? env->GetMethodID(activityClass, "startActivityForResult", "(Landroid/content/Intent;I)V")
        : nullptr;
    {
        std::scoped_lock lock(resultMutex_);
        activeKind_ = externalPlayerKindForPackage(app.packageName);
        pendingResult_.reset();
    }
    if (startActivityForResult) {
        env->CallVoidMethod(activity_, startActivityForResult, intent, static_cast<jint>(kRequestCode));
    }
    const bool failed = !startActivityForResult || clearException(env, "startActivityForResult", &error);
    if (activityClass) env->DeleteLocalRef(activityClass);
    if (intentClass) env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(intent);
    if (failed) {
        if (error.empty()) error = "Configured external player could not be launched";
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag, "Launching external player %s at %d ms", app.packageName.c_str(), safePosition);
    return true;
}

std::optional<ExternalPlayerResult> NativeExternalPlayer::takeResult() {
    std::scoped_lock lock(resultMutex_);
    auto result = std::move(pendingResult_);
    pendingResult_.reset();
    return result;
}

void NativeExternalPlayer::handleActivityResult(JNIEnv* env, int requestCode, int resultCode, jobject dataIntent) {
    if (requestCode != kRequestCode) return;

    ExternalPlayerKind kind = ExternalPlayerKind::Generic;
    {
        std::scoped_lock lock(resultMutex_);
        kind = activeKind_;
    }

    const char* positionKey = kind == ExternalPlayerKind::Vlc ? "extra_position" : "position";
    const bool hasPosition = dataIntent && hasExtra(env, dataIntent, positionKey);
    const auto outcome = externalPlayerOutcomeForResult(kind, resultCode, hasPosition);
    ExternalPlayerResult result{
        .success = outcome.success,
        .completionKnown = outcome.completionKnown,
        .completed = outcome.completed,
    };
    if (result.success && hasPosition) {
        result.positionMs = kind == ExternalPlayerKind::Vlc
            ? getLongExtra(env, dataIntent, positionKey)
            : getIntExtra(env, dataIntent, positionKey);
    }

    {
        std::scoped_lock lock(resultMutex_);
        pendingResult_ = result;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "External playback result code=%d success=%d completedKnown=%d completed=%d positionMs=%lld",
        resultCode,
        result.success ? 1 : 0,
        result.completionKnown ? 1 : 0,
        result.completed ? 1 : 0,
        static_cast<long long>(result.positionMs)
    );
}

extern "C" JNIEXPORT void JNICALL
Java_app_sloppatv_SloppaNativeActivity_nativeOnActivityResult(
    JNIEnv* env,
    jclass,
    jint requestCode,
    jint resultCode,
    jobject dataIntent
) {
    std::scoped_lock lock(gInstanceMutex);
    if (gInstance) gInstance->handleActivityResult(env, requestCode, resultCode, dataIntent);
}
