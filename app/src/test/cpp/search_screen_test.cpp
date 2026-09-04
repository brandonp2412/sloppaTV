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
    assert(state.finishSearch("bro", std::move(results)));
    assert(!state.loading());
    assert(state.results().size() == 7);

    state.moveSelection(1, 0, 5);
    assert(state.selection() == 1);
    state.moveSelection(0, 1, 5);
    assert(state.selection() == 6);
    state.moveSelection(1, 0, 5);
    assert(state.selection() == 6);
    state.moveSelection(-1, 0, 5);
    assert(state.selection() == 5);

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
