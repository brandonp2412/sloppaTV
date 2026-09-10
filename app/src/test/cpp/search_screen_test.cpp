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
    assert(!state.debouncePending());

    state.setQuery("bro");
    assert(state.scheduleDebounce(start));
    assert(state.debouncePending());
    assert(!state.debounceDue(start + 179ms));
    assert(state.debounceDue(start + 180ms));

    assert(state.beginSearch());
    assert(state.loading());
    assert(!state.debouncePending());

    std::vector<JellyfinItem> results(7);
    for (int i = 0; i < 7; ++i) results[static_cast<size_t>(i)].id = std::to_string(i);
    results[0].type = "Movie";
    results[1].type = "Episode";
    results[2].type = "Series";
    results[3].type = "Episode";
    results[4].type = "Movie";
    results[5].type = "Episode";
    results[6].type = "Series";
    assert(state.finishSearch("bro", std::move(results)));
    assert(!state.loading());
    assert(state.results().size() == 7);
    assert(state.topLevelCount() == 4);
    assert(state.rowItemCount(0) == 4);
    assert(state.rowItemCount(1) == 3);
    assert(state.results()[0].id == "0");
    assert(state.results()[1].id == "2");
    assert(state.results()[4].id == "1");

    SearchScreenState resized;
    resized.setQuery("cache");
    std::vector<JellyfinItem> resizedResults(3);
    resizedResults[0].type = "Movie";
    resizedResults[1].type = "Series";
    resizedResults[2].type = "Episode";
    assert(resized.finishSearch("cache", std::move(resizedResults)));
    assert(resized.topLevelCount() == 2);
    resized.results().erase(resized.results().begin());
    assert(resized.topLevelCount() == 1);

    state.moveSelection(1, 0, 5);
    assert(state.selection() == 1);
    state.moveSelection(0, 1, 5);
    assert(state.selection() == 5);
    assert(state.selectedRow() == 1);
    state.moveSelection(1, 0, 5);
    assert(state.selection() == 6);
    state.moveSelection(1, 0, 5);
    assert(state.selection() == 6);
    state.moveSelection(-1, 0, 5);
    assert(state.selection() == 5);
    state.moveSelection(0, -1, 5);
    assert(state.selection() == 1);
    assert(state.selectionOnFirstResultRow());

    SearchScreenState removal = state;
    removal.setSelection(6);
    removal.removeItem("3");
    assert(removal.results().size() == 6);
    assert(removal.selection() == 5);
    assert(removal.results()[static_cast<size_t>(removal.selection())].id == "5");
    assert(removal.topLevelCount() == 4);
    assert(removal.rowItemCount(1) == 2);
    removal.removeItem("0");
    assert(removal.topLevelCount() == 3);
    assert(removal.selection() == 4);
    removal.removeItem("missing");
    assert(removal.selection() == 4);

    state.setQuery("brook");
    state.setLoading(true);
    std::vector<JellyfinItem> stale(1);
    assert(!state.finishSearch("bro", std::move(stale)));
    assert(state.loading());
    assert(state.results().size() == 7);
    assert(!state.failSearch("bro"));
    assert(state.loading());
    assert(state.failSearch("brook"));
    assert(!state.loading());

    assert(state.backspace());
    assert(state.query() == "broo");
    state.setQuery("M\xC4\x81ori");
    assert(state.backspace());
    assert(state.query() == "M\xC4\x81or");
    assert(state.backspace());
    assert(state.query() == "M\xC4\x81o");
    assert(state.backspace());
    assert(state.query() == "M\xC4\x81");
    assert(state.backspace());
    assert(state.query() == "M");
    state.setQuery("");
    assert(!state.scheduleDebounce(start));
    assert(state.results().empty());
    assert(!state.loading());

    state.setQuery("fallout");
    assert(state.scheduleDebounce(start));
    state.cancelPending();
    assert(!state.debouncePending());
    assert(!state.loading());

    state.reset();
    assert(state.query().empty());
    assert(state.keyboard());
    assert(state.selection() == 0);
    return 0;
}
