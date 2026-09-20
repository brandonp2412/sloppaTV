#include "account_navigation_controller.hpp"
#include "queue_navigation_controller.hpp"
#include "seerr_drive_navigation_controller.hpp"
#include "settings_navigation_controller.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace {
JellyfinItem queueItem(std::string id) {
    JellyfinItem item;
    item.id = std::move(id);
    return item;
}

SeerrMediaItem media(std::string type, int tmdbId) {
    SeerrMediaItem item;
    item.id = seerrMediaId(type, tmdbId);
    item.name = "Example";
    item.mediaType = std::move(type);
    item.tmdbId = tmdbId;
    return item;
}

SeerrStorageTarget target(std::string type, int serverId, bool isDefault = false) {
    SeerrStorageTarget value;
    value.mediaType = std::move(type);
    value.serviceName = "Service";
    value.path = "/media/" + std::to_string(serverId);
    value.serverId = serverId;
    value.freeSpace = 25;
    value.totalSpace = 100;
    value.isDefault = isDefault;
    return value;
}
} // namespace

int main() {
    AccountScreenState account;
    account.setLoginFocus(AccountScreenState::kServerField);
    auto accountAction = AccountNavigationController::handleLogin(account, ScreenNavigationKey::Activate, false);
    assert(accountAction.type == AccountNavigationActionType::EditField);
    assert(accountAction.index == AccountScreenState::kServerField);

    account.setKeyboardActive(true);
    accountAction = AccountNavigationController::handleLogin(account, ScreenNavigationKey::Left, false);
    assert(accountAction.type == AccountNavigationActionType::MoveKeyboard);
    assert(accountAction.dx == -1);

    account.beginQuickConnect("ABC123");
    accountAction = AccountNavigationController::handleLogin(account, ScreenNavigationKey::Back, false);
    assert(accountAction.type == AccountNavigationActionType::CancelQuickConnect);

    account.beginProfiles(2);
    accountAction = AccountNavigationController::handleProfiles(account, ScreenNavigationKey::Down, 2);
    assert(accountAction.type == AccountNavigationActionType::None);
    accountAction = AccountNavigationController::handleProfiles(account, ScreenNavigationKey::Left, 2);
    assert(accountAction.type == AccountNavigationActionType::None);
    accountAction = AccountNavigationController::handleProfiles(account, ScreenNavigationKey::Activate, 2);
    assert(accountAction.type == AccountNavigationActionType::ForgetSession);
    assert(accountAction.index == 1);

    SettingsScreenState settingsScreen;
    settingsScreen.reset();
    AppSettings settings;
    const int originalUiTextSize = settings.uiTextSize;
    auto settingsAction = SettingsNavigationController::handle(settingsScreen, settings, ScreenNavigationKey::Right);
    assert(settingsAction.type == SettingsNavigationActionType::ApplyEffects);
    assert(settingsAction.effects != SettingChangeEffect::None);
    assert(settings.uiTextSize != originalUiTextSize);

    settingsScreen.reset();
    settingsAction = SettingsNavigationController::handle(settingsScreen, settings, ScreenNavigationKey::Search);
    assert(settingsAction.type == SettingsNavigationActionType::EditSearch);

    settingsScreen.toggleAdvanced();
    settingsScreen.setSearchText("switch user");
    settingsAction = SettingsNavigationController::handle(settingsScreen, settings, ScreenNavigationKey::Down);
    assert(settingsAction.type == SettingsNavigationActionType::None);
    settingsAction = SettingsNavigationController::handle(settingsScreen, settings, ScreenNavigationKey::Activate);
    assert(settingsAction.type == SettingsNavigationActionType::Activate);
    assert(settingsAction.activation == SettingActivation::SwitchUser);

    PlaybackQueueState queue;
    queue.replace({queueItem("a"), queueItem("b"), queueItem("c"), queueItem("d")}, 0);
    assert(queue.openOverlay());
    queue.setSelection(3);
    auto queueAction = QueueNavigationController::handle(queue, ScreenNavigationKey::Activate);
    assert(queueAction.type == QueueNavigationActionType::PlayIndex);
    assert(queueAction.index == 3);

    queue.moveAction(2);
    queue.setSelection(3);
    queueAction = QueueNavigationController::handle(queue, ScreenNavigationKey::Activate);
    assert(queueAction.type == QueueNavigationActionType::QueueChanged);
    assert(queue.selection() == 2);
    assert(queue.items()[2].id == "d");

    queue.moveAction(2);
    queueAction = QueueNavigationController::handle(queue, ScreenNavigationKey::Activate);
    assert(queueAction.type == QueueNavigationActionType::QueueChanged);
    assert(queue.size() == 3);

    queue.moveAction(2);
    queueAction = QueueNavigationController::handle(queue, ScreenNavigationKey::Activate);
    assert(queueAction.type == QueueNavigationActionType::QueueChanged);
    assert(queue.repeatMode() == QueueRepeatMode::One);

    using namespace std::chrono_literals;
    SeerrStorageState storage;
    const auto now = SeerrStorageState::Clock::now();
    assert(storage.beginRefresh(true, now));
    storage.finishRefresh({target("movie", 1), target("movie", 2, true)}, now);
    assert(storage.preparePicker(media("movie", 10)) == SeerrStorageState::PickerStatus::Ready);
    auto driveAction = SeerrDriveNavigationController::handle(storage, ScreenNavigationKey::Down);
    assert(driveAction.type == SeerrDriveNavigationActionType::None);
    driveAction = SeerrDriveNavigationController::handle(storage, ScreenNavigationKey::Activate);
    assert(driveAction.type == SeerrDriveNavigationActionType::Selected);
    assert(driveAction.selection);
    assert(driveAction.selection->target.serverId == 1);

    assert(storage.preparePicker(media("movie", 11)) == SeerrStorageState::PickerStatus::Ready);
    driveAction = SeerrDriveNavigationController::handle(storage, ScreenNavigationKey::Back);
    assert(driveAction.type == SeerrDriveNavigationActionType::Back);

    return 0;
}
