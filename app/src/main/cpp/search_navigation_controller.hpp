#pragma once

#include "screen_navigation_key.hpp"
#include "search_screen.hpp"

#include <optional>

enum class SearchNavigationActionType {
    None,
    Exit,
    SubmitSearch,
    MoveKeyboard,
    ActivateKeyboard,
    OpenTextInput,
    OpenContext,
    OpenDetails,
    RequestSeerr,
};

struct SearchNavigationAction {
    SearchNavigationActionType type = SearchNavigationActionType::None;
    int dx = 0;
    int dy = 0;
    std::optional<JellyfinItem> item;
    std::optional<SeerrMediaItem> seerrItem;
};

class SearchNavigationController {
public:
    [[nodiscard]] static SearchNavigationAction handle(SearchScreenState& state, ScreenNavigationKey key, int columns);
};
