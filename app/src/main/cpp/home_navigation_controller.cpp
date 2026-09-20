#include "home_navigation_controller.hpp"

#include "seerr_home_projection.hpp"

namespace {
HomeScreenInput homeInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return HomeScreenInput::Back;
    case ScreenNavigationKey::Search:
        return HomeScreenInput::Search;
    case ScreenNavigationKey::Context:
        return HomeScreenInput::Context;
    case ScreenNavigationKey::Left:
        return HomeScreenInput::Left;
    case ScreenNavigationKey::Right:
        return HomeScreenInput::Right;
    case ScreenNavigationKey::Up:
        return HomeScreenInput::Up;
    case ScreenNavigationKey::Down:
        return HomeScreenInput::Down;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return HomeScreenInput::Activate;
    case ScreenNavigationKey::None:
        return HomeScreenInput::None;
    }
    return HomeScreenInput::None;
}

HomeNavigationAction action(HomeNavigationActionType type) {
    HomeNavigationAction result;
    result.type = type;
    return result;
}

HomeNavigationAction itemAction(HomeNavigationActionType type, JellyfinItem item) {
    HomeNavigationAction result;
    result.type = type;
    result.item = std::move(item);
    return result;
}
} // namespace

HomeNavigationAction HomeNavigationController::handle(HomeScreenState& state, ScreenNavigationKey key,
                                                      const std::vector<JellyfinHomeRow>& rows,
                                                      const std::vector<SeerrMediaItem>& pendingSeerr) {
    const int previousFirstVisibleRow = state.firstVisibleRow();
    const HomeScreenCommand command = state.handleInput(homeInput(key), rows);
    switch (command.type) {
    case HomeScreenCommandType::FinishActivity:
        return action(HomeNavigationActionType::FinishActivity);
    case HomeScreenCommandType::OpenProfiles:
        return action(HomeNavigationActionType::OpenProfiles);
    case HomeScreenCommandType::OpenSearch:
        return action(HomeNavigationActionType::OpenSearch);
    case HomeScreenCommandType::OpenSettings:
        return action(HomeNavigationActionType::OpenSettings);
    case HomeScreenCommandType::OpenContext: {
        const auto& section = rows[static_cast<size_t>(command.rowIndex)];
        return itemAction(HomeNavigationActionType::OpenContext, section.items[static_cast<size_t>(command.itemIndex)]);
    }
    case HomeScreenCommandType::OpenSelected: {
        const auto& section = rows[static_cast<size_t>(command.rowIndex)];
        JellyfinItem selected = section.items[static_cast<size_t>(command.itemIndex)];
        if (section.title == "My Media") return itemAction(HomeNavigationActionType::OpenLibrary, std::move(selected));

        if (const auto* seerrMedia = findSeerrHomeMedia(section.title, selected.id, pendingSeerr)) {
            if (seerrMedia->jellyfinId.empty()) {
                return itemAction(HomeNavigationActionType::OpenContext, std::move(selected));
            }
            selected.id = seerrMedia->jellyfinId;
            selected.externalSource.clear();
        }
        return itemAction(HomeNavigationActionType::OpenDetails, std::move(selected));
    }
    case HomeScreenCommandType::None:
        break;
    }

    if (!command.finalizeRowNavigation) return {};

    HomeNavigationAction result;
    result.type = HomeNavigationActionType::FinalizeNavigation;
    result.previousFirstVisibleRow = previousFirstVisibleRow;
    result.currentFirstVisibleRow = state.firstVisibleRow();
    if (state.row() >= 0 && state.row() < static_cast<int>(state.selectionCount())) {
        const auto& row = rows[static_cast<size_t>(state.row())];
        result.prefetchRow = state.row();
        result.prefetchSelection = state.selection(state.row(), static_cast<int>(row.items.size()));
    }
    return result;
}
