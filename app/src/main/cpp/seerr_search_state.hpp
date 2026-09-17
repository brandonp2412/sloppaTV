#pragma once

#include "seerr_media.hpp"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

class SeerrSearchState {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kDebounceDelay = std::chrono::milliseconds(550);
    static constexpr size_t kMinQueryBytes = 3;

    struct BeginResult {
        bool started = false;
        bool resultsChanged = false;
    };

    void reset() {
        results_.clear();
        loading_ = false;
        error_.clear();
        debouncePending_ = false;
        debounceDeadline_ = {};
        query_.clear();
    }

    [[nodiscard]] bool loading() const { return loading_; }

    [[nodiscard]] const std::string& error() const { return error_; }

    [[nodiscard]] bool debouncePending() const { return debouncePending_; }

    [[nodiscard]] Clock::time_point debounceDeadline() const { return debounceDeadline_; }

    [[nodiscard]] const std::string& query() const { return query_; }

    [[nodiscard]] const std::vector<SeerrMediaItem>& results() const { return results_; }

    void setLoading(bool loading) { loading_ = loading; }

    void cancelPending() {
        debouncePending_ = false;
        loading_ = false;
    }

    [[nodiscard]] bool schedule(const std::string& query, Clock::time_point now, bool configured) {
        const bool eligible = configured && query.size() >= kMinQueryBytes;
        if (!eligible) {
            const bool changed =
                debouncePending_ || loading_ || !query_.empty() || !results_.empty() || !error_.empty();
            debouncePending_ = false;
            loading_ = false;
            debounceDeadline_ = {};
            query_.clear();
            results_.clear();
            error_.clear();
            return changed;
        }
        if (query == query_) return false;
        query_ = query;
        debouncePending_ = true;
        debounceDeadline_ = now + kDebounceDelay;
        loading_ = false;
        results_.clear();
        error_.clear();
        return true;
    }

    [[nodiscard]] bool debounceDue(Clock::time_point now) const { return debouncePending_ && now >= debounceDeadline_; }

    [[nodiscard]] bool beginDue(Clock::time_point now) {
        if (!debounceDue(now)) return false;
        debouncePending_ = false;
        loading_ = true;
        return true;
    }

    [[nodiscard]] BeginResult beginImmediate(const std::string& query) {
        if (query == query_) {
            if (!debouncePending_) return {};
            debouncePending_ = false;
            loading_ = true;
            return {.started = true};
        }
        query_ = query;
        debouncePending_ = false;
        loading_ = true;
        const bool resultsChanged = !results_.empty() || !error_.empty();
        results_.clear();
        error_.clear();
        return {
            .started = true,
            .resultsChanged = resultsChanged,
        };
    }

    [[nodiscard]] bool begin(const std::string& query) {
        const bool resultsChanged = !results_.empty() || !error_.empty();
        debouncePending_ = false;
        loading_ = true;
        query_ = query;
        error_.clear();
        results_.clear();
        return resultsChanged;
    }

    [[nodiscard]] bool finish(const std::string& query, std::vector<SeerrMediaItem> results) {
        if (query_ != query) return false;
        loading_ = false;
        error_.clear();
        results_ = std::move(results);
        return true;
    }

    [[nodiscard]] bool fail(const std::string& query, std::string error) {
        if (query_ != query) return false;
        loading_ = false;
        error_ = std::move(error);
        results_.clear();
        return true;
    }

    void markRequested(const std::string& itemId, std::string status, int requestId = 0) {
        for (auto& item : results_) {
            if (item.id != itemId) continue;
            item.requested = true;
            item.requestId = requestId;
            item.status = std::move(status);
            if (item.mediaStatus <= 1) item.mediaStatus = 2;
            break;
        }
    }

    void markUnrequested(const std::string& itemId) {
        for (auto& item : results_) {
            if (item.id != itemId) continue;
            item.requested = false;
            item.requestId = 0;
            item.mediaStatus = 0;
            item.status.clear();
            break;
        }
        query_.clear();
    }

    void clearResults() { results_.clear(); }

private:
    std::vector<SeerrMediaItem> results_;
    bool loading_ = false;
    std::string error_;
    bool debouncePending_ = false;
    Clock::time_point debounceDeadline_{};
    std::string query_;
};
