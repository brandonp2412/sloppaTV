#include "content_mutation_flow.hpp"

#include <cassert>
#include <utility>

namespace {
JellyfinItem item() {
    JellyfinItem value;
    value.id = "episode-1";
    value.name = "Episode";
    value.type = "Episode";
    return value;
}
} // namespace

int main() {
    JellyfinItem detail = item();

    JellyfinHomeData home;
    JellyfinHomeRow row;
    row.title = "Continue Watching";
    row.items.push_back(detail);
    home.rows.push_back(std::move(row));

    HomeScreenState homeState;
    homeState.setSelections({0});
    homeState.setRow(0);
    BrowseScreenState browseState;
    std::vector<SeerrMediaItem> seerrResults;
    SearchScreenState searchState(seerrResults);
    DetailsScreenState detailsState;
    PlaybackQueueState queueState;

    ContentMutationFlow flow;
    const auto preparation =
        flow.preparePlayedToggle(home, homeState, browseState, searchState, detailsState, queueState, detail, false, 7);
    assert(preparation.item.id == "episode-1");
    assert(!preparation.item.played);
    assert(detail.played);
    assert(home.rows.front().items.empty());

    flow.begin();
    assert(flow.loading());
    flow.rejectPlayedToggle(preparation.item, false, home, homeState, browseState, searchState, detailsState,
                            queueState, detail);

    assert(!flow.loading());
    assert(!detail.played);
    assert(home.rows.size() == 1);
    assert(home.rows.front().items.size() == 1);
    assert(home.rows.front().items.front().id == "episode-1");
    assert(!home.rows.front().items.front().played);
    assert(homeState.row() == 0);

    return 0;
}
