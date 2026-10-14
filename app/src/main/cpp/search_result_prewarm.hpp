#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Prewarm at most three top-ranked playable Remux results, serially and with
// spacing. Replacing a query discards pending work; each candidate can only be
// retried after a cooldown, even if later searches contain the same result.
class SearchResultPrewarmState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr std::size_t kMaxCandidates = 3;
    static constexpr auto kSpacing = std::chrono::seconds(3);
    static constexpr auto kCooldown = std::chrono::minutes(3);

    void reset() {
        identity_.clear();
        query_.clear();
        pending_.clear();
        attempted_.clear();
        nextDue_ = {};
    }

    void refresh(const JellyfinSession& session, std::string query, const std::vector<JellyfinItem>& results,
                 TimePoint now = Clock::now()) {
        const std::string identity = session.server + "\n" + session.userId;
        if (identity != identity_) {
            identity_ = identity;
            attempted_.clear();
            nextDue_ = {};
        }
        query_ = std::move(query);
        pending_.clear();
        if (!session.valid() || query_.size() < 3) return;
        std::unordered_set<std::string> seen;
        for (const JellyfinItem& item : results) {
            if (pending_.size() >= kMaxCandidates) break;
            if ((item.type != "Movie" && item.type != "Series" && item.type != "Episode") || item.id.empty() ||
                !item.externalSource.empty() || !seen.insert(item.id).second)
                continue;
            const auto previous = attempted_.find(item.id);
            if (previous != attempted_.end() && now - previous->second < kCooldown) continue;
            pending_.push_back(item);
        }
        if (!pending_.empty()) nextDue_ = now + kSpacing;
    }

    [[nodiscard]] std::optional<JellyfinItem> takeDue(const JellyfinSession& session, std::string_view query,
                                                      bool searchingRemux, TimePoint now = Clock::now()) {
        if (!searchingRemux || query != query_ || session.server + "\n" + session.userId != identity_ ||
            now < nextDue_ || pending_.empty())
            return std::nullopt;
        JellyfinItem candidate = std::move(pending_.front());
        pending_.erase(pending_.begin());
        attempted_[candidate.id] = now;
        nextDue_ = now + kSpacing;
        return candidate;
    }

private:
    std::string identity_;
    std::string query_;
    std::vector<JellyfinItem> pending_;
    std::unordered_map<std::string, TimePoint> attempted_;
    TimePoint nextDue_{};
};
