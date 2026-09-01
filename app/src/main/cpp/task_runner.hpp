#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class TaskRunner {
public:
    explicit TaskRunner(size_t workerCount = 4, std::function<void()> onTaskComplete = {});
    ~TaskRunner();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    bool submit(std::function<void()> task);
    void shutdown();

private:
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    std::function<void()> onTaskComplete_;
    bool stopping_ = false;
};
