#include "http_get_coordinator.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace {
HttpResponse ok(std::string body) {
    HttpResponse response;
    response.status = 200;
    response.body = std::move(body);
    return response;
}
} // namespace

int main() {
    assert(httpGetCacheKey("https://example/items", {{"A", "1"}, {"B", "2"}}) ==
           "https://example/items\nA:1\nB:2");

    HttpGetCoordinator cached;
    int cacheFetches = 0;
    const auto first = cached.request("cached", true, [&] {
        ++cacheFetches;
        return ok("first");
    });
    const auto second = cached.request("cached", true, [&] {
        ++cacheFetches;
        return ok("second");
    });
    assert(first.body == "first");
    assert(second.body == "first");
    assert(cacheFetches == 1);

    cached.invalidate();
    const auto refreshed = cached.request("cached", true, [&] {
        ++cacheFetches;
        return ok("refreshed");
    });
    assert(refreshed.body == "refreshed");
    assert(cacheFetches == 2);

    HttpGetCoordinator uncached;
    int uncachedFetches = 0;
    assert(uncached.request("uncached", false, [&] {
               ++uncachedFetches;
               return ok("one");
           }).body == "one");
    assert(uncached.request("uncached", false, [&] {
               ++uncachedFetches;
               return ok("two");
           }).body == "two");
    assert(uncachedFetches == 2);

    HttpGetCoordinator deduplicated;
    std::mutex dedupMutex;
    std::condition_variable dedupCondition;
    bool ownerStarted = false;
    bool releaseOwner = false;
    std::atomic<int> dedupFetches{0};
    HttpResponse ownerResponse;
    HttpResponse joinedResponse;

    std::thread owner([&] {
        ownerResponse = deduplicated.request("shared", false, [&] {
            ++dedupFetches;
            std::unique_lock lock(dedupMutex);
            ownerStarted = true;
            dedupCondition.notify_all();
            dedupCondition.wait(lock, [&] { return releaseOwner; });
            return ok("shared");
        });
    });
    {
        std::unique_lock lock(dedupMutex);
        dedupCondition.wait(lock, [&] { return ownerStarted; });
    }

    std::atomic<bool> joinerStarted{false};
    std::thread joiner([&] {
        joinerStarted = true;
        joinedResponse = deduplicated.request("shared", false, [&] {
            ++dedupFetches;
            return ok("duplicate");
        });
    });
    while (!joinerStarted.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(dedupFetches.load() == 1);
    {
        std::scoped_lock lock(dedupMutex);
        releaseOwner = true;
    }
    dedupCondition.notify_all();
    owner.join();
    joiner.join();
    assert(ownerResponse.body == "shared");
    assert(joinedResponse.body == "shared");
    assert(dedupFetches.load() == 1);

    HttpGetCoordinator generations;
    std::mutex generationMutex;
    std::condition_variable generationCondition;
    bool oldStarted = false;
    bool releaseOld = false;
    std::atomic<int> generationFetches{0};
    HttpResponse oldResponse;

    std::thread oldRequest([&] {
        oldResponse = generations.request("generation", true, [&] {
            ++generationFetches;
            std::unique_lock lock(generationMutex);
            oldStarted = true;
            generationCondition.notify_all();
            generationCondition.wait(lock, [&] { return releaseOld; });
            return ok("old");
        });
    });
    {
        std::unique_lock lock(generationMutex);
        generationCondition.wait(lock, [&] { return oldStarted; });
    }

    generations.invalidate();
    const auto current = generations.request("generation", true, [&] {
        ++generationFetches;
        return ok("current");
    });
    assert(current.body == "current");
    assert(generationFetches.load() == 2);

    {
        std::scoped_lock lock(generationMutex);
        releaseOld = true;
    }
    generationCondition.notify_all();
    oldRequest.join();
    assert(oldResponse.body == "old");

    const auto currentCached = generations.request("generation", true, [&] {
        ++generationFetches;
        return ok("unexpected");
    });
    assert(currentCached.body == "current");
    assert(generationFetches.load() == 2);

    return 0;
}
