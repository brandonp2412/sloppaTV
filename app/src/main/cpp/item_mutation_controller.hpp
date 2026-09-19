#pragma once

#include "browse_screen.hpp"
#include "details_screen.hpp"
#include "home_screen.hpp"
#include "playback_queue.hpp"
#include "search_screen.hpp"
#include "item_mutation_executor.hpp"

#include <cstdint>
#include <optional>
#include <string>

struct PlayedRollbackState {
    std::string itemId;
    uint64_t sessionEpoch = 0;
    JellyfinHomeData previousHome;
    HomeSelectionSnapshot previousHomeSelection;
};

struct NextUpReplacementAnimation {
    int index = -1;
    std::string itemId;
};

struct PlayedMutationPreparation {
    JellyfinItem item;
    bool desired = false;
    int nextUpReplacementIndex = -1;
    PlayedRollbackState rollback;
};

struct ItemMutationCompletionEffects {
    bool finishLoading = false;
    std::optional<JellyfinItem> cacheUpdate;
    std::optional<std::string> error;
    std::optional<std::string> notice;
    std::optional<std::string> removeCachedItemId;
    std::optional<PlayedRollbackState> playedRollback;
    std::optional<NextUpReplacementAnimation> nextUpAnimation;
    bool closeDeletedItem = false;
};

class ItemMutationController {
public:
    static void updateCachedUserData(JellyfinHomeData& home, HomeScreenState& homeState, BrowseScreenState& browseState,
                                     SearchScreenState& searchState, DetailsScreenState& detailsState,
                                     PlaybackQueueState& queueState, const JellyfinItem& updated, bool hiddenFromHome);

    static void removeCachedItem(JellyfinHomeData& home, BrowseScreenState& browseState, SearchScreenState& searchState,
                                 DetailsScreenState& detailsState, const std::string& itemId);

    [[nodiscard]] static PlayedMutationPreparation
    preparePlayedToggle(JellyfinHomeData& home, HomeScreenState& homeState, BrowseScreenState& browseState,
                        SearchScreenState& searchState, DetailsScreenState& detailsState,
                        PlaybackQueueState& queueState, JellyfinItem& detail, bool hiddenFromHome,
                        uint64_t sessionEpoch);

    [[nodiscard]] static ItemMutationCompletionEffects apply(FavoriteCompletion& completion, bool activeSession,
                                                             bool detailVisible, JellyfinItem& detail);

    [[nodiscard]] static ItemMutationCompletionEffects apply(PlayedCompletion& completion, bool activeSession,
                                                             bool detailVisible, bool replacementHidden,
                                                             std::optional<PlayedRollbackState>& rollbackState,
                                                             JellyfinHomeData& home, HomeScreenState& homeState,
                                                             JellyfinItem& detail);

    [[nodiscard]] static ItemMutationCompletionEffects apply(MetadataRefreshCompletion& completion, bool activeSession);

    [[nodiscard]] static ItemMutationCompletionEffects apply(DeleteItemCompletion& completion, bool activeSession,
                                                             bool activeItemMenu, JellyfinItem& detail,
                                                             DetailsScreenState& detailsState);

    static void restorePlayedRollback(ItemMutationCompletionEffects& effects, JellyfinHomeData& home,
                                      HomeScreenState& homeState);
};
