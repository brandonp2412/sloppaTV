#include "seerr_search_state.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

namespace {
SeerrMediaItem media(std::string id) {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = item.id;
    item.mediaType = "movie";
    item.tmdbId = 10;
    return item;
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    SeerrSearchState state;
    const auto start = SeerrSearchState::Clock::now();

    assert(!state.loading());
    assert(!state.debouncePending());
    assert(state.error().empty());
    assert(state.results().empty());

    assert(!state.schedule("ab", start, true));
    assert(!state.debouncePending());
    assert(state.query().empty());

    assert(state.schedule("brook", start, true));
    assert(state.query() == "brook");
    assert(state.debouncePending());
    assert(!state.debounceDue(start + 549ms));
    assert(state.debounceDue(start + 550ms));
    assert(state.beginDue(start + 550ms));
    assert(state.loading());
    assert(!state.debouncePending());

    std::vector<SeerrMediaItem> results{media("seerr:movie:10")};
    assert(state.finish("brook", std::move(results)));
    assert(!state.loading());
    assert(state.results().size() == 1);
    assert(!state.finish("stale", {}));
    assert(state.results().size() == 1);

    assert(state.schedule("brooks", start + 1s, true));
    const auto immediate = state.beginImmediate("brooks");
    assert(immediate.started);
    assert(!immediate.resultsChanged);
    assert(state.loading());
    assert(state.fail("brooks", "offline"));
    assert(!state.loading());
    assert(state.error() == "offline");
    assert(state.results().empty());

    const auto repeat = state.beginImmediate("brooks");
    assert(!repeat.started);
    assert(!repeat.resultsChanged);

    assert(state.schedule("movie", start + 2s, true));
    const auto changed = state.beginImmediate("other");
    assert(changed.started);
    assert(!changed.resultsChanged);
    assert(state.query() == "other");

    assert(state.finish("other", {media("seerr:movie:20")}));
    state.markRequested("seerr:movie:20", "Queued", 42);
    assert(state.results().front().requested);
    assert(state.results().front().requestId == 42);
    assert(state.results().front().status == "Queued");
    assert(state.results().front().mediaStatus == 2);

    state.markUnrequested("seerr:movie:20");
    assert(!state.results().front().requested);
    assert(state.results().front().requestId == 0);
    assert(state.results().front().status.empty());
    assert(state.query().empty());

    assert(state.schedule("movie", start + 3s, true));
    state.cancelPending();
    assert(!state.debouncePending());
    assert(!state.loading());

    assert(state.schedule("series", start + 4s, true));
    assert(state.schedule("", start + 5s, false));
    assert(state.query().empty());
    assert(state.results().empty());
    assert(state.error().empty());

    state.reset();
    assert(!state.loading());
    assert(!state.debouncePending());
    assert(state.results().empty());
    return 0;
}
