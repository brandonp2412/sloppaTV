#pragma once

#include "home_screen.hpp"
#include "screen_navigation_key.hpp"
#include "seerr_media.hpp"

#include <optional>
#include <vector>

enum class HomeNavigationActionType {
    None,
    FinishActivity,
    OpenProfiles,
    OpenSearch,
    OpenSettings,
    OpenContext,
    OpenLibrary,
    OpenDetails,
    FinalizeNavigation,
};

struct HomeNavigationAction {
    HomeNavigationActionType type = HomeNavigationActionType::None;
    std::optional<JellyfinItem> item;
    int previousFirstVisibleRow = 0;
    int currentFirstVisibleRow = 0;
    int prefetchRow = -1;
    int prefetchSelection = 0;
};

class HomeNavigationController {
public:
    [[nodiscard]] static HomeNavigationAction handle(HomeScreenState& state, ScreenNavigationKey key,
                                                     const std::vector<JellyfinHomeRow>& rows,
                                                     const std::vector<SeerrMediaItem>& pendingSeerr);
};
