#pragma once

#include "home_screen.hpp"
#include "jellyfin_types.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class HomeVisibility {
public:
    HomeVisibility(const JellyfinSession& session, std::unordered_set<std::string>& hiddenItems)
        : session_(session), hiddenItems_(hiddenItems) {}

    [[nodiscard]] bool isHidden(const JellyfinItem& item) const {
        return !item.id.empty() && hiddenItems_.contains(key(item.id));
    }

    void filter(JellyfinHomeData& data) const {
        for (auto& row : data.rows) {
            if (row.title == "My Media") continue;
            std::erase_if(row.items, [&](const JellyfinItem& item) { return isHidden(item); });
        }
    }

    [[nodiscard]] std::optional<bool> toggle(const JellyfinItem& item, JellyfinHomeData& home, HomeScreenState& state) {
        if (item.id.empty()) return std::nullopt;
        const std::string itemKey = key(item.id);
        const bool hiding = !hiddenItems_.contains(itemKey);
        if (hiding)
            hiddenItems_.insert(itemKey);
        else
            hiddenItems_.erase(itemKey);
        filter(home);
        clampSelections(home, state);
        return hiding;
    }

    [[nodiscard]] bool restoreForPlayback(const JellyfinItem& item) {
        bool changed = false;
        if (!item.id.empty()) changed = hiddenItems_.erase(key(item.id)) > 0 || changed;
        if (!item.seriesId.empty()) changed = hiddenItems_.erase(key(item.seriesId)) > 0 || changed;
        return changed;
    }

private:
    [[nodiscard]] std::string key(const std::string& itemId) const {
        return session_.server + "\n" + session_.userId + "\n" + itemId;
    }

    static void clampSelections(const JellyfinHomeData& home, HomeScreenState& state) {
        std::vector<int> itemCounts;
        itemCounts.reserve(home.rows.size());
        for (const auto& row : home.rows) itemCounts.push_back(static_cast<int>(row.items.size()));
        state.clampSelections(itemCounts);
    }

    const JellyfinSession& session_;
    std::unordered_set<std::string>& hiddenItems_;
};
