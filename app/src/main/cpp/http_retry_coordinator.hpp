#pragma once

#include "http_response.hpp"
#include "http_retry_policy.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

class HttpRetryCoordinator {
public:
    using RetryDelays = std::array<std::chrono::milliseconds, 2>;

    HttpRetryCoordinator()
        : HttpRetryCoordinator(RetryDelays{std::chrono::milliseconds{250}, std::chrono::milliseconds{750}}) {}

    explicit HttpRetryCoordinator(RetryDelays retryDelays) : retryDelays_(retryDelays) {}

    template <typename Fetch, typename OnRetry>
    HttpResponse request(std::string_view method, Fetch&& fetch, OnRetry&& onRetry) {
        HttpResponse response;
        const uint64_t generation = cancelGeneration_.load(std::memory_order_relaxed);
        const size_t retryCount = transientHttpRetryCount(method);
        for (size_t attempt = 0; attempt <= retryCount; ++attempt) {
            if (cancelled(generation)) return cancelledResponse();

            response = fetch(generation);
            // fetch() may call cancelPending() or race with it on another thread.
            // cppcheck-suppress identicalConditionAfterEarlyExit
            if (cancelled(generation)) return cancelledResponse();
            const bool retryable = shouldRetryTransientHttpResponse(method, response.status, !response.error.empty());
            if (!retryable || attempt == retryCount) return response;

            const auto delay = retryDelays_[attempt];
            onRetry(response, delay);
            std::unique_lock retryLock(retryMutex_);
            if (retryWake_.wait_for(retryLock, delay, [&] { return cancelled(generation); })) {
                return cancelledResponse();
            }
        }
        return response;
    }

    void cancelPending() {
        cancelGeneration_.fetch_add(1, std::memory_order_relaxed);
        retryWake_.notify_all();
    }

    bool cancelled(uint64_t generation) const {
        return cancelGeneration_.load(std::memory_order_relaxed) != generation;
    }

private:
    static HttpResponse cancelledResponse() {
        HttpResponse response;
        response.error = "Request cancelled";
        return response;
    }

    RetryDelays retryDelays_;
    std::atomic<uint64_t> cancelGeneration_{0};
    std::mutex retryMutex_;
    std::condition_variable retryWake_;
};
