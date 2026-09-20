#pragma once

#include "account_async_executor.hpp"
#include "account_flow.hpp"
#include "quick_connect_executor.hpp"
#include "request_epoch.hpp"
#include "screen_navigation_key.hpp"
#include "virtual_keyboard.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

template <typename T>
inline constexpr bool isAccountScreenCompletionV =
    std::is_same_v<std::remove_cvref_t<T>, DiscoveryCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, LoginCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, QuickConnectStartedCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, QuickConnectFailedCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, QuickConnectAuthenticatedCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, QuickConnectTimedOutCompletion>;

enum class AccountScreenHostAction {
    None,
    FinishActivity,
    CancelPendingRequests,
    ActivateKeyboard,
    EditField,
    OpenProfiles,
};

struct AccountScreenEffects {
    AccountScreenHostAction action = AccountScreenHostAction::None;
    int field = -1;
};

template <typename AccountAsync, typename QuickConnectAsync> class AccountScreenCoordinator {
public:
    AccountScreenCoordinator(AccountFlow& account, VirtualKeyboardState& keyboard, AccountAsync& accountAsync,
                             QuickConnectAsync& quickConnectAsync, RequestEpoch& authEpoch, bool& loading,
                             std::string& error)
        : account_(account), keyboard_(keyboard), accountAsync_(accountAsync), quickConnectAsync_(quickConnectAsync),
          authEpoch_(authEpoch), loading_(loading), error_(error) {}

    template <typename Completion> [[nodiscard]] std::optional<JellyfinSession> complete(Completion& completion) {
        AccountCompletionEffects effects = account_.complete(completion, authEpoch_.active(completion.generation));
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        return std::move(effects.authenticatedSession);
    }

    [[nodiscard]] AccountScreenEffects handleLogin(ScreenNavigationKey key) {
        const AccountNavigationAction navigation = account_.handleLogin(key);
        switch (navigation.type) {
        case AccountNavigationActionType::None:
            return {};
        case AccountNavigationActionType::FinishActivity:
            return {.action = AccountScreenHostAction::FinishActivity};
        case AccountNavigationActionType::CancelQuickConnect:
            authEpoch_.invalidate();
            loading_ = false;
            error_.clear();
            return {.action = AccountScreenHostAction::CancelPendingRequests};
        case AccountNavigationActionType::MoveKeyboard:
            keyboard_.move(navigation.dx, navigation.dy);
            return {};
        case AccountNavigationActionType::ActivateKeyboard:
            return {.action = AccountScreenHostAction::ActivateKeyboard};
        case AccountNavigationActionType::EditField:
            return {.action = AccountScreenHostAction::EditField, .field = navigation.index};
        case AccountNavigationActionType::Login:
            login();
            return {};
        case AccountNavigationActionType::QuickConnect:
            quickConnect();
            return {};
        case AccountNavigationActionType::Discover:
            discover();
            return {};
        case AccountNavigationActionType::OpenProfiles:
            return {.action = AccountScreenHostAction::OpenProfiles};
        case AccountNavigationActionType::ProfilesBack:
        case AccountNavigationActionType::AddAccount:
        case AccountNavigationActionType::SwitchSession:
        case AccountNavigationActionType::ForgetSession:
            return {};
        }
        return {};
    }

private:
    void discover() {
        if (!account_.beginDiscovery(loading_)) return;
        loading_ = true;
        error_.clear();
        if (accountAsync_.discover(authEpoch_.begin(), 1600)) return;
        authEpoch_.invalidate();
        loading_ = false;
        account_.state().clearDiscoveryStatus();
        error_ = "DISCOVERY COULD NOT BE STARTED";
    }

    void login() {
        auto fields = account_.beginLogin(loading_);
        if (!fields) return;
        loading_ = true;
        error_.clear();
        if (accountAsync_.login(std::move(*fields), account_.deviceId(), authEpoch_.begin())) return;
        authEpoch_.invalidate();
        loading_ = false;
        error_ = "LOGIN COULD NOT BE STARTED";
    }

    void quickConnect() {
        AccountQuickConnectPlan plan = account_.beginQuickConnect(loading_);
        if (plan.error) {
            error_ = std::move(*plan.error);
            return;
        }
        if (!plan.ready()) return;

        loading_ = true;
        error_.clear();
        if (quickConnectAsync_.connect(std::move(plan.server), account_.deviceId(), authEpoch_.beginToken())) return;
        authEpoch_.invalidate();
        loading_ = false;
        account_.cancelQuickConnect();
        error_ = "QUICK CONNECT COULD NOT BE STARTED";
    }

    AccountFlow& account_;
    VirtualKeyboardState& keyboard_;
    AccountAsync& accountAsync_;
    QuickConnectAsync& quickConnectAsync_;
    RequestEpoch& authEpoch_;
    bool& loading_;
    std::string& error_;
};
