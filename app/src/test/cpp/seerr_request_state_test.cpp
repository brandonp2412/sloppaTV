#include "seerr_request_state.hpp"

#include <cassert>
#include <chrono>
#include <vector>

namespace {
SeerrMediaItem media(std::string id, int requestId = 0, bool requested = false) {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = item.id;
    item.mediaType = "movie";
    item.tmdbId = 10;
    item.requestId = requestId;
    item.requested = requested;
    return item;
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    SeerrRequestState state;
    const auto start = SeerrRequestState::Clock::now();

    assert(!state.pendingLoading());
    assert(!state.pendingRefreshDue(start));
    assert(state.beginPendingRefresh());
    assert(state.pendingLoading());
    assert(!state.beginPendingRefresh());

    state.finishPendingRefresh({media("seerr:movie:1", 11, true)}, start);
    assert(!state.pendingLoading());
    assert(state.pending().size() == 1);
    assert(!state.pendingRefreshDue(start + 59s));
    assert(state.pendingRefreshDue(start + 60s));
    state.clearPendingRefreshDeadline();
    assert(!state.pendingRefreshDue(start + 60s));

    state.markRequestSucceeded(media("seerr:movie:2"), 22, "Queued for download", start + 1s);
    assert(state.pending().size() == 2);
    assert(state.pending().front().id == "seerr:movie:2");
    const auto* optimistic = state.findPending("seerr:movie:2");
    assert(optimistic);
    assert(optimistic->requested);
    assert(optimistic->requestId == 22);
    assert(optimistic->mediaStatus == 2);
    assert(optimistic->status == "Queued for download");
    assert(!state.pendingRefreshDue(start + 2999ms));
    assert(state.pendingRefreshDue(start + 3s));

    assert(state.beginPendingRefresh());
    state.finishPendingRefresh({media("seerr:movie:1", 11, true)}, start + 4s);
    assert(state.pending().size() == 2);
    assert(state.findPending("seerr:movie:2"));

    std::vector<SeerrMediaItem> searchResults{
        media("seerr:movie:2", 22, true),
        media("seerr:movie:3", 33, true),
        media("seerr:movie:4", 0, false),
    };
    assert(state.mergeRequestedSearch(searchResults, start + 5s));
    assert(state.pending().size() == 3);
    assert(state.findPending("seerr:movie:3"));
    assert(!state.findPending("seerr:movie:4"));

    state.erasePending("seerr:movie:3", 0);
    assert(!state.findPending("seerr:movie:3"));
    state.erasePending("missing", 22);
    assert(!state.findPending("seerr:movie:2"));

    assert(state.beginPendingRefresh());
    state.invalidatePendingRefresh(start + 6s);
    assert(!state.pendingLoading());
    assert(state.pendingRefreshDue(start + 6s));

    state.clearPendingRefreshDeadline();
    assert(state.beginPendingRefresh());
    state.failPendingRefresh(start + 7s);
    assert(!state.pendingRefreshDue(start + 66s));
    assert(state.pendingRefreshDue(start + 67s));

    state.resetPending();
    assert(state.pending().empty());
    assert(!state.pendingLoading());
    assert(!state.pendingRefreshDue(start + 10min));
    return 0;
}
