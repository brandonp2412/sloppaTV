#include "seerr_domain.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

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
    using namespace std::chrono_literals;
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

    SeerrDomainState connectionStarts;
    assert(connectionStarts.prepareConnect("", true) == SeerrDomainState::ConnectAction::MissingServer);
    assert(!connectionStarts.connection().connecting());
    assert(connectionStarts.prepareConnect("https://seerr.example.nz", false) ==
           SeerrDomainState::ConnectAction::MissingJellyfin);
    assert(!connectionStarts.connection().connecting());
    assert(connectionStarts.prepareConnect("https://seerr.example.nz", true) ==
           SeerrDomainState::ConnectAction::Submit);
    assert(connectionStarts.connection().connecting());
    assert(connectionStarts.prepareConnect("https://other.example.nz", true) ==
           SeerrDomainState::ConnectAction::AlreadyConnecting);
    connectionStarts.connection().endConnect();

    SeerrDomainState preAuthFailure;
    assert(preAuthFailure.prepareConnect("https://seerr.example.nz", true) ==
           SeerrDomainState::ConnectAction::Submit);
    preAuthFailure.connection().deferRequest(media("seerr:movie:1"));
    auto connectCompletion = preAuthFailure.completeConnect(false, false, true);
    assert(connectCompletion == SeerrDomainState::ConnectCompletionAction::PreAuthenticationFailed);
    assert(!preAuthFailure.connection().connecting());
    assert(!preAuthFailure.connection().takeDeferredWork().request);

    SeerrDomainState authConnectFailure;
    assert(authConnectFailure.prepareConnect("https://seerr.example.nz", true) ==
           SeerrDomainState::ConnectAction::Submit);
    authConnectFailure.connection().deferRequest(media("seerr:movie:2"));
    connectCompletion = authConnectFailure.completeConnect(false, true, true);
    assert(connectCompletion == SeerrDomainState::ConnectCompletionAction::AuthenticationFailed);
    assert(!authConnectFailure.connection().connecting());
    assert(!authConnectFailure.connection().takeDeferredWork().request);

    SeerrDomainState staleCompletion;
    assert(staleCompletion.prepareConnect("https://seerr.example.nz", true) ==
           SeerrDomainState::ConnectAction::Submit);
    staleCompletion.connection().deferRequest(media("seerr:movie:3"));
    connectCompletion = staleCompletion.completeConnect(false, true, false);
    assert(connectCompletion == SeerrDomainState::ConnectCompletionAction::Stale);
    assert(!staleCompletion.connection().connecting());
    auto staleDeferred = staleCompletion.connection().takeDeferredWork();
    assert(staleDeferred.request && staleDeferred.request->id == "seerr:movie:3");

    SeerrDomainState successfulConnect;
    assert(successfulConnect.prepareConnect("https://seerr.example.nz", true) ==
           SeerrDomainState::ConnectAction::Submit);
    successfulConnect.connection().deferRequest(media("seerr:movie:4"));
    successfulConnect.connection().deferSearchRetry();
    connectCompletion = successfulConnect.completeConnect(true, false, true);
    assert(connectCompletion == SeerrDomainState::ConnectCompletionAction::Connected);
    assert(!successfulConnect.connection().connecting());
    const auto successfulDeferred = successfulConnect.takeDeferredConnectionWork();
    assert(successfulDeferred.request && successfulDeferred.request->id == "seerr:movie:4");
    assert(successfulDeferred.retrySearch);
    assert(!successfulConnect.takeDeferredConnectionWork().request);

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

    const auto start = SeerrRequestState::Clock::now();

    SeerrDomainState searchCompletions;
    assert(!searchCompletions.completeSearchFailure("HTTP 401 unauthorized", false));
    assert(!searchCompletions.takeDeferredConnectionWork().retrySearch);
    assert(!searchCompletions.completeSearchFailure("HTTP 500", true));
    assert(!searchCompletions.takeDeferredConnectionWork().retrySearch);
    assert(searchCompletions.completeSearchFailure("HTTP 401 unauthorized", true));
    assert(searchCompletions.takeDeferredConnectionWork().retrySearch);

    auto requestedSearchItem = media("seerr:movie:19", true);
    assert(searchCompletions.completeSearchSuccess({requestedSearchItem}, start));
    assert(searchCompletions.requests().findPending("seerr:movie:19"));
    assert(!searchCompletions.completeSearchSuccess({media("seerr:movie:20")}, start));

    SeerrDomainState refreshStarts;
    refreshStarts.storage().finishRefresh({target(4)}, start);
    assert(refreshStarts.prepareStorageRefresh(disconnected, false, start) ==
           SeerrDomainState::RefreshStartAction::Reset);
    assert(refreshStarts.storage().targets().empty());
    assert(!refreshStarts.storage().loading());
    assert(refreshStarts.prepareStorageRefresh(configured, false, start) ==
           SeerrDomainState::RefreshStartAction::Submit);
    assert(refreshStarts.storage().loading());
    assert(refreshStarts.prepareStorageRefresh(configured, true, start) ==
           SeerrDomainState::RefreshStartAction::None);

    assert(refreshStarts.preparePendingRefresh(configured) == SeerrDomainState::RefreshStartAction::Submit);
    assert(refreshStarts.requests().pendingLoading());
    assert(refreshStarts.preparePendingRefresh(configured) == SeerrDomainState::RefreshStartAction::None);
    assert(refreshStarts.preparePendingRefresh(disconnected) == SeerrDomainState::RefreshStartAction::Reset);
    assert(!refreshStarts.requests().pendingLoading());

    refreshStarts.requests().finishPendingRefresh({}, start);
    const auto pendingDue = start + SeerrRequestState::kPendingRefreshInterval;
    assert(!refreshStarts.consumePendingRefreshDue(pendingDue, false));
    assert(refreshStarts.requests().pendingRefreshDue(pendingDue));
    assert(refreshStarts.consumePendingRefreshDue(pendingDue, true));
    assert(!refreshStarts.consumePendingRefreshDue(pendingDue, true));

    SeerrDomainState deferredStorage;
    deferredStorage.storage().finishRefresh({target(9)}, start);
    assert(deferredStorage.prepareStoragePicker(media("seerr:movie:21")) ==
           SeerrStorageState::PickerStatus::Ready);
    assert(!deferredStorage.takePendingStorageRequest(false));
    assert(deferredStorage.storage().pendingRequest());
    const auto pendingStorage = deferredStorage.takePendingStorageRequest(true);
    assert(pendingStorage && pendingStorage->id == "seerr:movie:21");
    assert(!deferredStorage.storage().pendingRequest());

    assert(deferredStorage.prepareStoragePicker(media("seerr:movie:22")) ==
           SeerrStorageState::PickerStatus::Ready);
    const auto pickerCommand =
        deferredStorage.handleStoragePickerInput(SeerrStorageState::PickerInput::Activate);
    assert(pickerCommand.type == SeerrStorageState::PickerCommandType::Selected);
    assert(pickerCommand.selection && pickerCommand.selection->item.id == "seerr:movie:22");

    const auto refreshDeadline = deferredStorage.storage().refreshDeadline();
    deferredStorage.invalidateStorageTargets();
    assert(deferredStorage.storage().targets().empty());
    assert(deferredStorage.storage().refreshDeadline() == refreshDeadline);
    deferredStorage.resetStorageForSessionClear();
    assert(deferredStorage.storage().refreshDeadline() == SeerrStorageState::TimePoint{});

    SeerrEndpoint rotated = configured;
    rotated.auth.sessionCookie = "rotated-session";

    SeerrDomainState mutations;
    const auto movieRequest =
        mutations.completeRequest(configured, configured, media("seerr:movie:15"), 51, true, start);
    assert(movieRequest.outcome == SeerrDomainState::MutationOutcome::Applied);
    assert(movieRequest.status == "Queued for download");
    const auto* requestedMovie = mutations.requests().findPending("seerr:movie:15");
    assert(requestedMovie && requestedMovie->requestId == 51 && requestedMovie->requested);

    auto television = media("seerr:tv:16");
    television.mediaType = "tv";
    const auto televisionRequest = mutations.completeRequest(configured, configured, television, 52, true, start);
    assert(televisionRequest.outcome == SeerrDomainState::MutationOutcome::Applied);
    assert(televisionRequest.status == "Queued");

    const auto failedRequest =
        mutations.completeRequest(configured, configured, media("seerr:movie:17"), 53, false, start);
    assert(failedRequest.outcome == SeerrDomainState::MutationOutcome::Failed);
    assert(!mutations.requests().findPending("seerr:movie:17"));

    const auto staleRequest = mutations.completeRequest(configured, rotated, media("seerr:movie:18"), 54, true, start);
    assert(staleRequest.outcome == SeerrDomainState::MutationOutcome::StaleEndpoint);
    assert(!mutations.requests().findPending("seerr:movie:18"));

    assert(mutations.completeDeleteRequest(configured, configured, "seerr:movie:15", 51, false) ==
           SeerrDomainState::MutationOutcome::Failed);
    assert(mutations.requests().findPending("seerr:movie:15"));
    assert(mutations.completeDeleteRequest(configured, rotated, "seerr:movie:15", 51, true) ==
           SeerrDomainState::MutationOutcome::StaleEndpoint);
    assert(mutations.requests().findPending("seerr:movie:15"));
    assert(mutations.completeDeleteRequest(configured, configured, "seerr:movie:15", 51, true) ==
           SeerrDomainState::MutationOutcome::Applied);
    assert(!mutations.requests().findPending("seerr:movie:15"));

    assert(state.requests().beginPendingRefresh());
    assert(state.completePendingRefresh(configured, configured, true, {media("seerr:movie:20", true)}, start) ==
           SeerrDomainState::RefreshOutcome::Applied);
    assert(state.requests().findPending("seerr:movie:20"));

    assert(state.requests().beginPendingRefresh());
    assert(state.completePendingRefresh(configured, rotated, true, {media("seerr:movie:21", true)}, start + 1s) ==
           SeerrDomainState::RefreshOutcome::StaleEndpoint);
    assert(state.requests().pendingRefreshDue(start + 1s));
    assert(!state.requests().findPending("seerr:movie:21"));

    state.requests().clearPendingRefreshDeadline();
    assert(state.requests().beginPendingRefresh());
    assert(state.completePendingRefresh(configured, configured, false, {}, start + 2s) ==
           SeerrDomainState::RefreshOutcome::Failed);
    assert(!state.requests().pendingRefreshDue(start + 61s));
    assert(state.requests().pendingRefreshDue(start + 62s));

    assert(state.storage().beginRefresh(true, start));
    const auto storageApplied = state.completeStorageRefresh(configured, configured, true, {target(1)}, {}, start);
    assert(storageApplied.outcome == SeerrDomainState::RefreshOutcome::Applied);
    assert(state.storage().targets().size() == 1);

    assert(state.storage().preparePicker(media("seerr:movie:30")) == SeerrStorageState::PickerStatus::Ready);
    assert(state.storage().beginRefresh(true, start + 1s));
    const auto authFailure =
        state.completeStorageRefresh(configured, configured, false, {}, "HTTP 401 unauthorized", start + 1s);
    assert(authFailure.outcome == SeerrDomainState::RefreshOutcome::Failed);
    assert(authFailure.reconnect);
    assert(!authFailure.clearedPendingRequest);
    assert(state.storage().pendingRequest());

    assert(state.storage().beginRefresh(true, start + 2s));
    const auto ordinaryFailure =
        state.completeStorageRefresh(configured, configured, false, {}, "HTTP 500", start + 2s);
    assert(ordinaryFailure.outcome == SeerrDomainState::RefreshOutcome::Failed);
    assert(!ordinaryFailure.reconnect);
    assert(ordinaryFailure.clearedPendingRequest);
    assert(!state.storage().pendingRequest());

    assert(state.storage().beginRefresh(true, start + 3s));
    const auto staleStorage = state.completeStorageRefresh(configured, rotated, true, {target(2)}, {}, start + 3s);
    assert(staleStorage.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint);
    assert(!state.storage().loading());
    assert(state.storage().targets().size() == 1);
    return 0;
}
