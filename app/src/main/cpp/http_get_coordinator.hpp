#pragma once

#include "http_cache_policy.hpp"
#include "http_response.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

inline std::string httpGetCacheKey(const std::string& url, const std::map<std::string, std::string>& headers) {
    std::ostringstream key;
    key << url;
    for (const auto& [name, value] : headers) key << '\n' << name << ':' << value;
    return key.str();
}

class HttpGetCoordinator {
public:
    template <typename Fetch>
    HttpResponse request(const std::string& key, bool cacheable, Fetch&& fetch) {
        std::shared_ptr<InFlightRequest> inFlight;
        bool owner = false;
        uint64_t requestGeneration = 0;
        {
            std::unique_lock lock(mutex_);
            requestGeneration = generation_;
            if (cacheable) {
                pruneExpiredLocked(std::chrono::steady_clock::now());
                const auto cached = cache_.find(key);
                if (cached != cache_.end()) return cached->second.response;
            }

            const auto pending = inFlight_.find(key);
            if (pending != inFlight_.end() &&
                shouldJoinInFlightApiGet(requestGeneration, pending->second->generation)) {
                inFlight = pending->second;
            } else {
                inFlight = std::make_shared<InFlightRequest>();
                inFlight->generation = requestGeneration;
                inFlight_[key] = inFlight;
                owner = true;
            }
        }

        if (!owner) {
            std::unique_lock lock(mutex_);
            inFlight->completed.wait(lock, [&] { return inFlight->done; });
            return inFlight->response;
        }

        HttpResponse response = std::forward<Fetch>(fetch)();
        {
            std::scoped_lock lock(mutex_);
            if (cacheable && response.ok() && requestGeneration == generation_) {
                const auto now = std::chrono::steady_clock::now();
                pruneExpiredLocked(now);
                if (cache_.size() >= kMaxApiGetCacheEntries) {
                    const auto oldest =
                        std::min_element(cache_.begin(), cache_.end(), [](const auto& left, const auto& right) {
                            return left.second.expiresAt < right.second.expiresAt;
                        });
                    if (oldest != cache_.end()) cache_.erase(oldest);
                }
                cache_[key] = CacheEntry{response, now + std::chrono::seconds(5)};
            }

            inFlight->response = response;
            inFlight->done = true;
            const auto pending = inFlight_.find(key);
            if (pending != inFlight_.end() && pending->second == inFlight) inFlight_.erase(pending);
        }
        inFlight->completed.notify_all();
        return response;
    }

    void invalidate() {
        std::scoped_lock lock(mutex_);
        cache_.clear();
        ++generation_;
    }

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

    void pruneExpiredLocked(std::chrono::steady_clock::time_point now) {
        std::erase_if(cache_, [&](const auto& entry) { return entry.second.expiresAt <= now; });
    }

    std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::unordered_map<std::string, std::shared_ptr<InFlightRequest>> inFlight_;
    uint64_t generation_ = 0;
};
