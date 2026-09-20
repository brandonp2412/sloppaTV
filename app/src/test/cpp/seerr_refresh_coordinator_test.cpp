#include "seerr_refresh_coordinator.hpp"

#include <cassert>
#include <chrono>
#include <utility>
#include <vector>

namespace {
struct FakeAsyncExecutor {
    void refreshStorage(SeerrEndpoint endpoint) { storage.push_back(std::move(endpoint)); }

    void refreshPending(SeerrEndpoint endpoint) { pending.push_back(std::move(endpoint)); }

    std::vector<SeerrEndpoint> storage;
    std::vector<SeerrEndpoint> pending;
};

SeerrEndpoint configuredEndpoint() {
    return {
        .server = "https://seerr.example.nz",
        .auth =
            {
                .sessionCookie = "session",
                .apiKey = "",
            },
    };
}

SeerrStorageTarget target(int serverId = 4) {
    SeerrStorageTarget value;
    value.mediaType = "movie";
    value.serverId = serverId;
    value.path = "/movies";
    return value;
}

SeerrMediaItem media(std::string id = "seerr:movie:10") {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = "Requested";
    item.mediaType = "movie";
    item.tmdbId = 10;
    return item;
}
} // namespace

int main() {
    using namespace std::chrono_literals;

    SeerrDomainState domain;
    FakeAsyncExecutor async;
    SeerrRefreshCoordinator coordinator(domain, async);
    const auto start = SeerrStorageState::Clock::now();

    const SeerrEndpoint disconnected;
    assert(coordinator.refreshStorage(disconnected, false, start) == SeerrDomainState::RefreshStartAction::Reset);
    assert(async.storage.empty());
    assert(domain.storageTargets().empty());

    assert(coordinator.refreshStorage(configuredEndpoint(), false, start) ==
           SeerrDomainState::RefreshStartAction::Submit);
    assert(async.storage.size() == 1);
    assert(async.storage.back().server == "https://seerr.example.nz");
    assert(async.storage.back().auth.sessionCookie == "session");
    assert(domain.storageLoading());

    assert(coordinator.refreshStorage(configuredEndpoint(), true, start + 1s) ==
           SeerrDomainState::RefreshStartAction::None);
    assert(async.storage.size() == 1);

    domain.storage().finishRefresh({}, start);
    assert(coordinator.refreshStorage(configuredEndpoint(), false, start + 1s) ==
           SeerrDomainState::RefreshStartAction::None);
    assert(async.storage.size() == 1);

    assert(coordinator.refreshStorage(configuredEndpoint(), true, start + 1s) ==
           SeerrDomainState::RefreshStartAction::Submit);
    assert(async.storage.size() == 2);

    assert(coordinator.refreshPending(configuredEndpoint()) == SeerrDomainState::RefreshStartAction::Submit);
    assert(async.pending.size() == 1);
    assert(async.pending.back().server == "https://seerr.example.nz");
    assert(domain.pendingRequestsLoading());

    assert(coordinator.refreshPending(configuredEndpoint()) == SeerrDomainState::RefreshStartAction::None);
    assert(async.pending.size() == 1);

    assert(coordinator.refreshPending(disconnected) == SeerrDomainState::RefreshStartAction::Reset);
    assert(async.pending.size() == 1);
    assert(!domain.pendingRequestsLoading());

    const auto requested = media();
    assert(domain.storage().preparePicker(requested) == SeerrStorageState::PickerStatus::Loading);
    auto storageCompletion = coordinator.completeStorage(configuredEndpoint(), configuredEndpoint(), true, {target(9)},
                                                         "", true, start + 2s);
    assert(storageCompletion.domain.outcome == SeerrDomainState::RefreshOutcome::Applied);
    assert(storageCompletion.targetCount == 1);
    assert(storageCompletion.pendingRequest);
    assert(storageCompletion.pendingRequest->id == requested.id);
    assert(!domain.storage().pendingRequest());

    auto staleEndpoint = configuredEndpoint();
    staleEndpoint.server = "https://other.example.nz";
    storageCompletion =
        coordinator.completeStorage(configuredEndpoint(), staleEndpoint, true, {target(10)}, "", true, start + 3s);
    assert(storageCompletion.domain.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint);
    assert(storageCompletion.targetCount == 0);
    assert(!storageCompletion.pendingRequest);
    assert(domain.storageTargets().size() == 1);
    assert(domain.storageTargets().front().serverId == 9);

    auto pendingCompletion =
        coordinator.completePending(configuredEndpoint(), configuredEndpoint(), true, {media()}, start + 4s);
    assert(pendingCompletion.outcome == SeerrDomainState::RefreshOutcome::Applied);
    assert(pendingCompletion.pendingCount == 1);
    assert(domain.pendingRequests().size() == 1);
    assert(domain.pendingRequests().front().id == "seerr:movie:10");

    pendingCompletion =
        coordinator.completePending(configuredEndpoint(), staleEndpoint, true, {media("seerr:movie:11")}, start + 5s);
    assert(pendingCompletion.outcome == SeerrDomainState::RefreshOutcome::StaleEndpoint);
    assert(pendingCompletion.pendingCount == 0);
    assert(domain.pendingRequests().size() == 1);

    return 0;
}
