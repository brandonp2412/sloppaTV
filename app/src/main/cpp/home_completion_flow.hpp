#pragma once

#include "home_completion_controller.hpp"
#include "home_visibility.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct HomeCoreFlowEffects {
    bool active = false;
    bool sessionExpired = false;
    bool loaded = false;
    std::optional<std::string> visibleError;
    std::optional<int> retryDelaySeconds;
    std::optional<HomePrefetchRequest> prefetch;
    std::vector<JellyfinItem> secondaryViews;
    int coreRestoredRow = -1;
};

struct HomeSecondaryFlowEffects {
    bool active = false;
    bool loaded = false;
    std::optional<std::string> visibleError;
    std::optional<HomePrefetchRequest> prefetch;
};

class HomeCompletionFlow {
public:
    HomeCompletionFlow(HomeVisibility& visibility, JellyfinHomeData& home, HomeScreenState& state, bool& loading,
                       std::chrono::steady_clock::time_point& retryAt, int& retryAttempt)
        : visibility_(visibility), home_(home), state_(state), loading_(loading), retryAt_(retryAt),
          retryAttempt_(retryAttempt) {}

    void resetForSessionChange() {
        loading_ = false;
        retryAt_ = {};
        retryAttempt_ = 0;
    }

    [[nodiscard]] HomeCoreFlowEffects complete(HomeCoreCompletion& completion, bool activeGeneration, bool activeScreen,
                                               std::chrono::steady_clock::time_point now);
    [[nodiscard]] HomeSecondaryFlowEffects complete(HomeSecondaryCompletion& completion, bool activeGeneration,
                                                    bool activeScreen);

private:
    HomeVisibility& visibility_;
    JellyfinHomeData& home_;
    HomeScreenState& state_;
    bool& loading_;
    std::chrono::steady_clock::time_point& retryAt_;
    int& retryAttempt_;
};
