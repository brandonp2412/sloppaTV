#include "details_navigation_controller.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    DetailsScreenState state;

    JellyfinItem movie;
    movie.id = "movie";
    movie.type = "Movie";
    movie.canDelete = true;

    auto navigation =
        DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Activate, movie);
    assert(navigation.type == DetailsNavigationActionType::ActivateDetailAction);
    assert(navigation.detailAction == DetailsAction::StartPlayback);

    JellyfinItem similar;
    similar.id = "similar";
    state.setSimilar({similar});
    navigation = DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Down, movie);
    assert(navigation.type == DetailsNavigationActionType::None);
    navigation = DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Activate, movie);
    assert(navigation.type == DetailsNavigationActionType::OpenSimilar);
    assert(navigation.item.has_value());
    assert(navigation.item->id == "similar");

    state.beginDetails();

    JellyfinItem series;
    series.id = "series";
    series.type = "Series";
    JellyfinItem season;
    season.id = "season";
    state.setEpisodeSeriesContext(series, {season});

    JellyfinItem episode;
    episode.id = "episode";
    episode.type = "Episode";
    navigation = DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Down, episode);
    assert(navigation.type == DetailsNavigationActionType::None);
    navigation = DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Activate, episode);
    assert(navigation.type == DetailsNavigationActionType::OpenEpisodeSeries);
    assert(navigation.item->id == "series");
    navigation = DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Right, episode);
    assert(navigation.type == DetailsNavigationActionType::None);
    navigation = DetailsNavigationController::handleDetails(state, DetailsNavigationKey::Activate, episode);
    assert(navigation.type == DetailsNavigationActionType::OpenEpisodeSeason);
    assert(navigation.item->id == "season");

    JellyfinPerson first;
    first.id = "person-1";
    first.name = "One";
    JellyfinPerson second;
    second.id = "person-2";
    second.name = "Two";
    std::vector<JellyfinPerson> people{first, second};
    state.resetCastSelection();
    navigation = DetailsNavigationController::handleCast(state, DetailsNavigationKey::Right, people, 5);
    assert(navigation.type == DetailsNavigationActionType::None);
    navigation = DetailsNavigationController::handleCast(state, DetailsNavigationKey::Activate, people, 5);
    assert(navigation.type == DetailsNavigationActionType::OpenPerson);
    assert(navigation.person->id == "person-2");

    state.beginPerson(first);
    JellyfinItem personItem;
    personItem.id = "person-item";
    state.setPersonItems({personItem});
    navigation = DetailsNavigationController::handlePersonItems(state, DetailsNavigationKey::Context, 5);
    assert(navigation.type == DetailsNavigationActionType::OpenPersonItemContext);
    assert(navigation.item->id == "person-item");
    navigation = DetailsNavigationController::handlePersonItems(state, DetailsNavigationKey::Activate, 5);
    assert(navigation.type == DetailsNavigationActionType::OpenPersonItem);
    assert(navigation.item->id == "person-item");

    state.beginItemMenu();
    navigation =
        DetailsNavigationController::handleItemMenu(state, DetailsNavigationKey::Activate, movie, false, false, false);
    assert(navigation.type == DetailsNavigationActionType::ActivateItemMenuAction);
    assert(navigation.itemMenuAction == ItemMenuAction::ToggleFavorite);

    for (int i = 0; i < 4; ++i) {
        navigation =
            DetailsNavigationController::handleItemMenu(state, DetailsNavigationKey::Down, movie, false, false, false);
        assert(navigation.type == DetailsNavigationActionType::None);
    }
    navigation =
        DetailsNavigationController::handleItemMenu(state, DetailsNavigationKey::Activate, movie, false, false, false);
    assert(navigation.type == DetailsNavigationActionType::None);
    assert(state.deleteConfirmation());
    navigation =
        DetailsNavigationController::handleItemMenu(state, DetailsNavigationKey::Left, movie, false, false, false);
    assert(navigation.type == DetailsNavigationActionType::None);
    navigation =
        DetailsNavigationController::handleItemMenu(state, DetailsNavigationKey::Activate, movie, false, false, false);
    assert(navigation.type == DetailsNavigationActionType::ConfirmDelete);

    state.beginSeries(series);
    state.setSeasons({season});
    navigation = DetailsNavigationController::handleSeasons(state, DetailsNavigationKey::Activate, 5);
    assert(navigation.type == DetailsNavigationActionType::OpenSeason);
    assert(navigation.item->id == "season");
    navigation = DetailsNavigationController::handleSeasons(state, DetailsNavigationKey::Back, 5);
    assert(navigation.type == DetailsNavigationActionType::Back);
    assert(navigation.item->id == "series");

    state.beginSeason(season);
    state.setEpisodes({episode});
    navigation = DetailsNavigationController::handleEpisodes(state, DetailsNavigationKey::Context, 5);
    assert(navigation.type == DetailsNavigationActionType::OpenEpisodeContext);
    assert(navigation.item->id == "episode");
    navigation = DetailsNavigationController::handleEpisodes(state, DetailsNavigationKey::Activate, 5);
    assert(navigation.type == DetailsNavigationActionType::OpenEpisode);
    assert(navigation.item->id == "episode");
    navigation = DetailsNavigationController::handleEpisodes(state, DetailsNavigationKey::Back, 5);
    assert(navigation.type == DetailsNavigationActionType::Back);

    return 0;
}
