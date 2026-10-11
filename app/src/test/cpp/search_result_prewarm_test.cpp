#include "search_result_prewarm.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

int main() {
    using namespace std::chrono_literals;
    SearchResultPrewarmState warm;
    const auto now = SearchResultPrewarmState::TimePoint{100s};
    JellyfinSession session;
    session.server = "https://remux.example";
    session.userId = "user-a";
    session.token = "token";
    session.deviceId = "device";
    std::vector<JellyfinItem> candidates;
    for (int i = 0; i < 9; ++i) {
        JellyfinItem item;
        item.id = "item-" + std::to_string(i);
        item.type = i == 0 ? "Series" : (i == 2 ? "Episode" : "Movie");
        candidates.push_back(std::move(item));
    }
    warm.refresh(session, "mobland", candidates, now);
    assert(!warm.takeDue(session, "mob", true, now).has_value());
    assert(!warm.takeDue(session, "mobland", false, now).has_value());
    auto first = warm.takeDue(session, "mobland", true, now);
    assert(first && first->id == "item-0");
    assert(!warm.takeDue(session, "mobland", true, now + 2s).has_value());
    auto second = warm.takeDue(session, "mobland", true, now + 3s);
    assert(second && second->id == "item-1");
    auto third = warm.takeDue(session, "mobland", true, now + 6s);
    assert(third && third->id == "item-2");
    assert(!warm.takeDue(session, "mobland", true, now + 30s).has_value());

    // Debouncing a new query replaces the pending candidates; already-warmed
    // items are not requeued when the user types a slightly different query.
    warm.refresh(session, "moblands", candidates, now + 7s);
    auto next = warm.takeDue(session, "moblands", true, now + 9s);
    assert(next && next->id == "item-3");
    warm.refresh(session, "another", candidates, now + 10s);
    assert(!warm.takeDue(session, "moblands", true, now + 12s).has_value());
    auto different = warm.takeDue(session, "another", true, now + 12s);
    assert(different && different->id == "item-4");

    JellyfinSession other = session;
    other.userId = "user-b";
    assert(!warm.takeDue(other, "another", true, now + 16s).has_value());
    warm.refresh(other, "another", candidates, now + 16s);
    auto newAccount = warm.takeDue(other, "another", true, now + 16s);
    assert(newAccount && newAccount->id == "item-0");

    std::vector<JellyfinItem> invalid(5);
    invalid[0].id = "folder";
    invalid[0].type = "Folder";
    invalid[1].id = "external";
    invalid[1].type = "Movie";
    invalid[1].externalSource = "seerr";
    invalid[2].id = "playable";
    invalid[2].type = "Movie";
    invalid[3] = invalid[2];
    invalid[4].id = "no-id";
    invalid[4].type = "Episode";
    warm.refresh(other, "abc", invalid, now + 20s);
    auto playable = warm.takeDue(other, "abc", true, now + 20s);
    assert(playable && playable->id == "playable");
    auto final = warm.takeDue(other, "abc", true, now + 23s);
    assert(final && final->id == "no-id");
    assert(!warm.takeDue(other, "abc", true, now + 26s).has_value());
    warm.refresh(other, "ab", invalid, now + 30s);
    assert(!warm.takeDue(other, "ab", true, now + 30s).has_value());
    warm.reset();
    assert(!warm.takeDue(other, "abc", true, now + 33s).has_value());
    return 0;
}
