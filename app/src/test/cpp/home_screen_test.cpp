#include "home_screen.hpp"
#include "home_resume_overlay.hpp"
#include "home_resume_merge.hpp"
#include "home_visibility.hpp"

#include <cassert>
#include <filesystem>

int main() {
    assert(homeRowDropsItemWhenPlayed("Continue Watching"));
    assert(homeRowDropsItemWhenPlayed("Next Up"));
    assert(!homeRowDropsItemWhenPlayed("Favorites"));
    assert(!homeRowDropsItemWhenPlayed("Latest Movies"));
    assert(!homeRowDropsItemWhenPlayed("My Media"));
    assert(homeImageKind(true, true, true) == ArtworkKind::Primary);
    assert(homeImageKind(false, true, true) == ArtworkKind::Thumb);
    assert(homeImageKind(false, false, true) == ArtworkKind::Backdrop);
    assert(preferHomeLandscapeArtwork("Movie"));
    assert(preferHomeLandscapeArtwork("Episode"));
    assert(preferHomeLandscapeArtwork("Series"));
    assert(preferHomeLandscapeArtwork("BoxSet"));
    assert(!preferHomeLandscapeArtwork("UserView"));
    assert(!preferHomeLandscapeArtwork("CollectionFolder"));
    assert(!preferHomeLandscapeArtwork("Folder"));

    const ArtworkReference episodeArtwork = homeArtworkReference("episode-id", "episode-tag", "series-id", "series-tag",
                                                                 true, "thumb-tag", "backdrop-tag", "episode-id");
    assert(episodeArtwork.itemId == "episode-id");
    assert(episodeArtwork.tag == "thumb-tag");
    assert(episodeArtwork.kind == ArtworkKind::Thumb);

    const ArtworkReference episodePrimaryArtwork =
        homeArtworkReference("episode-id", "episode-primary", "series-id", "series-tag", true, "", "", "series-id");
    assert(episodePrimaryArtwork.itemId == "episode-id");
    assert(episodePrimaryArtwork.tag == "episode-primary");
    assert(episodePrimaryArtwork.kind == ArtworkKind::Primary);

    const ArtworkReference parentBackdropArtwork =
        homeArtworkReference("episode-id", "", "series-id", "series-tag", true, "", "backdrop-tag", "series-id");
    assert(parentBackdropArtwork.itemId == "series-id");
    assert(parentBackdropArtwork.tag == "backdrop-tag");
    assert(parentBackdropArtwork.kind == ArtworkKind::Backdrop);

    const ArtworkReference thumbArtwork =
        homeArtworkReference("movie-id", "", "", "", false, "thumb-tag", "backdrop-tag", "movie-id");
    assert(thumbArtwork.itemId == "movie-id");
    assert(thumbArtwork.tag == "thumb-tag");
    assert(thumbArtwork.kind == ArtworkKind::Thumb);

    assert(homeFirstVisibleRow(0, 0, 5) == 0);
    assert(homeFirstVisibleRow(0, 1, 5) == 0);
    assert(homeFirstVisibleRow(0, 2, 5) == 1);
    assert(homeFirstVisibleRow(1, 1, 5) == 1);
    assert(homeFirstVisibleRow(1, 0, 5) == 0);
    assert(homeFirstVisibleRow(3, -1, 5) == 3);

    HomeScreenState state;
    state.reset();
    state.setSelections({0, 1, 2});
    assert(state.row() == 0);
    assert(state.navIndex() == 1);
    assert(state.selection(1, 4) == 1);
    state.moveSelection(1, 1, 4);
    assert(state.selection(1, 4) == 2);

    state.setSelections({0});
    for (int i = 0; i < 4; ++i) {
        state.moveSelection(0, 1, 8);
        state.updateItemViewport(0, 8, 5);
    }
    assert(state.selection(0, 8) == 4);
    assert(state.firstVisibleItem(0, 8, 5) == 0);
    state.moveSelection(0, 1, 8);
    state.updateItemViewport(0, 8, 5);
    assert(state.selection(0, 8) == 5);
    assert(state.firstVisibleItem(0, 8, 5) == 1);
    state.moveSelection(0, -1, 8);
    state.updateItemViewport(0, 8, 5);
    assert(state.selection(0, 8) == 4);
    assert(state.firstVisibleItem(0, 8, 5) == 1);

    state.setSelections({0, 1, 2});
    state.setRow(2);
    state.updateViewport(5);
    assert(state.firstVisibleRow() == 1);
    state.focusToolbar(3);
    assert(state.row() == -1);
    assert(state.navIndex() == 3);
    state.moveToolbar(1);
    assert(state.navIndex() == 0);
    state.moveToolbar(1);
    assert(state.navIndex() == 1);
    state.moveToolbar(-1);
    assert(state.navIndex() == 0);
    state.moveToolbar(-1);
    assert(state.navIndex() == 3);
    state.moveToolbar(-1);
    assert(state.navIndex() == 2);
    state.beginCenterPress();
    assert(state.centerPending());
    assert(state.consumeCenterRelease(true));
    state.beginCenterPress();
    state.markCenterLongPressed();
    assert(state.centerLongPressed());
    assert(!state.consumeCenterRelease(true));

    HomeScreenState inputState;
    inputState.reset();
    inputState.setSelections({0, 0});
    inputState.focusToolbar(1);
    auto toolbarCommand = inputState.handleToolbarInput(HomeToolbarInput::Left, 2);
    assert(toolbarCommand.type == HomeToolbarCommandType::None);
    assert(inputState.navIndex() == 0);
    toolbarCommand = inputState.handleToolbarInput(HomeToolbarInput::Activate, 2);
    assert(toolbarCommand.type == HomeToolbarCommandType::OpenProfiles);
    inputState.focusToolbar(2);
    toolbarCommand = inputState.handleToolbarInput(HomeToolbarInput::Activate, 2);
    assert(toolbarCommand.type == HomeToolbarCommandType::OpenSearch);
    inputState.focusToolbar(3);
    toolbarCommand = inputState.handleToolbarInput(HomeToolbarInput::Activate, 2);
    assert(toolbarCommand.type == HomeToolbarCommandType::OpenSettings);
    inputState.focusToolbar(1);
    toolbarCommand = inputState.handleToolbarInput(HomeToolbarInput::Activate, 2);
    assert(toolbarCommand.type == HomeToolbarCommandType::None);
    toolbarCommand = inputState.handleToolbarInput(HomeToolbarInput::Down, 2);
    assert(inputState.row() == 0);

    auto rowCommand = inputState.handleRowInput(HomeRowInput::Right, 2, 3, true);
    assert(rowCommand.type == HomeRowCommandType::None);
    assert(inputState.selection(0, 3) == 1);
    rowCommand = inputState.handleRowInput(HomeRowInput::Context, 2, 3, true);
    assert(rowCommand.type == HomeRowCommandType::OpenContext);
    rowCommand = inputState.handleRowInput(HomeRowInput::Context, 2, 3, false);
    assert(rowCommand.type == HomeRowCommandType::None);
    rowCommand = inputState.handleRowInput(HomeRowInput::Activate, 2, 3, true);
    assert(rowCommand.type == HomeRowCommandType::OpenSelected);
    rowCommand = inputState.handleRowInput(HomeRowInput::Down, 2, 3, true);
    assert(inputState.row() == 1);
    rowCommand = inputState.handleRowInput(HomeRowInput::Up, 2, 3, true);
    assert(inputState.row() == 0);
    rowCommand = inputState.handleRowInput(HomeRowInput::Up, 2, 3, true);
    assert(inputState.row() == -1);

    JellyfinHomeRow myMedia;
    myMedia.title = "My Media";
    myMedia.items = {JellyfinItem{}, JellyfinItem{}};
    myMedia.items[0].id = "library-1";
    myMedia.items[1].id = "library-2";
    JellyfinHomeRow latest;
    latest.title = "Latest Movies";
    latest.items = {JellyfinItem{}, JellyfinItem{}, JellyfinItem{}};
    latest.items[0].id = "movie-1";
    latest.items[1].id = "movie-2";
    latest.items[2].id = "movie-3";
    const std::vector<JellyfinHomeRow> unifiedRows{myMedia, latest};

    HomeScreenState unifiedState;
    unifiedState.reset();
    unifiedState.setSelections({0, 0});
    unifiedState.focusToolbar(3);
    auto screenCommand = unifiedState.handleInput(HomeScreenInput::Activate, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::OpenSettings);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Search, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::OpenSearch);

    unifiedState.focusToolbar(1);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Down, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::None);
    assert(!screenCommand.finalizeRowNavigation);
    assert(unifiedState.row() == 0);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Context, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::None);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Right, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::None);
    assert(screenCommand.finalizeRowNavigation);
    assert(unifiedState.selection(0, 2) == 1);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Activate, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::OpenSelected);
    assert(screenCommand.rowIndex == 0);
    assert(screenCommand.itemIndex == 1);

    screenCommand = unifiedState.handleInput(HomeScreenInput::Down, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::None);
    assert(unifiedState.row() == 1);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Context, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::OpenContext);
    assert(screenCommand.rowIndex == 1);
    assert(screenCommand.itemIndex == 0);
    screenCommand = unifiedState.handleInput(HomeScreenInput::Back, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::FinishActivity);

    unifiedState.setRow(9);
    screenCommand = unifiedState.handleInput(HomeScreenInput::None, unifiedRows);
    assert(screenCommand.type == HomeScreenCommandType::None);
    assert(unifiedState.row() == -1);

    JellyfinHomeRow continueWatching;
    continueWatching.title = "Continue Watching";
    continueWatching.items = {JellyfinItem{}, JellyfinItem{}, JellyfinItem{}};
    continueWatching.items[0].id = "a";
    continueWatching.items[1].id = "b";
    continueWatching.items[2].id = "c";
    JellyfinHomeRow nextUp;
    nextUp.title = "Next Up";
    nextUp.items = {JellyfinItem{}, JellyfinItem{}};
    nextUp.items[0].id = "d";
    nextUp.items[1].id = "e";

    state.setSelections({2, 1});
    state.setRow(1);
    const auto snapshot = state.snapshot({continueWatching, nextUp});
    assert(!snapshot.toolbarFocused);
    assert(snapshot.focusedRowTitle == "Next Up");
    assert(snapshot.selectedItemByRow.at("Continue Watching") == "c");
    assert(snapshot.selectedItemByRow.at("Next Up") == "e");

    JellyfinHomeRow reorderedNext = nextUp;
    reorderedNext.items = {nextUp.items[1], nextUp.items[0]};
    const auto plan = HomeScreenState::restorePlan(snapshot, {reorderedNext, continueWatching});
    assert(plan.focusedRow == 0);
    assert(plan.selections.size() == 2);
    assert(plan.selections[0] == 0);
    assert(plan.selections[1] == 2);

    state.focusToolbar(2);
    const auto toolbarSnapshot = state.snapshot({continueWatching, nextUp});
    const auto toolbarPlan = HomeScreenState::restorePlan(toolbarSnapshot, {nextUp, continueWatching});
    assert(toolbarSnapshot.toolbarFocused);
    assert(toolbarPlan.focusedRow == -1);
    // A nonempty server Resume list must not suppress a different partly watched episode.
    JellyfinItem resumedEpisode;
    resumedEpisode.id = "tatami-e2";
    resumedEpisode.type = "Episode";
    resumedEpisode.positionTicks = 400;
    resumedEpisode.runtimeTicks = 1000;
    JellyfinItem brieflyPlayed;
    brieflyPlayed.id = "tatami-e1";
    brieflyPlayed.type = "Episode";
    brieflyPlayed.positionTicks = 150;
    brieflyPlayed.runtimeTicks = 1000;
    // Some Jellyfin-compatible servers retain the played flag when rewatching.
    brieflyPlayed.played = true;
    JellyfinItem finished = brieflyPlayed;
    finished.id = "finished";
    finished.positionTicks = 1000;
    JellyfinItem unstarted = brieflyPlayed;
    unstarted.id = "unstarted";
    unstarted.positionTicks = 0;
    std::vector<JellyfinItem> serverResume{resumedEpisode};
    mergeHomeResumeCandidates(serverResume, {brieflyPlayed, resumedEpisode, finished, unstarted});
    assert(serverResume.size() == 2);
    assert(serverResume[0].id == "tatami-e2");
    assert(serverResume[1].id == "tatami-e1");
    assert(serverResume[1].positionTicks == 150);
    JellyfinItem trustedResume = resumedEpisode;
    trustedResume.positionTicks = 0; // Some server responses omit UserData.
    std::vector<JellyfinItem> trusted{trustedResume};
    mergeHomeResumeCandidates(trusted, {brieflyPlayed});
    assert(trusted.size() == 2 && trusted.front().id == "tatami-e2");
    std::vector<JellyfinItem> emptyResume;
    mergeHomeResumeCandidates(emptyResume, {brieflyPlayed});
    assert(emptyResume.size() == 1 && emptyResume.front().id == "tatami-e1");

    // A just-stopped episode is visible even if the server Resume feed is still empty.
    HomeResumeOverlay overlay;
    JellyfinSession brandog;
    brandog.server = "https://remux.example";
    brandog.userId = "brandog";
    brandog.token = "test";
    JellyfinSession other = brandog;
    other.userId = "other";
    JellyfinItem tatami;
    tatami.id = "tatami-s1e1";
    tatami.type = "Episode";
    tatami.name = "Tatami Galaxy";
    tatami.seasonName = "Season 1";
    tatami.parentIndexNumber = 1;
    tatami.indexNumber = 1;
    tatami.runtimeTicks = 1000;
    tatami.played = true; // Rewatch of an episode marked Watched on Remux.
    const auto now = HomeResumeOverlay::Clock::now();
    overlay.remember(brandog, tatami, 250, false, now);
    JellyfinHomeData remuxHome;
    JellyfinItem nextEpisode;
    nextEpisode.id = "tatami-s2e1";
    remuxHome.rows = {{"My Media", {}}, {"Next Up", {nextEpisode}}};
    overlay.apply(brandog, remuxHome, now);
    assert(remuxHome.rows.size() == 3);
    assert(remuxHome.rows[0].title == "Continue Watching");
    assert(remuxHome.rows[0].items.size() == 1);
    assert(remuxHome.rows[0].items[0].id == "tatami-s1e1");
    assert(remuxHome.rows[0].items[0].positionTicks == 250);
    assert(remuxHome.rows[1].title == "Next Up");
    assert(remuxHome.rows[2].title == "My Media");

    // The server becomes authoritative once it reports the same episode.
    remuxHome.rows[0].items[0].positionTicks = 300;
    overlay.apply(brandog, remuxHome, now);
    assert(remuxHome.rows[0].items.size() == 1);
    assert(remuxHome.rows[0].items[0].positionTicks == 300);

    // A partially watched rewatch survives a full app restart when the server
    // still reports it as Watched and omits it from Resume.
    const auto directory = std::filesystem::temp_directory_path() /
                           ("sloppatv-resume-test-" + std::to_string(now.time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    overlay.persist(directory.string());
    HomeResumeOverlay restored;
    restored.restore(directory.string());
    JellyfinHomeData restartedHome;
    restored.apply(brandog, restartedHome);
    assert(restartedHome.rows.size() == 1);
    assert(restartedHome.rows[0].title == "Continue Watching");
    assert(restartedHome.rows[0].items[0].id == "tatami-s1e1");
    assert(restartedHome.rows[0].items[0].seasonName == "Season 1");
    assert(restartedHome.rows[0].items[0].parentIndexNumber == 1);
    assert(restartedHome.rows[0].items[0].indexNumber == 1);
    std::filesystem::remove_all(directory);

    JellyfinHomeData otherHome;
    otherHome.rows = {{"My Media", {}}, {"Next Up", {}}};
    overlay.apply(other, otherHome, now);
    assert(otherHome.rows.size() == 2);
    assert(otherHome.rows[0].title == "Next Up");

    std::unordered_set<std::string> hidden = {brandog.server + "\n" + brandog.userId + "\n" + tatami.id};
    HomeVisibility visibility(brandog, hidden);
    JellyfinHomeData hiddenHome;
    overlay.apply(brandog, hiddenHome, now);
    visibility.filter(hiddenHome);
    assert(hiddenHome.rows.empty());

    std::unordered_set<std::string> toggleHidden;
    HomeVisibility toggleVisibility(brandog, toggleHidden);
    JellyfinHomeData toggleHome;
    toggleHome.rows = {{"Continue Watching", {tatami}}, {"Next Up", {nextEpisode}}};
    HomeScreenState toggleState;
    toggleState.setSelections({0, 0});
    toggleState.setRow(1);
    assert(toggleVisibility.toggle(tatami, toggleHome, toggleState) == true);
    assert(toggleHome.rows.size() == 1);
    assert(toggleHome.rows[0].title == "Next Up");
    assert(toggleState.row() == 0);

    JellyfinHomeData lateHome;
    lateHome.rows = {{"My Media", {}}};
    overlay.apply(brandog, lateHome, now + std::chrono::minutes(11));
    assert(lateHome.rows.size() == 2);
    assert(lateHome.rows[0].title == "Continue Watching");
    JellyfinHomeData expiredHome;
    expiredHome.rows = {{"My Media", {}}};
    overlay.apply(brandog, expiredHome, now + std::chrono::hours(24 * 31));
    assert(expiredHome.rows.size() == 1);
    // Both manual watched and unwatched reset resume. Local overlay must not
    // restore an old position or accept a stale in-flight server home result.
    overlay.forget(brandog, tatami.id, now);
    JellyfinHomeData staleHome;
    staleHome.rows = {{"Continue Watching", {tatami}}, {"Next Up", {nextEpisode}}};
    overlay.apply(brandog, staleHome, now + std::chrono::seconds(1));
    assert(staleHome.rows.size() == 1);
    assert(staleHome.rows[0].title == "Next Up");
    JellyfinHomeData otherAfterForget;
    otherAfterForget.rows = {{"Continue Watching", {tatami}}};
    overlay.apply(other, otherAfterForget, now + std::chrono::seconds(1));
    assert(otherAfterForget.rows[0].items.size() == 1);
    std::filesystem::create_directories(directory);
    overlay.persist(directory.string());
    HomeResumeOverlay forgottenAfterRestart;
    forgottenAfterRestart.restore(directory.string());
    JellyfinHomeData afterRestart;
    forgottenAfterRestart.apply(brandog, afterRestart);
    assert(afterRestart.rows.empty());
    std::filesystem::remove_all(directory);
    // Starting a new partial rewatch makes the item eligible again.
    overlay.remember(brandog, tatami, 400, false, now);
    JellyfinHomeData rewatchedHome;
    overlay.apply(brandog, rewatchedHome, now);
    assert(rewatchedHome.rows[0].items.size() == 1);
    overlay.remember(brandog, tatami, 0, true, now);
    JellyfinHomeData completedHome;
    overlay.apply(brandog, completedHome, now);
    assert(completedHome.rows.empty());
    return 0;
}
