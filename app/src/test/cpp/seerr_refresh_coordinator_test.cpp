#include "seerr_refresh_coordinator.hpp"

#include <cassert>
#include <chrono>
#include <utility>
#include <vector>

namespace {
struct FakeAsyncExecutor {
    void refreshStorage(SeerrEndpoint endpoint) {
        storage.push_back(std::move(endpoint));
    }

    void refreshPending(SeerrEndpoint endpoint) {
        pending.push_back(std::move(endpoint));
    }

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
} // namespace

int main() {
    using namespace std::chrono_literals;

    SeerrDomainState domain;
    FakeAsyncExecutor async;
    SeerrRefreshCoordinator coordinator(domain, async);
    const auto start = SeerrStorageState::Clock::now();

    const SeerrEndpoint disconnected;
    assert(coordinator.refreshStorage(disconnected, false, start) ==
           SeerrDomainState::RefreshStartAction::Reset);
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

    assert(coordinator.refreshPending(configuredEndpoint()) ==
           SeerrDomainState::RefreshStartAction::Submit);
    assert(async.pending.size() == 1);
    assert(async.pending.back().server == "https://seerr.example.nz");
    assert(domain.pendingRequestsLoading());

    assert(coordinator.refreshPending(configuredEndpoint()) ==
           SeerrDomainState::RefreshStartAction::None);
    assert(async.pending.size() == 1);

    assert(coordinator.refreshPending(disconnected) ==
           SeerrDomainState::RefreshStartAction::Reset);
    assert(async.pending.size() == 1);
    assert(!domain.pendingRequestsLoading());

    return 0;
}
