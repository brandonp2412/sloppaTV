#include "seerr_home_projection.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
SeerrMediaItem media(std::string id, int tmdbId) {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = item.id;
    item.mediaType = "movie";
    item.tmdbId = tmdbId;
    item.requested = true;
    item.requestId = tmdbId + 100;
    return item;
}
} // namespace

int main() {
    std::vector<JellyfinHomeRow> rows{
        {.title = "Continue watching", .items = {}},
        {.title = "Latest movies", .items = {}},
    };
    std::vector<SeerrMediaItem> pending{
        media("seerr:movie:10", 10),
        media("seerr:movie:20", 20),
    };

    projectSeerrHomeRow(rows, pending);
    assert(rows.size() == 3);
    assert(rows[0].title == "Continue watching");
    assert(rows[1].title == kSeerrHomeRowTitle);
    assert(rows[2].title == "Latest movies");
    assert(rows[1].items.size() == 2);
    assert(rows[1].items[0].id == "seerr:movie:10");
    assert(rows[1].items[0].externalSource == "seerr");
    assert(rows[1].items[0].externalRequestId == 110);
    assert(rows[1].items[1].id == "seerr:movie:20");

    projectSeerrHomeRow(rows, {media("seerr:movie:30", 30)});
    assert(rows.size() == 3);
    assert(rows[1].title == kSeerrHomeRowTitle);
    assert(rows[1].items.size() == 1);
    assert(rows[1].items[0].id == "seerr:movie:30");

    projectSeerrHomeRow(rows, {});
    assert(rows.size() == 2);
    assert(rows[0].title == "Continue watching");
    assert(rows[1].title == "Latest movies");

    std::vector<JellyfinHomeRow> empty;
    projectSeerrHomeRow(empty, {media("seerr:movie:40", 40)});
    assert(empty.size() == 1);
    assert(empty.front().title == kSeerrHomeRowTitle);
    assert(empty.front().items.front().id == "seerr:movie:40");
    return 0;
}
