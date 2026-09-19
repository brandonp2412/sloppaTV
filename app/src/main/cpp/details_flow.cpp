#include "details_flow.hpp"

namespace {
DetailsFlowCommand command(DetailsFlowAction action) {
    DetailsFlowCommand result;
    result.action = action;
    return result;
}

DetailsFlowCommand itemCommand(DetailsFlowAction action, JellyfinItem item, bool replaceCurrentDetails = false) {
    DetailsFlowCommand result;
    result.action = action;
    result.item = std::move(item);
    result.replaceCurrentDetails = replaceCurrentDetails;
    return result;
}

DetailsFlowCommand personCommand(DetailsFlowAction action, JellyfinPerson person) {
    DetailsFlowCommand result;
    result.action = action;
    result.person = std::move(person);
    return result;
}
} // namespace

void DetailsFlow::reset() {
    item_ = {};
    state_.reset();
}

void DetailsFlow::beginDetails(JellyfinItem item) {
    item_ = std::move(item);
    state_.beginDetails();
}

bool DetailsFlow::beginCast() {
    if (item_.people.empty()) return false;
    state_.resetCastSelection();
    return true;
}

bool DetailsFlow::beginPerson(const JellyfinPerson& person) {
    if (person.id.empty()) return false;
    state_.beginPerson(person);
    return true;
}

bool DetailsFlow::beginItemMenu() {
    if (item_.id.empty()) return false;
    state_.beginItemMenu();
    return true;
}

bool DetailsFlow::beginSeries() {
    if (item_.id.empty() || item_.type != "Series") return false;
    state_.beginSeries(item_);
    return true;
}

bool DetailsFlow::beginSeason(const JellyfinItem& season) {
    if (state_.seriesDetail().id.empty() || season.id.empty()) return false;
    state_.beginSeason(season);
    return true;
}

std::vector<std::string> DetailsFlow::detailActions(bool stillWatchingPrompt) const {
    return state_.actions(item_, stillWatchingPrompt);
}

std::vector<std::string> DetailsFlow::itemMenuActions(bool seerrRequest, bool hasExternalPlayer, bool hasQueue,
                                                      bool hiddenFromHome) const {
    return state_.itemMenuActions(item_, seerrRequest, hasExternalPlayer, hasQueue, hiddenFromHome);
}

DetailsFlowCommand DetailsFlow::handleDetails(DetailsNavigationKey key) {
    const DetailsNavigationAction navigation = DetailsNavigationController::handleDetails(state_, key, item_);
    switch (navigation.type) {
    case DetailsNavigationActionType::Back: {
        auto result = command(DetailsFlowAction::ExitDetails);
        result.cancelContentLoad = true;
        result.resetContinuationPrompt = true;
        return result;
    }
    case DetailsNavigationActionType::OpenContext:
        return command(DetailsFlowAction::OpenItemMenu);
    case DetailsNavigationActionType::OpenEpisodeSeries:
        if (navigation.item) return itemCommand(DetailsFlowAction::OpenDetails, *navigation.item, true);
        return {};
    case DetailsNavigationActionType::OpenEpisodeSeason:
        if (navigation.item) return itemCommand(DetailsFlowAction::OpenEpisodes, *navigation.item);
        return {};
    case DetailsNavigationActionType::OpenSimilar:
        if (navigation.item) return itemCommand(DetailsFlowAction::OpenDetails, *navigation.item);
        return {};
    case DetailsNavigationActionType::ActivateDetailAction:
        if (!navigation.detailAction) return {};
        switch (*navigation.detailAction) {
        case DetailsAction::StartPlayback:
            return command(DetailsFlowAction::BeginPlayback);
        case DetailsAction::OpenEpisodes:
            return command(DetailsFlowAction::OpenSeasons);
        case DetailsAction::PlayAll:
            return command(DetailsFlowAction::BeginSeriesPlayAll);
        case DetailsAction::ToggleFavorite:
            return command(DetailsFlowAction::ToggleFavorite);
        case DetailsAction::TogglePlayed:
            return command(DetailsFlowAction::TogglePlayed);
        case DetailsAction::OpenCast:
            return command(DetailsFlowAction::OpenCast);
        case DetailsAction::OpenItemMenu:
            return command(DetailsFlowAction::OpenItemMenu);
        case DetailsAction::Back: {
            auto result = command(DetailsFlowAction::ExitDetails);
            result.resetContinuationPrompt = true;
            return result;
        }
        }
        return {};
    default:
        return {};
    }
}

DetailsFlowCommand DetailsFlow::handleCast(DetailsNavigationKey key, int columns) {
    const DetailsNavigationAction navigation =
        DetailsNavigationController::handleCast(state_, key, item_.people, columns);
    if (navigation.type == DetailsNavigationActionType::Back) return command(DetailsFlowAction::BackToDetails);
    if (navigation.type == DetailsNavigationActionType::OpenPerson && navigation.person) {
        return personCommand(DetailsFlowAction::OpenPersonItems, *navigation.person);
    }
    return {};
}

DetailsFlowCommand DetailsFlow::handlePersonItems(DetailsNavigationKey key, int columns) {
    const DetailsNavigationAction navigation = DetailsNavigationController::handlePersonItems(state_, key, columns);
    if (navigation.type == DetailsNavigationActionType::Back) {
        auto result = command(DetailsFlowAction::BackToCast);
        result.cancelContentLoad = true;
        return result;
    }
    if (navigation.type == DetailsNavigationActionType::OpenPersonItemContext && navigation.item) {
        return itemCommand(DetailsFlowAction::OpenItemContext, *navigation.item);
    }
    if (navigation.type == DetailsNavigationActionType::OpenPersonItem && navigation.item) {
        return itemCommand(DetailsFlowAction::OpenDetails, *navigation.item);
    }
    return {};
}

DetailsFlowCommand DetailsFlow::handleItemMenu(DetailsNavigationKey key, bool seerrRequest, bool hasExternalPlayer,
                                               bool hasQueue) {
    const DetailsNavigationAction navigation =
        DetailsNavigationController::handleItemMenu(state_, key, item_, seerrRequest, hasExternalPlayer, hasQueue);
    if (navigation.type == DetailsNavigationActionType::Back) return command(DetailsFlowAction::BackToDetails);
    if (navigation.type == DetailsNavigationActionType::ConfirmDelete) {
        return command(seerrRequest ? DetailsFlowAction::ConfirmDeleteRequest : DetailsFlowAction::ConfirmDeleteMedia);
    }
    if (navigation.type != DetailsNavigationActionType::ActivateItemMenuAction || !navigation.itemMenuAction) return {};

    DetailsFlowCommand result;
    result.closeItemMenu = true;
    switch (*navigation.itemMenuAction) {
    case ItemMenuAction::PlayAll:
        result.action = DetailsFlowAction::BeginSeriesPlayAll;
        return result;
    case ItemMenuAction::PlayExternal:
        result.action = DetailsFlowAction::PlayExternal;
        return result;
    case ItemMenuAction::ViewQueue:
        result.action = DetailsFlowAction::ViewQueue;
        return result;
    case ItemMenuAction::ToggleFavorite:
        result.action = DetailsFlowAction::ToggleFavorite;
        return result;
    case ItemMenuAction::TogglePlayed:
        result.action = DetailsFlowAction::TogglePlayed;
        return result;
    case ItemMenuAction::ToggleHomeVisibility:
        result.action = DetailsFlowAction::ToggleHomeVisibility;
        return result;
    case ItemMenuAction::RefreshMetadata:
        result.action = DetailsFlowAction::RefreshMetadata;
        return result;
    case ItemMenuAction::Back:
        result.action = DetailsFlowAction::None;
        return result;
    case ItemMenuAction::DeleteMedia:
    case ItemMenuAction::DeleteRequest:
        return {};
    }
    return {};
}

DetailsFlowCommand DetailsFlow::handleSeasons(DetailsNavigationKey key, int columns) {
    const DetailsNavigationAction navigation = DetailsNavigationController::handleSeasons(state_, key, columns);
    if (navigation.type == DetailsNavigationActionType::Back) {
        if (navigation.item) item_ = *navigation.item;
        auto result = command(DetailsFlowAction::BackToDetails);
        result.cancelContentLoad = true;
        return result;
    }
    if (navigation.type == DetailsNavigationActionType::OpenSeason && navigation.item) {
        return itemCommand(DetailsFlowAction::OpenEpisodes, *navigation.item);
    }
    return {};
}

DetailsFlowCommand DetailsFlow::handleEpisodes(DetailsNavigationKey key, int columns) {
    const DetailsNavigationAction navigation = DetailsNavigationController::handleEpisodes(state_, key, columns);
    if (navigation.type == DetailsNavigationActionType::Back) {
        auto result = command(DetailsFlowAction::BackToSeasons);
        result.cancelContentLoad = true;
        return result;
    }
    if (navigation.type == DetailsNavigationActionType::OpenEpisodeContext && navigation.item) {
        return itemCommand(DetailsFlowAction::OpenItemContext, *navigation.item);
    }
    if (navigation.type == DetailsNavigationActionType::OpenEpisode && navigation.item) {
        return itemCommand(DetailsFlowAction::OpenDetails, *navigation.item);
    }
    return {};
}
