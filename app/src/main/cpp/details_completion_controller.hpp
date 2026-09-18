#pragma once

#include "details_async_executor.hpp"
#include "details_screen.hpp"

#include <optional>
#include <string>

struct DetailsCompletionEffects {
    bool finishLoading = false;
    std::optional<std::string> error;
};

class DetailsCompletionController {
public:
    [[nodiscard]] static DetailsCompletionEffects apply(ItemMenuDetailCompletion& completion, bool activeScreen,
                                                        JellyfinItem& detail);
    [[nodiscard]] static DetailsCompletionEffects apply(PersonItemsCompletion& completion, bool activeGeneration,
                                                        bool activeScreen, DetailsScreenState& state);
    [[nodiscard]] static DetailsCompletionEffects apply(SeasonsCompletion& completion, bool activeGeneration,
                                                        bool activeScreen, DetailsScreenState& state);
    [[nodiscard]] static DetailsCompletionEffects apply(EpisodesCompletion& completion, bool activeGeneration,
                                                        bool activeScreen, DetailsScreenState& state);
    [[nodiscard]] static DetailsCompletionEffects apply(DetailsItemCompletion& completion, bool activeGeneration,
                                                        bool activeScreen, JellyfinItem& detail);
    [[nodiscard]] static DetailsCompletionEffects apply(DetailsSimilarCompletion& completion, bool activeGeneration,
                                                        bool activeScreen, const JellyfinItem& detail,
                                                        DetailsScreenState& state);
    [[nodiscard]] static DetailsCompletionEffects apply(EpisodeSeriesContextCompletion& completion,
                                                        bool activeGeneration, bool activeScreen,
                                                        const JellyfinItem& detail, DetailsScreenState& state);
};
