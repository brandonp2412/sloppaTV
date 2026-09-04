#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

class AccountScreenState {
public:
    static constexpr int kServerField = 0;
    static constexpr int kUsernameField = 1;
    static constexpr int kPasswordField = 2;
    static constexpr int kLoginAction = 3;
    static constexpr int kQuickConnectAction = 4;
    static constexpr int kDiscoverAction = 5;
    static constexpr int kSavedUsersAction = 6;

    [[nodiscard]] const std::array<std::string, 3>& fields() const { return fields_; }
    [[nodiscard]] const std::string& field(int index) const { return fields_[static_cast<size_t>(index)]; }
    void setField(int index, std::string value) {
        if (index < 0 || index >= static_cast<int>(fields_.size())) return;
        fields_[static_cast<size_t>(index)] = std::move(value);
    }
    void appendToFocusedField(char value) {
        if (loginFocus_ >= 0 && loginFocus_ < static_cast<int>(fields_.size())) {
            fields_[static_cast<size_t>(loginFocus_)].push_back(value);
        }
    }
    bool backspaceFocusedField() {
        if (loginFocus_ < 0 || loginFocus_ >= static_cast<int>(fields_.size())) return false;
        auto& value = fields_[static_cast<size_t>(loginFocus_)];
        if (!value.empty()) value.pop_back();
        return true;
    }

    [[nodiscard]] int loginFocus() const { return loginFocus_; }
    void setLoginFocus(int focus) { loginFocus_ = std::clamp(focus, kServerField, kSavedUsersAction); }
    void moveLoginVertical(int direction) {
        if (direction < 0) loginFocus_ = loginFocus_ >= kLoginAction ? kPasswordField : std::max(kServerField, loginFocus_ - 1);
        else if (direction > 0) {
            if (loginFocus_ < kPasswordField) ++loginFocus_;
            else if (loginFocus_ == kPasswordField) loginFocus_ = kLoginAction;
        }
    }
    void moveLoginAction(int direction, bool hasSavedUsers) {
        if (loginFocus_ < kLoginAction || direction == 0) return;
        const int maximum = hasSavedUsers ? kSavedUsersAction : kDiscoverAction;
        if (direction < 0) loginFocus_ = loginFocus_ <= kLoginAction ? maximum : loginFocus_ - 1;
        else loginFocus_ = loginFocus_ >= maximum ? kLoginAction : loginFocus_ + 1;
    }
    void finishTextField(int field, std::string value) {
        setField(field, std::move(value));
        loginFocus_ = std::min(kLoginAction, field + 1);
    }

    [[nodiscard]] bool keyboardActive() const { return keyboardActive_; }
    void setKeyboardActive(bool active) { keyboardActive_ = active; }

    [[nodiscard]] bool quickConnectActive() const { return quickConnectActive_; }
    [[nodiscard]] const std::string& quickConnectCode() const { return quickConnectCode_; }
    void beginQuickConnect(std::string code = "------") {
        quickConnectActive_ = true;
        quickConnectCode_ = std::move(code);
    }
    void setQuickConnectCode(std::string code) { quickConnectCode_ = std::move(code); }
    void endQuickConnect(bool focusAction = false) {
        quickConnectActive_ = false;
        quickConnectCode_.clear();
        if (focusAction) loginFocus_ = kQuickConnectAction;
    }

    [[nodiscard]] const std::string& discoveryStatus() const { return discoveryStatus_; }
    void setDiscoveryStatus(std::string status) { discoveryStatus_ = std::move(status); }
    void clearDiscoveryStatus() { discoveryStatus_.clear(); }

    void clearSessionUi() {
        fields_[kUsernameField].clear();
        fields_[kPasswordField].clear();
        keyboardActive_ = false;
        endQuickConnect();
        discoveryStatus_.clear();
    }
    void beginAddAccount(const std::string& existingServer) {
        clearSessionUi();
        if (!existingServer.empty()) fields_[kServerField] = existingServer;
        loginFocus_ = kServerField;
    }
    void setAuthenticatedAccount(const std::string& server, const std::string& username) {
        fields_[kServerField] = server;
        fields_[kUsernameField] = username;
        fields_[kPasswordField].clear();
        keyboardActive_ = false;
        endQuickConnect();
    }

    void beginProfiles(int savedCount) {
        profileSelection_ = std::clamp(profileSelection_, 0, std::max(0, savedCount));
        profileAction_ = 0;
    }
    void moveProfile(int direction, int savedCount) {
        if (direction < 0) profileSelection_ = std::max(0, profileSelection_ - 1);
        else if (direction > 0) profileSelection_ = std::min(std::max(0, savedCount), profileSelection_ + 1);
        profileAction_ = 0;
    }
    void toggleProfileAction(int savedCount) {
        if (profileSelection_ < savedCount) profileAction_ = profileAction_ == 0 ? 1 : 0;
    }
    [[nodiscard]] int profileSelection() const { return profileSelection_; }
    [[nodiscard]] int profileAction() const { return profileAction_; }

private:
    std::array<std::string, 3> fields_{};
    int loginFocus_ = 0;
    bool keyboardActive_ = false;
    bool quickConnectActive_ = false;
    std::string quickConnectCode_;
    std::string discoveryStatus_;
    int profileSelection_ = 0;
    int profileAction_ = 0;
};
