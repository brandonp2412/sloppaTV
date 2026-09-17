#pragma once

#include "jellyfin_types.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "seerr_media.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::string_view kSeerrHomeRowTitle = "Seerr requests";

inline const SeerrMediaItem* findSeerrHomeMedia(std::string_view rowTitle, std::string_view itemId,
                                                const std::vector<SeerrMediaItem>& pending) {
    if (rowTitle != kSeerrHomeRowTitle || itemId.empty()) return nullptr;
    const auto found = std::find_if(pending.begin(), pending.end(),
                                    [&](const SeerrMediaItem& item) { return item.id == itemId; });
    return found == pending.end() ? nullptr : &*found;
}

inline void projectSeerrHomeRow(std::vector<JellyfinHomeRow>& rows, const std::vector<SeerrMediaItem>& pending) {
    std::erase_if(rows, [](const JellyfinHomeRow& row) { return row.title == kSeerrHomeRowTitle; });
    if (pending.empty()) return;

    std::vector<JellyfinItem> items;
    items.reserve(pending.size());
    for (const auto& media : pending) items.push_back(jellyfinItemFromSeerrMedia(media));

    const auto insertAt = rows.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(1, rows.size()));
    rows.insert(insertAt, JellyfinHomeRow{
                              .title = std::string(kSeerrHomeRowTitle),
                              .items = std::move(items),
                          });
}
