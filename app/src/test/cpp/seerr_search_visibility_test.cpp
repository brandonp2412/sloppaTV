#include "search_screen.hpp"
#include "seerr_media.hpp"

#include <cassert>
#include <vector>

int main() {
    SeerrMediaItem available;
    available.id = "seerr:movie:123";
    available.name = "Movie";
    available.mediaType = "movie";
    available.tmdbId = 123;
    available.available = true;
    assert(includeSeerrSearchResult(available));
    SeerrMediaItem invalid;
    invalid.id = "seerr:movie:0";
    invalid.mediaType = "movie";
    assert(!includeSeerrSearchResult(invalid));

    std::vector<SeerrMediaItem> remote{available};
    SearchScreenState state(remote);
    state.setQuery("Movie");
    state.refreshSeerrResults();
    assert(state.rowItemCount(SearchScreenState::kSeerrRow) == 1);
    assert(state.selectedSeerrResult() != nullptr);
    JellyfinItem local;
    local.id = "local:movie";
    local.name = "Movie";
    local.type = "Movie";
    local.tmdbId = "123";
    assert(state.finishLibrarySearch("Movie", {local}));
    assert(state.rowItemCount(SearchScreenState::kSeerrRow) == 0);
    assert(state.rowItemCount(SearchScreenState::kLibraryRow) == 1);
    return 0;
}
