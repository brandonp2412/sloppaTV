#include "session_registry.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
JellyfinSession session(std::string server, std::string userId, std::string token, std::string username = "User") {
    return {
        .server = std::move(server),
        .username = std::move(username),
        .userId = std::move(userId),
        .token = std::move(token),
        .deviceId = "old-device",
    };
}
}

int main() {
    SessionRegistry registry;
    assert(registry.empty());

    auto first = session("https://one.test", "user-1", "token-a", "Alpha");
    registry.remember(first, "device-1");
    assert(registry.size() == 1);
    assert(registry.at(0));
    assert(registry.at(0)->deviceId == "device-1");

    auto updated = first;
    updated.token = "token-b";
    updated.username = "Alpha Updated";
    registry.remember(updated, "device-2");
    assert(registry.size() == 1);
    assert(registry.at(0)->token == "token-b");
    assert(registry.at(0)->username == "Alpha Updated");
    assert(registry.at(0)->deviceId == "device-2");

    auto second = session("https://two.test", "user-2", "token-c", "Beta");
    registry.remember(second, "device-2");
    assert(registry.size() == 2);
    assert(registry.at(0)->userId == "user-2");
    assert(registry.at(1)->userId == "user-1");

    auto stored = registry.exportStored();
    assert(stored.size() == 2);
    assert(stored[0].username == "Beta");
    assert(stored[1].token == "token-b");

    SessionRegistry imported;
    imported.importStored(stored, "device-3");
    assert(imported.size() == 2);
    assert(imported.at(0)->deviceId == "device-3");
    assert(imported.at(1)->deviceId == "device-3");

    std::vector<StoredSession> oversized;
    oversized.push_back(SessionRegistry::toStored(second));
    auto duplicateSecond = SessionRegistry::toStored(second);
    duplicateSecond.token = "older-token";
    oversized.push_back(duplicateSecond);
    oversized.push_back({
        .server = "https://invalid.test",
        .username = "",
        .userId = "",
        .token = "",
    });
    for (int i = 0; i < 20; ++i) {
        oversized.push_back(SessionRegistry::toStored(session(
            "https://server-" + std::to_string(i) + ".test",
            "user-" + std::to_string(i),
            "token-" + std::to_string(i)
        )));
    }
    SessionRegistry bounded;
    bounded.importStored(oversized, "device-bounded");
    assert(bounded.size() == 16);
    assert(bounded.at(0)->userId == "user-2");
    assert(bounded.at(0)->token == "token-c");
    assert(bounded.at(1)->userId == "user-0");
    assert(bounded.at(15)->userId == "user-14");

    assert(SessionRegistry::sameIdentity(*imported.at(0), second));
    assert(imported.removeIdentity(second));
    assert(imported.size() == 1);
    assert(!imported.removeIdentity(second));
    assert(imported.eraseAt(0));
    assert(imported.empty());
    assert(!imported.eraseAt(0));

    const auto roundTrip = SessionRegistry::fromStored(SessionRegistry::toStored(first), "device-4");
    assert(roundTrip.server == first.server);
    assert(roundTrip.userId == first.userId);
    assert(roundTrip.token == first.token);
    assert(roundTrip.deviceId == "device-4");
    return 0;
}
