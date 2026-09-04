#include "request_epoch.hpp"

#include <cassert>

int main() {
    RequestEpoch epoch;
    const uint64_t first = epoch.begin();
    assert(epoch.active(first));
    const uint64_t snapshot = epoch.snapshot();
    assert(snapshot == first);
    epoch.invalidate();
    assert(!epoch.active(first));

    RequestEpochs epochs;
    const uint64_t auth = epochs.auth.begin();
    const uint64_t home = epochs.home.begin();
    const uint64_t search = epochs.search.begin();
    const uint64_t content = epochs.content.begin();
    const uint64_t playback = epochs.playback.begin();
    const uint64_t session = epochs.session.begin();

    const uint64_t secondContent = epochs.content.begin();
    assert(!epochs.content.active(content));
    assert(epochs.home.active(home));
    assert(epochs.search.active(search));
    assert(epochs.playback.active(playback));
    assert(epochs.session.active(session));
    assert(epochs.content.active(secondContent));

    epochs.invalidateTransient();
    assert(!epochs.auth.active(auth));
    assert(!epochs.home.active(home));
    assert(!epochs.search.active(search));
    assert(!epochs.content.active(secondContent));
    assert(!epochs.playback.active(playback));
    assert(epochs.session.active(session));

    const uint64_t nextSession = epochs.session.begin();
    epochs.invalidateAll();
    assert(!epochs.session.active(nextSession));
    return 0;
}
