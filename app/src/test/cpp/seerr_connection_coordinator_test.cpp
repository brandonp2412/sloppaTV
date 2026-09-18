#include "seerr_connection_coordinator.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Submission {
    std::string server;
    JellyfinSession jellyfin;
    bool announce = false;
};

struct FakeAsyncExecutor {
    void connect(std::string server, JellyfinSession jellyfin, bool announce) {
        submissions.push_back({
            .server = std::move(server),
            .jellyfin = std::move(jellyfin),
            .announce = announce,
        });
    }

    std::vector<Submission> submissions;
};

JellyfinSession session() {
    JellyfinSession value;
    value.server = "https://jellyfin.example.nz";
    value.userId = "user-1";
    value.token = "token";
    return value;
}
} // namespace

int main() {
    SeerrDomainState domain;
    FakeAsyncExecutor async;
    SeerrConnectionCoordinator coordinator(domain, async);

    auto plan = coordinator.prepare("", session());
    assert(plan.action == SeerrDomainState::ConnectAction::MissingServer);
    assert(!plan.ready());
    coordinator.submit(std::move(plan), true);
    assert(async.submissions.empty());
    assert(!domain.connection().connecting());

    JellyfinSession invalid;
    plan = coordinator.prepare("https://seerr.example.nz", invalid);
    assert(plan.action == SeerrDomainState::ConnectAction::MissingJellyfin);
    assert(!plan.ready());
    assert(async.submissions.empty());
    assert(!domain.connection().connecting());

    plan = coordinator.prepare("https://seerr.example.nz", session());
    assert(plan.action == SeerrDomainState::ConnectAction::Submit);
    assert(plan.ready());
    assert(plan.server == "https://seerr.example.nz");
    assert(plan.jellyfin.userId == "user-1");
    assert(domain.connection().connecting());

    auto duplicate = coordinator.prepare("https://other.example.nz", session());
    assert(duplicate.action == SeerrDomainState::ConnectAction::AlreadyConnecting);
    assert(!duplicate.ready());

    coordinator.submit(std::move(plan), true);
    assert(async.submissions.size() == 1);
    assert(async.submissions.front().server == "https://seerr.example.nz");
    assert(async.submissions.front().jellyfin.userId == "user-1");
    assert(async.submissions.front().announce);

    coordinator.submit(std::move(duplicate), false);
    assert(async.submissions.size() == 1);

    auto completion = coordinator.complete(false, false, "https://seerr.example.nz", "user-1",
                                           "https://seerr.example.nz", "user-1");
    assert(completion.action == SeerrDomainState::ConnectCompletionAction::PreAuthenticationFailed);
    assert(!completion.deferred.request);
    assert(!completion.deferred.retrySearch);
    assert(!domain.connection().connecting());

    plan = coordinator.prepare("https://seerr.example.nz", session());
    assert(plan.ready());
    completion = coordinator.complete(true, false, "https://seerr.example.nz", "user-1",
                                      "https://other.example.nz", "user-1");
    assert(completion.action == SeerrDomainState::ConnectCompletionAction::Stale);
    assert(!domain.connection().connecting());

    plan = coordinator.prepare("https://seerr.example.nz", session());
    assert(plan.ready());
    completion = coordinator.complete(false, true, "https://seerr.example.nz", "user-1",
                                      "https://seerr.example.nz", "user-1");
    assert(completion.action == SeerrDomainState::ConnectCompletionAction::AuthenticationFailed);
    assert(!domain.connection().connecting());

    plan = coordinator.prepare("https://seerr.example.nz", session());
    assert(plan.ready());
    SeerrMediaItem deferredRequest;
    deferredRequest.id = "seerr:movie:42";
    deferredRequest.mediaType = "movie";
    deferredRequest.tmdbId = 42;
    domain.connection().deferRequest(deferredRequest);
    domain.connection().deferSearchRetry();
    completion = coordinator.complete(true, false, "https://seerr.example.nz", "user-1",
                                      "https://seerr.example.nz", "user-1");
    assert(completion.action == SeerrDomainState::ConnectCompletionAction::Connected);
    assert(completion.deferred.request && completion.deferred.request->id == "seerr:movie:42");
    assert(completion.deferred.retrySearch);
    assert(!domain.connection().connecting());

    return 0;
}
