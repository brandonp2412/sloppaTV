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
    const auto actions = state.actions(series, false);
    assert(actions.front() == "PLAY NEXT");
    assert(actions[1] == "EPISODES");
    assert(actions.back() == "BACK");

    state.moveAction(1, static_cast<int>(actions.size()));
    assert(state.actionSelection() == 1);
    state.moveAction(-10, static_cast<int>(actions.size()));
    assert(state.actionSelection() == 0);

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

    state.beginItemMenu();
    auto menu = state.itemMenuActions(series, true, true, false);
    assert(menu.front() == "PLAY ALL");
    assert(menu[1] == "PLAY EXTERNAL");
    assert(menu[2] == "VIEW QUEUE");
    assert(menu[menu.size() - 2] == "DELETE MEDIA");
    state.moveItemMenu(3, static_cast<int>(menu.size()));
    assert(state.itemMenuSelection() == 3);
    state.setDeleteConfirmation(true);
    assert(state.deleteConfirmation());
    state.setDeleteConfirmationSelection(0);
    assert(state.deleteConfirmationSelection() == 0);
    state.setDeleteConfirmation(false);
    assert(!state.deleteConfirmation());
    assert(state.deleteConfirmationSelection() == 1);

    state.resetCastSelection();
    state.moveCastSelection(series.people, 1, 0, 5);
    assert(state.castSelection() == 1);
    state.moveCastSelection(series.people, 0, 1, 5);
    assert(state.castSelection() == 1);
    state.moveCastSelection(series.people, -1, 0, 5);
    assert(state.castSelection() == 0);
    assert(state.selectedCastPerson(series.people)->name == "One");

    state.beginPerson(series.people.front());
    std::vector<JellyfinItem> personItems(6);
    for (size_t i = 0; i < personItems.size(); ++i) personItems[i].id = "person-item-" + std::to_string(i);
    state.setPersonItems(std::move(personItems));
    assert(state.selectedPerson().id == "p1");
    state.movePersonItem(1, 0, 5);
    assert(state.personItemSelection() == 1);
    assert(state.selectedPersonItem()->id == "person-item-1");

    JellyfinItem seasonOne;
    seasonOne.id = "season-1";
    seasonOne.name = "Season 1";
    JellyfinItem seasonTwo;
    seasonTwo.id = "season-2";
    seasonTwo.name = "Season 2";
    state.beginSeries(series);
    state.setSeasons({seasonOne, seasonTwo});
    assert(state.seriesDetail().id == "series");
    state.moveSeason(1, 0, 5);
    assert(state.selectedSeasonItem()->id == "season-2");

    state.beginSeason(seasonTwo);
    JellyfinItem episodeOne;
    episodeOne.id = "episode-1";
    JellyfinItem episodeTwo;
    episodeTwo.id = "episode-2";
    state.setEpisodes({episodeOne, episodeTwo});
    state.moveEpisode(1, 0, 5);
    assert(state.selectedSeason().id == "season-2");
    assert(state.selectedEpisodeItem()->id == "episode-2");

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
    assert(state.itemMenuSelection() == 0);
    return 0;
}
