#pragma once

#include "screen_navigation_key.hpp"
#include "seerr_storage_state.hpp"

#include <optional>

enum class SeerrDriveNavigationActionType {
    None,
    Back,
    Selected,
};

struct SeerrDriveNavigationAction {
    SeerrDriveNavigationActionType type = SeerrDriveNavigationActionType::None;
    std::optional<SeerrStorageState::Selection> selection;
};

class SeerrDriveNavigationController {
public:
    [[nodiscard]] static SeerrDriveNavigationAction handle(SeerrStorageState& state, ScreenNavigationKey key);
};
