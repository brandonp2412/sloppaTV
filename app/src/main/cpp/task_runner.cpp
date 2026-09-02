#include "task_runner.hpp"

#include <algorithm>
#include <exception>
#include <utility>

TaskRunner::TaskRunner(
    size_t workerCount,
    std::function<void()> onTaskComplete,
    std::function<void(const std::string&)> onTaskError
) : onTaskComplete_(std::move(onTaskComplete)),
    onTaskError_(std::move(onTaskError)) {
    workerCount = std::clamp<size_t>(workerCount, 1, 8);
    workers_.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back(&TaskRunner::workerLoop, this);
    }
}

TaskRunner::~TaskRunner() {
    shutdown();
}

bool TaskRunner::submit(std::function<void()> task) {
    if (!task) return false;
    {
        std::scoped_lock lock(mutex_);
        if (stopping_) return false;
        queue_.push_back(std::move(task));
    }
    condition_.notify_one();
    return true;
}

void TaskRunner::shutdown() {
    {
        std::scoped_lock lock(mutex_);
        if (stopping_ && workers_.empty()) return;
        stopping_ = true;
        queue_.clear();
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

void TaskRunner::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        try {
            task();
        } catch (const std::exception& error) {
            if (onTaskError_) onTaskError_(error.what());
        } catch (...) {
            if (onTaskError_) onTaskError_("Unknown background task exception");
        }
        if (onTaskComplete_) onTaskComplete_();
    }
}
