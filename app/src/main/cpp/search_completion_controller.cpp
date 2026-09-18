#include "search_completion_controller.hpp"

#include <utility>

SearchCompletionEffects SearchCompletionController::apply(JellyfinSearchCompletion& completion, bool activeGeneration,
                                                          bool activeScreen, SearchScreenState& state) {
    if (!activeGeneration) return {};
    if (!activeScreen) {
        state.setLoading(false);
        return {};
    }

    if (!completion.result.ok) {
        if (!state.failLibrarySearch(completion.query)) return {};
        SearchCompletionEffects effects;
        effects.error = completion.result.error;
        return effects;
    }

    if (!state.finishLibrarySearch(completion.query, std::move(completion.result.value))) return {};
    SearchCompletionEffects effects;
    effects.clearError = true;
    return effects;
}
