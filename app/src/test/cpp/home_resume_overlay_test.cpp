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

    JellyfinHomeData noOther;
    noOther.rows = {{"Continue Watching", {active}}, {"Next Up", {sameEpisode}}};
    overlay.apply(session, noOther);
    assert(noOther.rows.size() == 1 && noOther.rows[0].title == "Continue Watching");
    return 0;
}
