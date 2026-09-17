#pragma once

#include "jellyfin_types.hpp"

#include <string>
#include <utility>

struct SeerrQuickConnectRequest {
    std::string code;
    std::string secret;
    std::string csrfCookie;
    std::string csrfToken;
};

enum class SeerrQuickConnectStage {
    None,
    Initiate,
    AuthorizeJellyfin,
    AuthenticateSeerr,
};

struct SeerrQuickConnectResult {
    bool ok = false;
    SeerrQuickConnectStage failedStage = SeerrQuickConnectStage::None;
    std::string sessionCookie;
    std::string error;
};

template <typename Initiate, typename Authorize, typename Authenticate>
SeerrQuickConnectResult runSeerrQuickConnect(Initiate&& initiate, Authorize&& authorize, Authenticate&& authenticate) {
    auto initiated = std::forward<Initiate>(initiate)();
    if (!initiated.ok) {
        return {
            .ok = false,
            .failedStage = SeerrQuickConnectStage::Initiate,
            .sessionCookie = {},
            .error = std::move(initiated.error),
        };
    }

    auto authorized = std::forward<Authorize>(authorize)(initiated.value.code);
    if (!authorized.ok || !authorized.value) {
        return {
            .ok = false,
            .failedStage = SeerrQuickConnectStage::AuthorizeJellyfin,
            .sessionCookie = {},
            .error = std::move(authorized.error),
        };
    }

    auto authenticated = std::forward<Authenticate>(authenticate)(initiated.value);
    if (!authenticated.ok) {
        return {
            .ok = false,
            .failedStage = SeerrQuickConnectStage::AuthenticateSeerr,
            .sessionCookie = {},
            .error = std::move(authenticated.error),
        };
    }

    return {
        .ok = true,
        .failedStage = SeerrQuickConnectStage::None,
        .sessionCookie = std::move(authenticated.value),
        .error = {},
    };
}
