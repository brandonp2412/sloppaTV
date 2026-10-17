#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Avoid probing entire server lists or monopolizing the playback request pool.
// Home fetches are best-effort, serial, and rearm only after a cooldown.
class HomeEpisodePrewarmState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr size_t kFirstPageCards = 5;
    static constexpr auto kSpacing = std::chrono::seconds(2);
    static constexpr auto kCooldown = std::chrono::minutes(2);

    void refresh(const JellyfinSession& session, const JellyfinHomeData& home) {
        const std::string identity = session.server + "\n" + session.userId;
        if (identity != identity_) {
            identity_ = identity;
            attempted_.clear();
            nextDue_ = {};
        }
        pending_.clear();
        std::unordered_set<std::string> seen;
        bool movieQueued = false;
        for (const auto& row : home.rows) {
            const bool resumeRow = row.title == "Continue Watching" || row.title == "Resume Playing";
            if (!resumeRow && row.title != "Next Up") continue;
            for (size_t i = 0; i < std::min(kFirstPageCards, row.items.size()); ++i) {
                const auto& item = row.items[i];
                const bool resumeMovie = resumeRow && item.type == "Movie" && !movieQueued;
                if ((item.type != "Episode" && !resumeMovie) || item.id.empty() || !seen.insert(item.id).second)
                    continue;
                pending_.push_back(item);
                if (resumeMovie) movieQueued = true;
            }
        }
    }

    [[nodiscard]] std::optional<JellyfinItem> takeDue(TimePoint now, bool homeVisible, bool isRemux) {
        if (!homeVisible || !isRemux || now < nextDue_) return std::nullopt;
        while (!pending_.empty()) {
            JellyfinItem item = std::move(pending_.front());
            pending_.erase(pending_.begin());
            const auto previous = attempted_.find(item.id);
            if (previous != attempted_.end() && now - previous->second < kCooldown) continue;
            attempted_[item.id] = now;
            nextDue_ = now + kSpacing;
            return item;
        }
        return std::nullopt;
    }

private:
    std::string identity_;
    std::vector<JellyfinItem> pending_;
    std::unordered_map<std::string, TimePoint> attempted_;
    TimePoint nextDue_{};
};
