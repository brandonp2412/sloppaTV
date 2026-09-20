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

} // namespace

int main() {
    BrowseScreenState state;
    JellyfinItem movies = item("movies", "Movies", "CollectionFolder");
    movies.collectionType = "movies";
    state.resetForLibrary(movies);
    assert(state.activeContainer().id == "movies");
    assert(state.hasFilterBar());
    assert(state.filterLabels().size() == 5);
    assert(state.heading() == "Movies");
    assert(state.activeFilterSelection() == 0);

    state.moveFilter(1);
    assert(state.filterSelection() == 1);
    assert(state.activeFilterSelection() == 0);
    state.moveFilter(1);
    assert(state.filterSelection() == 2);
    assert(state.activeFilterSelection() == 0);
    assert(state.applyFilter(state.filterSelection()));
    assert(state.mode() == BrowseContentMode::Genres);
    assert(state.activeFilterSelection() == 2);
    assert(state.heading() == "Movies - GENRES");

    state.replacePage({item("g1", "Comedy", "Genre"), item("g2", "Drama", "Genre")}, 60);
    state.setSelection(1);
    assert(state.selection() == 1);
    state.selectGenre("Drama");
    assert(state.mode() == BrowseContentMode::GenreItems);
    assert(state.activeFilterSelection() == 2);
    assert(state.heading() == "Movies - Drama");
    assert(state.items().empty());
    assert(state.back() == BrowseBackAction::Reload);
    assert(state.mode() == BrowseContentMode::Genres);

    assert(!state.applyFilter(3));
    assert(state.mode() == BrowseContentMode::Letters);
    assert(state.activeFilterSelection() == 3);
    assert(state.syntheticPage());
    assert(state.items().size() == 26);
    assert(state.items().front().name == "A");
    assert(state.items().back().name == "Z");
    state.selectLetter("M");
    assert(state.mode() == BrowseContentMode::LetterItems);
    assert(state.activeFilterSelection() == 3);
    assert(state.heading() == "Movies - M");
    assert(state.back() == BrowseBackAction::LocalPage);
    assert(state.mode() == BrowseContentMode::Letters);
    assert(state.items().size() == 26);

    assert(state.applyFilter(4));
    assert(state.mode() == BrowseContentMode::Collections);
    assert(state.activeFilterSelection() == 4);
    assert(state.applyFilter(0));
    assert(state.activeFilterSelection() == 0);
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
    assert(state.nextIndex() == 3);

    state.setSelection(1);
    state.openContainer(box, true);
    state.replacePage({item("child", "Child")}, 60);
    state.removeItem("m2");
    assert(state.back() == BrowseBackAction::RestoredSnapshot);
    assert(state.items().size() == 2);
    assert(state.nextIndex() == 2);
    assert(state.selection() == 1);

    state.replacePage({item("a", "A"), item("b", "B"), item("c", "C")}, 60);
    state.setSelection(1);
    state.replacePage({item("x", "X"), item("b", "B"), item("c", "C")}, 60);
    assert(state.selection() == 1);
    assert(state.items()[static_cast<size_t>(state.selection())].id == "b");
    state.removeItem("x");
    assert(state.selection() == 0);
    assert(state.items()[static_cast<size_t>(state.selection())].id == "b");

    state.setSelection(1);
    state.openContainer(box, true);
    state.replacePage({item("child", "Child")}, 60);
    state.removeItem("b");
    assert(state.back() == BrowseBackAction::RestoredSnapshot);
    assert(state.items().size() == 1);
    assert(state.selection() == 0);
    assert(state.items()[0].id == "c");

    BrowseScreenState inputState;
    inputState.resetForLibrary(movies);
    inputState.replacePage({item("i0", "0"), item("i1", "1"), item("i2", "2"), item("i3", "3"), item("i4", "4"),
                            item("i5", "5"), item("i6", "6")},
                           60);
    auto command = inputState.handleInput(BrowseScreenInput::Up, 5);
    assert(command.type == BrowseScreenCommandType::None);
    assert(inputState.filterFocused());
    command = inputState.handleInput(BrowseScreenInput::Right, 5);
    assert(command.type == BrowseScreenCommandType::None);
    assert(inputState.filterSelection() == 1);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::ApplyFilter);
    assert(!inputState.filterFocused());

    command = inputState.handleInput(BrowseScreenInput::Right, 5);
    assert(command.type == BrowseScreenCommandType::SelectionChanged);
    assert(inputState.selection() == 1);
    command = inputState.handleInput(BrowseScreenInput::Down, 5);
    assert(command.type == BrowseScreenCommandType::SelectionChanged);
    assert(inputState.selection() == 6);
    command = inputState.handleInput(BrowseScreenInput::Context, 5);
    assert(command.type == BrowseScreenCommandType::OpenContext);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::OpenDetails);

    inputState.replacePage({item("genre", "Comedy", "Genre")}, 60);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::SelectGenre);

    inputState.replacePage({item("letter", "C", "Letter")}, 60);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::SelectLetter);

    inputState.replacePage({item("folder", "Folder", "Folder")}, 60);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::OpenContainer);

    inputState.replacePage({item("box", "Box", "BoxSet")}, 60);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::OpenContainer);

    inputState.replacePage({item("library", "Library", "CollectionFolder")}, 60);
    command = inputState.handleInput(BrowseScreenInput::Activate, 5);
    assert(command.type == BrowseScreenCommandType::OpenContainer);

    inputState.openContainer(box, true);
    inputState.replacePage({item("nested", "Nested")}, 60);
    command = inputState.handleInput(BrowseScreenInput::Back, 5);
    assert(command.type == BrowseScreenCommandType::Back);
    assert(command.backAction == BrowseBackAction::RestoredSnapshot);
    assert(inputState.activeContainer().id == "movies");

    state.clear();
    assert(state.activeContainer().id.empty());
    assert(state.items().empty());
    assert(state.back() == BrowseBackAction::Exit);
    return 0;
}
