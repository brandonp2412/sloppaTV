#pragma once

#include "details_screen.hpp"

#include <optional>
#include <vector>

enum class DetailsNavigationKey {
    None,
    Back,
    Context,
    Left,
    Right,
    Up,
    Down,
    Activate,
};

enum class DetailsNavigationActionType {
    None,
    Back,
    OpenContext,
    OpenEpisodeSeries,
    OpenEpisodeSeason,
    OpenSimilar,
    ActivateDetailAction,
    OpenPerson,
    OpenPersonItemContext,
    OpenPersonItem,
    ConfirmDelete,
    ActivateItemMenuAction,
    OpenSeason,
    OpenEpisodeContext,
    OpenEpisode,
};

struct DetailsNavigationAction {
    DetailsNavigationActionType type = DetailsNavigationActionType::None;
    std::optional<DetailsAction> detailAction;
    std::optional<ItemMenuAction> itemMenuAction;
    std::optional<JellyfinItem> item;
    std::optional<JellyfinPerson> person;
};

class DetailsNavigationController {
public:
    [[nodiscard]] static DetailsNavigationAction handleDetails(DetailsScreenState& state, DetailsNavigationKey key,
                                                                const JellyfinItem& item);
    [[nodiscard]] static DetailsNavigationAction handleCast(DetailsScreenState& state, DetailsNavigationKey key,
                                                             const std::vector<JellyfinPerson>& people, int columns);
    [[nodiscard]] static DetailsNavigationAction handlePersonItems(DetailsScreenState& state, DetailsNavigationKey key,
                                                                    int columns);
    [[nodiscard]] static DetailsNavigationAction handleItemMenu(DetailsScreenState& state, DetailsNavigationKey key,
                                                                 const JellyfinItem& item, bool seerrRequest,
                                                                 bool hasExternalPlayer, bool hasQueue);
    [[nodiscard]] static DetailsNavigationAction handleSeasons(DetailsScreenState& state, DetailsNavigationKey key,
                                                                int columns);
    [[nodiscard]] static DetailsNavigationAction handleEpisodes(DetailsScreenState& state, DetailsNavigationKey key,
                                                                 int columns);
};
