#include "account_flow.hpp"

#include <utility>

AccountNavigationAction AccountFlow::handleLogin(ScreenNavigationKey key) {
    return AccountNavigationController::handleLogin(state_, key, !sessions_.empty());
}

AccountProfileCommand AccountFlow::handleProfiles(ScreenNavigationKey key, const JellyfinSession& currentSession) {
    const AccountNavigationAction navigation =
        AccountNavigationController::handleProfiles(state_, key, static_cast<int>(sessions_.size()));

    AccountProfileCommand command;
    switch (navigation.type) {
    case AccountNavigationActionType::ProfilesBack:
        command.action = AccountProfileAction::Back;
        return command;
    case AccountNavigationActionType::AddAccount:
        command.action = AccountProfileAction::AddAccount;
        return command;
    case AccountNavigationActionType::SwitchSession: {
        const JellyfinSession* saved = sessions_.at(static_cast<std::size_t>(navigation.index));
        if (!saved) return command;
        command.action = AccountProfileAction::SwitchSession;
        command.session = *saved;
        command.session->deviceId = deviceId_;
        return command;
    }
    case AccountNavigationActionType::ForgetSession: {
        const JellyfinSession* saved = sessions_.at(static_cast<std::size_t>(navigation.index));
        if (!saved) return command;

        command.action = AccountProfileAction::ForgetSession;
        command.removedSession = *saved;
        command.removedCurrent = SessionRegistry::sameIdentity(currentSession, *saved);
        sessions_.eraseAt(static_cast<std::size_t>(navigation.index));
        state_.beginProfiles(static_cast<int>(sessions_.size()));
        command.sessionsEmpty = sessions_.empty();
        return command;
    }
    default:
        return command;
    }
}

bool AccountFlow::beginProfiles() {
    if (sessions_.empty()) return false;
    state_.beginProfiles(static_cast<int>(sessions_.size()));
    return true;
}

void AccountFlow::beginAddAccount(const std::string& existingServer) {
    state_.beginAddAccount(existingServer);
}

void AccountFlow::activateSession(const JellyfinSession& session) {
    state_.setAuthenticatedAccount(session.server, session.username);
}

bool AccountFlow::beginDiscovery(bool busy) {
    if (busy) return false;
    state_.setDiscoveryStatus("SEARCHING LOCAL NETWORK...");
    return true;
}

std::optional<std::array<std::string, 3>> AccountFlow::beginLogin(bool busy) const {
    if (busy) return std::nullopt;
    return state_.fields();
}

AccountQuickConnectPlan AccountFlow::beginQuickConnect(bool busy) {
    if (busy || state_.quickConnectActive()) return {};

    if (state_.field(AccountScreenState::kServerField).empty()) {
        state_.setLoginFocus(AccountScreenState::kServerField);
        AccountQuickConnectPlan plan;
        plan.error = "ENTER THE JELLYFIN SERVER ADDRESS FIRST";
        return plan;
    }

    AccountQuickConnectPlan plan;
    plan.server = state_.field(AccountScreenState::kServerField);
    state_.beginQuickConnect();
    return plan;
}

void AccountFlow::cancelQuickConnect() {
    state_.endQuickConnect(true);
}

AccountCompletionEffects AccountFlow::complete(DiscoveryCompletion& completion, bool active) {
    return AccountCompletionController::apply(completion, active, state_);
}

AccountCompletionEffects AccountFlow::complete(LoginCompletion& completion, bool active) {
    return AccountCompletionController::apply(completion, active, state_);
}

AccountCompletionEffects AccountFlow::complete(QuickConnectStartedCompletion& completion, bool active) {
    return AccountCompletionController::apply(completion, active, state_);
}

AccountCompletionEffects AccountFlow::complete(QuickConnectFailedCompletion& completion, bool active) {
    return AccountCompletionController::apply(completion, active, state_);
}

AccountCompletionEffects AccountFlow::complete(QuickConnectAuthenticatedCompletion& completion, bool active) {
    return AccountCompletionController::apply(completion, active, state_);
}

AccountCompletionEffects AccountFlow::complete(QuickConnectTimedOutCompletion& completion, bool active) {
    return AccountCompletionController::apply(completion, active, state_);
}

AccountPersistedState AccountFlow::restore(const std::string& dataPath) {
    AccountPersistedState result;
    StoredSessionState stored = loadSessionState(dataPath, generateDeviceId(), result.warning);
    deviceId_ = std::move(stored.deviceId);
    sessions_.importStored(stored.savedSessions, deviceId_);
    result.session = SessionRegistry::fromStored(stored.currentSession, deviceId_);
    result.hiddenHomeItems = std::move(stored.hiddenHomeItems);
    result.settings = std::move(stored.settings);
    if (result.session.valid()) {
        sessions_.remember(result.session, deviceId_);
        activateSession(result.session);
    }
    return result;
}

bool AccountFlow::persist(const std::string& dataPath, const JellyfinSession& session,
                          const std::unordered_set<std::string>& hiddenHomeItems, const AppSettings& settings,
                          std::string& warning) {
    if (session.valid()) sessions_.remember(session, deviceId_);
    StoredSessionState stored;
    stored.deviceId = deviceId_;
    stored.currentSession = SessionRegistry::toStored(session);
    stored.savedSessions = sessions_.exportStored();
    stored.hiddenHomeItems = hiddenHomeItems;
    stored.settings = settings;
    return saveSessionState(dataPath, stored, warning);
}

void AccountFlow::importStored(const std::vector<StoredSession>& stored, const std::string& deviceId) {
    deviceId_ = deviceId;
    sessions_.importStored(stored, deviceId_);
}

void AccountFlow::remember(const JellyfinSession& session) {
    sessions_.remember(session, deviceId_);
}

bool AccountFlow::removeIdentity(const JellyfinSession& session) {
    return sessions_.removeIdentity(session);
}

std::vector<StoredSession> AccountFlow::exportStored() const {
    return sessions_.exportStored();
}
