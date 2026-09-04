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
#include <vector>

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

std::string readStream(JNIEnv* env, jobject stream, std::string& error) {
    if (!stream) return {};

    jclass inputClass = env->FindClass("java/io/InputStream");
    if (clearException(env, "FindClass(InputStream)", error) || !inputClass) return {};
    jmethodID readMethod = env->GetMethodID(inputClass, "read", "([B)I");
    jmethodID closeMethod = env->GetMethodID(inputClass, "close", "()V");
    if (clearException(env, "InputStream methods", error) || !readMethod) {
        env->DeleteLocalRef(inputClass);
        return {};
    }

    jbyteArray buffer = env->NewByteArray(16 * 1024);
    std::string result;
    std::vector<jbyte> temp(16 * 1024);

    while (true) {
        const jint count = env->CallIntMethod(stream, readMethod, buffer);
        if (clearException(env, "InputStream.read", error)) break;
        if (count <= 0) break;
        env->GetByteArrayRegion(buffer, 0, count, temp.data());
        if (clearException(env, "GetByteArrayRegion", error)) break;
        result.append(reinterpret_cast<const char*>(temp.data()), static_cast<size_t>(count));
    }

    if (closeMethod) {
        env->CallVoidMethod(stream, closeMethod);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    env->DeleteLocalRef(buffer);
    env->DeleteLocalRef(inputClass);
    return result;
}
}  // namespace

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
    if (!env) {
        response.error = "Unable to attach native HTTP thread to JVM";
        return response;
    }

    jclass urlClass = env->FindClass("java/net/URL");
    jclass connectionClass = env->FindClass("java/net/HttpURLConnection");
    if (clearException(env, "HTTP classes", response.error) || !urlClass || !connectionClass) return response;

    jmethodID urlCtor = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
    jmethodID openConnection = env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
    jmethodID setMethod = env->GetMethodID(connectionClass, "setRequestMethod", "(Ljava/lang/String;)V");
    jmethodID setProperty = env->GetMethodID(connectionClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID setConnectTimeout = env->GetMethodID(connectionClass, "setConnectTimeout", "(I)V");
    jmethodID setReadTimeout = env->GetMethodID(connectionClass, "setReadTimeout", "(I)V");
    jmethodID setDoOutput = env->GetMethodID(connectionClass, "setDoOutput", "(Z)V");
    jmethodID setFollowRedirects = env->GetMethodID(connectionClass, "setInstanceFollowRedirects", "(Z)V");
    jmethodID getOutputStream = env->GetMethodID(connectionClass, "getOutputStream", "()Ljava/io/OutputStream;");
    jmethodID getResponseCode = env->GetMethodID(connectionClass, "getResponseCode", "()I");
    jmethodID getInputStream = env->GetMethodID(connectionClass, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID getErrorStream = env->GetMethodID(connectionClass, "getErrorStream", "()Ljava/io/InputStream;");
    jmethodID disconnect = env->GetMethodID(connectionClass, "disconnect", "()V");

    if (clearException(env, "HTTP method lookup", response.error) || !urlCtor || !openConnection || !setMethod || !getResponseCode) {
        env->DeleteLocalRef(urlClass);
        env->DeleteLocalRef(connectionClass);
        return response;
    }

    jstring jUrl = toJString(env, url);
    jobject urlObject = env->NewObject(urlClass, urlCtor, jUrl);
    if (clearException(env, "URL constructor", response.error) || !urlObject) {
        env->DeleteLocalRef(jUrl);
        env->DeleteLocalRef(urlClass);
        env->DeleteLocalRef(connectionClass);
        return response;
    }

    jobject connection = env->CallObjectMethod(urlObject, openConnection);
    if (clearException(env, "URL.openConnection", response.error) || !connection) {
        env->DeleteLocalRef(urlObject);
        env->DeleteLocalRef(jUrl);
        env->DeleteLocalRef(urlClass);
        env->DeleteLocalRef(connectionClass);
        return response;
    }

    env->CallVoidMethod(connection, setConnectTimeout, 10000);
    env->CallVoidMethod(connection, setReadTimeout, 30000);
    env->CallVoidMethod(connection, setFollowRedirects, JNI_TRUE);

    jstring jMethod = toJString(env, method);
    env->CallVoidMethod(connection, setMethod, jMethod);
    env->DeleteLocalRef(jMethod);

    for (const auto& [key, value] : headers) {
        jstring jKey = toJString(env, key);
        jstring jValue = toJString(env, value);
        env->CallVoidMethod(connection, setProperty, jKey, jValue);
        env->DeleteLocalRef(jKey);
        env->DeleteLocalRef(jValue);
    }
    if (clearException(env, "HTTP request setup", response.error)) goto cleanup;

    if (!body.empty()) {
        env->CallVoidMethod(connection, setDoOutput, JNI_TRUE);
        jobject output = env->CallObjectMethod(connection, getOutputStream);
        if (clearException(env, "getOutputStream", response.error) || !output) goto cleanup;

        jclass outputClass = env->FindClass("java/io/OutputStream");
        jmethodID writeMethod = env->GetMethodID(outputClass, "write", "([B)V");
        jmethodID closeMethod = env->GetMethodID(outputClass, "close", "()V");
        jbyteArray bytes = env->NewByteArray(static_cast<jsize>(body.size()));
        env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(body.size()), reinterpret_cast<const jbyte*>(body.data()));
        env->CallVoidMethod(output, writeMethod, bytes);
        if (closeMethod) env->CallVoidMethod(output, closeMethod);
        clearException(env, "OutputStream.write", response.error);
        env->DeleteLocalRef(bytes);
        env->DeleteLocalRef(outputClass);
        env->DeleteLocalRef(output);
        if (!response.error.empty()) goto cleanup;
    }

    response.status = env->CallIntMethod(connection, getResponseCode);
    if (clearException(env, "getResponseCode", response.error)) {
        const std::string safeUrl = requestUrlForLog(url);
        __android_log_print(
            ANDROID_LOG_ERROR,
            kTag,
            "%s %s failed before HTTP status: %s",
            method.c_str(),
            safeUrl.c_str(),
            response.error.c_str()
        );
        goto cleanup;
    }

    {
        jobject stream = nullptr;
        if (response.status >= 400 && getErrorStream) {
            stream = env->CallObjectMethod(connection, getErrorStream);
        } else if (getInputStream) {
            stream = env->CallObjectMethod(connection, getInputStream);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            stream = nullptr;
        }
        if (stream) {
            response.body = readStream(env, stream, response.error);
            env->DeleteLocalRef(stream);
        }
    }

cleanup:
    if (connection && disconnect) {
        env->CallVoidMethod(connection, disconnect);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    env->DeleteLocalRef(connection);
    env->DeleteLocalRef(urlObject);
    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(urlClass);
    env->DeleteLocalRef(connectionClass);
    return response;
}
