#pragma once

#include <jni.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct HttpResponse {
    int status = 0;
    std::string body;
    std::string error;

    [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
};

class JniHttpClient {
public:
    explicit JniHttpClient(JavaVM* vm) : vm_(vm) {}

    HttpResponse request(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers = {},
        const std::string& body = {}
    ) const;
    void invalidateGetCache() const;
    void cancelPending() const;

private:
    struct CacheEntry {
        HttpResponse response;
        std::chrono::steady_clock::time_point expiresAt{};
    };

    struct InFlightRequest {
        std::condition_variable completed;
        uint64_t generation = 0;
        bool done = false;
        HttpResponse response;
    };

    HttpResponse requestWithRetry(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body
    ) const;
    std::string getCacheKey(
        const std::string& url,
        const std::map<std::string, std::string>& headers
    ) const;
    HttpResponse requestOnce(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body
    ) const;

    JavaVM* vm_ = nullptr;
    mutable std::mutex cacheMutex_;
    mutable std::unordered_map<std::string, CacheEntry> getCache_;
    mutable std::unordered_map<std::string, std::shared_ptr<InFlightRequest>> inFlightGets_;
    mutable uint64_t cacheGeneration_ = 0;
    mutable std::atomic<uint64_t> cancelGeneration_{0};
    mutable std::mutex retryMutex_;
    mutable std::condition_variable retryWake_;
};
