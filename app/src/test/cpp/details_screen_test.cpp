#include "details_screen.hpp"

#include <cassert>

int main() {
    DetailsScreenState state;
    JellyfinItem series;
    series.id = "series";
    series.name = "Example";
    series.type = "Series";
    series.favorite = false;
    series.played = false;
    series.canDelete = true;
    series.people.push_back({.id = "p1", .name = "One", .imageTag = "", .role = ""});
    series.people.push_back({.id = "p2", .name = "Two", .imageTag = "", .role = ""});
    series.people.push_back({.id = "p3", .name = "Three", .imageTag = "", .role = ""});
    series.people.push_back({.id = "p4", .name = "Four", .imageTag = "", .role = ""});
    series.people.push_back({.id = "p5", .name = "Five", .imageTag = "", .role = ""});
    series.people.push_back({.id = "p6", .name = "Six", .imageTag = "", .role = ""});

    state.beginDetails();
    const auto actionIds = detailActionIdsFor(series);
    assert(actionIds.front() == DetailsAction::StartPlayback);
    assert(actionIds[1] == DetailsAction::OpenEpisodes);
    assert(actionIds[2] == DetailsAction::PlayAll);
    const auto actions = state.actions(series, false);
    assert(actions.front() == "PLAY NEXT");
    assert(actions[1] == "EPISODES");
    assert(actions[2] == "PLAY ALL");
    assert(std::find(actions.begin(), actions.end(), "MORE") == actions.end());
    assert(actions.back() == "BACK");

    state.moveAction(1, static_cast<int>(actions.size()));
    assert(state.actionSelection() == 1);
    assert(state.selectedAction(series) == DetailsAction::OpenEpisodes);
    state.moveAction(-10, static_cast<int>(actions.size()));
    assert(state.actionSelection() == 0);

    DetailsScreenState inputState;
    inputState.beginDetails();
    auto inputCommand = inputState.handleInput(DetailsScreenInput::Right, static_cast<int>(actions.size()), false);
    assert(inputCommand.type == DetailsScreenCommandType::None);
    assert(inputState.actionSelection() == 1);
    inputCommand = inputState.handleInput(DetailsScreenInput::Activate, static_cast<int>(actions.size()), false);
    assert(inputCommand.type == DetailsScreenCommandType::ActivateAction);
    inputCommand = inputState.handleInput(DetailsScreenInput::Context, static_cast<int>(actions.size()), false);
    assert(inputCommand.type == DetailsScreenCommandType::OpenContext);
    inputCommand = inputState.handleInput(DetailsScreenInput::Back, static_cast<int>(actions.size()), false);
    assert(inputCommand.type == DetailsScreenCommandType::Back);

    std::vector<JellyfinItem> inputSimilar(3);
    inputSimilar[0].id = "input-a";
    inputSimilar[1].id = "input-b";
    inputSimilar[2].id = "input-c";
    inputState.setSimilar(std::move(inputSimilar));
    inputCommand = inputState.handleInput(DetailsScreenInput::Down, static_cast<int>(actions.size()), false);
    assert(inputCommand.type == DetailsScreenCommandType::None);
    assert(inputState.similarFocused());
    inputCommand = inputState.handleInput(DetailsScreenInput::Right, static_cast<int>(actions.size()), false);
    assert(inputState.similarSelection() == 1);
    inputCommand = inputState.handleInput(DetailsScreenInput::Activate, static_cast<int>(actions.size()), false);
    assert(inputCommand.type == DetailsScreenCommandType::OpenSimilar);
    inputCommand = inputState.handleInput(DetailsScreenInput::Up, static_cast<int>(actions.size()), false);
    assert(!inputState.similarFocused());

    std::vector<JellyfinItem> similar(3);
    similar[0].id = "a";
    similar[1].id = "b";
    similar[2].id = "c";
    state.setSimilar(std::move(similar));
    state.setSimilarFocused(true);
    state.moveSimilar(2);
    assert(state.similarFocused());
    assert(state.similarSelection() == 2);
    assert(state.selectedSimilar()->id == "c");
    std::vector<JellyfinItem> focusSimilar(3);
    focusSimilar[0].id = "a";
    focusSimilar[1].id = "b";
    focusSimilar[2].id = "c";
    state.setSimilar(std::move(focusSimilar));
    state.moveSimilar(1);
    assert(state.selectedSimilar()->id == "b");
    state.removeItem("a");
    assert(state.similarSelection() == 0);
    assert(state.selectedSimilar()->id == "b");

    state.beginItemMenu();
    const auto menuIds = itemMenuActionIdsFor(series, false, true, true);
    assert(menuIds.front() == ItemMenuAction::PlayAll);
    assert(menuIds[1] == ItemMenuAction::PlayExternal);
    assert(menuIds[2] == ItemMenuAction::ViewQueue);
    auto menu = state.itemMenuActions(series, false, true, true, false);
    assert(menu.front() == "PLAY ALL");
    assert(menu[1] == "PLAY EXTERNAL");
    assert(menu[2] == "VIEW QUEUE");
    assert(menu[menu.size() - 2] == "DELETE MEDIA");
    auto menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Down, static_cast<int>(menu.size()));
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Down, static_cast<int>(menu.size()));
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Down, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::None);
    assert(state.itemMenuSelection() == 3);
    assert(state.selectedItemMenuAction(series, false, true, true) == ItemMenuAction::ToggleFavorite);
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Activate, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::ActivateAction);

    const auto seerrMenu = state.itemMenuActions(series, true, false, false, false);
    assert(seerrMenu.size() == 2);
    assert(seerrMenu[0] == "DELETE REQUEST");
    assert(seerrMenu[1] == "BACK");
    const auto seerrMenuIds = itemMenuActionIdsFor(series, true, false, false);
    assert(seerrMenuIds[0] == ItemMenuAction::DeleteRequest);
    assert(seerrMenuIds[1] == ItemMenuAction::Back);

    state.setDeleteConfirmation(true);
    assert(state.deleteConfirmation());
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Left, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::None);
    assert(state.deleteConfirmationSelection() == 0);
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Activate, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::ConfirmDelete);
    assert(state.deleteConfirmation());
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Right, static_cast<int>(menu.size()));
    assert(state.deleteConfirmationSelection() == 1);
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Activate, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::None);
    assert(!state.deleteConfirmation());
    assert(state.deleteConfirmationSelection() == 1);
    state.setDeleteConfirmation(true);
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Back, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::None);
    assert(!state.deleteConfirmation());
    menuCommand = state.handleItemMenuInput(ItemMenuScreenInput::Back, static_cast<int>(menu.size()));
    assert(menuCommand.type == ItemMenuScreenCommandType::Back);

    state.resetCastSelection();
    auto castCommand = state.handleCastInput(CastScreenInput::Right, series.people, 5);
    assert(castCommand.type == CastScreenCommandType::None);
    assert(state.castSelection() == 1);
    castCommand = state.handleCastInput(CastScreenInput::Down, series.people, 5);
    assert(state.castSelection() == 5);
    assert(state.selectedCastPerson(series.people)->name == "Six");
    castCommand = state.handleCastInput(CastScreenInput::Activate, series.people, 5);
    assert(castCommand.type == CastScreenCommandType::OpenPerson);
    castCommand = state.handleCastInput(CastScreenInput::Up, series.people, 5);
    assert(state.castSelection() == 0);
    assert(state.selectedCastPerson(series.people)->name == "One");
    castCommand = state.handleCastInput(CastScreenInput::Back, series.people, 5);
    assert(castCommand.type == CastScreenCommandType::Back);

    state.beginPerson(series.people.front());
    std::vector<JellyfinItem> personItems(6);
    for (size_t i = 0; i < personItems.size(); ++i) personItems[i].id = "person-item-" + std::to_string(i);
    state.setPersonItems(std::move(personItems));
    assert(state.selectedPerson().id == "p1");
    auto personCommand = state.handlePersonItemsInput(DetailGridScreenInput::Right, 5);
    assert(personCommand.type == DetailGridScreenCommandType::None);
    assert(state.personItemSelection() == 1);
    assert(state.selectedPersonItem()->id == "person-item-1");
    personCommand = state.handlePersonItemsInput(DetailGridScreenInput::Context, 5);
    assert(personCommand.type == DetailGridScreenCommandType::OpenContext);
    personCommand = state.handlePersonItemsInput(DetailGridScreenInput::Activate, 5);
    assert(personCommand.type == DetailGridScreenCommandType::OpenSelected);
    personCommand = state.handlePersonItemsInput(DetailGridScreenInput::Back, 5);
    assert(personCommand.type == DetailGridScreenCommandType::Back);
    state.removeItem("person-item-0");
    assert(state.personItemSelection() == 0);
    assert(state.selectedPersonItem()->id == "person-item-1");

    JellyfinItem episodeDetail;
    episodeDetail.id = "episode-detail";
    episodeDetail.type = "Episode";
    episodeDetail.seriesId = "series";
    auto contextRequest = episodeSeriesContextRequest(episodeDetail);
    assert(contextRequest.has_value());
    assert(contextRequest->itemId == "episode-detail");
    assert(contextRequest->seriesId == "series");
    assert(contextRequest->matches(episodeDetail));

    JellyfinItem otherEpisode = episodeDetail;
    otherEpisode.id = "other-episode";
    assert(!contextRequest->matches(otherEpisode));

    JellyfinItem movieDetail = episodeDetail;
    movieDetail.type = "Movie";
    assert(!episodeSeriesContextRequest(movieDetail).has_value());
    assert(!contextRequest->matches(movieDetail));

    JellyfinItem detachedEpisode = episodeDetail;
    detachedEpisode.seriesId.clear();
    assert(!episodeSeriesContextRequest(detachedEpisode).has_value());

    JellyfinItem seasonOne;
    seasonOne.id = "season-1";
    seasonOne.name = "Season 1";
    JellyfinItem seasonTwo;
    seasonTwo.id = "season-2";
    seasonTwo.name = "Season 2";

    inputState.setEpisodeSeriesContext(series, {seasonOne, seasonTwo});
    inputCommand = inputState.handleInput(DetailsScreenInput::Down, static_cast<int>(actions.size()), true);
    assert(inputState.episodeContextFocused());
    inputCommand = inputState.handleInput(DetailsScreenInput::Right, static_cast<int>(actions.size()), true);
    assert(inputState.episodeContextSelection() == 1);
    inputCommand = inputState.handleInput(DetailsScreenInput::Activate, static_cast<int>(actions.size()), true);
    assert(inputCommand.type == DetailsScreenCommandType::OpenEpisodeSeason);
    inputCommand = inputState.handleInput(DetailsScreenInput::Left, static_cast<int>(actions.size()), true);
    assert(inputState.episodeContextSelection() == 0);
    inputCommand = inputState.handleInput(DetailsScreenInput::Activate, static_cast<int>(actions.size()), true);
    assert(inputCommand.type == DetailsScreenCommandType::OpenEpisodeSeries);
    inputCommand = inputState.handleInput(DetailsScreenInput::Up, static_cast<int>(actions.size()), true);
    assert(!inputState.episodeContextFocused());

    state.beginSeries(series);
    state.setSeasons({seasonOne, seasonTwo});
    assert(state.seriesDetail().id == "series");
    auto seasonCommand = state.handleSeasonsInput(DetailGridScreenInput::Right, 5);
    assert(seasonCommand.type == DetailGridScreenCommandType::None);
    assert(state.selectedSeasonItem()->id == "season-2");
    seasonCommand = state.handleSeasonsInput(DetailGridScreenInput::Context, 5);
    assert(seasonCommand.type == DetailGridScreenCommandType::None);
    seasonCommand = state.handleSeasonsInput(DetailGridScreenInput::Activate, 5);
    assert(seasonCommand.type == DetailGridScreenCommandType::OpenSelected);
    seasonCommand = state.handleSeasonsInput(DetailGridScreenInput::Back, 5);
    assert(seasonCommand.type == DetailGridScreenCommandType::Back);

    state.setEpisodeSeriesContext(series, {seasonOne, seasonTwo});
    assert(state.hasEpisodeSeriesContext());
    assert(state.episodeContextCount() == 3);
    state.setEpisodeContextFocused(true);
    assert(state.episodeContextFocused());
    state.moveEpisodeContext(2);
    assert(state.episodeContextSelection() == 2);
    assert(state.selectedEpisodeContextSeason()->id == "season-2");

    state.beginSeason(seasonTwo);
    JellyfinItem episodeOne;
    episodeOne.id = "episode-1";
    JellyfinItem episodeTwo;
    episodeTwo.id = "episode-2";
    state.setEpisodes({episodeOne, episodeTwo});
    auto episodeCommand = state.handleEpisodesInput(DetailGridScreenInput::Right, 5);
    assert(episodeCommand.type == DetailGridScreenCommandType::None);
    assert(state.selectedSeason().id == "season-2");
    assert(state.selectedEpisodeItem()->id == "episode-2");
    episodeCommand = state.handleEpisodesInput(DetailGridScreenInput::Context, 5);
    assert(episodeCommand.type == DetailGridScreenCommandType::OpenContext);
    episodeCommand = state.handleEpisodesInput(DetailGridScreenInput::Activate, 5);
    assert(episodeCommand.type == DetailGridScreenCommandType::OpenSelected);
    episodeCommand = state.handleEpisodesInput(DetailGridScreenInput::Back, 5);
    assert(episodeCommand.type == DetailGridScreenCommandType::Back);

    JellyfinItem updatedEpisode = episodeTwo;
    updatedEpisode.favorite = true;
    updatedEpisode.played = true;
    updatedEpisode.positionTicks = 42;
    state.updateCachedUserData(updatedEpisode);
    assert(state.episodes()[1].favorite);
    assert(state.episodes()[1].played);
    assert(state.episodes()[1].positionTicks == 42);
    state.removeItem("episode-2");
    assert(state.episodes().size() == 1);
    assert(state.episodeSelection() == 0);

    state.reset();
    assert(state.similar().empty());
    assert(state.personItems().empty());
    assert(state.seasons().empty());
    assert(state.episodes().empty());
    assert(!state.episodeContextFocused());
    assert(state.episodeContextSelection() == 0);
    assert(state.itemMenuSelection() == 0);
    return 0;
}
