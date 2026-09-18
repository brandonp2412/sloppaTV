#include "browse_navigation_controller.hpp"
#include "home_navigation_controller.hpp"
#include "search_navigation_controller.hpp"
#include "seerr_search_state.hpp"

#include <cassert>
#include <string>
#include <utility>
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
    JellyfinHomeRow media;
    media.title = "My Media";
    media.items = {item("library", "Movies", "CollectionFolder")};

    JellyfinHomeRow latest;
    latest.title = "Latest Movies";
    latest.items = {item("movie", "Movie")};

    HomeScreenState home;
    home.reset();
    home.setSelections({0, 0});
    home.focusToolbar(1);
    auto homeAction = HomeNavigationController::handle(home, ScreenNavigationKey::Down, {media, latest}, {});
    assert(homeAction.type == HomeNavigationActionType::None);
    homeAction = HomeNavigationController::handle(home, ScreenNavigationKey::Activate, {media, latest}, {});
    assert(homeAction.type == HomeNavigationActionType::OpenLibrary);
    assert(homeAction.item->id == "library");

    home.setRow(1);
    homeAction = HomeNavigationController::handle(home, ScreenNavigationKey::Activate, {media, latest}, {});
    assert(homeAction.type == HomeNavigationActionType::OpenDetails);
    assert(homeAction.item->id == "movie");

    JellyfinHomeRow seerrRow;
    seerrRow.title = "Seerr requests";
    JellyfinItem seerrProjected = item("seerr:movie:1", "Requested");
    seerrProjected.externalSource = "seerr";
    seerrRow.items = {seerrProjected};
    SeerrMediaItem pending;
    pending.id = "seerr:movie:1";
    pending.jellyfinId = "jellyfin-movie";
    home.setSelections({0});
    home.setRow(0);
    homeAction = HomeNavigationController::handle(home, ScreenNavigationKey::Activate, {seerrRow}, {pending});
    assert(homeAction.type == HomeNavigationActionType::OpenDetails);
    assert(homeAction.item->id == "jellyfin-movie");
    assert(homeAction.item->externalSource.empty());

    pending.jellyfinId.clear();
    homeAction = HomeNavigationController::handle(home, ScreenNavigationKey::Activate, {seerrRow}, {pending});
    assert(homeAction.type == HomeNavigationActionType::OpenContext);

    home.setSelections({0, 0});
    home.setRow(0);
    homeAction = HomeNavigationController::handle(home, ScreenNavigationKey::Down, {media, latest}, {});
    assert(homeAction.type == HomeNavigationActionType::FinalizeNavigation);
    assert(homeAction.prefetchRow == 1);
    assert(homeAction.prefetchSelection == 0);

    BrowseScreenState browse;
    JellyfinItem library = item("movies", "Movies", "CollectionFolder");
    library.collectionType = "movies";
    browse.resetForLibrary(library);
    browse.replacePage({item("genre", "Drama", "Genre")}, 60);
    auto browseAction = BrowseNavigationController::handle(browse, ScreenNavigationKey::Activate, 5, false);
    assert(browseAction.type == BrowseNavigationActionType::ReloadPage);
    assert(browse.mode() == BrowseContentMode::GenreItems);
    assert(browse.genre() == "Drama");

    browse.resetForLibrary(library);
    browse.replacePage({item("folder", "Folder", "Folder")}, 60);
    browseAction = BrowseNavigationController::handle(browse, ScreenNavigationKey::Submit, 5, false);
    assert(browseAction.type == BrowseNavigationActionType::OpenContainer);
    assert(browseAction.item->id == "folder");

    browse.resetForLibrary(library);
    std::vector<JellyfinItem> many;
    for (int i = 0; i < 13; ++i) many.push_back(item(std::to_string(i), std::to_string(i)));
    browse.replacePage(std::move(many), 13);
    browse.setSelection(1);
    browseAction = BrowseNavigationController::handle(browse, ScreenNavigationKey::Right, 5, false);
    assert(browseAction.type == BrowseNavigationActionType::SelectionChanged);
    assert(browseAction.loadMore);

    SeerrSearchState seerrState;
    SearchScreenState search(seerrState.results());
    auto searchAction = SearchNavigationController::handle(search, ScreenNavigationKey::Left, 5);
    assert(searchAction.type == SearchNavigationActionType::MoveKeyboard);
    assert(searchAction.dx == -1);
    searchAction = SearchNavigationController::handle(search, ScreenNavigationKey::Submit, 5);
    assert(searchAction.type == SearchNavigationActionType::SubmitSearch);
    searchAction = SearchNavigationController::handle(search, ScreenNavigationKey::Back, 5);
    assert(searchAction.type == SearchNavigationActionType::Exit);
    assert(!search.loading());

    search.reset();
    search.setKeyboard(false);
    JellyfinItem local = item("local", "Local");
    search.setQuery("local");
    assert(search.finishLibrarySearch("local", {local}));
    searchAction = SearchNavigationController::handle(search, ScreenNavigationKey::Context, 5);
    assert(searchAction.type == SearchNavigationActionType::OpenContext);
    assert(searchAction.item->id == "local");
    searchAction = SearchNavigationController::handle(search, ScreenNavigationKey::Activate, 5);
    assert(searchAction.type == SearchNavigationActionType::OpenDetails);

    SeerrSearchState remoteState;
    SearchScreenState remote(remoteState.results());
    remote.setQuery("remote");
    assert(remoteState.beginImmediate(remote.query()).started);
    SeerrMediaItem remoteItem;
    remoteItem.id = "seerr:movie:2";
    remoteItem.mediaType = "movie";
    remoteItem.tmdbId = 2;
    assert(remoteState.finish("remote", {remoteItem}));
    remote.refreshSeerrResults();
    remote.setKeyboard(false);
    searchAction = SearchNavigationController::handle(remote, ScreenNavigationKey::Submit, 5);
    assert(searchAction.type == SearchNavigationActionType::RequestSeerr);
    assert(searchAction.seerrItem->id == "seerr:movie:2");

    return 0;
}
