#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>

inline std::string homeVisibilityKey(const JellyfinSession& session, std::string_view itemId) {
    return session.server + "\n" + session.userId + "\n" + std::string(itemId);
}

inline bool homeItemHidden(const JellyfinSession& session, const JellyfinItem& item,
                           const std::unordered_set<std::string>& hiddenItems) {
    return !item.id.empty() && hiddenItems.contains(homeVisibilityKey(session, item.id));
}

inline void filterHiddenHomeItems(JellyfinHomeData& data, const JellyfinSession& session,
                                  const std::unordered_set<std::string>& hiddenItems) {
    for (auto& row : data.rows) {
        if (row.title == "My Media") continue;
        std::erase_if(row.items, [&](const JellyfinItem& item) { return homeItemHidden(session, item, hiddenItems); });
    }
}
