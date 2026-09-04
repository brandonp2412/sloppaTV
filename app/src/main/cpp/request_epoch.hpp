#pragma once

#include <atomic>
#include <cstdint>

class RequestEpoch {
public:
    [[nodiscard]] uint64_t begin() { return value_.fetch_add(1, std::memory_order_relaxed) + 1; }
    [[nodiscard]] uint64_t snapshot() const { return value_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool active(uint64_t token) const { return token == snapshot(); }
    void invalidate() { value_.fetch_add(1, std::memory_order_relaxed); }

private:
    std::atomic<uint64_t> value_{0};
};

struct RequestEpochs {
    RequestEpoch auth;
    RequestEpoch home;
    RequestEpoch search;
    RequestEpoch content;
    RequestEpoch playback;
    RequestEpoch session;

    void invalidateTransient() {
        auth.invalidate();
        home.invalidate();
        search.invalidate();
        content.invalidate();
        playback.invalidate();
    }

    void invalidateAll() {
        invalidateTransient();
        session.invalidate();
    }
};
