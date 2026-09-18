#include "details_navigation_controller.hpp"

namespace {
DetailsNavigationAction navigationAction(DetailsNavigationActionType type) {
    DetailsNavigationAction result;
    result.type = type;
    return result;
}

DetailsNavigationAction navigationItemAction(DetailsNavigationActionType type, const JellyfinItem& item) {
    DetailsNavigationAction result;
    result.type = type;
    result.item = item;
    return result;
}

DetailsNavigationAction navigationPersonAction(DetailsNavigationActionType type, const JellyfinPerson& person) {
    DetailsNavigationAction result;
    result.type = type;
    result.person = person;
    return result;
}

DetailsNavigationAction navigationDetailAction(DetailsAction action) {
    DetailsNavigationAction result;
    result.type = DetailsNavigationActionType::ActivateDetailAction;
    result.detailAction = action;
    return result;
}

DetailsNavigationAction navigationItemMenuAction(ItemMenuAction action) {
    DetailsNavigationAction result;
    result.type = DetailsNavigationActionType::ActivateItemMenuAction;
    result.itemMenuAction = action;
    return result;
}

DetailsScreenInput detailsInput(DetailsNavigationKey key) {
    switch (key) {
    case DetailsNavigationKey::Back:
        return DetailsScreenInput::Back;
    case DetailsNavigationKey::Context:
        return DetailsScreenInput::Context;
    case DetailsNavigationKey::Left:
        return DetailsScreenInput::Left;
    case DetailsNavigationKey::Right:
        return DetailsScreenInput::Right;
    case DetailsNavigationKey::Up:
        return DetailsScreenInput::Up;
    case DetailsNavigationKey::Down:
        return DetailsScreenInput::Down;
    case DetailsNavigationKey::Activate:
        return DetailsScreenInput::Activate;
    case DetailsNavigationKey::None:
        return DetailsScreenInput::None;
    }
    return DetailsScreenInput::None;
}

CastScreenInput castInput(DetailsNavigationKey key) {
    switch (key) {
    case DetailsNavigationKey::Back:
        return CastScreenInput::Back;
    case DetailsNavigationKey::Left:
        return CastScreenInput::Left;
    case DetailsNavigationKey::Right:
        return CastScreenInput::Right;
    case DetailsNavigationKey::Up:
        return CastScreenInput::Up;
    case DetailsNavigationKey::Down:
        return CastScreenInput::Down;
    case DetailsNavigationKey::Activate:
        return CastScreenInput::Activate;
    case DetailsNavigationKey::None:
    case DetailsNavigationKey::Context:
        return CastScreenInput::None;
    }
    return CastScreenInput::None;
}

DetailGridScreenInput gridInput(DetailsNavigationKey key) {
    switch (key) {
    case DetailsNavigationKey::Back:
        return DetailGridScreenInput::Back;
    case DetailsNavigationKey::Context:
        return DetailGridScreenInput::Context;
    case DetailsNavigationKey::Left:
        return DetailGridScreenInput::Left;
    case DetailsNavigationKey::Right:
        return DetailGridScreenInput::Right;
    case DetailsNavigationKey::Up:
        return DetailGridScreenInput::Up;
    case DetailsNavigationKey::Down:
        return DetailGridScreenInput::Down;
    case DetailsNavigationKey::Activate:
        return DetailGridScreenInput::Activate;
    case DetailsNavigationKey::None:
        return DetailGridScreenInput::None;
    }
    return DetailGridScreenInput::None;
}

ItemMenuScreenInput itemMenuInput(DetailsNavigationKey key) {
    switch (key) {
    case DetailsNavigationKey::Back:
        return ItemMenuScreenInput::Back;
    case DetailsNavigationKey::Left:
        return ItemMenuScreenInput::Left;
    case DetailsNavigationKey::Right:
        return ItemMenuScreenInput::Right;
    case DetailsNavigationKey::Up:
        return ItemMenuScreenInput::Up;
    case DetailsNavigationKey::Down:
        return ItemMenuScreenInput::Down;
    case DetailsNavigationKey::Activate:
        return ItemMenuScreenInput::Activate;
    case DetailsNavigationKey::None:
    case DetailsNavigationKey::Context:
        return ItemMenuScreenInput::None;
    }
    return ItemMenuScreenInput::None;
}
} // namespace

DetailsNavigationAction DetailsNavigationController::handleDetails(DetailsScreenState& state, DetailsNavigationKey key,
                                                                    const JellyfinItem& item) {
    const DetailsScreenCommand command =
        state.handleInput(detailsInput(key), static_cast<int>(detailActionIdsFor(item).size()), item.type == "Episode");
    switch (command.type) {
    case DetailsScreenCommandType::None:
        return {};
    case DetailsScreenCommandType::Back:
        return navigationAction(DetailsNavigationActionType::Back);
    case DetailsScreenCommandType::OpenContext:
        return navigationAction(DetailsNavigationActionType::OpenContext);
    case DetailsScreenCommandType::OpenEpisodeSeries: {
        const JellyfinItem& series = state.seriesDetail();
        if (series.id.empty()) return {};
        return navigationItemAction(DetailsNavigationActionType::OpenEpisodeSeries, series);
    }
    case DetailsScreenCommandType::OpenEpisodeSeason:
        if (const auto* season = state.selectedEpisodeContextSeason()) {
            return navigationItemAction(DetailsNavigationActionType::OpenEpisodeSeason, *season);
        }
        return {};
    case DetailsScreenCommandType::OpenSimilar:
        if (const auto* selected = state.selectedSimilar()) {
            return navigationItemAction(DetailsNavigationActionType::OpenSimilar, *selected);
        }
        return {};
    case DetailsScreenCommandType::ActivateAction:
        if (const auto action = state.selectedAction(item)) {
            return navigationDetailAction(*action);
        }
        return {};
    }
    return {};
}

DetailsNavigationAction DetailsNavigationController::handleCast(DetailsScreenState& state, DetailsNavigationKey key,
                                                                 const std::vector<JellyfinPerson>& people,
                                                                 int columns) {
    const CastScreenCommand command = state.handleCastInput(castInput(key), people, columns);
    if (command.type == CastScreenCommandType::Back) return navigationAction(DetailsNavigationActionType::Back);
    if (command.type != CastScreenCommandType::OpenPerson) return {};
    if (const auto* person = state.selectedCastPerson(people)) {
        return navigationPersonAction(DetailsNavigationActionType::OpenPerson, *person);
    }
    return {};
}

DetailsNavigationAction DetailsNavigationController::handlePersonItems(DetailsScreenState& state,
                                                                        DetailsNavigationKey key, int columns) {
    const DetailGridScreenCommand command = state.handlePersonItemsInput(gridInput(key), columns);
    if (command.type == DetailGridScreenCommandType::Back) return navigationAction(DetailsNavigationActionType::Back);
    if (command.type == DetailGridScreenCommandType::OpenContext) {
        if (const auto* item = state.selectedPersonItem()) {
            return navigationItemAction(DetailsNavigationActionType::OpenPersonItemContext, *item);
        }
        return {};
    }
    if (command.type == DetailGridScreenCommandType::OpenSelected) {
        if (const auto* item = state.selectedPersonItem()) {
            return navigationItemAction(DetailsNavigationActionType::OpenPersonItem, *item);
        }
    }
    return {};
}

DetailsNavigationAction DetailsNavigationController::handleItemMenu(DetailsScreenState& state, DetailsNavigationKey key,
                                                                     const JellyfinItem& item, bool seerrRequest,
                                                                     bool hasExternalPlayer, bool hasQueue) {
    const int actionCount = static_cast<int>(itemMenuActionIdsFor(item, seerrRequest, hasExternalPlayer, hasQueue).size());
    const ItemMenuScreenCommand command = state.handleItemMenuInput(itemMenuInput(key), actionCount);
    if (command.type == ItemMenuScreenCommandType::Back) return navigationAction(DetailsNavigationActionType::Back);
    if (command.type == ItemMenuScreenCommandType::ConfirmDelete) {
        return navigationAction(DetailsNavigationActionType::ConfirmDelete);
    }
    if (command.type != ItemMenuScreenCommandType::ActivateAction) return {};

    const auto action = state.selectedItemMenuAction(item, seerrRequest, hasExternalPlayer, hasQueue);
    if (!action) return {};
    if (*action == ItemMenuAction::DeleteMedia || *action == ItemMenuAction::DeleteRequest) {
        state.setDeleteConfirmation(true);
        return {};
    }
    return navigationItemMenuAction(*action);
}

DetailsNavigationAction DetailsNavigationController::handleSeasons(DetailsScreenState& state, DetailsNavigationKey key,
                                                                    int columns) {
    const DetailGridScreenCommand command = state.handleSeasonsInput(gridInput(key), columns);
    if (command.type == DetailGridScreenCommandType::Back) {
        return navigationItemAction(DetailsNavigationActionType::Back, state.seriesDetail());
    }
    if (command.type == DetailGridScreenCommandType::OpenSelected) {
        if (const auto* season = state.selectedSeasonItem()) {
            return navigationItemAction(DetailsNavigationActionType::OpenSeason, *season);
        }
    }
    return {};
}

DetailsNavigationAction DetailsNavigationController::handleEpisodes(DetailsScreenState& state, DetailsNavigationKey key,
                                                                     int columns) {
    const DetailGridScreenCommand command = state.handleEpisodesInput(gridInput(key), columns);
    if (command.type == DetailGridScreenCommandType::Back) return navigationAction(DetailsNavigationActionType::Back);
    if (command.type == DetailGridScreenCommandType::OpenContext) {
        if (const auto* episode = state.selectedEpisodeItem()) {
            return navigationItemAction(DetailsNavigationActionType::OpenEpisodeContext, *episode);
        }
        return {};
    }
    if (command.type == DetailGridScreenCommandType::OpenSelected) {
        if (const auto* episode = state.selectedEpisodeItem()) {
            return navigationItemAction(DetailsNavigationActionType::OpenEpisode, *episode);
        }
    }
    return {};
}
