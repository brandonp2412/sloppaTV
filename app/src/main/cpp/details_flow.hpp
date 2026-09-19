#pragma once

#include "details_navigation_controller.hpp"

#include <optional>
#include <string>
#include <vector>

enum class DetailsFlowAction {
    None,
    ExitDetails,
    OpenItemMenu,
    OpenDetails,
    OpenEpisodes,
    BeginPlayback,
    OpenSeasons,
    BeginSeriesPlayAll,
    ToggleFavorite,
    TogglePlayed,
    OpenCast,
    BackToDetails,
    OpenPersonItems,
    BackToCast,
    OpenItemContext,
    ConfirmDeleteMedia,
    ConfirmDeleteRequest,
    PlayExternal,
    ViewQueue,
    ToggleHomeVisibility,
    RefreshMetadata,
    BackToSeasons,
};

struct DetailsFlowCommand {
    DetailsFlowAction action = DetailsFlowAction::None;
    std::optional<JellyfinItem> item;
    std::optional<JellyfinPerson> person;
    bool replaceCurrentDetails = false;
    bool cancelContentLoad = false;
    bool resetContinuationPrompt = false;
    bool closeItemMenu = false;
};

class DetailsFlow {
public:
    [[nodiscard]] JellyfinItem& item() { return item_; }

    [[nodiscard]] const JellyfinItem& item() const { return item_; }

    [[nodiscard]] DetailsScreenState& state() { return state_; }

    [[nodiscard]] const DetailsScreenState& state() const { return state_; }

    void reset();
    void beginDetails(JellyfinItem item);
    [[nodiscard]] bool beginCast();
    [[nodiscard]] bool beginPerson(const JellyfinPerson& person);
    [[nodiscard]] bool beginItemMenu();
    [[nodiscard]] bool beginSeries();
    [[nodiscard]] bool beginSeason(const JellyfinItem& season);

    [[nodiscard]] std::vector<std::string> detailActions(bool stillWatchingPrompt) const;
    [[nodiscard]] std::vector<std::string> itemMenuActions(bool seerrRequest, bool hasExternalPlayer, bool hasQueue,
                                                           bool hiddenFromHome) const;

    [[nodiscard]] DetailsFlowCommand handleDetails(DetailsNavigationKey key);
    [[nodiscard]] DetailsFlowCommand handleCast(DetailsNavigationKey key, int columns);
    [[nodiscard]] DetailsFlowCommand handlePersonItems(DetailsNavigationKey key, int columns);
    [[nodiscard]] DetailsFlowCommand handleItemMenu(DetailsNavigationKey key, bool seerrRequest, bool hasExternalPlayer,
                                                    bool hasQueue);
    [[nodiscard]] DetailsFlowCommand handleSeasons(DetailsNavigationKey key, int columns);
    [[nodiscard]] DetailsFlowCommand handleEpisodes(DetailsNavigationKey key, int columns);

private:
    JellyfinItem item_;
    DetailsScreenState state_;
};
