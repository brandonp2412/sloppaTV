#include "search_screen.hpp"
#include "seerr_search_state.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    SeerrSearchState seerrState;
    SearchScreenState state(seerrState.results());
    const auto start = SearchScreenState::Clock::now();

    assert(state.query().empty());
    assert(state.results().empty());
    assert(state.keyboard());
    assert(!state.loading());
    assert(!seerrState.loading());

    state.setQuery("bro");
    assert(state.scheduleDebounce(start));
    assert(seerrState.schedule(state.query(), start, true));
    state.refreshSeerrResults();
    assert(!seerrState.loading());
    assert(!state.debounceDue(start + 179ms));
    assert(state.debounceDue(start + 180ms));
    assert(!seerrState.debounceDue(start + 549ms));
    assert(seerrState.debounceDue(start + 550ms));
    assert(state.beginSearch());
    assert(seerrState.beginDue(start + 550ms));
    assert(state.loading());
    assert(seerrState.loading());

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
    assert(seerrState.finish("bro", std::move(seerr)));
    state.refreshSeerrResults();
    assert(!seerrState.loading());
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
    const auto* selectedSeerr = state.selectedSeerrResult();
    assert(selectedSeerr);
    assert(selectedSeerr->id == "seerr:tv:300");
    assert(selectedSeerr->jellyfinId == "jellyfin-series-300");
    state.moveSelection(0, 1, 5);
    assert(state.selectedRow() == SearchScreenState::kEpisodeRow);
    state.moveSelection(0, -1, 5);
    assert(state.selectedRow() == SearchScreenState::kSeerrRow);
    state.moveSelection(0, -1, 5);
    assert(state.selectedRow() == SearchScreenState::kLibraryRow);
    assert(state.selectionOnFirstResultRow());
    assert(state.selectedSeerrResult() == nullptr);

    seerrState.markRequested("seerr:tv:300", "Episode 1 queued", 42);
    state.refreshSeerrResults();
    const auto requested = std::find_if(state.results().begin(), state.results().end(),
                                        [](const JellyfinItem& item) { return item.id == "seerr:tv:300"; });
    assert(requested != state.results().end());
    assert(requested->externalRequested);
    assert(requested->externalRequestId == 42);
    assert(requested->externalStatus == "Episode 1 queued");
    seerrState.markUnrequested("seerr:tv:300");
    state.refreshSeerrResults();
    const auto unrequested = std::find_if(state.results().begin(), state.results().end(),
                                          [](const JellyfinItem& item) { return item.id == "seerr:tv:300"; });
    assert(unrequested != state.results().end());
    assert(!unrequested->externalRequested);
    assert(unrequested->externalRequestId == 0);

    state.setQuery("brook");
    state.setLoading(true);
    assert(seerrState.schedule(state.query(), start + 1s, true));
    state.refreshSeerrResults();
    std::vector<JellyfinItem> stale(1);
    assert(!state.finishLibrarySearch("bro", std::move(stale)));
    assert(state.loading());
    assert(!state.failLibrarySearch("bro"));
    assert(state.failLibrarySearch("brook"));
    assert(!state.loading());
    assert(!seerrState.fail("bro", "stale"));
    assert(seerrState.beginDue(start + 1550ms));
    assert(seerrState.fail("brook", "offline"));
    state.refreshSeerrResults();
    assert(seerrState.error() == "offline");
    assert(!seerrState.schedule(state.query(), start + 2s, true));

    std::vector<SeerrMediaItem> noDeletionSeerr;
    SearchScreenState deletion(noDeletionSeerr);
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
    assert(seerrState.schedule(state.query(), start + 3s, true));
    state.refreshSeerrResults();
    assert(!seerrState.loading());
    assert(!seerrState.debouncePending());

    state.setQuery("");
    assert(!state.scheduleDebounce(start));
    assert(!seerrState.schedule(state.query(), start, true));
    state.refreshSeerrResults();
    assert(state.results().empty());
    assert(!state.loading());
    assert(!seerrState.loading());

    std::vector<SeerrMediaItem> noInputSeerr;
    SearchScreenState inputState(noInputSeerr);
    auto command = inputState.handleInput(SearchScreenInput::Left, 5);
    assert(command.type == SearchScreenCommandType::MoveKeyboard);
    assert(command.dx == -1);
    command = inputState.handleInput(SearchScreenInput::Activate, 5);
    assert(command.type == SearchScreenCommandType::ActivateKeyboard);
    command = inputState.handleInput(SearchScreenInput::Submit, 5);
    assert(command.type == SearchScreenCommandType::SubmitSearch);
    assert(!inputState.keyboard());
    command = inputState.handleInput(SearchScreenInput::Back, 5);
    assert(command.type == SearchScreenCommandType::Exit);

    inputState.reset();
    command = inputState.handleInput(SearchScreenInput::Back, 5);
    assert(command.type == SearchScreenCommandType::None);
    assert(!inputState.keyboard());
    command = inputState.handleInput(SearchScreenInput::Activate, 5);
    assert(command.type == SearchScreenCommandType::OpenTextInput);

    JellyfinItem localResult;
    localResult.id = "local";
    localResult.type = "Movie";
    inputState.setQuery("local");
    assert(inputState.finishLibrarySearch("local", {localResult}));
    inputState.setKeyboard(false);
    command = inputState.handleInput(SearchScreenInput::Context, 5);
    assert(command.type == SearchScreenCommandType::OpenContext);
    command = inputState.handleInput(SearchScreenInput::Activate, 5);
    assert(command.type == SearchScreenCommandType::OpenDetails);
    command = inputState.handleInput(SearchScreenInput::Up, 5);
    assert(command.type == SearchScreenCommandType::OpenTextInput);

    SeerrSearchState seerrInputState;
    SearchScreenState seerrInput(seerrInputState.results());
    seerrInput.setQuery("remote");
    assert(seerrInputState.beginImmediate(seerrInput.query()).started);
    SeerrMediaItem remoteResult;
    remoteResult.id = "seerr:movie:999";
    remoteResult.mediaType = "movie";
    remoteResult.tmdbId = 999;
    assert(seerrInputState.finish("remote", {remoteResult}));
    seerrInput.refreshSeerrResults();
    seerrInput.setKeyboard(false);
    command = seerrInput.handleInput(SearchScreenInput::Context, 5);
    assert(command.type == SearchScreenCommandType::None);
    command = seerrInput.handleInput(SearchScreenInput::Submit, 5);
    assert(command.type == SearchScreenCommandType::RequestSeerr);

    seerrState.reset();
    state.reset();
    assert(state.query().empty());
    assert(state.keyboard());
    assert(state.selection() == 0);
    return 0;
}
