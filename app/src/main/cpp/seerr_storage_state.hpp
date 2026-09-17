#pragma once

#include "seerr_media.hpp"
#include "seerr_storage.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class SeerrStorageState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto kRefreshInterval = std::chrono::seconds(60);

    enum class PickerStatus {
        Ready,
        Loading,
        Unavailable,
    };

    struct Selection {
        SeerrMediaItem item;
        SeerrStorageTarget target;
    };

    [[nodiscard]] const std::vector<SeerrStorageTarget>& targets() const { return targets_; }
    [[nodiscard]] bool empty() const { return targets_.empty(); }
    [[nodiscard]] bool loading() const { return loading_; }
    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] TimePoint refreshDeadline() const { return refreshAt_; }
    [[nodiscard]] const std::optional<SeerrMediaItem>& pendingRequest() const { return pendingRequest_; }
    [[nodiscard]] const std::vector<SeerrStorageTarget>& driveChoices() const { return driveChoices_; }
    [[nodiscard]] int driveSelection() const { return driveSelection_; }

    void clearTargets() { targets_.clear(); }
    void clearRefreshDeadline() { refreshAt_ = {}; }

    void resetUnavailable() {
        targets_.clear();
        loading_ = false;
        refreshAt_ = {};
    }

    bool beginRefresh(bool force, TimePoint now) {
        if (loading_) return false;
        if (!force && refreshAt_ != TimePoint{} && now < refreshAt_) return false;
        loading_ = true;
        return true;
    }

    void invalidateRefresh() { loading_ = false; }

    void failRefresh(std::string error) {
        loading_ = false;
        refreshAt_ = {};
        error_ = std::move(error);
    }

    void finishRefresh(std::vector<SeerrStorageTarget> targets, TimePoint now) {
        loading_ = false;
        refreshAt_ = now + kRefreshInterval;
        error_.clear();
        targets_ = std::move(targets);
    }

    PickerStatus preparePicker(const SeerrMediaItem& item) {
        driveChoices_.clear();
        for (const auto& target : targets_) {
            if (target.mediaType == item.mediaType) driveChoices_.push_back(target);
        }
        if (driveChoices_.empty()) {
            if (loading_) {
                pendingRequest_ = item;
                return PickerStatus::Loading;
            }
            pendingRequest_.reset();
            return PickerStatus::Unavailable;
        }
        std::stable_sort(driveChoices_.begin(), driveChoices_.end(),
                         [](const auto& left, const auto& right) { return left.isDefault > right.isDefault; });
        pendingRequest_ = item;
        driveSelection_ = 0;
        return PickerStatus::Ready;
    }

    void clearPendingRequest() { pendingRequest_.reset(); }

    [[nodiscard]] std::optional<SeerrMediaItem> takePendingRequest() {
        if (!pendingRequest_) return std::nullopt;
        std::optional<SeerrMediaItem> item = std::move(pendingRequest_);
        pendingRequest_.reset();
        return item;
    }

    void cancelPicker() {
        pendingRequest_.reset();
        driveChoices_.clear();
    }

    void moveSelection(int delta) {
        if (driveChoices_.empty()) return;
        driveSelection_ = std::clamp(driveSelection_, 0, static_cast<int>(driveChoices_.size()) - 1);
        driveSelection_ = std::clamp(driveSelection_ + delta, 0, static_cast<int>(driveChoices_.size()) - 1);
    }

    [[nodiscard]] std::optional<Selection> takeSelection() {
        if (!pendingRequest_ || driveChoices_.empty()) return std::nullopt;
        driveSelection_ = std::clamp(driveSelection_, 0, static_cast<int>(driveChoices_.size()) - 1);
        Selection selected{
            .item = *pendingRequest_,
            .target = driveChoices_[static_cast<size_t>(driveSelection_)],
        };
        pendingRequest_.reset();
        driveChoices_.clear();
        return selected;
    }

private:
    std::vector<SeerrStorageTarget> targets_;
    bool loading_ = false;
    std::string error_;
    TimePoint refreshAt_{};
    std::optional<SeerrMediaItem> pendingRequest_;
    std::vector<SeerrStorageTarget> driveChoices_;
    int driveSelection_ = 0;
};
