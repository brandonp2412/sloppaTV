#include "search_completion_controller.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace {
JellyfinItem item(std::string id, std::string type = "Movie") {
    JellyfinItem value;
    value.id = std::move(id);
    value.name = value.id;
    value.type = std::move(type);
    return value;
}

SeerrMediaItem seerrItem(std::string id) {
    SeerrMediaItem value;
    value.id = std::move(id);
    value.name = value.id;
    value.mediaType = "movie";
    value.tmdbId = 42;
    return value;
}

struct FakeCoordinator {
    void abandonCompletion() { ++abandonCalls; }

    SeerrSearchCompletionPlan completeFailure(std::string_view query, std::string error, bool hasSessionCookie) {
        ++failureCalls;
        lastQuery = std::string(query);
        lastError = std::move(error);
        lastHasSessionCookie = hasSessionCookie;
        return failurePlan;
    }

    SeerrSearchCompletionPlan completeSuccess(std::string_view query, std::vector<SeerrMediaItem> results,
                                              std::chrono::steady_clock::time_point now) {
        ++successCalls;
        lastQuery = std::string(query);
        lastNow = now;
        if (seerrResults) *seerrResults = std::move(results);
        return successPlan;
    }

    std::vector<SeerrMediaItem>* seerrResults = nullptr;
    SeerrSearchCompletionPlan failurePlan;
    SeerrSearchCompletionPlan successPlan;
    int abandonCalls = 0;
    int failureCalls = 0;
    int successCalls = 0;
    bool lastHasSessionCookie = false;
    std::string lastQuery;
    std::string lastError;
    std::chrono::steady_clock::time_point lastNow{};
};

void assertNoEffects(const SearchCompletionEffects& effects) {
    assert(!effects.reconnectSeerr);
    assert(!effects.syncSeerrHome);
    assert(!effects.clearError);
    assert(!effects.error);
}
} // namespace

int main() {
    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        state.setQuery("matrix");
        assert(state.beginSearch());

        JellyfinSearchCompletion completion;
        completion.query = "matrix";
        completion.generation = 7;
        completion.result.ok = true;
        completion.result.value = {item("movie-1")};

        const auto effects = SearchCompletionController::apply(completion, false, true, state);
        assertNoEffects(effects);
        assert(state.loading());
        assert(state.results().empty());
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        state.setQuery("matrix");
        assert(state.beginSearch());

        JellyfinSearchCompletion completion;
        completion.query = "matrix";
        completion.generation = 8;
        completion.result.ok = true;
        completion.result.value = {item("movie-1")};

        const auto effects = SearchCompletionController::apply(completion, true, false, state);
        assertNoEffects(effects);
        assert(!state.loading());
        assert(state.results().empty());
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        state.setQuery("current");
        assert(state.beginSearch());

        JellyfinSearchCompletion completion;
        completion.query = "old";
        completion.generation = 9;
        completion.result.ok = false;
        completion.result.error = "old failure";

        const auto effects = SearchCompletionController::apply(completion, true, true, state);
        assertNoEffects(effects);
        assert(state.loading());
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        state.setQuery("arrival");
        assert(state.beginSearch());

        JellyfinSearchCompletion completion;
        completion.query = "arrival";
        completion.generation = 10;
        completion.result.ok = false;
        completion.result.error = "library failed";

        const auto effects = SearchCompletionController::apply(completion, true, true, state);
        assert(!effects.reconnectSeerr);
        assert(!effects.syncSeerrHome);
        assert(!effects.clearError);
        assert(effects.error == "library failed");
        assert(!state.loading());
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        state.setQuery("alien");
        assert(state.beginSearch());

        JellyfinSearchCompletion completion;
        completion.query = "alien";
        completion.generation = 11;
        completion.result.ok = true;
        completion.result.value = {item("episode-1", "Episode"), item("movie-1"), item("series-1", "Series")};

        const auto effects = SearchCompletionController::apply(completion, true, true, state);
        assert(effects.clearError);
        assert(!effects.error);
        assert(!state.loading());
        assert(state.results().size() == 3);
        assert(state.results()[0].id == "movie-1");
        assert(state.results()[1].id == "series-1");
        assert(state.results()[2].id == "episode-1");
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        FakeCoordinator coordinator;
        SeerrSearchCompletion completion;
        completion.query = "dune";
        completion.generation = 12;
        completion.result.ok = true;
        completion.result.value = {seerrItem("seerr:movie:42")};

        const auto effects = SearchCompletionController::apply(completion, false, true, true, state, coordinator,
                                                               std::chrono::steady_clock::now());
        assertNoEffects(effects);
        assert(coordinator.abandonCalls == 0);
        assert(coordinator.successCalls == 0);
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        FakeCoordinator coordinator;
        SeerrSearchCompletion completion;
        completion.query = "dune";
        completion.generation = 13;
        completion.result.ok = true;
        completion.result.value = {seerrItem("seerr:movie:42")};

        const auto effects = SearchCompletionController::apply(completion, true, false, true, state, coordinator,
                                                               std::chrono::steady_clock::now());
        assertNoEffects(effects);
        assert(coordinator.abandonCalls == 1);
        assert(coordinator.successCalls == 0);
    }

    {
        std::vector<SeerrMediaItem> seerrResults = {seerrItem("seerr:movie:1")};
        SearchScreenState state(seerrResults);
        state.refreshSeerrResults();
        assert(state.results().size() == 1);

        FakeCoordinator coordinator;
        coordinator.seerrResults = &seerrResults;
        coordinator.failurePlan.resultsChanged = true;
        coordinator.failurePlan.reconnect = true;

        SeerrSearchCompletion completion;
        completion.query = "dune";
        completion.generation = 14;
        completion.result.ok = false;
        completion.result.error = "unauthorized";

        seerrResults.clear();
        const auto effects = SearchCompletionController::apply(completion, true, true, true, state, coordinator,
                                                               std::chrono::steady_clock::now());
        assert(effects.reconnectSeerr);
        assert(!effects.syncSeerrHome);
        assert(!effects.clearError);
        assert(!effects.error);
        assert(coordinator.failureCalls == 1);
        assert(coordinator.lastQuery == "dune");
        assert(coordinator.lastError == "unauthorized");
        assert(coordinator.lastHasSessionCookie);
        assert(state.results().empty());
    }

    {
        std::vector<SeerrMediaItem> seerrResults;
        SearchScreenState state(seerrResults);
        FakeCoordinator coordinator;
        coordinator.seerrResults = &seerrResults;
        coordinator.successPlan.resultsChanged = true;
        coordinator.successPlan.pendingChanged = true;

        SeerrSearchCompletion completion;
        completion.query = "dune";
        completion.generation = 15;
        completion.result.ok = true;
        completion.result.value = {seerrItem("seerr:movie:42")};
        const auto now = std::chrono::steady_clock::now();

        const auto effects = SearchCompletionController::apply(completion, true, true, false, state, coordinator, now);
        assert(!effects.reconnectSeerr);
        assert(effects.syncSeerrHome);
        assert(!effects.clearError);
        assert(!effects.error);
        assert(coordinator.successCalls == 1);
        assert(coordinator.lastQuery == "dune");
        assert(coordinator.lastNow == now);
        assert(state.results().size() == 1);
        assert(state.results().front().id == "seerr:movie:42");
    }

    return 0;
}
