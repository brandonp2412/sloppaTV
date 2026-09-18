#include "browse_navigation_controller.hpp"

namespace {
BrowseScreenInput browseInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return BrowseScreenInput::Back;
    case ScreenNavigationKey::Context:
        return BrowseScreenInput::Context;
    case ScreenNavigationKey::Left:
        return BrowseScreenInput::Left;
    case ScreenNavigationKey::Right:
        return BrowseScreenInput::Right;
    case ScreenNavigationKey::Up:
        return BrowseScreenInput::Up;
    case ScreenNavigationKey::Down:
        return BrowseScreenInput::Down;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return BrowseScreenInput::Activate;
    case ScreenNavigationKey::None:
    case ScreenNavigationKey::Search:
        return BrowseScreenInput::None;
    }
    return BrowseScreenInput::None;
}

BrowseNavigationAction action(BrowseNavigationActionType type) {
    BrowseNavigationAction result;
    result.type = type;
    return result;
}

BrowseNavigationAction itemAction(BrowseNavigationActionType type, const JellyfinItem& item) {
    BrowseNavigationAction result;
    result.type = type;
    result.item = item;
    return result;
}
} // namespace

BrowseNavigationAction BrowseNavigationController::handle(BrowseScreenState& state, ScreenNavigationKey key, int columns,
                                                           bool loading) {
    const BrowseScreenCommand command = state.handleInput(browseInput(key), columns);
    switch (command.type) {
    case BrowseScreenCommandType::None:
        return {};
    case BrowseScreenCommandType::Back:
        if (command.backAction == BrowseBackAction::Reload) return action(BrowseNavigationActionType::ReloadPage);
        if (command.backAction == BrowseBackAction::LocalPage) return action(BrowseNavigationActionType::LocalPage);
        if (command.backAction == BrowseBackAction::Exit) return action(BrowseNavigationActionType::Exit);
        return {};
    case BrowseScreenCommandType::ApplyFilter: {
        BrowseNavigationAction result = action(BrowseNavigationActionType::ApplyFilter);
        result.filterSelection = state.filterSelection();
        return result;
    }
    case BrowseScreenCommandType::OpenContext:
        return itemAction(BrowseNavigationActionType::OpenContext,
                          state.items()[static_cast<size_t>(state.selection())]);
    case BrowseScreenCommandType::SelectGenre: {
        const JellyfinItem selected = state.items()[static_cast<size_t>(state.selection())];
        state.selectGenre(selected.name);
        return action(BrowseNavigationActionType::ReloadPage);
    }
    case BrowseScreenCommandType::SelectLetter: {
        const JellyfinItem selected = state.items()[static_cast<size_t>(state.selection())];
        state.selectLetter(selected.name);
        return action(BrowseNavigationActionType::ReloadPage);
    }
    case BrowseScreenCommandType::OpenContainer:
        return itemAction(BrowseNavigationActionType::OpenContainer,
                          state.items()[static_cast<size_t>(state.selection())]);
    case BrowseScreenCommandType::OpenDetails:
        return itemAction(BrowseNavigationActionType::OpenDetails,
                          state.items()[static_cast<size_t>(state.selection())]);
    case BrowseScreenCommandType::SelectionChanged: {
        BrowseNavigationAction result = action(BrowseNavigationActionType::SelectionChanged);
        result.loadMore = state.hasMore() && !loading &&
                          state.selection() >= static_cast<int>(state.items().size()) - 12;
        return result;
    }
    }
    return {};
}
