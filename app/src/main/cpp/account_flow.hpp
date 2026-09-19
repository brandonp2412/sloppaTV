#pragma once

#include "account_completion_controller.hpp"
#include "account_navigation_controller.hpp"
#include "session_registry.hpp"
#include "session_store.hpp"

#include <array>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

enum class AccountProfileAction {
    None,
    Back,
    AddAccount,
    SwitchSession,
    ForgetSession,
};

struct AccountProfileCommand {
    AccountProfileAction action = AccountProfileAction::None;
    std::optional<JellyfinSession> session;
    std::optional<JellyfinSession> removedSession;
    bool removedCurrent = false;
    bool sessionsEmpty = false;
};

struct AccountQuickConnectPlan {
    std::string server;
    std::optional<std::string> error;

    [[nodiscard]] bool ready() const { return !server.empty() && !error; }
};

struct AccountPersistedState {
    JellyfinSession session;
    std::unordered_set<std::string> hiddenHomeItems;
    AppSettings settings;
    std::string warning;
};

class AccountFlow {
public:
    [[nodiscard]] AccountScreenState& state() { return state_; }

    [[nodiscard]] const AccountScreenState& state() const { return state_; }

    [[nodiscard]] bool hasSavedSessions() const { return !sessions_.empty(); }

    [[nodiscard]] std::size_t savedSessionCount() const { return sessions_.size(); }

    [[nodiscard]] const JellyfinSession* savedSession(std::size_t index) const { return sessions_.at(index); }

    [[nodiscard]] AccountNavigationAction handleLogin(ScreenNavigationKey key);
    [[nodiscard]] AccountProfileCommand handleProfiles(ScreenNavigationKey key, const JellyfinSession& currentSession);

    [[nodiscard]] bool beginProfiles();
    void beginAddAccount(const std::string& existingServer);
    void activateSession(const JellyfinSession& session);

    [[nodiscard]] bool beginDiscovery(bool busy);
    [[nodiscard]] std::optional<std::array<std::string, 3>> beginLogin(bool busy) const;
    [[nodiscard]] AccountQuickConnectPlan beginQuickConnect(bool busy);
    void cancelQuickConnect();

    [[nodiscard]] AccountCompletionEffects complete(DiscoveryCompletion& completion, bool active);
    [[nodiscard]] AccountCompletionEffects complete(LoginCompletion& completion, bool active);
    [[nodiscard]] AccountCompletionEffects complete(QuickConnectStartedCompletion& completion, bool active);
    [[nodiscard]] AccountCompletionEffects complete(QuickConnectFailedCompletion& completion, bool active);
    [[nodiscard]] AccountCompletionEffects complete(QuickConnectAuthenticatedCompletion& completion, bool active);
    [[nodiscard]] AccountCompletionEffects complete(QuickConnectTimedOutCompletion& completion, bool active);

    [[nodiscard]] AccountPersistedState restore(const std::string& dataPath);
    [[nodiscard]] bool persist(const std::string& dataPath, const JellyfinSession& session,
                               const std::unordered_set<std::string>& hiddenHomeItems, const AppSettings& settings,
                               std::string& warning);

    [[nodiscard]] const std::string& deviceId() const { return deviceId_; }

    void importStored(const std::vector<StoredSession>& stored, const std::string& deviceId);
    void remember(const JellyfinSession& session);
    [[nodiscard]] bool removeIdentity(const JellyfinSession& session);
    [[nodiscard]] std::vector<StoredSession> exportStored() const;

private:
    AccountScreenState state_;
    SessionRegistry sessions_;
    std::string deviceId_;
};
