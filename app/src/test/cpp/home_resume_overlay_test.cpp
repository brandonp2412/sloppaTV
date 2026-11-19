#include "home_resume_overlay.hpp"

#include <cassert>
#include <algorithm>

int main() {
    JellyfinSession session;
    session.server = "http://localhost";
    session.userId = "person";
    session.token = "test";

    JellyfinItem active;
    active.id = "tatami-5";
    active.type = "Episode";
    active.seriesId = "tatami";
    active.seriesName = "The Tatami Time Machine Blues";
    active.positionTicks = 100;
    active.runtimeTicks = 1000;

    JellyfinItem sameEpisode = active;
    JellyfinItem nextEpisode = active;
    nextEpisode.id = "tatami-6";
    JellyfinItem other;
    other.id = "other-1";
    other.type = "Episode";
    other.seriesId = "different";
    other.seriesName = "Different Show";

    HomeResumeOverlay overlay;
    JellyfinHomeData home;
    home.rows = {{"Continue Watching", {active}}, {"Next Up", {sameEpisode, nextEpisode, other}}};
    overlay.apply(session, home);
    assert(home.rows.size() == 2);
    assert(home.rows[0].title == "Continue Watching");
    assert(home.rows[1].title == "Next Up");
    assert(home.rows[1].items.size() == 1 && home.rows[1].items[0].id == other.id);

    // Local resume must also override stale server-reported Next Up.
    JellyfinHomeData local;
    local.rows = {{"Next Up", {sameEpisode, other}}};
    overlay.remember(session, active, active.positionTicks, false);
    overlay.apply(session, local);
    assert(local.rows[0].title == "Continue Watching");
    assert(local.rows[0].items[0].id == active.id);
    assert(local.rows[1].items.size() == 1 && local.rows[1].items[0].id == other.id);

    // After backing out, a stale server resume list must not bury the just-played
    // episode or overwrite its final locally observed progress.
    JellyfinHomeData staleServer;
    JellyfinItem older = other;
    older.positionTicks = 500;
    JellyfinItem staleActive = active;
    staleActive.positionTicks = 20;
    staleServer.rows = {{"Continue Watching", {older, staleActive}}};
    overlay.remember(session, active, 320, false);
    overlay.apply(session, staleServer);
    assert(staleServer.rows[0].items.size() == 2);
    assert(staleServer.rows[0].items[0].id == active.id);
    assert(staleServer.rows[0].items[0].positionTicks == 320);
    assert(staleServer.rows[0].items[1].id == older.id);

    JellyfinHomeData noOther;
    noOther.rows = {{"Continue Watching", {active}}, {"Next Up", {sameEpisode}}};
    overlay.apply(session, noOther);
    assert(noOther.rows.size() == 1 && noOther.rows[0].title == "Continue Watching");

    // Watched completion must clear both the local resume overlay and any
    // stale server row loaded before the playback-stopped report was applied.
    overlay.remember(session, active, active.runtimeTicks, true);
    JellyfinHomeData completedStale;
    completedStale.rows = {{"Continue Watching", {active, older}}, {"My Media", {}}};
    overlay.apply(session, completedStale);
    assert(completedStale.rows[0].title == "Continue Watching");
    assert(completedStale.rows[0].items.size() == 1);
    assert(completedStale.rows[0].items[0].id == older.id);
    JellyfinHomeData completedOnly;
    completedOnly.rows = {{"Continue Watching", {active}}};
    overlay.apply(session, completedOnly);
    assert(completedOnly.rows.empty());
    // A stale partial Friends S4E9 rewatch must not suppress the actual S4E19
    // Next Up after finishing S4E18.
    JellyfinItem olderFriends = active;
    olderFriends.id = "friends-s4e9";
    olderFriends.seriesId = "friends";
    olderFriends.seriesName = "Friends";
    olderFriends.parentIndexNumber = 4;
    olderFriends.indexNumber = 9;
    JellyfinItem nextFriends = olderFriends;
    nextFriends.id = "friends-s4e19";
    nextFriends.indexNumber = 19;
    HomeResumeOverlay friendsOverlay;
    JellyfinHomeData friendsHome;
    friendsHome.rows = {{"Continue Watching", {olderFriends}}, {"Next Up", {nextFriends}}};
    friendsOverlay.apply(session, friendsHome);
    assert(friendsHome.rows.size() == 2);
    assert(friendsHome.rows[1].items.size() == 1 && friendsHome.rows[1].items[0].id == nextFriends.id);

    // An incomplete immediate predecessor still takes precedence over Next Up.
    JellyfinItem currentFriends = olderFriends;
    currentFriends.id = "friends-s4e18";
    currentFriends.indexNumber = 18;
    JellyfinHomeData currentHome;
    currentHome.rows = {{"Continue Watching", {currentFriends}}, {"Next Up", {nextFriends}}};
    friendsOverlay.apply(session, currentHome);
    assert(currentHome.rows.size() == 1 && currentHome.rows[0].title == "Continue Watching");

    // Hidden resume episodes must not mask a visible Next Up episode.
    JellyfinHomeData hiddenHome;
    hiddenHome.rows = {{"Continue Watching", {currentFriends}}, {"Next Up", {nextFriends}}};
    friendsOverlay.apply(session, hiddenHome, HomeResumeOverlay::Clock::now(),
                         [&](const JellyfinItem& item) { return item.id == currentFriends.id; });
    assert(hiddenHome.rows.size() == 1 && hiddenHome.rows[0].title == "Next Up");
    assert(hiddenHome.rows[0].items[0].id == nextFriends.id);

    friendsOverlay.remember(session, olderFriends, 200, false);
    JellyfinHomeData localOlderFriends;
    localOlderFriends.rows = {{"Next Up", {nextFriends}}};
    friendsOverlay.apply(session, localOlderFriends);
    assert(localOlderFriends.rows.size() == 2 && localOlderFriends.rows[1].items[0].id == nextFriends.id);
    return 0;
}
