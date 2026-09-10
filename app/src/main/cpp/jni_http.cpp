#include "jni_http.hpp"
#include "http_cache_policy.hpp"
#include "http_error_policy.hpp"
#include "http_retry_policy.hpp"
#include "jni_env.hpp"

#include <android/log.h>

#include <array>
#include <chrono>
#include <sstream>
#include <thread>

namespace {
constexpr const char* kTag = "sloppaTV/http";

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
    return env->NewStringUTF(value.c_str());
}

}

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

std::string JniHttpClient::getCacheKey(
    const std::string& url,
    const std::map<std::string, std::string>& headers
) const {
    std::ostringstream key;
    key << url;
    for (const auto& [name, value] : headers) key << '\n' << name << ':' << value;
    return key.str();
}

void JniHttpClient::invalidateGetCache() const {
    std::scoped_lock lock(cacheMutex_);
    getCache_.clear();
    ++cacheGeneration_;
}

void JniHttpClient::cancelPending() const {
    cancelGeneration_.fetch_add(1, std::memory_order_relaxed);
    retryWake_.notify_all();
}

HttpResponse JniHttpClient::request(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body
) const {
    const bool deduplicate = method == "GET" && body.empty();
    if (!deduplicate) {
        HttpResponse response = requestWithRetry(method, url, headers, body);
        if (response.ok()) invalidateGetCache();
        return response;
    }

    const bool cacheable = shouldCacheApiGet(url);
    const std::string key = getCacheKey(url, headers);
    std::shared_ptr<InFlightRequest> inFlight;
    bool owner = false;
    uint64_t requestGeneration = 0;
    {
        std::unique_lock lock(cacheMutex_);
        requestGeneration = cacheGeneration_;
        if (cacheable) {
            const auto now = std::chrono::steady_clock::now();
            std::erase_if(getCache_, [&](const auto& entry) {
                return entry.second.expiresAt <= now;
            });
            const auto cached = getCache_.find(key);
            if (cached != getCache_.end()) return cached->second.response;
        }
        const auto pending = inFlightGets_.find(key);
        if (pending != inFlightGets_.end()
            && shouldJoinInFlightApiGet(requestGeneration, pending->second->generation)) {
            inFlight = pending->second;
        } else {
            inFlight = std::make_shared<InFlightRequest>();
            inFlight->generation = requestGeneration;
            inFlightGets_[key] = inFlight;
            owner = true;
        }
    }

    if (!owner) {
        std::unique_lock lock(cacheMutex_);
        inFlight->completed.wait(lock, [&] { return inFlight->done; });
        return inFlight->response;
    }

    HttpResponse response = requestWithRetry(method, url, headers, body);
    {
        std::scoped_lock lock(cacheMutex_);
        if (cacheable && response.ok() && requestGeneration == cacheGeneration_) {
            const auto now = std::chrono::steady_clock::now();
            std::erase_if(getCache_, [&](const auto& entry) {
                return entry.second.expiresAt <= now;
            });
            if (getCache_.size() >= kMaxApiGetCacheEntries) {
                const auto oldest = std::min_element(
                    getCache_.begin(),
                    getCache_.end(),
                    [](const auto& left, const auto& right) {
                        return left.second.expiresAt < right.second.expiresAt;
                    }
                );
                if (oldest != getCache_.end()) getCache_.erase(oldest);
            }
            getCache_[key] = CacheEntry{response, now + std::chrono::seconds(5)};
        }
        inFlight->response = response;
        inFlight->done = true;
        const auto pending = inFlightGets_.find(key);
        if (pending != inFlightGets_.end() && pending->second == inFlight) {
            inFlightGets_.erase(pending);
        }
    }
    inFlight->completed.notify_all();
    return response;
}

HttpResponse JniHttpClient::requestWithRetry(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body
) const {
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
        response = requestOnce(method, url, headers, body);
        if (response.status != 0 || response.error.empty()) return response;
        if (attempt == retryCount) break;
        __android_log_print(
            ANDROID_LOG_WARN,
            kTag,
            "Transient request failure (%s); retrying in %lldms",
            response.error.c_str(),
            static_cast<long long>(retryDelays[attempt].count())
        );
        std::unique_lock retryLock(retryMutex_);
        if (retryWake_.wait_for(retryLock, retryDelays[attempt], [&] {
                return cancelGeneration_.load(std::memory_order_relaxed) != generation;
            })) {
            response = {};
            response.error = "Request cancelled";
            return response;
        }
    }
    return response;
}

HttpResponse JniHttpClient::requestOnce(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body
) const {
    HttpResponse response;
    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env || !activity_) {
        response.error = "Unable to access Android HTTP bridge";
        return response;
    }

    jclass activityClass = env->GetObjectClass(activity_);
    jmethodID performRequest = activityClass
        ? env->GetMethodID(
            activityClass,
            "performHttpRequestBridge",
            "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[B)Lapp/sloppatv/SloppaNativeActivity$HttpResult;"
        )
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

    jstring jMethod = toJString(env, method);
    jstring jUrl = toJString(env, url);
    jobjectArray jHeaders = env->NewObjectArray(static_cast<jsize>(headers.size() * 2), stringClass, nullptr);
    jsize headerIndex = 0;
    for (const auto& [key, value] : headers) {
        jstring jKey = toJString(env, key);
        jstring jValue = toJString(env, value);
        env->SetObjectArrayElement(jHeaders, headerIndex++, jKey);
        env->SetObjectArrayElement(jHeaders, headerIndex++, jValue);
        env->DeleteLocalRef(jKey);
        env->DeleteLocalRef(jValue);
    }

    jbyteArray jBody = env->NewByteArray(static_cast<jsize>(body.size()));
    if (jBody && !body.empty()) {
        env->SetByteArrayRegion(
            jBody,
            0,
            static_cast<jsize>(body.size()),
            reinterpret_cast<const jbyte*>(body.data())
        );
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

    jobject result = env->CallObjectMethod(activity_, performRequest, jMethod, jUrl, jHeaders, jBody);
    if (clearException(env, "HTTP bridge request", response.error) || !result) {
        const std::string safeUrl = requestUrlForLog(url);
        __android_log_print(
            ANDROID_LOG_ERROR,
            kTag,
            "%s %s failed before HTTP status: %s",
            method.c_str(),
            safeUrl.c_str(),
            response.error.c_str()
        );
    } else {
        jclass resultClass = env->GetObjectClass(result);
        jfieldID statusField = resultClass ? env->GetFieldID(resultClass, "status", "I") : nullptr;
        jfieldID bodyField = resultClass ? env->GetFieldID(resultClass, "body", "[B") : nullptr;
        jfieldID errorField = resultClass ? env->GetFieldID(resultClass, "error", "Ljava/lang/String;") : nullptr;
        if (!clearException(env, "HTTP bridge result fields", response.error)
            && statusField && bodyField && errorField) {
            response.status = env->GetIntField(result, statusField);
            auto responseBytes = static_cast<jbyteArray>(env->GetObjectField(result, bodyField));
            auto errorText = static_cast<jstring>(env->GetObjectField(result, errorField));

            if (responseBytes) {
                const jsize length = env->GetArrayLength(responseBytes);
                if (length > 0) {
                    response.body.resize(static_cast<size_t>(length));
                    env->GetByteArrayRegion(
                        responseBytes,
                        0,
                        length,
                        reinterpret_cast<jbyte*>(response.body.data())
                    );
                }
                env->DeleteLocalRef(responseBytes);
            }
            if (errorText) {
                const std::string detail = jniString(env, errorText);
                if (!detail.empty()) {
                    response.error = userFacingJavaHttpError("HTTP request", detail);
                    __android_log_print(ANDROID_LOG_ERROR, kTag, "%s (%s)", response.error.c_str(), detail.c_str());
                }
                env->DeleteLocalRef(errorText);
            }
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
