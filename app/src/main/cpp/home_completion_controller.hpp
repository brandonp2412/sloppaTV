#pragma once

#include "home_async_executor.hpp"
#include "home_screen.hpp"

#include <optional>
#include <string>
#include <vector>

struct HomePrefetchRequest {
    int row = -1;
    int selection = 0;
};

struct HomeCoreCompletionEffects {
    bool finishLoading = false;
    bool sessionExpired = false;
    bool resetRetry = false;
    bool loaded = false;
    bool updateVisibleError = false;
    int nextRetryAttempt = 0;
    std::optional<int> retryDelaySeconds;
    std::string visibleError;
    std::vector<JellyfinItem> secondaryViews;
    int coreRestoredRow = -1;
    std::optional<HomePrefetchRequest> prefetch;
};

struct HomeSecondaryCompletionEffects {
    bool updateVisibleError = false;
    std::string visibleError;
    std::optional<HomePrefetchRequest> prefetch;
};

class HomeCompletionController {
public:
    [[nodiscard]] static HomeCoreCompletionEffects apply(HomeCoreCompletion& completion, bool activeGeneration,
                                                         bool activeScreen, int retryAttempt, JellyfinHomeData& home,
                                                         HomeScreenState& state);

    [[nodiscard]] static HomeSecondaryCompletionEffects apply(HomeSecondaryCompletion& completion,
                                                              bool activeGeneration, bool activeScreen,
                                                              JellyfinHomeData& home, HomeScreenState& state);
};
