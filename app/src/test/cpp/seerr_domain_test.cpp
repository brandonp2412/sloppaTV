#include "seerr_domain.hpp"

#include <cassert>
#include <string>

namespace {
SeerrMediaItem media(std::string id, bool requested = false) {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = item.id;
    item.mediaType = "movie";
    item.tmdbId = 10;
    item.requested = requested;
    return item;
}

SeerrStorageTarget target(int serverId) {
    SeerrStorageTarget value;
    value.mediaType = "movie";
    value.serverId = serverId;
    value.path = "/media";
    return value;
}
} // namespace

int main() {
    const SeerrEndpoint configured{
        .server = "https://seerr.example.nz",
        .auth =
            {
                .sessionCookie = "session",
                .apiKey = "",
            },
    };
    assert(configured.configured());
    assert(configured.matches("https://seerr.example.nz", {.sessionCookie = "session", .apiKey = ""}));
    assert(!configured.matches("https://other.example.nz", {.sessionCookie = "session", .apiKey = ""}));
    assert(!configured.matches("https://seerr.example.nz", {.sessionCookie = "", .apiKey = "other"}));
    assert(isSeerrAuthError("HTTP 401 unauthorized"));
    assert(isSeerrAuthError("request failed with HTTP 403"));
    assert(!isSeerrAuthError("HTTP 500"));

    SeerrDomainState state;
    assert(state.prepareRequest({}, configured, false, false).action == SeerrDomainState::RequestAction::Invalid);
    assert(state.prepareRequest(media("seerr:movie:10", true), configured, false, false).action ==
           SeerrDomainState::RequestAction::AlreadyRequested);

    const SeerrEndpoint disconnected;
    assert(state.prepareRequest(media("seerr:movie:10"), disconnected, false, false).action ==
           SeerrDomainState::RequestAction::NotConfigured);

    assert(state.connection().beginConnect());
    const auto deferred = state.prepareRequest(media("seerr:movie:11"), configured, false, false);
    assert(deferred.action == SeerrDomainState::RequestAction::DeferredForConnection);
    state.connection().endConnect();
    const auto deferredWork = state.connection().takeDeferredWork();
    assert(deferredWork.request && deferredWork.request->id == "seerr:movie:11");

    const auto storage = state.prepareRequest(media("seerr:movie:12"), configured, true, false);
    assert(storage.action == SeerrDomainState::RequestAction::ChooseStorage);
    assert(storage.refreshStorage);

    auto selectedTarget = target(7);
    const auto submit = state.prepareRequest(media("seerr:movie:13"), configured, true, false, &selectedTarget);
    assert(submit.action == SeerrDomainState::RequestAction::Submit);
    assert(submit.target && submit.target->serverId == 7);

    const auto skipPicker = state.prepareRequest(media("seerr:movie:14"), configured, true, true);
    assert(skipPicker.action == SeerrDomainState::RequestAction::Submit);
    assert(!skipPicker.target);

    assert(state.connection().beginConnect());
    assert(state.deferSearchIfConnecting());
    state.connection().endConnect();
    assert(state.connection().takeDeferredWork().retrySearch);
    assert(!state.deferSearchIfConnecting());
    return 0;
}
