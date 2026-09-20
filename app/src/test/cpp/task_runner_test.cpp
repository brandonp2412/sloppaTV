#include "task_runner.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>

int main() {
    std::mutex mutex;
    std::condition_variable completed;
    int completionCount = 0;
    int taskRuns = 0;
    std::string lastError;

    TaskRunner runner(
        1,
        [&] {
            std::scoped_lock lock(mutex);
            ++completionCount;
            completed.notify_all();
        },
        [&](const std::string& error) {
            std::scoped_lock lock(mutex);
            lastError = error;
        });

    assert(runner.submit([] { throw std::runtime_error("boom"); }));
    assert(runner.submit([&] { ++taskRuns; }));

    {
        std::unique_lock lock(mutex);
        const bool finished = completed.wait_for(lock, std::chrono::seconds(2), [&] { return completionCount == 2; });
        assert(finished);
    }
    assert(taskRuns == 1);
    assert(lastError == "boom");

    assert(runner.submit([] { throw 42; }));
    {
        std::unique_lock lock(mutex);
        const bool finished = completed.wait_for(lock, std::chrono::seconds(2), [&] { return completionCount == 3; });
        assert(finished);
    }
    assert(lastError == "Unknown background task exception");

    runner.shutdown();
    assert(!runner.submit([] {}));

    {
        std::mutex callbackMutex;
        std::condition_variable callbackProgress;
        bool secondTaskRan = false;
        std::atomic<int> completionAttempts{0};
        std::atomic<int> errorReports{0};
        TaskRunner callbackSafe(
            1,
            [&] {
                ++completionAttempts;
                throw std::runtime_error("completion boom");
            },
            [&](const std::string&) { ++errorReports; });

        assert(callbackSafe.submit([] {}));
        assert(callbackSafe.submit([&] {
            std::scoped_lock lock(callbackMutex);
            secondTaskRan = true;
            callbackProgress.notify_all();
        }));
        {
            std::unique_lock lock(callbackMutex);
            const bool finished =
                callbackProgress.wait_for(lock, std::chrono::seconds(2), [&] { return secondTaskRan; });
            assert(finished);
        }
        callbackSafe.shutdown();
        assert(completionAttempts.load() == 2);
        assert(errorReports.load() == 2);
    }

    {
        std::mutex errorMutex;
        std::condition_variable errorProgress;
        bool survivorRan = false;
        std::atomic<int> errorAttempts{0};
        TaskRunner errorSafe(1, {}, [&](const std::string&) {
            ++errorAttempts;
            throw std::runtime_error("error callback boom");
        });

        assert(errorSafe.submit([] { throw std::runtime_error("task boom"); }));
        assert(errorSafe.submit([&] {
            std::scoped_lock lock(errorMutex);
            survivorRan = true;
            errorProgress.notify_all();
        }));
        {
            std::unique_lock lock(errorMutex);
            const bool finished = errorProgress.wait_for(lock, std::chrono::seconds(2), [&] { return survivorRan; });
            assert(finished);
        }
        errorSafe.shutdown();
        assert(errorAttempts.load() == 1);
    }

    return 0;
}
