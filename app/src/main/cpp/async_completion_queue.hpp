#pragma once

#include <mutex>
#include <utility>
#include <vector>

template <typename Event> class AsyncCompletionQueue {
public:
    void push(Event event) {
        std::scoped_lock lock(mutex_);
        events_.push_back(std::move(event));
    }

    [[nodiscard]] std::vector<Event> takeAll() {
        std::scoped_lock lock(mutex_);
        std::vector<Event> result;
        result.swap(events_);
        return result;
    }

private:
    std::mutex mutex_;
    std::vector<Event> events_;
};
