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

jclass findClassChecked(JNIEnv* env, const char* name, const char* operation, std::string* error = nullptr) {
    if (!env) return nullptr;
    jclass value = env->FindClass(name);
    if (!clearException(env, operation, error)) return value;
    if (value) env->DeleteLocalRef(value);
    return nullptr;
}

jclass objectClassChecked(JNIEnv* env, jobject object, const char* operation, std::string* error = nullptr) {
    if (!env || !object) return nullptr;
    jclass value = env->GetObjectClass(object);
    if (!clearException(env, operation, error)) return value;
    if (value) env->DeleteLocalRef(value);
    return nullptr;
}

jmethodID methodChecked(JNIEnv* env, jclass clazz, const char* name, const char* signature, const char* operation,
                        std::string* error = nullptr) {
    if (!env || !clazz) return nullptr;
    jmethodID value = env->GetMethodID(clazz, name, signature);
    return clearException(env, operation, error) ? nullptr : value;
}

jmethodID staticMethodChecked(JNIEnv* env, jclass clazz, const char* name, const char* signature, const char* operation,
                              std::string* error = nullptr) {
    if (!env || !clazz) return nullptr;
    jmethodID value = env->GetStaticMethodID(clazz, name, signature);
    return clearException(env, operation, error) ? nullptr : value;
}

jfieldID fieldChecked(JNIEnv* env, jclass clazz, const char* name, const char* signature, const char* operation,
                      std::string* error = nullptr) {
    if (!env || !clazz) return nullptr;
    jfieldID value = env->GetFieldID(clazz, name, signature);
    return clearException(env, operation, error) ? nullptr : value;
}

jobject createVideoIntent(JNIEnv* env, const std::string& url) {
    jclass intentClass = findClassChecked(env, "android/content/Intent", "intent class lookup");
    if (!intentClass) return nullptr;
    jclass uriClass = findClassChecked(env, "android/net/Uri", "URI class lookup");
    if (!uriClass) {
        env->DeleteLocalRef(intentClass);
        return nullptr;
    }

    jmethodID intentCtor =
        methodChecked(env, intentClass, "<init>", "(Ljava/lang/String;)V", "intent constructor lookup");
    jmethodID uriParse =
        staticMethodChecked(env, uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;", "URI parser lookup");
    jmethodID setDataAndType =
        methodChecked(env, intentClass, "setDataAndType",
                      "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;", "intent data method lookup");
    if (!intentCtor || !uriParse || !setDataAndType) {
        env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(uriClass);
        return nullptr;
    }

    jstring action = jniNewString(env, kActionView);
    bool failed = clearException(env, "intent action creation") || !action;
    jobject intent = nullptr;
    if (!failed) {
        intent = env->NewObject(intentClass, intentCtor, action);
        failed = clearException(env, "intent construction") || !intent;
    }
    if (action) env->DeleteLocalRef(action);

    jstring jUrl = nullptr;
    jobject uri = nullptr;
    if (!failed) {
        jUrl = jniNewString(env, url);
        failed = clearException(env, "intent URL creation") || !jUrl;
    }
    if (!failed) {
        uri = env->CallStaticObjectMethod(uriClass, uriParse, jUrl);
        failed = clearException(env, "intent URI parsing") || !uri;
    }
    if (jUrl) env->DeleteLocalRef(jUrl);

    jstring mime = nullptr;
    if (!failed) {
        mime = jniNewString(env, kVideoMime);
        failed = clearException(env, "intent MIME creation") || !mime;
    }
    if (!failed) {
        env->CallObjectMethod(intent, setDataAndType, uri, mime);
        failed = clearException(env, "video intent construction");
    }
    if (mime) env->DeleteLocalRef(mime);
    if (uri) env->DeleteLocalRef(uri);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(uriClass);
    if (failed) {
        if (intent) env->DeleteLocalRef(intent);
        return nullptr;
    }
    return intent;
}

void putStringExtra(JNIEnv* env, jobject intent, const char* key, const std::string& value) {
    if (!env || !intent || !key || value.empty()) return;
    jclass intentClass = objectClassChecked(env, intent, "string intent class lookup");
    jmethodID method =
        methodChecked(env, intentClass, "putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                      "string intent method lookup");
    jstring jKey = nullptr;
    jstring jValue = nullptr;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "string intent key creation") || !jKey;
    }
    if (!failed) {
        jValue = jniNewString(env, value);
        failed = clearException(env, "string intent value creation") || !jValue;
    }
    if (!failed) {
        env->CallObjectMethod(intent, method, jKey, jValue);
        clearException(env, "string intent extra");
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (jValue) env->DeleteLocalRef(jValue);
    if (intentClass) env->DeleteLocalRef(intentClass);
}

void putUriArrayExtra(JNIEnv* env, jobject intent, const char* key, const std::string& url) {
    if (!env || !intent || !key || url.empty()) return;
    jclass intentClass = objectClassChecked(env, intent, "URI array intent class lookup");
    jclass uriClass = findClassChecked(env, "android/net/Uri", "URI array URI class lookup");
    jclass parcelableClass = findClassChecked(env, "android/os/Parcelable", "URI array Parcelable class lookup");
    jmethodID parse =
        staticMethodChecked(env, uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;", "URI array parser lookup");
    jmethodID putExtra = methodChecked(env, intentClass, "putExtra",
                                       "(Ljava/lang/String;[Landroid/os/Parcelable;)Landroid/content/Intent;",
                                       "URI array intent method lookup");

    jstring jUrl = nullptr;
    jobject uri = nullptr;
    jobjectArray values = nullptr;
    jstring jKey = nullptr;
    bool failed = !intentClass || !uriClass || !parcelableClass || !parse || !putExtra;
    if (!failed) {
        jUrl = jniNewString(env, url);
        failed = clearException(env, "URI array URL creation") || !jUrl;
    }
    if (!failed) {
        uri = env->CallStaticObjectMethod(uriClass, parse, jUrl);
        failed = clearException(env, "URI array parsing") || !uri;
    }
    if (!failed) {
        values = env->NewObjectArray(1, parcelableClass, nullptr);
        failed = clearException(env, "URI array allocation") || !values;
    }
    if (!failed) {
        env->SetObjectArrayElement(values, 0, uri);
        failed = clearException(env, "URI array population");
    }
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "URI array key creation") || !jKey;
    }
    if (!failed) {
        env->CallObjectMethod(intent, putExtra, jKey, values);
        clearException(env, "URI array intent extra");
    }

    if (jKey) env->DeleteLocalRef(jKey);
    if (values) env->DeleteLocalRef(values);
    if (uri) env->DeleteLocalRef(uri);
    if (jUrl) env->DeleteLocalRef(jUrl);
    if (parcelableClass) env->DeleteLocalRef(parcelableClass);
    if (uriClass) env->DeleteLocalRef(uriClass);
    if (intentClass) env->DeleteLocalRef(intentClass);
}

void putByteExtra(JNIEnv* env, jobject intent, const char* key, int value) {
    if (!env || !intent || !key) return;
    jclass intentClass = objectClassChecked(env, intent, "byte intent class lookup");
    jmethodID method = methodChecked(env, intentClass, "putExtra", "(Ljava/lang/String;B)Landroid/content/Intent;",
                                     "byte intent method lookup");
    jstring jKey = nullptr;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "byte intent key creation") || !jKey;
    }
    if (!failed) {
        env->CallObjectMethod(intent, method, jKey, static_cast<jbyte>(value));
        clearException(env, "byte intent extra");
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
}

void putIntExtra(JNIEnv* env, jobject intent, const char* key, int value) {
    if (!env || !intent || !key) return;
    jclass intentClass = objectClassChecked(env, intent, "integer intent class lookup");
    jmethodID method = methodChecked(env, intentClass, "putExtra", "(Ljava/lang/String;I)Landroid/content/Intent;",
                                     "integer intent method lookup");
    jstring jKey = nullptr;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "integer intent key creation") || !jKey;
    }
    if (!failed) {
        env->CallObjectMethod(intent, method, jKey, static_cast<jint>(value));
        clearException(env, "integer intent extra");
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
}

bool hasExtra(JNIEnv* env, jobject intent, const char* key) {
    if (!env || !intent || !key) return false;
    jclass intentClass = objectClassChecked(env, intent, "has-extra intent class lookup");
    jmethodID method = methodChecked(env, intentClass, "hasExtra", "(Ljava/lang/String;)Z", "has-extra method lookup");
    jstring jKey = nullptr;
    bool result = false;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "has-extra key creation") || !jKey;
    }
    if (!failed) {
        result = env->CallBooleanMethod(intent, method, jKey) == JNI_TRUE;
        if (clearException(env, "has intent extra")) result = false;
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
    return result;
}

int getIntExtra(JNIEnv* env, jobject intent, const char* key, int fallback = -1) {
    if (!env || !intent || !key) return fallback;
    jclass intentClass = objectClassChecked(env, intent, "integer result intent class lookup");
    jmethodID method =
        methodChecked(env, intentClass, "getIntExtra", "(Ljava/lang/String;I)I", "integer result method lookup");
    jstring jKey = nullptr;
    int result = fallback;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "integer result key creation") || !jKey;
    }
    if (!failed) {
        result = env->CallIntMethod(intent, method, jKey, static_cast<jint>(fallback));
        if (clearException(env, "integer result extra")) result = fallback;
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
    return result;
}

int64_t getLongExtra(JNIEnv* env, jobject intent, const char* key, int64_t fallback = -1) {
    if (!env || !intent || !key) return fallback;
    jclass intentClass = objectClassChecked(env, intent, "long result intent class lookup");
    jmethodID method =
        methodChecked(env, intentClass, "getLongExtra", "(Ljava/lang/String;J)J", "long result method lookup");
    jstring jKey = nullptr;
    int64_t result = fallback;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "long result key creation") || !jKey;
    }
    if (!failed) {
        result = env->CallLongMethod(intent, method, jKey, static_cast<jlong>(fallback));
        if (clearException(env, "long result extra")) result = fallback;
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
    return result;
}

void putBoolExtra(JNIEnv* env, jobject intent, const char* key, bool value) {
    if (!env || !intent || !key) return;
    jclass intentClass = objectClassChecked(env, intent, "boolean intent class lookup");
    jmethodID method = methodChecked(env, intentClass, "putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;",
                                     "boolean intent method lookup");
    jstring jKey = nullptr;
    bool failed = !intentClass || !method;
    if (!failed) {
        jKey = jniNewString(env, key);
        failed = clearException(env, "boolean intent key creation") || !jKey;
    }
    if (!failed) {
        env->CallObjectMethod(intent, method, jKey, static_cast<jboolean>(value));
        clearException(env, "boolean intent extra");
    }
    if (jKey) env->DeleteLocalRef(jKey);
    if (intentClass) env->DeleteLocalRef(intentClass);
}
} // namespace

NativeExternalPlayer::NativeExternalPlayer(JavaVM* vm, jobject activity) : vm_(vm) {
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    activity_ = env->NewGlobalRef(activity);
    if (clearException(env, "external player activity retention") || !activity_) return;
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

    jclass activityClass = objectClassChecked(env, activity_, "external player activity class lookup");
    if (!activityClass) return result;
    jmethodID getPackageManager =
        methodChecked(env, activityClass, "getPackageManager", "()Landroid/content/pm/PackageManager;",
                      "package manager method lookup");
    jmethodID getPackageName =
        methodChecked(env, activityClass, "getPackageName", "()Ljava/lang/String;", "package name method lookup");
    if (!getPackageManager || !getPackageName) {
        env->DeleteLocalRef(activityClass);
        return result;
    }

    jobject packageManager = env->CallObjectMethod(activity_, getPackageManager);
    if (clearException(env, "package manager lookup") || !packageManager) {
        if (packageManager) env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }
    jstring ownPackageValue = static_cast<jstring>(env->CallObjectMethod(activity_, getPackageName));
    const bool packageNameFailed = clearException(env, "package name lookup");
    const std::string ownPackage = packageNameFailed ? std::string{} : jniString(env, ownPackageValue);
    if (ownPackageValue) env->DeleteLocalRef(ownPackageValue);
    if (packageNameFailed) {
        env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }

    jobject intent = createVideoIntent(env, kSampleVideoUrl);
    jclass packageManagerClass =
        objectClassChecked(env, packageManager, "external player package manager class lookup");
    jmethodID queryIntentActivities =
        methodChecked(env, packageManagerClass, "queryIntentActivities", "(Landroid/content/Intent;I)Ljava/util/List;",
                      "external player query method lookup");
    jobject list = intent && queryIntentActivities
                       ? env->CallObjectMethod(packageManager, queryIntentActivities, intent, static_cast<jint>(0))
                       : nullptr;
    const bool queryFailed = clearException(env, "external player query");
    if (intent) env->DeleteLocalRef(intent);
    if (queryFailed || !list) {
        if (list) env->DeleteLocalRef(list);
        if (packageManagerClass) env->DeleteLocalRef(packageManagerClass);
        env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }

    jclass listClass = objectClassChecked(env, list, "external player result list class lookup");
    jmethodID sizeMethod = methodChecked(env, listClass, "size", "()I", "external player result list size lookup");
    jmethodID getMethod =
        methodChecked(env, listClass, "get", "(I)Ljava/lang/Object;", "external player result list get lookup");
    if (!listClass || !sizeMethod || !getMethod) {
        if (listClass) env->DeleteLocalRef(listClass);
        env->DeleteLocalRef(list);
        if (packageManagerClass) env->DeleteLocalRef(packageManagerClass);
        env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }

    const jint size = env->CallIntMethod(list, sizeMethod);
    if (clearException(env, "external player result list size")) {
        env->DeleteLocalRef(listClass);
        env->DeleteLocalRef(list);
        if (packageManagerClass) env->DeleteLocalRef(packageManagerClass);
        env->DeleteLocalRef(packageManager);
        env->DeleteLocalRef(activityClass);
        return result;
    }

    std::unordered_set<std::string> seen;
    for (jint i = 0; i < size; ++i) {
        jobject resolveInfo = env->CallObjectMethod(list, getMethod, i);
        if (clearException(env, "external player result lookup")) {
            if (resolveInfo) env->DeleteLocalRef(resolveInfo);
            break;
        }
        if (!resolveInfo) continue;

        jclass resolveInfoClass = objectClassChecked(env, resolveInfo, "external player result class lookup");
        jfieldID priorityField =
            fieldChecked(env, resolveInfoClass, "priority", "I", "external player result priority lookup");
        jfieldID activityInfoField =
            fieldChecked(env, resolveInfoClass, "activityInfo", "Landroid/content/pm/ActivityInfo;",
                         "external player activity info lookup");
        if (!resolveInfoClass || !priorityField || !activityInfoField) {
            if (resolveInfoClass) env->DeleteLocalRef(resolveInfoClass);
            env->DeleteLocalRef(resolveInfo);
            continue;
        }

        const jint priority = env->GetIntField(resolveInfo, priorityField);
        if (clearException(env, "external player result priority")) {
            env->DeleteLocalRef(resolveInfoClass);
            env->DeleteLocalRef(resolveInfo);
            continue;
        }

        jobject activityInfo = env->GetObjectField(resolveInfo, activityInfoField);
        if (clearException(env, "external player activity info")) {
            if (activityInfo) env->DeleteLocalRef(activityInfo);
            env->DeleteLocalRef(resolveInfoClass);
            env->DeleteLocalRef(resolveInfo);
            continue;
        }

        if (priority >= 0 && activityInfo) {
            jclass activityInfoClass =
                objectClassChecked(env, activityInfo, "external player activity info class lookup");
            jfieldID packageField = fieldChecked(env, activityInfoClass, "packageName", "Ljava/lang/String;",
                                                 "external player package field lookup");
            jfieldID nameField = fieldChecked(env, activityInfoClass, "name", "Ljava/lang/String;",
                                              "external player activity field lookup");

            jstring packageValue = nullptr;
            jstring nameValue = nullptr;
            if (packageField) {
                packageValue = static_cast<jstring>(env->GetObjectField(activityInfo, packageField));
                if (clearException(env, "external player package value")) {
                    if (packageValue) env->DeleteLocalRef(packageValue);
                    packageValue = nullptr;
                }
            }
            if (nameField) {
                nameValue = static_cast<jstring>(env->GetObjectField(activityInfo, nameField));
                if (clearException(env, "external player activity value")) {
                    if (nameValue) env->DeleteLocalRef(nameValue);
                    nameValue = nullptr;
                }
            }

            const std::string packageName = jniString(env, packageValue);
            const std::string activityName = jniString(env, nameValue);
            if (packageValue) env->DeleteLocalRef(packageValue);
            if (nameValue) env->DeleteLocalRef(nameValue);

            if (!packageName.empty() && packageName != ownPackage && !activityName.empty()) {
                const std::string component = packageName + "/" + activityName;
                if (seen.insert(component).second) {
                    std::string label = packageName;
                    jmethodID loadLabel = methodChecked(env, resolveInfoClass, "loadLabel",
                                                        "(Landroid/content/pm/PackageManager;)Ljava/lang/CharSequence;",
                                                        "external player label method lookup");
                    jobject labelValue =
                        loadLabel ? env->CallObjectMethod(resolveInfo, loadLabel, packageManager) : nullptr;
                    if (clearException(env, "external player label")) {
                        if (labelValue) env->DeleteLocalRef(labelValue);
                        labelValue = nullptr;
                    }
                    if (labelValue) {
                        jclass labelClass = objectClassChecked(env, labelValue, "external player label class lookup");
                        jmethodID toString = methodChecked(env, labelClass, "toString", "()Ljava/lang/String;",
                                                           "external player label string method lookup");
                        jstring labelString =
                            toString ? static_cast<jstring>(env->CallObjectMethod(labelValue, toString)) : nullptr;
                        if (clearException(env, "external player label string")) {
                            if (labelString) env->DeleteLocalRef(labelString);
                            labelString = nullptr;
                        }
                        const std::string parsedLabel = jniString(env, labelString);
                        if (!parsedLabel.empty()) label = parsedLabel;
                        if (labelString) env->DeleteLocalRef(labelString);
                        if (labelClass) env->DeleteLocalRef(labelClass);
                        env->DeleteLocalRef(labelValue);
                    }
                    result.push_back({component, packageName, label});
                }
            }
            if (activityInfoClass) env->DeleteLocalRef(activityInfoClass);
        }
        if (activityInfo) env->DeleteLocalRef(activityInfo);
        env->DeleteLocalRef(resolveInfoClass);
        env->DeleteLocalRef(resolveInfo);
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

bool NativeExternalPlayer::launch(const ExternalPlayerApp& app, const std::string& url, const std::string& title,
                                  int positionMs, const std::string& subtitleUrl, const std::string& skipSegmentsJson,
                                  std::string& error) {
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
    jclass intentClass = objectClassChecked(env, intent, "external player intent class lookup", &error);
    jclass componentClass =
        findClassChecked(env, "android/content/ComponentName", "external player component class lookup", &error);
    jmethodID unflatten = staticMethodChecked(env, componentClass, "unflattenFromString",
                                              "(Ljava/lang/String;)Landroid/content/ComponentName;",
                                              "external player component lookup", &error);
    jmethodID setComponent =
        methodChecked(env, intentClass, "setComponent", "(Landroid/content/ComponentName;)Landroid/content/Intent;",
                      "external player setComponent lookup", &error);
    jstring componentValue = nullptr;
    jobject component = nullptr;
    bool componentFailed = !intentClass || !componentClass || !unflatten || !setComponent;
    if (!componentFailed) {
        componentValue = jniNewString(env, app.componentName);
        componentFailed = clearException(env, "external player component name", &error) || !componentValue;
    }
    if (!componentFailed) {
        component = env->CallStaticObjectMethod(componentClass, unflatten, componentValue);
        componentFailed = clearException(env, "external player component", &error) || !component;
    }
    if (componentValue) env->DeleteLocalRef(componentValue);
    if (componentFailed) {
        if (component) env->DeleteLocalRef(component);
        if (componentClass) env->DeleteLocalRef(componentClass);
        if (intentClass) env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
        if (error.empty()) error = "Configured external player is unavailable";
        return false;
    }

    env->CallObjectMethod(intent, setComponent, component);
    componentFailed = clearException(env, "external player component assignment", &error);
    env->DeleteLocalRef(component);
    env->DeleteLocalRef(componentClass);
    if (componentFailed) {
        if (intentClass) env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
        if (error.empty()) error = "Configured external player is unavailable";
        return false;
    }

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
        putStringExtra(env, intent, "title", title);
        putIntExtra(env, intent, "position", safePosition);
        putByteExtra(env, intent, "decode_mode", externalMpvDecodeModeForPackage(app.packageName));
        putStringExtra(env, intent, "skip_segments", skipSegmentsJson);
        putUriArrayExtra(env, intent, "subs", subtitleUrl);
        putUriArrayExtra(env, intent, "subs.enable", subtitleUrl);
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

    jclass activityClass = objectClassChecked(env, activity_, "external player activity class lookup", &error);
    jmethodID startActivityForResult =
        methodChecked(env, activityClass, "startActivityForResult", "(Landroid/content/Intent;I)V",
                      "startActivityForResult lookup", &error);
    bool failed = !activityClass || !startActivityForResult;
    if (!failed) {
        {
            std::scoped_lock lock(resultMutex_);
            activeKind_ = externalPlayerKindForPackage(app.packageName);
            pendingResult_.reset();
        }
        env->CallVoidMethod(activity_, startActivityForResult, intent, static_cast<jint>(kRequestCode));
        failed = clearException(env, "startActivityForResult", &error);
    }
    if (activityClass) env->DeleteLocalRef(activityClass);
    if (intentClass) env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(intent);
    if (failed) {
        std::scoped_lock lock(resultMutex_);
        activeKind_ = ExternalPlayerKind::Generic;
        pendingResult_.reset();
        if (error.empty()) error = "Configured external player could not be launched";
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag, "Launching external player %s at %d ms", app.packageName.c_str(),
                        safePosition);
    return true;
}

std::optional<ExternalPlayerResult> NativeExternalPlayer::takeResult() {
    std::scoped_lock lock(resultMutex_);
    auto result = pendingResult_;
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
        result.positionMs = kind == ExternalPlayerKind::Vlc ? getLongExtra(env, dataIntent, positionKey)
                                                            : getIntExtra(env, dataIntent, positionKey);
    }

    {
        std::scoped_lock lock(resultMutex_);
        pendingResult_ = result;
    }
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "External playback result code=%d success=%d completedKnown=%d completed=%d positionMs=%lld",
                        resultCode, result.success ? 1 : 0, result.completionKnown ? 1 : 0, result.completed ? 1 : 0,
                        static_cast<long long>(result.positionMs));
}

extern "C" JNIEXPORT void JNICALL Java_app_sloppatv_SloppaNativeActivity_nativeOnActivityResult(JNIEnv* env, jclass,
                                                                                                jint requestCode,
                                                                                                jint resultCode,
                                                                                                jobject dataIntent) {
    std::scoped_lock lock(gInstanceMutex);
    if (gInstance) gInstance->handleActivityResult(env, requestCode, resultCode, dataIntent);
}
