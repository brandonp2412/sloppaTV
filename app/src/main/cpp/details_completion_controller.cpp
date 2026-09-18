#include "details_completion_controller.hpp"

#include <utility>

namespace {
DetailsCompletionEffects finishLoading() {
    DetailsCompletionEffects effects;
    effects.finishLoading = true;
    return effects;
}

DetailsCompletionEffects finishWithError(std::string error) {
    DetailsCompletionEffects effects = finishLoading();
    effects.error = std::move(error);
    return effects;
}
} // namespace

DetailsCompletionEffects DetailsCompletionController::apply(ItemMenuDetailCompletion& completion, bool activeScreen,
                                                            JellyfinItem& detail) {
    if (!completion.result.ok || !activeScreen || detail.id != completion.itemId) return {};
    detail = std::move(completion.result.value);
    return {};
}

DetailsCompletionEffects DetailsCompletionController::apply(PersonItemsCompletion& completion, bool activeGeneration,
                                                            bool activeScreen, DetailsScreenState& state) {
    if (!activeGeneration) return {};

    if (!activeScreen || state.selectedPerson().id != completion.personId) return finishLoading();
    if (!completion.result.ok) return finishWithError("PERSON: " + completion.result.error);

    state.setPersonItems(std::move(completion.result.value));
    return finishLoading();
}

DetailsCompletionEffects DetailsCompletionController::apply(SeasonsCompletion& completion, bool activeGeneration,
                                                            bool activeScreen, DetailsScreenState& state) {
    if (!activeGeneration) return {};

    if (!activeScreen || state.seriesDetail().id != completion.seriesId) return finishLoading();
    if (!completion.result.ok) return finishWithError(completion.result.error);

    state.setSeasons(std::move(completion.result.value));
    return finishLoading();
}

DetailsCompletionEffects DetailsCompletionController::apply(EpisodesCompletion& completion, bool activeGeneration,
                                                            bool activeScreen, DetailsScreenState& state) {
    if (!activeGeneration) return {};

    if (!activeScreen || state.seriesDetail().id != completion.seriesId ||
        state.selectedSeason().id != completion.seasonId) {
        return finishLoading();
    }
    if (!completion.result.ok) return finishWithError(completion.result.error);

    state.setEpisodes(std::move(completion.result.value));
    return finishLoading();
}

DetailsCompletionEffects DetailsCompletionController::apply(DetailsItemCompletion& completion, bool activeGeneration,
                                                            bool activeScreen, JellyfinItem& detail) {
    if (!activeGeneration) return {};

    if (!activeScreen || detail.id != completion.itemId) return finishLoading();
    if (!completion.result.ok) return finishWithError("DETAILS: " + completion.result.error);

    detail = std::move(completion.result.value);
    return finishLoading();
}

DetailsCompletionEffects DetailsCompletionController::apply(DetailsSimilarCompletion& completion, bool activeGeneration,
                                                            bool activeScreen, const JellyfinItem& detail,
                                                            DetailsScreenState& state) {
    if (!activeGeneration || !activeScreen || detail.id != completion.itemId) return {};

    state.setSimilar(std::move(completion.items));
    return {};
}

DetailsCompletionEffects DetailsCompletionController::apply(EpisodeSeriesContextCompletion& completion,
                                                            bool activeGeneration, bool activeScreen,
                                                            const JellyfinItem& detail, DetailsScreenState& state) {
    if (!activeGeneration || !activeScreen || detail.id != completion.itemId || detail.type != "Episode") return {};

    state.setEpisodeSeriesContext(std::move(completion.series), std::move(completion.seasons));
    return {};
}
