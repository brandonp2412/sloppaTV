#include "browse_completion_controller.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
JellyfinItem item(std::string id) {
    JellyfinItem value;
    value.id = std::move(id);
    value.type = "Movie";
    return value;
}

BrowseScreenState stateFor(std::string containerId) {
    BrowseScreenState state;
    JellyfinItem container;
    container.id = std::move(containerId);
    container.type = "CollectionFolder";
    state.resetForLibrary(container);
    return state;
}

BrowsePageCompletion successfulCompletion(std::string containerId, int startIndex, bool append,
                                          std::vector<JellyfinItem> items) {
    BrowsePageCompletion completion;
    completion.containerId = std::move(containerId);
    completion.startIndex = startIndex;
    completion.append = append;
    completion.generation = 7;
    completion.result.ok = true;
    completion.result.value = std::move(items);
    return completion;
}
} // namespace

int main() {
    constexpr int pageSize = 60;

    {
        auto state = stateFor("library");
        auto completion = successfulCompletion("library", 0, false, {item("movie-1")});

        const auto effects =
            BrowseCompletionController::apply(completion, false, true, state.activeContainer().id, state, pageSize);
        assert(!effects.finishLoading);
        assert(!effects.clearError);
        assert(!effects.prefetchArtwork);
        assert(!effects.error);
        assert(state.items().empty());
    }

    {
        auto state = stateFor("library");
        auto completion = successfulCompletion("library", 0, false, {item("movie-1")});

        const auto effects =
            BrowseCompletionController::apply(completion, true, false, state.activeContainer().id, state, pageSize);
        assert(effects.finishLoading);
        assert(!effects.clearError);
        assert(!effects.prefetchArtwork);
        assert(state.items().empty());
    }

    {
        auto state = stateFor("library");
        auto completion = successfulCompletion("other", 0, false, {item("movie-1")});

        const auto effects =
            BrowseCompletionController::apply(completion, true, true, state.activeContainer().id, state, pageSize);
        assert(effects.finishLoading);
        assert(!effects.clearError);
        assert(!effects.prefetchArtwork);
        assert(state.items().empty());
    }

    {
        auto state = stateFor("library");
        BrowsePageCompletion completion;
        completion.containerId = "library";
        completion.generation = 7;
        completion.result.ok = false;
        completion.result.error = "browse failed";

        const auto effects =
            BrowseCompletionController::apply(completion, true, true, state.activeContainer().id, state, pageSize);
        assert(effects.finishLoading);
        assert(!effects.clearError);
        assert(!effects.prefetchArtwork);
        assert(effects.error == "browse failed");
    }

    {
        auto state = stateFor("library");
        auto completion = successfulCompletion("library", 0, false, {item("movie-1"), item("movie-2")});

        const auto effects =
            BrowseCompletionController::apply(completion, true, true, state.activeContainer().id, state, pageSize);
        assert(effects.finishLoading);
        assert(effects.clearError);
        assert(effects.prefetchArtwork);
        assert(!effects.error);
        assert(state.items().size() == 2);
        assert(state.items()[0].id == "movie-1");
        assert(state.items()[1].id == "movie-2");
        assert(state.nextIndex() == 2);
        assert(!state.hasMore());
    }

    {
        auto state = stateFor("library");
        state.replacePage({item("movie-1"), item("movie-2")}, pageSize);
        auto completion = successfulCompletion("library", 2, true, {item("movie-3")});

        const auto effects =
            BrowseCompletionController::apply(completion, true, true, state.activeContainer().id, state, pageSize);
        assert(effects.finishLoading);
        assert(effects.clearError);
        assert(effects.prefetchArtwork);
        assert(state.items().size() == 3);
        assert(state.items()[2].id == "movie-3");
        assert(state.nextIndex() == 3);
    }

    return 0;
}
