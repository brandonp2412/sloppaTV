#pragma once

#include "item_mutation_controller.hpp"

#include <chrono>
#include <optional>
#include <string>

struct ContentMutationHostEffects {
    std::optional<std::string> error;
    std::optional<std::string> notice;
    bool closeDeletedItem = false;
    bool renderAnimationStarted = false;
};

class ContentMutationFlow {
public:
    [[nodiscard]] bool loading() const { return loading_; }

    void begin() { loading_ = true; }

    void finish() { loading_ = false; }

    void reset();

    [[nodiscard]] PlayedMutationPreparation preparePlayedToggle(JellyfinHomeData& home, HomeScreenState& homeState,
                                                                BrowseScreenState& browseState,
                                                                SearchScreenState& searchState,
                                                                DetailsScreenState& detailsState,
                                                                PlaybackQueueState& queueState, JellyfinItem& detail,
                                                                bool hiddenFromHome, uint64_t sessionEpoch);

    void rejectPlayedToggle(const JellyfinItem& original, bool hiddenFromHome, JellyfinHomeData& home,
                            HomeScreenState& homeState, BrowseScreenState& browseState, SearchScreenState& searchState,
                            DetailsScreenState& detailsState, PlaybackQueueState& queueState, JellyfinItem& detail);

    [[nodiscard]] ContentMutationHostEffects complete(FavoriteCompletion& completion, bool activeSession,
                                                      bool detailVisible, bool updatedHiddenFromHome,
                                                      JellyfinHomeData& home, HomeScreenState& homeState,
                                                      BrowseScreenState& browseState, SearchScreenState& searchState,
                                                      DetailsScreenState& detailsState, PlaybackQueueState& queueState,
                                                      JellyfinItem& detail);

    [[nodiscard]] ContentMutationHostEffects complete(PlayedCompletion& completion, bool activeSession,
                                                      bool detailVisible, bool replacementHiddenFromHome,
                                                      bool updatedHiddenFromHome, JellyfinHomeData& home,
                                                      HomeScreenState& homeState, BrowseScreenState& browseState,
                                                      SearchScreenState& searchState, DetailsScreenState& detailsState,
                                                      PlaybackQueueState& queueState, JellyfinItem& detail);

    [[nodiscard]] ContentMutationHostEffects complete(MetadataRefreshCompletion& completion, bool activeSession);

    [[nodiscard]] ContentMutationHostEffects complete(DeleteItemCompletion& completion, bool activeSession,
                                                      bool activeItemMenu, JellyfinHomeData& home,
                                                      HomeScreenState& homeState, BrowseScreenState& browseState,
                                                      SearchScreenState& searchState, DetailsScreenState& detailsState,
                                                      PlaybackQueueState& queueState, JellyfinItem& detail);

    [[nodiscard]] int nextUpReplacementFadeIndex() const { return nextUpReplacementFadeIndex_; }

    [[nodiscard]] const std::string& nextUpReplacementFadeItemId() const { return nextUpReplacementFadeItemId_; }

    [[nodiscard]] std::chrono::steady_clock::time_point nextUpReplacementFadeStarted() const {
        return nextUpReplacementFadeStarted_;
    }

private:
    [[nodiscard]] ContentMutationHostEffects apply(ItemMutationCompletionEffects effects, bool updatedHiddenFromHome,
                                                   JellyfinHomeData& home, HomeScreenState& homeState,
                                                   BrowseScreenState& browseState, SearchScreenState& searchState,
                                                   DetailsScreenState& detailsState, PlaybackQueueState& queueState);

    bool loading_ = false;
    std::optional<PlayedRollbackState> playedRollback_;
    int nextUpReplacementFadeIndex_ = -1;
    std::string nextUpReplacementFadeItemId_;
    std::chrono::steady_clock::time_point nextUpReplacementFadeStarted_{};
};
