#include "jni_http.hpp"
#include "http_cache_policy.hpp"
#include "http_error_policy.hpp"
#include "http_retry_policy.hpp"
#include "jni_env.hpp"

#include <android/log.h>

#include <chrono>
#include <limits>
#include <vector>

namespace {
constexpr const char* kTag = "sloppaTV/http";
std::atomic<uint64_t> gNextHttpRequestId{1};

using ScopedEnv = ScopedJniEnv;

bool clearException(JNIEnv* env, const char* where, std::string& error) {
    if (!env || !env->ExceptionCheck()) return false;

    jthrowable exception = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string detail;
    if (exception) {
        jclass throwableClass = env->FindClass("java/lang/Throwable");
        if (throwableClass && !env->ExceptionCheck()) {
            jmethodID toString = env->GetMethodID(throwableClass, "toString", "()Ljava/lang/String;");
            if (toString && !env->ExceptionCheck()) {
                auto description = static_cast<jstring>(env->CallObjectMethod(exception, toString));
                if (description && !env->ExceptionCheck()) {
                    detail = jniString(env, description);
                    env->DeleteLocalRef(description);
                }
            }
            env->DeleteLocalRef(throwableClass);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(exception);
    }

    error = userFacingJavaHttpError(where, detail);
    if (!detail.empty() && error != std::string("Java exception at ") + where + ": " + detail) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "%s (%s)", error.c_str(), detail.c_str());
    } else {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", error.c_str());
    }
    return true;
}

jclass findClassChecked(JNIEnv* env, const char* name, const char* where, std::string& error) {
    if (!env || !name) return nullptr;
    jclass value = env->FindClass(name);
    if (!clearException(env, where, error)) return value;
    if (value) env->DeleteLocalRef(value);
    return nullptr;
}

jclass objectClassChecked(JNIEnv* env, jobject object, const char* where, std::string& error) {
    if (!env || !object) return nullptr;
    jclass value = env->GetObjectClass(object);
    if (!clearException(env, where, error)) return value;
    if (value) env->DeleteLocalRef(value);
    return nullptr;
}

jmethodID methodChecked(JNIEnv* env, jclass clazz, const char* name, const char* signature, const char* where,
                        std::string& error) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jmethodID value = env->GetMethodID(clazz, name, signature);
    return clearException(env, where, error) ? nullptr : value;
}

jfieldID fieldChecked(JNIEnv* env, jclass clazz, const char* name, const char* signature, const char* where,
                      std::string& error) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jfieldID value = env->GetFieldID(clazz, name, signature);
    return clearException(env, where, error) ? nullptr : value;
}

std::string requestUrlForLog(const std::string& url) {
    const size_t query = url.find('?');
    return query == std::string::npos ? url : url.substr(0, query);
}

jstring toJString(JNIEnv* env, const std::string& value) {
    return jniNewString(env, value);
}

} // namespace

JniHttpClient::JniHttpClient(JavaVM* vm, jobject activity) : vm_(vm) {
    if (!vm_ || !activity) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) return;
    activity_ = env->NewGlobalRef(activity);
    std::string error;
    if (clearException(env, "HTTP activity retention", error) || !activity_) {
        if (activity_) env->DeleteGlobalRef(activity_);
        activity_ = nullptr;
    }
}

JniHttpClient::~JniHttpClient() {
    if (!activity_) return;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (env) env->DeleteGlobalRef(activity_);
    activity_ = nullptr;
}

void JniHttpClient::invalidateGetCache() const {
    getCoordinator_.invalidate();
}

void JniHttpClient::cancelPending() const {
    retryCoordinator_.cancelPending();
    std::vector<uint64_t> activeRequests;
    {
        std::scoped_lock lock(activeRequestsMutex_);
        activeRequests.assign(activeRequestIds_.begin(), activeRequestIds_.end());
    }
    for (const uint64_t requestId : activeRequests) cancelRequest(requestId);
}

HttpResponse JniHttpClient::request(const std::string& method, const std::string& url,
                                    const std::map<std::string, std::string>& headers, const std::string& body) const {
    const bool deduplicate = method == "GET" && body.empty();
    if (!deduplicate) {
        HttpResponse response = requestWithRetry(method, url, headers, body);
        if (response.ok()) invalidateGetCache();
        return response;
    }

    return getCoordinator_.request(httpGetCacheKey(url, headers), shouldCacheApiGet(url),
                                   [&] { return requestWithRetry(method, url, headers, body); });
}

HttpResponse JniHttpClient::requestWithRetry(const std::string& method, const std::string& url,
                                             const std::map<std::string, std::string>& headers,
                                             const std::string& body) const {
    return retryCoordinator_.request(
        method,
        [&](uint64_t generation) {
            const uint64_t requestId = gNextHttpRequestId.fetch_add(1, std::memory_order_relaxed);
            return requestOnce(method, url, headers, body, requestId, generation);
        },
        [](const HttpResponse& response, std::chrono::milliseconds delay) {
            const std::string failure =
                response.status != 0 ? "HTTP " + std::to_string(response.status) : response.error;
            __android_log_print(ANDROID_LOG_WARN, kTag, "Transient request failure (%s); retrying in %lldms",
                                failure.c_str(), static_cast<long long>(delay.count()));
        });
}

HttpResponse JniHttpClient::requestOnce(const std::string& method, const std::string& url,
                                        const std::map<std::string, std::string>& headers, const std::string& body,
                                        uint64_t requestId, uint64_t generation) const {
    HttpResponse response;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) {
        response.error = "Unable to access Android HTTP bridge";
        return response;
    }

    jclass activityClass = objectClassChecked(env, activity_, "HTTP bridge activity class", response.error);
    jmethodID performRequest =
        methodChecked(env, activityClass, "performHttpRequestBridge",
                      "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[BJ)Lapp/sloppatv/HttpBridge$Result;",
                      "HTTP bridge lookup", response.error);
    if (!activityClass || !performRequest) {
        if (activityClass) env->DeleteLocalRef(activityClass);
        return response;
    }

    jclass stringClass = findClassChecked(env, "java/lang/String", "HTTP bridge String class", response.error);
    if (!stringClass) {
        env->DeleteLocalRef(activityClass);
        return response;
    }

    constexpr size_t maxJniArrayLength = static_cast<size_t>(std::numeric_limits<jsize>::max());
    if (headers.size() > maxJniArrayLength / 2 || body.size() > maxJniArrayLength) {
        response.error = "HTTP request exceeds the Android bridge size limit";
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(activityClass);
        return response;
    }

    jstring jMethod = toJString(env, method);
    bool requestSetupFailed = clearException(env, "HTTP bridge request method", response.error) || !jMethod;
    jstring jUrl = nullptr;
    if (!requestSetupFailed) {
        jUrl = toJString(env, url);
        requestSetupFailed = clearException(env, "HTTP bridge request URL", response.error) || !jUrl;
    }
    jobjectArray jHeaders = nullptr;
    if (!requestSetupFailed) {
        jHeaders = env->NewObjectArray(static_cast<jsize>(headers.size() * 2), stringClass, nullptr);
        requestSetupFailed = clearException(env, "HTTP bridge request headers allocation", response.error) || !jHeaders;
    }
    if (requestSetupFailed) {
        if (response.error.empty()) response.error = "Unable to allocate Android HTTP request strings";
        if (jHeaders) env->DeleteLocalRef(jHeaders);
        if (jUrl) env->DeleteLocalRef(jUrl);
        if (jMethod) env->DeleteLocalRef(jMethod);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(activityClass);
        return response;
    }
    jsize headerIndex = 0;
    bool headerSetupFailed = false;
    for (const auto& [key, value] : headers) {
        jstring jKey = toJString(env, key);
        headerSetupFailed = clearException(env, "HTTP bridge header key", response.error) || !jKey;
        jstring jValue = nullptr;
        if (!headerSetupFailed) {
            jValue = toJString(env, value);
            headerSetupFailed = clearException(env, "HTTP bridge header value", response.error) || !jValue;
        }
        if (!headerSetupFailed) {
            env->SetObjectArrayElement(jHeaders, headerIndex++, jKey);
            headerSetupFailed = clearException(env, "HTTP bridge header key insertion", response.error);
        }
        if (!headerSetupFailed) {
            env->SetObjectArrayElement(jHeaders, headerIndex++, jValue);
            headerSetupFailed = clearException(env, "HTTP bridge header value insertion", response.error);
        }
        if (jKey) env->DeleteLocalRef(jKey);
        if (jValue) env->DeleteLocalRef(jValue);
        if (headerSetupFailed) break;
    }

    if (headerSetupFailed) {
        clearException(env, "HTTP bridge request headers", response.error);
        if (response.error.empty()) response.error = "Unable to populate Android HTTP request headers";
        env->DeleteLocalRef(jHeaders);
        env->DeleteLocalRef(jUrl);
        env->DeleteLocalRef(jMethod);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(activityClass);
        return response;
    }

    jbyteArray jBody = env->NewByteArray(static_cast<jsize>(body.size()));
    if (clearException(env, "HTTP bridge request body", response.error) || !jBody) {
        if (response.error.empty()) response.error = "Unable to allocate Android HTTP request body";
        if (jBody) env->DeleteLocalRef(jBody);
        env->DeleteLocalRef(jHeaders);
        env->DeleteLocalRef(jUrl);
        env->DeleteLocalRef(jMethod);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(activityClass);
        return response;
    }
    if (!body.empty()) {
        env->SetByteArrayRegion(jBody, 0, static_cast<jsize>(body.size()), reinterpret_cast<const jbyte*>(body.data()));
    }
    if (clearException(env, "HTTP bridge request setup", response.error)) {
        if (jBody) env->DeleteLocalRef(jBody);
        if (jHeaders) env->DeleteLocalRef(jHeaders);
        if (jUrl) env->DeleteLocalRef(jUrl);
        if (jMethod) env->DeleteLocalRef(jMethod);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(activityClass);
        return response;
    }

    const bool registered = registerRequest(requestId);
    if (!registered) {
        response.error = "Unable to register Android HTTP request";
    } else {
        {
            std::scoped_lock lock(activeRequestsMutex_);
            activeRequestIds_.insert(requestId);
        }
        if (retryCoordinator_.cancelled(generation)) cancelRequest(requestId);
    }
    jobject result = response.error.empty() ? env->CallObjectMethod(activity_, performRequest, jMethod, jUrl, jHeaders,
                                                                    jBody, static_cast<jlong>(requestId))
                                            : nullptr;
    const bool requestThrew = clearException(env, "HTTP bridge request", response.error);
    if (registered) unregisterRequest(requestId);
    {
        std::scoped_lock lock(activeRequestsMutex_);
        activeRequestIds_.erase(requestId);
    }
    if (requestThrew || !result) {
        if (!result && response.error.empty()) response.error = "Android HTTP bridge returned no result";
        const std::string safeUrl = requestUrlForLog(url);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "%s %s failed before HTTP status: %s", method.c_str(),
                            safeUrl.c_str(), response.error.c_str());
    } else {
        jclass resultClass = objectClassChecked(env, result, "HTTP bridge result class", response.error);
        jfieldID statusField =
            fieldChecked(env, resultClass, "status", "I", "HTTP bridge result status field", response.error);
        jfieldID bodyField =
            fieldChecked(env, resultClass, "body", "[B", "HTTP bridge result body field", response.error);
        jfieldID errorField = fieldChecked(env, resultClass, "error", "Ljava/lang/String;",
                                           "HTTP bridge result error field", response.error);
        jfieldID setCookieField = fieldChecked(env, resultClass, "setCookie", "Ljava/lang/String;",
                                               "HTTP bridge result cookie field", response.error);
        bool resultFailed = !resultClass || !statusField || !bodyField || !errorField || !setCookieField;

        jbyteArray responseBytes = nullptr;
        jstring errorText = nullptr;
        jstring setCookieText = nullptr;
        if (!resultFailed) {
            response.status = env->GetIntField(result, statusField);
            resultFailed = clearException(env, "HTTP bridge result status", response.error);
        }
        if (!resultFailed) {
            responseBytes = static_cast<jbyteArray>(env->GetObjectField(result, bodyField));
            resultFailed = clearException(env, "HTTP bridge result body", response.error);
        }
        if (!resultFailed) {
            errorText = static_cast<jstring>(env->GetObjectField(result, errorField));
            resultFailed = clearException(env, "HTTP bridge result error", response.error);
        }
        if (!resultFailed) {
            setCookieText = static_cast<jstring>(env->GetObjectField(result, setCookieField));
            resultFailed = clearException(env, "HTTP bridge result cookie", response.error);
        }

        if (!resultFailed && responseBytes) {
            const jsize length = env->GetArrayLength(responseBytes);
            resultFailed = clearException(env, "HTTP bridge result body length", response.error);
            if (!resultFailed && length > 0) {
                response.body.resize(static_cast<size_t>(length));
                env->GetByteArrayRegion(responseBytes, 0, length, reinterpret_cast<jbyte*>(response.body.data()));
                resultFailed = clearException(env, "HTTP bridge result body copy", response.error);
            }
        }
        if (!resultFailed && setCookieText) response.setCookie = jniString(env, setCookieText);
        if (!resultFailed && errorText) {
            const std::string detail = jniString(env, errorText);
            if (!detail.empty()) {
                response.error = userFacingJavaHttpError("HTTP request", detail);
                __android_log_print(ANDROID_LOG_ERROR, kTag, "%s (%s)", response.error.c_str(), detail.c_str());
            }
        }

        if (responseBytes) env->DeleteLocalRef(responseBytes);
        if (setCookieText) env->DeleteLocalRef(setCookieText);
        if (errorText) env->DeleteLocalRef(errorText);
        if (resultFailed && response.error.empty()) response.error = "Android HTTP bridge returned an invalid result";
        if (resultClass) env->DeleteLocalRef(resultClass);
        env->DeleteLocalRef(result);
    }

    if (jBody) env->DeleteLocalRef(jBody);
    if (jHeaders) env->DeleteLocalRef(jHeaders);
    if (jUrl) env->DeleteLocalRef(jUrl);
    if (jMethod) env->DeleteLocalRef(jMethod);
    env->DeleteLocalRef(stringClass);
    env->DeleteLocalRef(activityClass);
    return response;
}

bool JniHttpClient::registerRequest(uint64_t requestId) const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) return false;
    std::string error;
    jclass activityClass = objectClassChecked(env, activity_, "HTTP request registration class", error);
    jmethodID registerRequest = methodChecked(env, activityClass, "registerHttpRequestBridge", "(J)V",
                                              "HTTP request registration lookup", error);
    bool ok = activityClass && registerRequest;
    if (ok) {
        env->CallVoidMethod(activity_, registerRequest, static_cast<jlong>(requestId));
        ok = !clearException(env, "HTTP request registration", error);
    }
    if (activityClass) env->DeleteLocalRef(activityClass);
    return ok;
}

void JniHttpClient::unregisterRequest(uint64_t requestId) const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) return;
    std::string error;
    jclass activityClass = objectClassChecked(env, activity_, "HTTP request unregistration class", error);
    jmethodID unregister = methodChecked(env, activityClass, "unregisterHttpRequestBridge", "(J)V",
                                         "HTTP request unregistration lookup", error);
    if (unregister) {
        env->CallVoidMethod(activity_, unregister, static_cast<jlong>(requestId));
        clearException(env, "HTTP request unregistration", error);
    }
    if (activityClass) env->DeleteLocalRef(activityClass);
}

void JniHttpClient::cancelRequest(uint64_t requestId) const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) return;
    std::string error;
    jclass activityClass = objectClassChecked(env, activity_, "HTTP request cancellation class", error);
    jmethodID cancel =
        methodChecked(env, activityClass, "cancelHttpRequestBridge", "(J)V", "HTTP request cancellation lookup", error);
    if (cancel) {
        env->CallVoidMethod(activity_, cancel, static_cast<jlong>(requestId));
        clearException(env, "HTTP request cancellation", error);
    }
    if (activityClass) env->DeleteLocalRef(activityClass);
}
