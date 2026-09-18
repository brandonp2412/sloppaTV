#include "home_completion_controller.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace {
bool isTransientHomeLoadError(std::string_view error) {
    return error.find("Server hostname could not be resolved") != std::string_view::npos ||
           error.find("Server connection timed out") != std::string_view::npos ||
           error.find("Unable to connect to server") != std::string_view::npos ||
           error.find("HTTP 408") != std::string_view::npos || error.find("HTTP 425") != std::string_view::npos ||
           error.find("HTTP 429") != std::string_view::npos || error.find("HTTP 5") != std::string_view::npos;
}

std::optional<HomePrefetchRequest> selectedPrefetch(const JellyfinHomeData& home, const HomeScreenState& state) {
    if (state.row() < 0 || state.row() >= static_cast<int>(home.rows.size())) return std::nullopt;
    const auto& row = home.rows[static_cast<size_t>(state.row())];
    return HomePrefetchRequest{
        .row = state.row(),
        .selection = state.selection(state.row(), static_cast<int>(row.items.size())),
    };
}
} // namespace

HomeCoreCompletionEffects HomeCompletionController::apply(HomeCoreCompletion& completion, bool activeGeneration,
                                                          bool activeScreen, int retryAttempt, JellyfinHomeData& home,
                                                          HomeScreenState& state) {
    if (!activeGeneration) return {};

    HomeCoreCompletionEffects effects;
    effects.finishLoading = true;
    effects.nextRetryAttempt = retryAttempt;

    if (!completion.result.ok) {
        const std::string& error = completion.result.error;
        if (error.find("HTTP 401") != std::string::npos) {
            effects.sessionExpired = true;
            effects.resetRetry = true;
            effects.nextRetryAttempt = 0;
            return effects;
        }

        if (isTransientHomeLoadError(error)) {
            effects.retryDelaySeconds = std::min(30, 1 << std::min(retryAttempt, 5));
            effects.nextRetryAttempt = retryAttempt + 1;
        } else {
            effects.resetRetry = true;
            effects.nextRetryAttempt = 0;
        }

        if (activeScreen) {
            effects.updateVisibleError = true;
            effects.visibleError = error;
        }
        return effects;
    }

    effects.resetRetry = true;
    effects.nextRetryAttempt = 0;
    effects.loaded = true;
    effects.secondaryViews = completion.result.value.views;

    HomeRestorePlan restorePlan = HomeScreenState::restorePlan(completion.snapshot, completion.result.value.rows);
    effects.coreRestoredRow = restorePlan.focusedRow;

    home = std::move(completion.result.value);
    state.setSelections(std::move(restorePlan.selections));
    state.setRow(effects.coreRestoredRow);
    state.updateViewport(static_cast<int>(home.rows.size()));

    if (activeScreen) {
        effects.updateVisibleError = true;
        effects.visibleError = home.warning;
    }
    effects.prefetch = selectedPrefetch(home, state);
    return effects;
}

HomeSecondaryCompletionEffects HomeCompletionController::apply(HomeSecondaryCompletion& completion,
                                                               bool activeGeneration, bool activeScreen,
                                                               JellyfinHomeData& home, HomeScreenState& state) {
    if (!activeGeneration) return {};

    HomeSecondaryCompletionEffects effects;
    if (!completion.result.ok) {
        if (!home.warning.empty()) home.warning += " | ";
        home.warning += "SECONDARY HOME ROWS UNAVAILABLE";
        if (activeScreen) {
            effects.updateVisibleError = true;
            effects.visibleError = home.warning;
        }
        return effects;
    }

    const size_t baseRowCount = home.rows.size();
    for (auto& section : completion.result.value.rows) {
        const int restoredSelection = HomeScreenState::restoredSelection(completion.snapshot, section);
        const bool focusAppended = !completion.snapshot.toolbarFocused &&
                                   section.title == completion.snapshot.focusedRowTitle &&
                                   state.row() == completion.coreRestoredRow;
        home.rows.push_back(std::move(section));
        state.appendSelection(restoredSelection);
        if (focusAppended) state.setRow(static_cast<int>(home.rows.size()) - 1);
    }

    if (!completion.result.value.warning.empty()) {
        if (!home.warning.empty()) home.warning += " | ";
        home.warning += completion.result.value.warning;
        if (activeScreen) {
            effects.updateVisibleError = true;
            effects.visibleError = home.warning;
        }
    }

    state.updateViewport(static_cast<int>(home.rows.size()));
    if (state.row() >= static_cast<int>(baseRowCount) && state.row() < static_cast<int>(state.selectionCount())) {
        effects.prefetch = selectedPrefetch(home, state);
    }
    return effects;
}
