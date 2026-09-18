#include "seerr_request_coordinator.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Submission {
    SeerrEndpoint endpoint;
    SeerrMediaItem item;
    std::optional<SeerrStorageTarget> target;
};

struct FakeAsyncExecutor {
    void requestMedia(SeerrEndpoint endpoint, SeerrMediaItem item, std::optional<SeerrStorageTarget> target) {
        submissions.push_back({
            .endpoint = std::move(endpoint),
            .item = std::move(item),
            .target = std::move(target),
        });
    }

    std::vector<Submission> submissions;
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

SeerrMediaItem media(std::string id = "seerr:movie:10") {
    SeerrMediaItem item;
    item.id = std::move(id);
    item.name = "Requested";
    item.mediaType = "movie";
    item.tmdbId = 10;
    return item;
}

SeerrStorageTarget target(int serverId = 4) {
    SeerrStorageTarget value;
    value.mediaType = "movie";
    value.serverId = serverId;
    value.path = "/movies";
    return value;
}
} // namespace

int main() {
    SeerrDomainState domain;
    FakeAsyncExecutor async;
    SeerrRequestCoordinator coordinator(domain, async);

    auto plan = coordinator.prepare({}, configuredEndpoint(), false, false);
    assert(plan.action == SeerrDomainState::RequestAction::Invalid);
    assert(!plan.ready());
    coordinator.submit(std::move(plan));
    assert(async.submissions.empty());

    auto requested = media();
    requested.requested = true;
    requested.status = "Queued";
    plan = coordinator.prepare(requested, configuredEndpoint(), false, false);
    assert(plan.action == SeerrDomainState::RequestAction::AlreadyRequested);
    assert(!plan.ready());

    plan = coordinator.prepare(media(), {}, false, false);
    assert(plan.action == SeerrDomainState::RequestAction::NotConfigured);
    assert(!plan.ready());

    assert(domain.prepareConnect("https://seerr.example.nz", true) == SeerrDomainState::ConnectAction::Submit);
    const auto deferredItem = media("seerr:movie:11");
    plan = coordinator.prepare(deferredItem, configuredEndpoint(), false, false);
    assert(plan.action == SeerrDomainState::RequestAction::DeferredForConnection);
    const auto deferred = domain.takeDeferredConnectionWork();
    assert(deferred.request && deferred.request->id == "seerr:movie:11");
    domain.connection().endConnect();

    plan = coordinator.prepare(media(), configuredEndpoint(), true, false);
    assert(plan.action == SeerrDomainState::RequestAction::ChooseStorage);
    assert(plan.refreshStorage);
    assert(!plan.ready());

    domain.storage().finishRefresh({target(7)}, SeerrStorageState::Clock::now());
    plan = coordinator.prepare(media(), configuredEndpoint(), true, false);
    assert(plan.action == SeerrDomainState::RequestAction::ChooseStorage);
    assert(!plan.refreshStorage);

    plan = coordinator.prepare(media(), configuredEndpoint(), false, false);
    assert(plan.action == SeerrDomainState::RequestAction::Submit);
    assert(plan.ready());
    assert(plan.endpoint.server == "https://seerr.example.nz");
    assert(plan.item.id == "seerr:movie:10");
    assert(!plan.target);
    coordinator.submit(std::move(plan));
    assert(async.submissions.size() == 1);
    assert(async.submissions.back().endpoint.auth.sessionCookie == "session");
    assert(async.submissions.back().item.id == "seerr:movie:10");
    assert(!async.submissions.back().target);

    const auto selected = target(9);
    plan = coordinator.prepare(media(), configuredEndpoint(), true, false, &selected);
    assert(plan.action == SeerrDomainState::RequestAction::Submit);
    assert(plan.target && plan.target->serverId == 9);
    coordinator.submit(std::move(plan));
    assert(async.submissions.size() == 2);
    assert(async.submissions.back().target && async.submissions.back().target->serverId == 9);

    plan = coordinator.prepare(media(), configuredEndpoint(), true, true);
    assert(plan.action == SeerrDomainState::RequestAction::Submit);
    assert(!plan.target);

    return 0;
}
