#pragma once

#include "jellyfin_search_executor.hpp"
#include "search_screen.hpp"
#include "seerr_async_executor.hpp"
#include "seerr_search_coordinator.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

struct SearchCompletionEffects {
    bool reconnectSeerr = false;
    bool syncSeerrHome = false;
    bool clearError = false;
    std::optional<std::string> error;
};

class SearchCompletionController {
public:
    [[nodiscard]] static SearchCompletionEffects apply(JellyfinSearchCompletion& completion, bool activeGeneration,
                                                       bool activeScreen, SearchScreenState& state);

    template <typename Coordinator>
    [[nodiscard]] static SearchCompletionEffects
    apply(SeerrSearchCompletion& completion, bool activeGeneration, bool activeScreen, bool hasSessionCookie,
          SearchScreenState& state, Coordinator& coordinator, std::chrono::steady_clock::time_point now) {
        if (!activeGeneration) return {};
        if (!activeScreen) {
            coordinator.abandonCompletion();
            return {};
        }

        if (!completion.result.ok) {
            const auto plan = coordinator.completeFailure(completion.query, completion.result.error, hasSessionCookie);
            if (plan.resultsChanged) state.refreshSeerrResults();
            SearchCompletionEffects effects;
            effects.reconnectSeerr = plan.reconnect;
            return effects;
        }

        const auto plan = coordinator.completeSuccess(completion.query, std::move(completion.result.value), now);
        if (plan.resultsChanged) state.refreshSeerrResults();
        SearchCompletionEffects effects;
        effects.syncSeerrHome = plan.pendingChanged;
        return effects;
    }
};
