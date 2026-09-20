#include "jni_http.hpp"
#include "http_cache_policy.hpp"
#include "http_error_policy.hpp"
#include "http_retry_policy.hpp"
#include "jni_env.hpp"

#include <android/log.h>

#include <array>
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
    if (env) activity_ = env->NewGlobalRef(activity);
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
    cancelGeneration_.fetch_add(1, std::memory_order_relaxed);
    std::vector<uint64_t> activeRequests;
    {
        std::scoped_lock lock(activeRequestsMutex_);
        activeRequests.assign(activeRequestIds_.begin(), activeRequestIds_.end());
    }
    for (const uint64_t requestId : activeRequests) cancelRequest(requestId);
    retryWake_.notify_all();
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
    HttpResponse response;
    const uint64_t generation = cancelGeneration_.load(std::memory_order_relaxed);
    constexpr std::array<std::chrono::milliseconds, 2> retryDelays{
        std::chrono::milliseconds{250},
        std::chrono::milliseconds{750},
    };
    const size_t retryCount = transientHttpRetryCount(method);
    for (size_t attempt = 0; attempt <= retryCount; ++attempt) {
        if (cancelGeneration_.load(std::memory_order_relaxed) != generation) {
            response = {};
            response.error = "Request cancelled";
            return response;
        }
        const uint64_t requestId = gNextHttpRequestId.fetch_add(1, std::memory_order_relaxed);
        response = requestOnce(method, url, headers, body, requestId, generation);
        if (cancelGeneration_.load(std::memory_order_relaxed) != generation) {
            response = {};
            response.error = "Request cancelled";
            return response;
        }
        const bool retryable = shouldRetryTransientHttpResponse(method, response.status, !response.error.empty());
        if (!retryable || attempt == retryCount) return response;
        const std::string failure = response.status != 0 ? "HTTP " + std::to_string(response.status) : response.error;
        __android_log_print(ANDROID_LOG_WARN, kTag, "Transient request failure (%s); retrying in %lldms",
                            failure.c_str(), static_cast<long long>(retryDelays[attempt].count()));
        std::unique_lock retryLock(retryMutex_);
        if (retryWake_.wait_for(retryLock, retryDelays[attempt],
                                [&] { return cancelGeneration_.load(std::memory_order_relaxed) != generation; })) {
            response = {};
            response.error = "Request cancelled";
            return response;
        }
    }
    return response;
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

    jclass activityClass = env->GetObjectClass(activity_);
    jmethodID performRequest =
        activityClass ? env->GetMethodID(activityClass, "performHttpRequestBridge",
                                         "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[BJ)Lapp/"
                                         "sloppatv/HttpBridge$Result;")
                      : nullptr;
    if (clearException(env, "HTTP bridge lookup", response.error) || !activityClass || !performRequest) {
        if (activityClass) env->DeleteLocalRef(activityClass);
        return response;
    }

    jclass stringClass = env->FindClass("java/lang/String");
    if (clearException(env, "HTTP bridge String class", response.error) || !stringClass) {
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
    jstring jUrl = toJString(env, url);
    jobjectArray jHeaders = env->NewObjectArray(static_cast<jsize>(headers.size() * 2), stringClass, nullptr);
    if (clearException(env, "HTTP bridge request strings", response.error) || !jMethod || !jUrl || !jHeaders) {
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
        jstring jValue = toJString(env, value);
        if (!jKey || !jValue || env->ExceptionCheck()) {
            if (jKey) env->DeleteLocalRef(jKey);
            if (jValue) env->DeleteLocalRef(jValue);
            headerSetupFailed = true;
            break;
        }
        env->SetObjectArrayElement(jHeaders, headerIndex++, jKey);
        if (!env->ExceptionCheck()) env->SetObjectArrayElement(jHeaders, headerIndex++, jValue);
        env->DeleteLocalRef(jKey);
        env->DeleteLocalRef(jValue);
        if (env->ExceptionCheck()) {
            headerSetupFailed = true;
            break;
        }
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
    if (!jBody) {
        clearException(env, "HTTP bridge request body", response.error);
        if (response.error.empty()) response.error = "Unable to allocate Android HTTP request body";
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
        if (cancelGeneration_.load(std::memory_order_relaxed) != generation) cancelRequest(requestId);
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
        jclass resultClass = env->GetObjectClass(result);
        jfieldID statusField = resultClass ? env->GetFieldID(resultClass, "status", "I") : nullptr;
        jfieldID bodyField = resultClass ? env->GetFieldID(resultClass, "body", "[B") : nullptr;
        jfieldID errorField = resultClass ? env->GetFieldID(resultClass, "error", "Ljava/lang/String;") : nullptr;
        jfieldID setCookieField =
            resultClass ? env->GetFieldID(resultClass, "setCookie", "Ljava/lang/String;") : nullptr;
        if (!clearException(env, "HTTP bridge result fields", response.error) && statusField && bodyField &&
            errorField && setCookieField) {
            response.status = env->GetIntField(result, statusField);
            auto responseBytes = static_cast<jbyteArray>(env->GetObjectField(result, bodyField));
            auto errorText = static_cast<jstring>(env->GetObjectField(result, errorField));
            auto setCookieText = static_cast<jstring>(env->GetObjectField(result, setCookieField));

            if (responseBytes) {
                const jsize length = env->GetArrayLength(responseBytes);
                if (length > 0) {
                    response.body.resize(static_cast<size_t>(length));
                    env->GetByteArrayRegion(responseBytes, 0, length, reinterpret_cast<jbyte*>(response.body.data()));
                }
                env->DeleteLocalRef(responseBytes);
            }
            if (setCookieText) {
                response.setCookie = jniString(env, setCookieText);
                env->DeleteLocalRef(setCookieText);
            }
            if (errorText) {
                const std::string detail = jniString(env, errorText);
                if (!detail.empty()) {
                    response.error = userFacingJavaHttpError("HTTP request", detail);
                    __android_log_print(ANDROID_LOG_ERROR, kTag, "%s (%s)", response.error.c_str(), detail.c_str());
                }
                env->DeleteLocalRef(errorText);
            }
        } else if (response.error.empty()) {
            response.error = "Android HTTP bridge returned an invalid result";
        }
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
    jclass activityClass = env->GetObjectClass(activity_);
    jmethodID registerRequest =
        activityClass ? env->GetMethodID(activityClass, "registerHttpRequestBridge", "(J)V") : nullptr;
    bool ok = registerRequest != nullptr && !env->ExceptionCheck();
    if (ok) {
        env->CallVoidMethod(activity_, registerRequest, static_cast<jlong>(requestId));
        ok = !env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (activityClass) env->DeleteLocalRef(activityClass);
    return ok;
}

void JniHttpClient::unregisterRequest(uint64_t requestId) const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) return;
    jclass activityClass = env->GetObjectClass(activity_);
    jmethodID unregister =
        activityClass ? env->GetMethodID(activityClass, "unregisterHttpRequestBridge", "(J)V") : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (unregister) env->CallVoidMethod(activity_, unregister, static_cast<jlong>(requestId));
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (activityClass) env->DeleteLocalRef(activityClass);
}

void JniHttpClient::cancelRequest(uint64_t requestId) const {
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) return;
    jclass activityClass = env->GetObjectClass(activity_);
    jmethodID cancel = activityClass ? env->GetMethodID(activityClass, "cancelHttpRequestBridge", "(J)V") : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cancel) env->CallVoidMethod(activity_, cancel, static_cast<jlong>(requestId));
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (activityClass) env->DeleteLocalRef(activityClass);
}
