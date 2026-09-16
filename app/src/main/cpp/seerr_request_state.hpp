#pragma once

#include "seerr_media.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class SeerrRequestState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto kPendingRefreshInterval = std::chrono::seconds(60);
    static constexpr auto kOptimisticPendingWindow = std::chrono::minutes(5);
    static constexpr auto kPostRequestRefreshDelay = std::chrono::seconds(2);

    [[nodiscard]] bool pendingLoading() const { return pendingLoading_; }
    [[nodiscard]] TimePoint pendingRefreshDeadline() const { return pendingRefreshAt_; }
    [[nodiscard]] bool pendingRefreshDue(TimePoint now) const {
        return !pendingLoading_ && pendingRefreshAt_ != TimePoint{} && now >= pendingRefreshAt_;
    }
    void clearPendingRefreshDeadline() { pendingRefreshAt_ = {}; }

    bool beginPendingRefresh() {
        if (pendingLoading_) return false;
        pendingLoading_ = true;
        return true;
    }

    void resetPending() {
        pendingLoading_ = false;
        pendingRefreshAt_ = {};
        optimisticPendingUntil_ = {};
        pending_.clear();
    }

    void invalidatePendingRefresh(TimePoint now) {
        pendingLoading_ = false;
        pendingRefreshAt_ = now;
    }

    void failPendingRefresh(TimePoint now) {
        pendingLoading_ = false;
        pendingRefreshAt_ = now + kPendingRefreshInterval;
    }

    void finishPendingRefresh(std::vector<SeerrMediaItem> refreshed, TimePoint now) {
        pendingLoading_ = false;
        pendingRefreshAt_ = now + kPendingRefreshInterval;
        if (now < optimisticPendingUntil_) {
            for (const auto& local : pending_) upsert(refreshed, local);
        } else {
            optimisticPendingUntil_ = {};
        }
        pending_ = std::move(refreshed);
    }

    [[nodiscard]] const std::vector<SeerrMediaItem>& pending() const { return pending_; }

    [[nodiscard]] const SeerrMediaItem* findPending(std::string_view id) const {
        const auto found = std::find_if(pending_.begin(), pending_.end(), [&](const SeerrMediaItem& item) {
            return item.id == id;
        });
        return found == pending_.end() ? nullptr : &*found;
    }

    void markRequestSucceeded(
        SeerrMediaItem item,
        int requestId,
        std::string status,
        TimePoint now
    ) {
        item.requested = true;
        item.requestId = requestId;
        item.mediaStatus = 2;
        item.status = std::move(status);
        const auto found = std::find_if(pending_.begin(), pending_.end(), [&](const SeerrMediaItem& candidate) {
            return sameRequest(candidate, item);
        });
        if (found == pending_.end()) pending_.insert(pending_.begin(), std::move(item));
        else *found = std::move(item);
        optimisticPendingUntil_ = now + kOptimisticPendingWindow;
        pendingRefreshAt_ = now + kPostRequestRefreshDelay;
    }

    [[nodiscard]] bool mergeRequestedSearch(const std::vector<SeerrMediaItem>& results, TimePoint now) {
        bool discovered = false;
        for (const auto& item : results) {
            if (!item.requested) continue;
            upsert(pending_, item);
            discovered = true;
        }
        if (discovered) optimisticPendingUntil_ = now + kOptimisticPendingWindow;
        return discovered;
    }

    void erasePending(std::string_view id, int requestId) {
        std::erase_if(pending_, [&](const SeerrMediaItem& item) {
            return item.id == id || (requestId > 0 && item.requestId == requestId);
        });
    }

private:
    static bool sameRequest(const SeerrMediaItem& left, const SeerrMediaItem& right) {
        return left.id == right.id
            || (left.requestId > 0 && right.requestId > 0 && left.requestId == right.requestId);
    }

    template <typename Item>
    static void upsert(std::vector<SeerrMediaItem>& items, Item&& item) {
        const auto found = std::find_if(items.begin(), items.end(), [&](const SeerrMediaItem& candidate) {
            return sameRequest(candidate, item);
        });
        if (found == items.end()) items.push_back(std::forward<Item>(item));
        else *found = std::forward<Item>(item);
    }

    std::vector<SeerrMediaItem> pending_;
    bool pendingLoading_ = false;
    TimePoint pendingRefreshAt_{};
    TimePoint optimisticPendingUntil_{};
};
