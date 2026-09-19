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
        return true;
    }
};

struct FakeJellyfinSearch {
    int searchCount = 0;
    uint64_t lastGeneration = 0;
    std::string lastQuery;

    bool search(JellyfinSession, std::string query, uint64_t generation) {
        ++searchCount;
        lastGeneration = generation;
        lastQuery = std::move(query);
        return true;
    }
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

    return 0;
}
