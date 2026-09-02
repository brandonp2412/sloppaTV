#include "task_runner.hpp"

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
        }
    );

    assert(runner.submit([] { throw std::runtime_error("boom"); }));
    assert(runner.submit([&] { ++taskRuns; }));

    {
        std::unique_lock lock(mutex);
        const bool finished = completed.wait_for(lock, std::chrono::seconds(2), [&] {
            return completionCount == 2;
        });
        assert(finished);
    }
    assert(taskRuns == 1);
    assert(lastError == "boom");

    assert(runner.submit([] { throw 42; }));
    {
        std::unique_lock lock(mutex);
        const bool finished = completed.wait_for(lock, std::chrono::seconds(2), [&] {
            return completionCount == 3;
        });
        assert(finished);
    }
    assert(lastError == "Unknown background task exception");

    runner.shutdown();
    assert(!runner.submit([] {}));
    return 0;
}
