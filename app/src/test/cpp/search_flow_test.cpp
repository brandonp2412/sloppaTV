#include "search_flow.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace {
struct FakeSeerrAsync {
    int cancelCount = 0;
    int searchCount = 0;
    uint64_t lastGeneration = 0;
    std::string lastQuery;

    void cancelSearch() { ++cancelCount; }

    bool search(SeerrEndpoint, std::string query, uint64_t generation) {
        ++searchCount;
        lastGeneration = generation;
        lastQuery = std::move(query);
        return accept;
    }

    bool accept = true;
};

struct FakeJellyfinSearch {
    int searchCount = 0;
    uint64_t lastGeneration = 0;
    std::string lastQuery;

    bool search(JellyfinSession, std::string query, uint64_t generation) {
        ++searchCount;
        lastGeneration = generation;
        lastQuery = std::move(query);
        return accept;
    }

    bool accept = true;
};

JellyfinSession session() {
    JellyfinSession value;
    value.server = "https://jellyfin.example";
    value.userId = "user";
    value.token = "token";
    value.deviceId = "device";
    return value;
}

SeerrEndpoint endpoint() {
    SeerrEndpoint value;
    value.server = "https://seerr.example";
    value.auth.apiKey = "key";
    return value;
}
} // namespace

int main() {
    SeerrDomainState domain;
    FakeSeerrAsync seerr;
    FakeJellyfinSearch jellyfin;
    RequestEpoch jellyfinEpoch;
    RequestEpoch seerrEpoch;
    SearchFlow<FakeSeerrAsync, FakeJellyfinSearch> flow(domain, seerr, jellyfin, jellyfinEpoch, seerrEpoch);

    const auto now = std::chrono::steady_clock::now();
    flow.state().setQuery("caminandes");
    const SearchScheduleEffects schedule = flow.scheduleLive(now, true);
    assert(!schedule.clearError);
    assert(flow.state().debouncePending());
    assert(domain.searchDebouncePending());
    assert(seerr.cancelCount == 1);
    assert(jellyfinEpoch.snapshot() == 1);
    assert(seerrEpoch.snapshot() == 1);

    const SearchDispatchEffects dispatched = flow.search(session(), endpoint(), true, now);
    assert(dispatched.clearError);
    assert(dispatched.refreshSeerrStorage);
    assert(jellyfin.searchCount == 1);
    assert(jellyfin.lastQuery == "caminandes");
    assert(jellyfin.lastGeneration == jellyfinEpoch.snapshot());
    assert(seerr.searchCount == 1);
    assert(seerr.lastQuery == "caminandes");
    assert(seerr.lastGeneration == seerrEpoch.snapshot());
    assert(flow.state().loading());
    assert(domain.searchLoading());

    flow.cancel();
    assert(!flow.state().loading());
    assert(!domain.searchLoading());
    assert(seerr.cancelCount == 2);
    assert(jellyfinEpoch.snapshot() == 3);
    assert(seerrEpoch.snapshot() == 3);

    jellyfin.accept = false;
    seerr.accept = false;
    flow.state().setQuery("matrix");
    const SearchDispatchEffects rejected = flow.search(session(), endpoint(), true, now);
    assert(rejected.clearError);
    assert(!rejected.refreshSeerrStorage);
    assert(!flow.state().loading());
    assert(!domain.searchLoading());
    assert(jellyfin.searchCount == 2);
    assert(seerr.searchCount == 2);
    assert(jellyfinEpoch.snapshot() == 5);
    assert(seerrEpoch.snapshot() == 5);

    // Remux search only uses the Jellyfin-compatible endpoint. Even when
    // Seerr was previously populated/configured, its requests stay disabled.
    SeerrDomainState remuxDomain;
    FakeSeerrAsync remuxSeerr;
    FakeJellyfinSearch remuxSearch;
    RequestEpoch remuxEpoch;
    RequestEpoch remuxSeerrEpoch;
    SearchFlow<FakeSeerrAsync, FakeJellyfinSearch> remuxFlow(remuxDomain, remuxSeerr, remuxSearch, remuxEpoch,
                                                             remuxSeerrEpoch);
    remuxFlow.state().setQuery("mob");
    assert(!remuxFlow.scheduleLive(now, false).clearError);
    assert(!remuxFlow.runDue(true, session(), SeerrEndpoint{}, now + std::chrono::milliseconds(499)).clearError);
    assert(remuxSearch.searchCount == 0);
    remuxFlow.state().setQuery("mobland");
    assert(!remuxFlow.scheduleLive(now + std::chrono::milliseconds(450), false).clearError);
    assert(!remuxFlow.runDue(true, session(), SeerrEndpoint{}, now + std::chrono::milliseconds(500)).clearError);
    assert(remuxSearch.searchCount == 0);
    const auto due = remuxFlow.runDue(true, session(), SeerrEndpoint{}, now + std::chrono::milliseconds(950));
    assert(due.clearError && !due.refreshSeerrStorage);
    assert(remuxSearch.searchCount == 1);
    assert(remuxSearch.lastQuery == "mobland");
    assert(remuxSeerr.searchCount == 0);
    assert(!remuxDomain.searchLoading());
    remuxFlow.cancel();

    return 0;
}
