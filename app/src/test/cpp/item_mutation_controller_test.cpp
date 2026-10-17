#include "item_mutation_controller.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <utility>

namespace {
JellyfinItem item(std::string id) {
    JellyfinItem value;
    value.id = std::move(id);
    value.type = "Movie";
    return value;
}

JellyfinHomeData homeWithNextUp(const JellyfinItem& value) {
    JellyfinHomeData home;
    JellyfinHomeRow row;
    row.title = "Next Up";
    row.items.push_back(value);
    home.rows.push_back(std::move(row));
    return home;
}
} // namespace

int main() {
    {
        JellyfinItem detail = item("movie");
        FavoriteCompletion completion;
        completion.item = detail;
        completion.desired = true;
        completion.sessionEpoch = 4;
        completion.result.ok = true;
        auto effects = ItemMutationController::apply(completion, true, true, detail);
        assert(effects.finishLoading);
        assert(effects.cacheUpdate && effects.cacheUpdate->favorite);
        assert(detail.favorite);
    }

    {
        JellyfinItem detail = item("movie");
        FavoriteCompletion completion;
        completion.item = detail;
        completion.desired = true;
        completion.sessionEpoch = 4;
        completion.result.ok = false;
        completion.result.error = "favorite failed";
        auto effects = ItemMutationController::apply(completion, true, true, detail);
        assert(effects.finishLoading);
        assert(effects.error == "favorite failed");
        assert(!detail.favorite);
    }

    {
        // A watched episode should vanish from Next Up immediately, without waiting
        // for the server to return the next episode (or leaving the old card visible).
        JellyfinItem current = item("episode-1");
        JellyfinHomeData home = homeWithNextUp(current);
        HomeScreenState homeState;
        BrowseScreenState browse;
        const std::vector<SeerrMediaItem> seerrItems;
        SearchScreenState search(seerrItems);
        DetailsScreenState details;
        PlaybackQueueState queue;
        JellyfinItem detail = current;
        const auto prepared = ItemMutationController::preparePlayedToggle(home, homeState, browse, search, details,
                                                                          queue, detail, false, 9);
        assert(prepared.desired);
        assert(prepared.nextUpReplacementIndex == 0);
        assert(home.rows[0].items.empty());
        std::optional<PlayedRollbackState> rollback = prepared.rollback;
        PlayedCompletion completion;
        completion.item = current;
        completion.desired = true;
        completion.sessionEpoch = 9;
        completion.nextUpReplacementIndex = 0;
        completion.nextUpReplacement = item("episode-2");
        completion.result.ok = true;
        auto effects = ItemMutationController::apply(completion, true, true, false, rollback, home, homeState, detail);
        assert(home.rows[0].items.size() == 1);
        assert(home.rows[0].items[0].id == "episode-2");
        assert(effects.nextUpAnimation);
    }
    {
        JellyfinItem current = item("episode-1");
        JellyfinItem replacement = item("episode-2");
        JellyfinHomeData home = homeWithNextUp(current);
        HomeScreenState homeState;
        JellyfinItem detail = current;
        std::optional<PlayedRollbackState> rollback;
        PlayedCompletion completion;
        completion.item = current;
        completion.desired = true;
        completion.sessionEpoch = 9;
        completion.nextUpReplacementIndex = 0;
        completion.nextUpReplacement = replacement;
        completion.result.ok = true;
        auto effects = ItemMutationController::apply(completion, true, true, false, rollback, home, homeState, detail);
        assert(effects.finishLoading);
        assert(effects.nextUpAnimation && effects.nextUpAnimation->index == 0);
        assert(home.rows[0].items[0].id == "episode-2");
        assert(detail.played);
    }

    {
        JellyfinItem current = item("episode-1");
        JellyfinHomeData home = homeWithNextUp(current);
        HomeScreenState homeState;
        homeState.setRow(0);
        JellyfinItem detail = current;
        std::optional<PlayedRollbackState> rollback = PlayedRollbackState{
            .itemId = current.id,
            .sessionEpoch = 9,
            .previousHome = home,
            .previousHomeSelection = homeState.snapshot(home.rows),
        };
        home.rows.clear();
        PlayedCompletion completion;
        completion.item = current;
        completion.desired = true;
        completion.sessionEpoch = 9;
        completion.result.ok = false;
        completion.result.error = "played failed";
        auto effects = ItemMutationController::apply(completion, true, true, false, rollback, home, homeState, detail);
        assert(effects.playedRollback);
        ItemMutationController::restorePlayedRollback(effects, home, homeState);
        assert(!effects.playedRollback);
        assert(home.rows.size() == 1);
        assert(home.rows[0].items[0].id == "episode-1");
    }

    {
        MetadataRefreshCompletion completion;
        completion.result.ok = true;
        auto effects = ItemMutationController::apply(completion, true);
        assert(effects.finishLoading);
        assert(!effects.notice);
    }

    {
        JellyfinItem detail = item("delete-me");
        DetailsScreenState detailsState;
        detailsState.setDeleteConfirmation(true);
        DeleteItemCompletion completion;
        completion.itemId = detail.id;
        completion.sessionEpoch = 3;
        completion.result.ok = true;
        auto effects = ItemMutationController::apply(completion, true, true, detail, detailsState);
        assert(effects.finishLoading);
        assert(effects.removeCachedItemId == "delete-me");
        assert(effects.closeDeletedItem);
        assert(!effects.notice);
        assert(detail.id.empty());
        assert(!detailsState.deleteConfirmation());
    }

    return 0;
}
