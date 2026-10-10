#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

// The server's Resume list can lag a successful playback stop. Keep locally
// resumable episodes/movies visible across home refreshes and restarts, without mixing profiles.
class HomeResumeOverlay {
public:
    using Clock = std::chrono::system_clock;

    // Keep unfinished rewatches available after an app restart, even when the
    // server still labels an episode Watched. Never store credentials here.
    void restore(const std::string& dataPath) {
        recent_.clear();
        if (dataPath.empty()) return;
        std::ifstream in(dataPath + "/recent-resume.json");
        if (!in) return;
        try {
            const auto data = nlohmann::json::parse(in);
            if (!data.is_array()) return;
            for (const auto& saved : data) {
                if (!saved.is_object() || recent_.size() >= 12) break;
                Entry entry;
                entry.server = saved.value("server", std::string{});
                entry.userId = saved.value("userId", std::string{});
                entry.item.id = saved.value("id", std::string{});
                entry.item.type = saved.value("type", std::string{});
                entry.item.name = saved.value("name", std::string{});
                entry.item.seriesId = saved.value("seriesId", std::string{});
                entry.item.seriesName = saved.value("seriesName", std::string{});
                entry.item.seriesPrimaryImageTag = saved.value("seriesPrimaryImageTag", std::string{});
                entry.item.imageTag = saved.value("imageTag", std::string{});
                entry.item.thumbTag = saved.value("thumbTag", std::string{});
                entry.item.backdropTag = saved.value("backdropTag", std::string{});
                entry.item.backdropItemId = saved.value("backdropItemId", std::string{});
                entry.item.positionTicks = saved.value("positionTicks", int64_t{0});
                entry.item.runtimeTicks = saved.value("runtimeTicks", int64_t{0});
                entry.time = Clock::time_point{std::chrono::milliseconds(saved.value("savedAtMs", int64_t{0}))};
                if (entry.server.empty() || entry.userId.empty() || entry.item.id.empty() ||
                    (entry.item.type != "Episode" && entry.item.type != "Movie") ||
                    entry.item.positionTicks <= 0 || entry.time > Clock::now() ||
                    Clock::now() - entry.time > std::chrono::hours(24 * 30)) continue;
                recent_.push_back(std::move(entry));
            }
        } catch (const nlohmann::json::exception&) {
            recent_.clear();
        }
    }

    void persist(const std::string& dataPath) const {
        if (dataPath.empty()) return;
        nlohmann::json data = nlohmann::json::array();
        for (const auto& entry : recent_) {
            if (entry.time > Clock::now() || Clock::now() - entry.time > std::chrono::hours(24 * 30)) continue;
            data.push_back({
                {"server", entry.server}, {"userId", entry.userId}, {"id", entry.item.id},
                {"type", entry.item.type}, {"name", entry.item.name},
                {"seriesId", entry.item.seriesId}, {"seriesName", entry.item.seriesName},
                {"seriesPrimaryImageTag", entry.item.seriesPrimaryImageTag},
                {"imageTag", entry.item.imageTag}, {"thumbTag", entry.item.thumbTag},
                {"backdropTag", entry.item.backdropTag}, {"backdropItemId", entry.item.backdropItemId},
                {"positionTicks", entry.item.positionTicks}, {"runtimeTicks", entry.item.runtimeTicks},
                {"savedAtMs", std::chrono::duration_cast<std::chrono::milliseconds>(entry.time.time_since_epoch()).count()},
            });
        }
        const std::string path = dataPath + "/recent-resume.json";
        const std::string tmp = path + ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out) return;
            out << data.dump();
            out.flush();
            if (!out) return;
        }
        if (std::rename(tmp.c_str(), path.c_str()) != 0) std::remove(tmp.c_str());
    }

    void remember(const JellyfinSession& session, JellyfinItem item, int64_t positionTicks, bool completed,
                  Clock::time_point now = Clock::now()) {
        std::erase_if(recent_, [&](const Entry& entry) {
            return entry.server == session.server && entry.userId == session.userId && entry.item.id == item.id;
        });
        // An item may be marked Watched from an earlier viewing. A new partial
        // rewatch still belongs in Continue Watching until it is finished.
        if (!session.valid() || item.id.empty() || (item.type != "Episode" && item.type != "Movie") || completed ||
            positionTicks <= 0 || (item.runtimeTicks > 0 && positionTicks >= item.runtimeTicks))
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
                now - entry->time > std::chrono::hours(24 * 30) || now < entry->time)
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
