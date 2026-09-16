#include "seerr_connection_state.hpp"

#include <cassert>

namespace {
SeerrMediaItem media(std::string id) {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = item.id;
    item.mediaType = "movie";
    item.tmdbId = 10;
    return item;
}
}

int main() {
    SeerrConnectionState state;

    assert(!state.connecting());
    assert(state.beginConnect());
    assert(state.connecting());
    assert(!state.beginConnect());

    state.deferRequest(media("seerr:movie:10"));
    state.deferSearchRetry();
    state.endConnect();
    assert(!state.connecting());

    auto deferred = state.takeDeferredWork();
    assert(deferred.request);
    assert(deferred.request->id == "seerr:movie:10");
    assert(deferred.retrySearch);

    deferred = state.takeDeferredWork();
    assert(!deferred.request);
    assert(!deferred.retrySearch);

    assert(state.beginConnect());
    state.deferRequest(media("seerr:movie:20"));
    state.deferSearchRetry();
    state.failConnect();
    assert(!state.connecting());
    deferred = state.takeDeferredWork();
    assert(!deferred.request);
    assert(!deferred.retrySearch);

    assert(state.beginConnect());
    state.deferRequest(media("seerr:movie:30"));
    state.endConnect();
    assert(state.beginConnect());
    state.endConnect();
    deferred = state.takeDeferredWork();
    assert(deferred.request);
    assert(deferred.request->id == "seerr:movie:30");
    assert(!deferred.retrySearch);
    return 0;
}
