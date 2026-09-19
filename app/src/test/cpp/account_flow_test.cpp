#include "account_flow.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
StoredSession storedSession(std::string server, std::string username, std::string userId) {
    StoredSession stored;
    stored.server = std::move(server);
    stored.username = std::move(username);
    stored.userId = std::move(userId);
    stored.token = "token";
    return stored;
}

JellyfinSession activeSession(std::string server, std::string userId) {
    JellyfinSession session;
    session.server = std::move(server);
    session.userId = std::move(userId);
    session.token = "token";
    return session;
}
} // namespace

int main() {
    AccountFlow flow;

    auto quick = flow.beginQuickConnect(false);
    assert(!quick.ready());
    assert(quick.error == "ENTER THE JELLYFIN SERVER ADDRESS FIRST");
    assert(flow.state().loginFocus() == AccountScreenState::kServerField);

    flow.state().setField(AccountScreenState::kServerField, "https://jellyfin.example");
    quick = flow.beginQuickConnect(false);
    assert(quick.ready());
    assert(quick.server == "https://jellyfin.example");
    assert(flow.state().quickConnectActive());
    flow.cancelQuickConnect();
    assert(!flow.state().quickConnectActive());

    std::vector<StoredSession> stored{
        storedSession("https://one.example", "One", "user-1"),
        storedSession("https://two.example", "Two", "user-2"),
    };
    flow.importStored(stored, "device");
    assert(flow.savedSessionCount() == 2);
    assert(flow.beginProfiles());

    JellyfinSession current = activeSession("https://one.example", "user-1");
    auto profile = flow.handleProfiles(ScreenNavigationKey::Activate, current);
    assert(profile.action == AccountProfileAction::SwitchSession);
    assert(profile.session);
    assert(profile.session->userId == "user-1");
    assert(profile.session->deviceId == "device");

    profile = flow.handleProfiles(ScreenNavigationKey::Right, current);
    assert(profile.action == AccountProfileAction::None);
    profile = flow.handleProfiles(ScreenNavigationKey::Activate, current);
    assert(profile.action == AccountProfileAction::ForgetSession);
    assert(profile.removedSession);
    assert(profile.removedSession->userId == "user-1");
    assert(profile.removedCurrent);
    assert(!profile.sessionsEmpty);
    assert(flow.savedSessionCount() == 1);

    flow.beginAddAccount("https://existing.example");
    assert(flow.state().field(AccountScreenState::kServerField) == "https://existing.example");
    assert(flow.state().loginFocus() == AccountScreenState::kServerField);

    return 0;
}
