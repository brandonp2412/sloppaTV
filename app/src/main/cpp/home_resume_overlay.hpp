#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

// The server's Resume list can lag a successful playback stop. Keep locally
// resumable episodes/movies visible briefly, without mixing user profiles.
class HomeResumeOverlay {
public:
    using Clock = std::chrono::steady_clock;

    void remember(const JellyfinSession& session, JellyfinItem item, int64_t positionTicks, bool completed,
                  Clock::time_point now = Clock::now()) {
        std::erase_if(recent_, [&](const Entry& entry) {
            return entry.server == session.server && entry.userId == session.userId && entry.item.id == item.id;
        });
        if (!session.valid() || item.id.empty() || (item.type != "Episode" && item.type != "Movie") || completed ||
            item.played || positionTicks <= 0 || (item.runtimeTicks > 0 && positionTicks >= item.runtimeTicks))
            return;
        item.positionTicks = positionTicks;
        recent_.insert(recent_.begin(), {session.server, session.userId, std::move(item), now});
        if (recent_.size() > 12) recent_.resize(12);
    }

    void apply(const JellyfinSession& session, JellyfinHomeData& home,
               Clock::time_point now = Clock::now()) const {
        auto resume = std::find_if(home.rows.begin(), home.rows.end(),
                                   [](const JellyfinHomeRow& row) { return row.title == "Continue Watching"; });
        for (auto entry = recent_.rbegin(); entry != recent_.rend(); ++entry) {
            if (entry->server != session.server || entry->userId != session.userId ||
                now - entry->time > std::chrono::minutes(10) || now < entry->time)
                continue;
            if (resume == home.rows.end()) {
                home.rows.push_back({"Continue Watching", {}});
                resume = std::prev(home.rows.end());
            }
            const auto existing = std::find_if(resume->items.begin(), resume->items.end(),
                                               [&](const JellyfinItem& item) { return item.id == entry->item.id; });
            if (existing == resume->items.end()) resume->items.insert(resume->items.begin(), entry->item);
        }
        // Prioritize active viewing over browsing and hypothetical next episodes.
        std::stable_sort(home.rows.begin(), home.rows.end(), [](const JellyfinHomeRow& a, const JellyfinHomeRow& b) {
            const auto rank = [](const std::string& title) {
                if (title == "Continue Watching") return 0;
                if (title == "Next Up") return 1;
                if (title == "My Media") return 2;
                return 3;
            };
            return rank(a.title) < rank(b.title);
        });
    }

private:
    struct Entry {
        std::string server;
        std::string userId;
        JellyfinItem item;
        Clock::time_point time;
    };
    std::vector<Entry> recent_;
};
