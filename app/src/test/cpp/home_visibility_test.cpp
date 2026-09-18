#include "home_visibility.hpp"

#include <cassert>
#include <unordered_set>

namespace {
JellyfinItem item(std::string id) {
    JellyfinItem value;
    value.id = std::move(id);
    return value;
}
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    const std::string hiddenKey = homeVisibilityKey(session, "movie-1");
    assert(hiddenKey == "https://jellyfin.example.nz\nuser-1\nmovie-1");

    std::unordered_set<std::string> hidden{hiddenKey};
    assert(homeItemHidden(session, item("movie-1"), hidden));
    assert(!homeItemHidden(session, item("movie-2"), hidden));
    assert(!homeItemHidden(session, item(""), hidden));

    JellyfinHomeData data;
    data.rows = {
        {.title = "Continue Watching", .items = {item("movie-1"), item("movie-2")}},
        {.title = "My Media", .items = {item("movie-1"), item("movie-3")}},
    };
    filterHiddenHomeItems(data, session, hidden);
    assert(data.rows[0].items.size() == 1);
    assert(data.rows[0].items.front().id == "movie-2");
    assert(data.rows[1].items.size() == 2);

    JellyfinSession otherUser = session;
    otherUser.userId = "user-2";
    JellyfinHomeData otherData;
    otherData.rows = {{.title = "Continue Watching", .items = {item("movie-1")}}};
    filterHiddenHomeItems(otherData, otherUser, hidden);
    assert(otherData.rows.front().items.size() == 1);

    return 0;
}
