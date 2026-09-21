#include "http_retry_coordinator.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace {
HttpResponse response(int status) {
    HttpResponse result;
    result.status = status;
    return result;
}

HttpResponse response(int status, std::string error) {
    HttpResponse result = response(status);
    result.error = std::move(error);
    return result;
}
} // namespace

int main() {
    using namespace std::chrono_literals;

    HttpRetryCoordinator coordinator({1ms, 1ms});

    int getAttempts = 0;
    int retryNotices = 0;
    const HttpResponse recovered = coordinator.request(
        "GET",
        [&](uint64_t) {
            ++getAttempts;
            return getAttempts == 1 ? response(0, "temporary transport error") : response(200);
        },
        [&](const HttpResponse& failed, std::chrono::milliseconds delay) {
            ++retryNotices;
            assert(failed.status == 0);
            assert(failed.error == "temporary transport error");
            assert(delay == 1ms);
        });
    assert(recovered.status == 200);
    assert(recovered.error.empty());
    assert(getAttempts == 2);
    assert(retryNotices == 1);

    int postAttempts = 0;
    const HttpResponse post = coordinator.request(
        "POST",
        [&](uint64_t) {
            ++postAttempts;
            return response(503);
        },
        [&](const HttpResponse&, std::chrono::milliseconds) { assert(false); });
    assert(post.status == 503);
    assert(postAttempts == 1);

    HttpRetryCoordinator cancellable({10s, 10s});
    std::mutex mutex;
    std::condition_variable retryStarted;
    bool waitingToRetry = false;
    std::atomic<int> cancelAttempts{0};
    HttpResponse cancelled;

    std::thread request([&] {
        cancelled = cancellable.request(
            "GET",
            [&](uint64_t) {
                ++cancelAttempts;
                return response(0, "offline");
            },
            [&](const HttpResponse&, std::chrono::milliseconds) {
                {
                    std::scoped_lock lock(mutex);
                    waitingToRetry = true;
                }
                retryStarted.notify_one();
            });
    });

    {
        std::unique_lock lock(mutex);
        retryStarted.wait(lock, [&] { return waitingToRetry; });
    }
    cancellable.cancelPending();
    request.join();
    assert(cancelled.status == 0);
    assert(cancelled.error == "Request cancelled");
    assert(cancelAttempts.load() == 1);

    int afterCancelAttempts = 0;
    const HttpResponse afterCancel = cancellable.request(
        "GET",
        [&](uint64_t) {
            ++afterCancelAttempts;
            return response(200);
        },
        [](const HttpResponse&, std::chrono::milliseconds) {});
    assert(afterCancel.status == 200);
    assert(afterCancelAttempts == 1);

    return 0;
}
