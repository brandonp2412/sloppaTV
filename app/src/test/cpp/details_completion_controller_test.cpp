#include "details_completion_controller.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {
JellyfinItem item(std::string id, std::string type = "Movie") {
    JellyfinItem value;
    value.id = std::move(id);
    value.name = value.id;
    value.type = std::move(type);
    return value;
}

JellyfinPerson person(std::string id) {
    JellyfinPerson value;
    value.id = std::move(id);
    value.name = value.id;
    return value;
}

void assertNoEffects(const DetailsCompletionEffects& effects) {
    assert(!effects.finishLoading);
    assert(!effects.error);
}
} // namespace

int main() {
    {
        DetailsScreenState state;
        state.beginPerson(person("person-1"));
        PersonItemsCompletion completion;
        completion.personId = "person-1";
        completion.generation = 4;
        completion.result.ok = true;
        completion.result.value = {item("movie-1")};

        const auto effects = DetailsCompletionController::apply(completion, false, true, state);
        assertNoEffects(effects);
        assert(state.personItems().empty());
    }

    {
        DetailsScreenState state;
        state.beginPerson(person("person-1"));
        PersonItemsCompletion completion;
        completion.personId = "person-1";
        completion.generation = 5;
        completion.result.ok = true;
        completion.result.value = {item("movie-1")};

        const auto effects = DetailsCompletionController::apply(completion, true, false, state);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(state.personItems().empty());
    }

    {
        DetailsScreenState state;
        state.beginPerson(person("person-1"));
        PersonItemsCompletion completion;
        completion.personId = "person-1";
        completion.generation = 6;
        completion.result.ok = false;
        completion.result.error = "lookup failed";

        const auto effects = DetailsCompletionController::apply(completion, true, true, state);
        assert(effects.finishLoading);
        assert(effects.error == "PERSON: lookup failed");
        assert(state.personItems().empty());
    }

    {
        DetailsScreenState state;
        state.beginPerson(person("person-1"));
        PersonItemsCompletion completion;
        completion.personId = "person-1";
        completion.generation = 7;
        completion.result.ok = true;
        completion.result.value = {item("movie-1"), item("movie-2")};

        const auto effects = DetailsCompletionController::apply(completion, true, true, state);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(state.personItems().size() == 2);
        assert(state.personItems().front().id == "movie-1");
    }

    {
        DetailsScreenState state;
        state.beginSeries(item("series-1", "Series"));
        SeasonsCompletion completion;
        completion.seriesId = "series-2";
        completion.generation = 8;
        completion.result.ok = true;
        completion.result.value = {item("season-wrong", "Season")};

        const auto effects = DetailsCompletionController::apply(completion, true, true, state);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(state.seasons().empty());

        completion.seriesId = "series-1";
        completion.result.value = {item("season-1", "Season")};
        const auto applied = DetailsCompletionController::apply(completion, true, true, state);
        assert(applied.finishLoading);
        assert(!applied.error);
        assert(state.seasons().size() == 1);
        assert(state.seasons().front().id == "season-1");
    }

    {
        DetailsScreenState state;
        state.beginSeries(item("series-1", "Series"));
        state.beginSeason(item("season-1", "Season"));

        EpisodesCompletion completion;
        completion.seriesId = "series-1";
        completion.seasonId = "season-1";
        completion.generation = 9;
        completion.result.ok = false;
        completion.result.error = "episodes failed";

        const auto failed = DetailsCompletionController::apply(completion, true, true, state);
        assert(failed.finishLoading);
        assert(failed.error == "episodes failed");
        assert(state.episodes().empty());

        completion.result.ok = true;
        completion.result.error.clear();
        completion.result.value = {item("episode-1", "Episode")};
        const auto applied = DetailsCompletionController::apply(completion, true, true, state);
        assert(applied.finishLoading);
        assert(!applied.error);
        assert(state.episodes().size() == 1);
        assert(state.episodes().front().id == "episode-1");
    }

    {
        JellyfinItem detail = item("movie-1");
        ItemMenuDetailCompletion completion;
        completion.itemId = "movie-1";
        completion.result.ok = true;
        completion.result.value = item("movie-1");
        completion.result.value.name = "Updated";

        const auto inactive = DetailsCompletionController::apply(completion, false, detail);
        assertNoEffects(inactive);
        assert(detail.name == "movie-1");

        const auto applied = DetailsCompletionController::apply(completion, true, detail);
        assertNoEffects(applied);
        assert(detail.name == "Updated");
    }

    {
        JellyfinItem detail = item("movie-1");
        DetailsItemCompletion completion;
        completion.itemId = "movie-1";
        completion.generation = 10;
        completion.result.ok = false;
        completion.result.error = "missing";

        const auto stale = DetailsCompletionController::apply(completion, false, true, detail);
        assertNoEffects(stale);

        const auto failed = DetailsCompletionController::apply(completion, true, true, detail);
        assert(failed.finishLoading);
        assert(failed.error == "DETAILS: missing");

        completion.result.ok = true;
        completion.result.error.clear();
        completion.result.value = item("movie-1");
        completion.result.value.name = "Fresh";
        const auto applied = DetailsCompletionController::apply(completion, true, true, detail);
        assert(applied.finishLoading);
        assert(!applied.error);
        assert(detail.name == "Fresh");
    }

    {
        DetailsScreenState state;
        JellyfinItem detail = item("movie-1");
        DetailsSimilarCompletion completion;
        completion.itemId = "movie-1";
        completion.generation = 11;
        completion.items = {item("similar-1")};

        const auto inactive = DetailsCompletionController::apply(completion, true, false, detail, state);
        assertNoEffects(inactive);
        assert(state.similar().empty());

        completion.items = {item("similar-1"), item("similar-2")};
        const auto applied = DetailsCompletionController::apply(completion, true, true, detail, state);
        assertNoEffects(applied);
        assert(state.similar().size() == 2);
    }

    {
        DetailsScreenState state;
        JellyfinItem detail = item("episode-1", "Episode");
        EpisodeSeriesContextCompletion completion;
        completion.itemId = "episode-1";
        completion.generation = 12;
        completion.series = item("series-1", "Series");
        completion.seasons = {item("season-1", "Season"), item("season-2", "Season")};

        const auto applied = DetailsCompletionController::apply(completion, true, true, detail, state);
        assertNoEffects(applied);
        assert(state.seriesDetail().id == "series-1");
        assert(state.seasons().size() == 2);

        DetailsScreenState rejectedState;
        JellyfinItem movieDetail = item("episode-1", "Movie");
        completion.series = item("series-2", "Series");
        completion.seasons = {item("season-x", "Season")};
        const auto rejected = DetailsCompletionController::apply(completion, true, true, movieDetail, rejectedState);
        assertNoEffects(rejected);
        assert(rejectedState.seriesDetail().id.empty());
        assert(rejectedState.seasons().empty());
    }

    return 0;
}
