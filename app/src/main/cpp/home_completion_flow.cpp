#include "home_completion_flow.hpp"

#include <utility>

HomeCoreFlowEffects HomeCompletionFlow::complete(HomeCoreCompletion& completion, bool activeGeneration,
                                                 bool activeScreen, std::chrono::steady_clock::time_point now) {
    HomeCoreFlowEffects flow;
    flow.active = activeGeneration;
    if (!activeGeneration) return flow;

    if (completion.result.ok) visibility_.filter(completion.result.value);
    HomeCoreCompletionEffects effects =
        HomeCompletionController::apply(completion, true, activeScreen, retryAttempt_, home_, state_);

    if (effects.finishLoading) loading_ = false;
    retryAttempt_ = effects.nextRetryAttempt;
    if (effects.resetRetry) retryAt_ = {};
    if (effects.retryDelaySeconds) retryAt_ = now + std::chrono::seconds(*effects.retryDelaySeconds);

    flow.sessionExpired = effects.sessionExpired;
    flow.loaded = effects.loaded;
    if (effects.updateVisibleError) flow.visibleError = std::move(effects.visibleError);
    flow.retryDelaySeconds = effects.retryDelaySeconds;
    flow.prefetch = effects.prefetch;
    flow.secondaryViews = std::move(effects.secondaryViews);
    flow.coreRestoredRow = effects.coreRestoredRow;
    return flow;
}

HomeSecondaryFlowEffects HomeCompletionFlow::complete(HomeSecondaryCompletion& completion, bool activeGeneration,
                                                      bool activeScreen) {
    HomeSecondaryFlowEffects flow;
    flow.active = activeGeneration;
    if (!activeGeneration) return flow;

    if (completion.result.ok) visibility_.filter(completion.result.value);
    HomeSecondaryCompletionEffects effects =
        HomeCompletionController::apply(completion, true, activeScreen, home_, state_);
    flow.loaded = completion.result.ok;
    if (effects.updateVisibleError) flow.visibleError = std::move(effects.visibleError);
    flow.prefetch = effects.prefetch;
    return flow;
}
