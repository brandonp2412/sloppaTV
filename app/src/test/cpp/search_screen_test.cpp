#include "search_screen.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    SearchScreenState state;
    const auto start = SearchScreenState::Clock::now();

    assert(state.query().empty());
    assert(state.results().empty());
    assert(state.keyboard());
    assert(!state.loading());
    assert(!state.seerrLoading());

    state.setQuery("bro");
    assert(state.scheduleDebounce(start));
    assert(state.scheduleSeerrDebounce(start, true));
    assert(!state.seerrLoading());
    assert(!state.debounceDue(start + 179ms));
    assert(state.debounceDue(start + 180ms));
    assert(!state.seerrDebounceDue(start + 549ms));
    assert(state.seerrDebounceDue(start + 550ms));
    assert(state.beginSearch());
    assert(state.beginDueSeerrSearch(start + 550ms));
    assert(state.loading());
    assert(state.seerrLoading());

    std::vector<JellyfinItem> library(7);
    for (int i = 0; i < 7; ++i) library[static_cast<size_t>(i)].id = std::to_string(i);
    library[0].type = "Movie";
    library[0].tmdbId = "100";
    library[1].type = "Episode";
    library[2].type = "Series";
    library[2].tmdbId = "200";
    library[3].type = "Episode";
    library[4].type = "Movie";
    library[5].type = "Episode";
    library[6].type = "Series";
    assert(state.finishLibrarySearch("bro", std::move(library)));
    assert(!state.loading());
    assert(state.rowItemCount(SearchScreenState::kLibraryRow) == 4);
    assert(state.rowItemCount(SearchScreenState::kSeerrRow) == 0);
    assert(state.rowItemCount(SearchScreenState::kEpisodeRow) == 3);
    assert(state.results().size() == 7);
    assert(state.results()[0].id == "0");
    assert(state.results()[1].id == "2");
    assert(state.results()[4].id == "1");

    std::vector<SeerrMediaItem> seerr(2);
    seerr[0].id = "seerr:movie:100";
    seerr[0].mediaType = "movie";
    seerr[0].tmdbId = 100;
    seerr[1].id = "seerr:tv:300";
    seerr[1].mediaType = "tv";
    seerr[1].tmdbId = 300;
    seerr[1].jellyfinId = "jellyfin-series-300";
    seerr[1].available = true;
    assert(state.finishSeerrSearch("bro", std::move(seerr)));
    assert(!state.seerrLoading());
    assert(state.rowItemCount(SearchScreenState::kSeerrRow) == 1);
    assert(state.results().size() == 8);
    assert(state.results()[4].id == "seerr:tv:300");
    assert(state.results()[4].externalSource == "seerr");
    assert(state.results()[4].externalJellyfinId == "jellyfin-series-300");
    assert(state.results()[4].externalAvailable);
    assert(state.results()[5].id == "1");

    state.moveSelection(1, 0, 5);
    assert(state.selection() == 1);
    state.moveSelection(0, 1, 5);
    assert(state.selectedRow() == SearchScreenState::kSeerrRow);
    assert(state.results()[static_cast<size_t>(state.selection())].id == "seerr:tv:300");
    state.moveSelection(0, 1, 5);
    assert(state.selectedRow() == SearchScreenState::kEpisodeRow);
    state.moveSelection(0, -1, 5);
    assert(state.selectedRow() == SearchScreenState::kSeerrRow);
    state.moveSelection(0, -1, 5);
    assert(state.selectedRow() == SearchScreenState::kLibraryRow);
    assert(state.selectionOnFirstResultRow());

    state.markSeerrRequested("seerr:tv:300", "Episode 1 queued", 42);
    const auto requested = std::find_if(state.results().begin(), state.results().end(), [](const JellyfinItem& item) {
        return item.id == "seerr:tv:300";
    });
    assert(requested != state.results().end());
    assert(requested->externalRequested);
    assert(requested->externalRequestId == 42);
    assert(requested->externalStatus == "Episode 1 queued");
    state.markSeerrUnrequested("seerr:tv:300");
    const auto unrequested = std::find_if(state.results().begin(), state.results().end(), [](const JellyfinItem& item) {
        return item.id == "seerr:tv:300";
    });
    assert(unrequested != state.results().end());
    assert(!unrequested->externalRequested);
    assert(unrequested->externalRequestId == 0);

    state.setQuery("brook");
    state.setLoading(true);
    assert(state.scheduleSeerrDebounce(start + 1s, true));
    std::vector<JellyfinItem> stale(1);
    assert(!state.finishLibrarySearch("bro", std::move(stale)));
    assert(state.loading());
    assert(!state.failLibrarySearch("bro"));
    assert(state.failLibrarySearch("brook"));
    assert(!state.loading());
    assert(!state.failSeerrSearch("bro", "stale"));
    assert(state.beginDueSeerrSearch(start + 1550ms));
    assert(state.failSeerrSearch("brook", "offline"));
    assert(state.seerrError() == "offline");
    assert(!state.scheduleSeerrDebounce(start + 2s, true));

    SearchScreenState deletion;
    deletion.setQuery("delete");
    std::vector<JellyfinItem> deletionResults(3);
    deletionResults[0].id = "movie";
    deletionResults[0].type = "Movie";
    deletionResults[1].id = "series";
    deletionResults[1].type = "Series";
    deletionResults[2].id = "episode";
    deletionResults[2].type = "Episode";
    assert(deletion.finishLibrarySearch("delete", std::move(deletionResults)));
    deletion.setSelection(2);
    assert(deletion.removeItem("episode"));
    assert(deletion.results().size() == 2);
    assert(deletion.selection() == 1);
    assert(!deletion.removeItem("missing"));
    assert(deletion.removeItem("movie"));
    assert(deletion.results().front().id == "series");

    state.setQuery("M\xC4\x81ori");
    assert(state.backspace());
    assert(state.query() == "M\xC4\x81or");
    assert(state.backspace());
    assert(state.query() == "M\xC4\x81o");
    assert(state.backspace());
    assert(state.query() == "M\xC4\x81");
    assert(state.backspace());
    assert(state.query() == "M");

    state.setQuery("ab");
    assert(state.scheduleSeerrDebounce(start + 3s, true));
    assert(!state.seerrLoading());
    assert(!state.seerrDebouncePending());

    state.setQuery("");
    assert(!state.scheduleDebounce(start));
    assert(!state.scheduleSeerrDebounce(start, true));
    assert(state.results().empty());
    assert(!state.loading());
    assert(!state.seerrLoading());

    state.reset();
    assert(state.query().empty());
    assert(state.keyboard());
    assert(state.selection() == 0);
    return 0;
}
