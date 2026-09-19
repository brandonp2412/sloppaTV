#include "browse_completion_controller.hpp"

#include <utility>

BrowseCompletionEffects BrowseCompletionController::apply(BrowsePageCompletion& completion, bool active,
                                                          bool browseScreenActive, std::string_view activeContainerId,
                                                          BrowseScreenState& state, int pageSize) {
    if (!active) return {};

    BrowseCompletionEffects effects;
    effects.finishLoading = true;
    if (!browseScreenActive || activeContainerId != completion.containerId) return effects;

    if (!completion.result.ok) {
        effects.error = completion.result.error;
        return effects;
    }

    if (completion.append) {
        state.appendPage(std::move(completion.result.value), completion.startIndex, pageSize);
    } else {
        state.replacePage(std::move(completion.result.value), pageSize);
    }
    effects.clearError = true;
    effects.prefetchArtwork = true;
    return effects;
}
