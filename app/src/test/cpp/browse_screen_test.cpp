#include "browse_screen.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

JellyfinItem item(std::string id, std::string name, std::string type = "Movie") {
    JellyfinItem value;
    value.id = std::move(id);
    value.name = std::move(name);
    value.type = std::move(type);
    return value;
}

}

int main() {
    BrowseScreenState state;
    JellyfinItem movies = item("movies", "Movies", "CollectionFolder");
    movies.collectionType = "movies";
    state.resetForLibrary(movies);
    assert(state.activeContainer().id == "movies");
    assert(state.hasFilterBar());
    assert(state.filterLabels().size() == 5);
    assert(state.heading() == "Movies");

    state.moveFilter(2);
    assert(state.filterSelection() == 2);
    assert(state.applyFilter(state.filterSelection()));
    assert(state.mode() == BrowseContentMode::Genres);
    assert(state.heading() == "Movies - GENRES");

    state.replacePage({item("g1", "Comedy", "Genre"), item("g2", "Drama", "Genre")}, 60);
    state.setSelection(1);
    assert(state.selection() == 1);
    state.selectGenre("Drama");
    assert(state.mode() == BrowseContentMode::GenreItems);
    assert(state.heading() == "Movies - Drama");
    assert(state.items().empty());
    assert(state.back() == BrowseBackAction::Reload);
    assert(state.mode() == BrowseContentMode::Genres);

    assert(!state.applyFilter(3));
    assert(state.mode() == BrowseContentMode::Letters);
    assert(state.syntheticPage());
    assert(state.items().size() == 26);
    assert(state.items().front().name == "A");
    assert(state.items().back().name == "Z");
    state.selectLetter("M");
    assert(state.mode() == BrowseContentMode::LetterItems);
    assert(state.heading() == "Movies - M");
    assert(state.back() == BrowseBackAction::LocalPage);
    assert(state.mode() == BrowseContentMode::Letters);
    assert(state.items().size() == 26);

    assert(state.applyFilter(0));
    state.replacePage({item("m1", "One"), item("m2", "Two")}, 2);
    assert(state.hasMore());
    assert(state.nextIndex() == 2);
    state.appendPage({item("m3", "Three"), item("m4", "Four")}, 2, 2);
    assert(state.items().size() == 4);
    assert(state.nextIndex() == 4);
    assert(state.hasMore());

    JellyfinItem box = item("box", "Collection", "BoxSet");
    state.setSelection(2);
    state.openContainer(box, true);
    assert(state.nested());
    assert(!state.hasFilterBar());
    assert(state.activeContainer().id == "box");
    state.replacePage({item("child", "Child")}, 60);
    assert(state.back() == BrowseBackAction::RestoredSnapshot);
    assert(state.activeContainer().id == "movies");
    assert(state.items().size() == 4);
    assert(state.selection() == 2);
    assert(state.nextIndex() == 4);
    state.removeItem("m3");
    assert(state.items().size() == 3);
    assert(state.selection() == 2);

    state.clear();
    assert(state.activeContainer().id.empty());
    assert(state.items().empty());
    assert(state.back() == BrowseBackAction::Exit);
    return 0;
}
