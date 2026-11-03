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

    // Unwatched also clears the old resume position, not just the played flag.
    detail.played = true;
    detail.positionTicks = 40;
    home.rows[0].items = {detail};
    const auto unwatch =
        flow.preparePlayedToggle(home, homeState, browseState, searchState, detailsState, queueState, detail, false, 8);
    assert(!unwatch.desired);
    assert(detail.positionTicks == 0);
    assert(!detail.played);
    assert(home.rows[0].items.empty());
    flow.rejectPlayedToggle(unwatch.item, false, home, homeState, browseState, searchState, detailsState, queueState,
                            detail);
    assert(detail.played && detail.positionTicks == 40);
    assert(home.rows[0].items.size() == 1);
    assert(home.rows[0].items[0].positionTicks == 40);

    // Remux success must commit the optimistic state, not restore the old home row.
    const auto committedUnwatch =
        flow.preparePlayedToggle(home, homeState, browseState, searchState, detailsState, queueState, detail, false, 9);
    assert(!committedUnwatch.desired);
    flow.begin();
    PlayedCompletion unwatchCompletion;
    unwatchCompletion.item = committedUnwatch.item;
    unwatchCompletion.desired = false;
    unwatchCompletion.sessionEpoch = 9;
    unwatchCompletion.result.ok = true;
    const auto unwatchEffects = flow.complete(unwatchCompletion, true, true, false, false, home, homeState, browseState,
                                              searchState, detailsState, queueState, detail);
    assert(!unwatchEffects.error);
    assert(!flow.loading());
    assert(!detail.played && detail.positionTicks == 0);
    assert(home.rows[0].items.empty());

    // The watched action must likewise keep the item removed from Continue Watching.
    detail = item();
    home.rows[0].items = {detail};
    const auto committedWatch = flow.preparePlayedToggle(home, homeState, browseState, searchState, detailsState,
                                                         queueState, detail, false, 10);
    assert(committedWatch.desired);
    flow.begin();
    PlayedCompletion watchCompletion;
    watchCompletion.item = committedWatch.item;
    watchCompletion.desired = true;
    watchCompletion.sessionEpoch = 10;
    watchCompletion.result.ok = true;
    const auto watchEffects = flow.complete(watchCompletion, true, true, false, false, home, homeState, browseState,
                                            searchState, detailsState, queueState, detail);
    assert(!watchEffects.error);
    assert(!flow.loading());
    assert(detail.played && detail.positionTicks == 0);
    assert(home.rows[0].items.empty());

    return 0;
}
