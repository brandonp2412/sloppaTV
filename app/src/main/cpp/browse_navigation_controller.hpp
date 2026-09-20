#pragma once

#include "browse_screen.hpp"
#include "screen_navigation_key.hpp"

#include <optional>

enum class BrowseNavigationActionType {
    None,
    ReloadPage,
    LocalPage,
    Exit,
    ApplyFilter,
    OpenContext,
    OpenContainer,
    OpenDetails,
    SelectionChanged,
};

struct BrowseNavigationAction {
    BrowseNavigationActionType type = BrowseNavigationActionType::None;
    std::optional<JellyfinItem> item;
    int filterSelection = 0;
    bool loadMore = false;
};

class BrowseNavigationController {
public:
    [[nodiscard]] static BrowseNavigationAction handle(BrowseScreenState& state, ScreenNavigationKey key, int columns,
                                                       bool loading);
};
