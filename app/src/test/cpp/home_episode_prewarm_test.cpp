#include "home_episode_prewarm.hpp"

#include <cassert>

int main() {
    HomeEpisodePrewarmState state;
    JellyfinSession account;
    account.server = "https://remux.example";
    account.userId = "user";
    JellyfinItem a, b, c, movie;
    a.id = "continue-1";
    a.type = "Episode";
    b.id = "next-1";
    b.type = "Episode";
    c.id = "next-2";
    c.type = "Episode";
    movie.id = "movie";
    movie.type = "Movie";
    JellyfinHomeData home;
    home.rows = {{"Next Up", {b, c, a, a, b, c}}, {"Continue Watching", {a, movie}}};
    state.refresh(account, home);
    const auto now = HomeEpisodePrewarmState::Clock::now();
    assert(!state.takeDue(now, false, true));
    assert(!state.takeDue(now, true, false));
    auto first = state.takeDue(now, true, true);
    assert(first && first->id == "next-1");
    assert(!state.takeDue(now + std::chrono::seconds(1), true, true));
    auto second = state.takeDue(now + std::chrono::seconds(2), true, true);
    assert(second && second->id == "next-2");
    auto third = state.takeDue(now + std::chrono::seconds(4), true, true);
    assert(third && third->id == "continue-1");
    auto fourth = state.takeDue(now + std::chrono::seconds(6), true, true);
    assert(fourth && fourth->id == "movie");
    assert(!state.takeDue(now + std::chrono::seconds(8), true, true));
    state.refresh(account, home);
    assert(!state.takeDue(now + std::chrono::seconds(10), true, true));
    state.refresh(account, home);
    assert(state.takeDue(now + std::chrono::minutes(3), true, true)->id == "next-1");
    JellyfinSession other = account;
    other.userId = "other";
    state.refresh(other, home);
    assert(state.takeDue(now + std::chrono::minutes(3) + std::chrono::seconds(2), true, true)->id == "next-1");

    JellyfinHomeData many;
    many.rows = {{"Next Up", {}}};
    for (int i = 0; i < 7; ++i) {
        JellyfinItem item;
        item.id = "ep-" + std::to_string(i);
        item.type = "Episode";
        many.rows[0].items.push_back(item);
    }
    state.refresh(other, many);
    for (int i = 0; i < 5; ++i) {
        const auto next = state.takeDue(now + std::chrono::minutes(4) + std::chrono::seconds(2 * i), true, true);
        assert(next && next->id == "ep-" + std::to_string(i));
    }
    assert(!state.takeDue(now + std::chrono::minutes(5), true, true));
    return 0;
}
