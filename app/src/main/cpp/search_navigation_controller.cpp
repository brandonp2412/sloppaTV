#include "search_navigation_controller.hpp"

namespace {
SearchScreenInput searchInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return SearchScreenInput::Back;
    case ScreenNavigationKey::Search:
        return SearchScreenInput::Search;
    case ScreenNavigationKey::Context:
        return SearchScreenInput::Context;
    case ScreenNavigationKey::Left:
        return SearchScreenInput::Left;
    case ScreenNavigationKey::Right:
        return SearchScreenInput::Right;
    case ScreenNavigationKey::Up:
        return SearchScreenInput::Up;
    case ScreenNavigationKey::Down:
        return SearchScreenInput::Down;
    case ScreenNavigationKey::Activate:
        return SearchScreenInput::Activate;
    case ScreenNavigationKey::Submit:
        return SearchScreenInput::Submit;
    case ScreenNavigationKey::None:
        return SearchScreenInput::None;
    }
    return SearchScreenInput::None;
}

SearchNavigationAction action(SearchNavigationActionType type) {
    SearchNavigationAction result;
    result.type = type;
    return result;
}
} // namespace

SearchNavigationAction SearchNavigationController::handle(SearchScreenState& state, ScreenNavigationKey key,
                                                          int columns) {
    const SearchScreenCommand command = state.handleInput(searchInput(key), columns);
    switch (command.type) {
    case SearchScreenCommandType::None:
        return {};
    case SearchScreenCommandType::Exit:
        state.cancelPending();
        return action(SearchNavigationActionType::Exit);
    case SearchScreenCommandType::SubmitSearch:
        return action(SearchNavigationActionType::SubmitSearch);
    case SearchScreenCommandType::MoveKeyboard: {
        SearchNavigationAction result = action(SearchNavigationActionType::MoveKeyboard);
        result.dx = command.dx;
        result.dy = command.dy;
        return result;
    }
    case SearchScreenCommandType::ActivateKeyboard:
        return action(SearchNavigationActionType::ActivateKeyboard);
    case SearchScreenCommandType::OpenTextInput:
        return action(SearchNavigationActionType::OpenTextInput);
    case SearchScreenCommandType::OpenContext: {
        SearchNavigationAction result = action(SearchNavigationActionType::OpenContext);
        result.item = state.results()[static_cast<size_t>(state.selection())];
        return result;
    }
    case SearchScreenCommandType::OpenDetails: {
        SearchNavigationAction result = action(SearchNavigationActionType::OpenDetails);
        result.item = state.results()[static_cast<size_t>(state.selection())];
        return result;
    }
    case SearchScreenCommandType::RequestSeerr: {
        const auto* selected = state.selectedSeerrResult();
        if (!selected) return {};
        SearchNavigationAction result = action(SearchNavigationActionType::RequestSeerr);
        result.seerrItem = *selected;
        return result;
    }
    }
    return {};
}
