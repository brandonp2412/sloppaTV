#pragma once

#include "http_get_coordinator.hpp"
#include "http_response.hpp"

#include <jni.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>

class JniHttpClient {
public:
    JniHttpClient(JavaVM* vm, jobject activity);
    ~JniHttpClient();

    HttpResponse request(const std::string& method, const std::string& url,
                         const std::map<std::string, std::string>& headers = {}, const std::string& body = {}) const;
    void invalidateGetCache() const;
    void cancelPending() const;

private:
    HttpResponse requestWithRetry(const std::string& method, const std::string& url,
                                  const std::map<std::string, std::string>& headers, const std::string& body) const;
    HttpResponse requestOnce(const std::string& method, const std::string& url,
                             const std::map<std::string, std::string>& headers, const std::string& body,
                             uint64_t requestId, uint64_t generation) const;
    bool registerRequest(uint64_t requestId) const;
    void unregisterRequest(uint64_t requestId) const;
    void cancelRequest(uint64_t requestId) const;

    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    mutable HttpGetCoordinator getCoordinator_;
    mutable std::atomic<uint64_t> cancelGeneration_{0};
    mutable std::mutex retryMutex_;
    mutable std::condition_variable retryWake_;
    mutable std::mutex activeRequestsMutex_;
    mutable std::unordered_set<uint64_t> activeRequestIds_;
};
