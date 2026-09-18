#include "home_completion_controller.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace {
JellyfinItem item(std::string id) {
    JellyfinItem value;
    value.id = std::move(id);
    value.name = value.id;
    return value;
}

JellyfinHomeRow row(std::string title, std::vector<JellyfinItem> items) {
    JellyfinHomeRow value;
    value.title = std::move(title);
    value.items = std::move(items);
    return value;
}

HomeSelectionSnapshot snapshot() {
    HomeSelectionSnapshot value;
    value.focusedRowTitle = "Continue Watching";
    value.selectedItemByRow["Continue Watching"] = "episode-2";
    return value;
}
} // namespace

int main() {
    {
        JellyfinHomeData home;
        HomeScreenState state;
        HomeCoreCompletion completion;
        completion.generation = 1;
        completion.result.ok = true;
        completion.result.value.rows = {row("Continue Watching", {item("episode-1")})};

        const auto effects = HomeCompletionController::apply(completion, false, true, 0, home, state);
        assert(!effects.finishLoading);
        assert(home.rows.empty());
    }

    {
        JellyfinHomeData home;
        HomeScreenState state;
        HomeCoreCompletion completion;
        completion.generation = 2;
        completion.result.ok = false;
        completion.result.error = "HTTP 503 upstream unavailable";

        const auto effects = HomeCompletionController::apply(completion, true, true, 2, home, state);
        assert(effects.finishLoading);
        assert(!effects.sessionExpired);
        assert(!effects.resetRetry);
        assert(effects.retryDelaySeconds == 4);
        assert(effects.nextRetryAttempt == 3);
        assert(effects.updateVisibleError);
        assert(effects.visibleError == "HTTP 503 upstream unavailable");
    }

    {
        JellyfinHomeData home;
        HomeScreenState state;
        HomeCoreCompletion completion;
        completion.generation = 3;
        completion.result.ok = false;
        completion.result.error = "HTTP 401 Unauthorized";

        const auto effects = HomeCompletionController::apply(completion, true, true, 4, home, state);
        assert(effects.finishLoading);
        assert(effects.sessionExpired);
        assert(effects.resetRetry);
        assert(effects.nextRetryAttempt == 0);
        assert(!effects.retryDelaySeconds);
        assert(!effects.updateVisibleError);
    }

    {
        JellyfinHomeData home;
        HomeScreenState state;
        HomeCoreCompletion completion;
        completion.generation = 4;
        completion.snapshot = snapshot();
        completion.result.ok = true;
        completion.result.value.warning = "partial";
        completion.result.value.views = {item("view-1")};
        completion.result.value.rows = {
            row("Continue Watching", {item("episode-1"), item("episode-2")}),
            row("Latest", {item("movie-1")}),
        };

        const auto effects = HomeCompletionController::apply(completion, true, true, 3, home, state);
        assert(effects.finishLoading);
        assert(effects.resetRetry);
        assert(effects.loaded);
        assert(effects.nextRetryAttempt == 0);
        assert(effects.secondaryViews.size() == 1);
        assert(effects.secondaryViews.front().id == "view-1");
        assert(effects.coreRestoredRow == 0);
        assert(state.row() == 0);
        assert(state.selection(0, 2) == 1);
        assert(effects.prefetch);
        assert(effects.prefetch->row == 0);
        assert(effects.prefetch->selection == 1);
        assert(effects.updateVisibleError);
        assert(effects.visibleError == "partial");
    }

    {
        JellyfinHomeData home;
        home.rows = {row("Continue Watching", {item("episode-1")})};
        HomeScreenState state;
        state.setSelections({0});
        state.setRow(0);

        HomeSecondaryCompletion completion;
        completion.generation = 5;
        completion.coreRestoredRow = 0;
        completion.snapshot.toolbarFocused = false;
        completion.snapshot.focusedRowTitle = "Latest";
        completion.snapshot.selectedItemByRow["Latest"] = "movie-2";
        completion.result.ok = true;
        completion.result.value.warning = "secondary warning";
        completion.result.value.rows = {row("Latest", {item("movie-1"), item("movie-2")})};

        const auto effects = HomeCompletionController::apply(completion, true, true, home, state);
        assert(home.rows.size() == 2);
        assert(state.row() == 1);
        assert(state.selection(1, 2) == 1);
        assert(home.warning == "secondary warning");
        assert(effects.updateVisibleError);
        assert(effects.visibleError == "secondary warning");
        assert(effects.prefetch);
        assert(effects.prefetch->row == 1);
        assert(effects.prefetch->selection == 1);
    }

    {
        JellyfinHomeData home;
        home.warning = "core warning";
        HomeScreenState state;
        HomeSecondaryCompletion completion;
        completion.generation = 6;
        completion.result.ok = false;
        completion.result.error = "timeout";

        const auto effects = HomeCompletionController::apply(completion, true, true, home, state);
        assert(home.warning == "core warning | SECONDARY HOME ROWS UNAVAILABLE");
        assert(effects.updateVisibleError);
        assert(effects.visibleError == home.warning);
        assert(!effects.prefetch);
    }

    return 0;
}
