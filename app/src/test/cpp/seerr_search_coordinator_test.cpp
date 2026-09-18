#include "seerr_search_coordinator.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Submission {
    SeerrEndpoint endpoint;
    std::string query;
    uint64_t generation = 0;
};

struct FakeAsyncExecutor {
    void search(SeerrEndpoint endpoint, std::string query, uint64_t generation) {
        submissions.push_back({
            .endpoint = std::move(endpoint),
            .query = std::move(query),
            .generation = generation,
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

SeerrMediaItem media() {
    SeerrMediaItem item;
    item.id = "seerr:movie:10";
    item.name = "Existing";
    item.mediaType = "movie";
    item.tmdbId = 10;
    return item;
}
} // namespace

int main() {
    using namespace std::chrono_literals;

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);
        assert(domain.beginImmediateSearch("matrix", true).started);
        assert(domain.searchLoading());
        assert(domain.prepareConnect("https://seerr.example.nz", true) ==
               SeerrDomainState::ConnectAction::Submit);

        auto plan = coordinator.prepareImmediate(configuredEndpoint(), "matrix");
        assert(plan.action == SeerrSearchDispatchAction::DeferredForConnection);
        assert(!plan.ready());
        assert(!domain.searchLoading());
        const auto deferred = domain.takeDeferredConnectionWork();
        assert(deferred.retrySearch);
        coordinator.submit(std::move(plan), 1);
        assert(async.submissions.empty());
    }

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);
        assert(domain.beginImmediateSearch("old", true).started);
        assert(domain.finishSearch("old", {media()}));

        auto plan = coordinator.prepareImmediate({}, "new");
        assert(plan.action == SeerrSearchDispatchAction::None);
        assert(plan.resultsChanged);
        assert(!plan.ready());
        assert(domain.searchResults().empty());
        assert(!domain.searchLoading());
    }

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);

        auto plan = coordinator.prepareImmediate(configuredEndpoint(), "arrival");
        assert(plan.action == SeerrSearchDispatchAction::Submit);
        assert(plan.ready());
        assert(!plan.resultsChanged);
        assert(plan.endpoint.server == "https://seerr.example.nz");
        assert(plan.query == "arrival");
        assert(domain.searchLoading());

        coordinator.submit(std::move(plan), 14);
        assert(async.submissions.size() == 1);
        assert(async.submissions.back().endpoint.auth.sessionCookie == "session");
        assert(async.submissions.back().query == "arrival");
        assert(async.submissions.back().generation == 14);
    }

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);
        const auto start = SeerrSearchState::Clock::now();
        assert(domain.scheduleSearch("brook", start, true));

        auto plan = coordinator.prepareDue(configuredEndpoint(), "brook", start + 549ms);
        assert(plan.action == SeerrSearchDispatchAction::None);
        assert(!plan.ready());
        assert(async.submissions.empty());

        plan = coordinator.prepareDue(configuredEndpoint(), "brook", start + 550ms);
        assert(plan.action == SeerrSearchDispatchAction::Submit);
        assert(plan.ready());
        assert(domain.searchLoading());

        coordinator.submit(std::move(plan), 27);
        assert(async.submissions.size() == 1);
        assert(async.submissions.back().query == "brook");
        assert(async.submissions.back().generation == 27);
    }

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);
        const auto start = SeerrSearchState::Clock::now();
        assert(domain.beginImmediateSearch("arrival", true).started);

        auto requested = media();
        requested.requested = true;
        requested.requestId = 42;
        requested.status = "Queued";
        const auto completion = coordinator.completeSuccess("arrival", {requested}, start);
        assert(completion.resultsChanged);
        assert(completion.pendingChanged);
        assert(!completion.reconnect);
        assert(!domain.searchLoading());
        assert(domain.searchResults().size() == 1);
        assert(domain.pendingRequests().size() == 1);
        assert(domain.pendingRequests().front().requestId == 42);
    }

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);
        assert(domain.beginImmediateSearch("arrival", true).started);

        const auto completion = coordinator.completeFailure("arrival", "HTTP 401 unauthorized", true);
        assert(completion.resultsChanged);
        assert(!completion.pendingChanged);
        assert(completion.reconnect);
        assert(!domain.searchLoading());
        assert(domain.searchResults().empty());
        const auto deferred = domain.takeDeferredConnectionWork();
        assert(deferred.retrySearch);
    }

    {
        SeerrDomainState domain;
        FakeAsyncExecutor async;
        SeerrSearchCoordinator coordinator(domain, async);
        assert(domain.beginImmediateSearch("arrival", true).started);
        assert(domain.searchLoading());

        coordinator.abandonCompletion();
        assert(!domain.searchLoading());
    }

    return 0;
}
