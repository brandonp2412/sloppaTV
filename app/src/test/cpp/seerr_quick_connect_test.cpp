#include "seerr_quick_connect.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace {
template <typename T> ApiValueResult<T> success(T value) {
    ApiValueResult<T> result;
    result.ok = true;
    result.value = std::move(value);
    return result;
}

template <typename T> ApiValueResult<T> failure(std::string error) {
    ApiValueResult<T> result;
    result.error = std::move(error);
    return result;
}

SeerrQuickConnectRequest request() {
    return {
        .code = "CODE",
        .secret = "SECRET",
        .csrfCookie = "COOKIE",
        .csrfToken = "TOKEN",
    };
}
} // namespace

int main() {
    int initiated = 0;
    int authorized = 0;
    int authenticated = 0;
    auto connected = runSeerrQuickConnect(
        [&] {
            ++initiated;
            return success(request());
        },
        [&](const std::string& code) {
            ++authorized;
            assert(code == "CODE");
            return success(true);
        },
        [&](const SeerrQuickConnectRequest& pending) {
            ++authenticated;
            assert(pending.secret == "SECRET");
            return success(std::string("session-cookie"));
        });
    assert(connected.ok);
    assert(connected.failedStage == SeerrQuickConnectStage::None);
    assert(connected.sessionCookie == "session-cookie");
    assert(connected.error.empty());
    assert(initiated == 1 && authorized == 1 && authenticated == 1);

    initiated = authorized = authenticated = 0;
    auto initiateFailed = runSeerrQuickConnect(
        [&] {
            ++initiated;
            return failure<SeerrQuickConnectRequest>("init failed");
        },
        [&](const std::string&) {
            ++authorized;
            return success(true);
        },
        [&](const SeerrQuickConnectRequest&) {
            ++authenticated;
            return success(std::string("unused"));
        });
    assert(!initiateFailed.ok);
    assert(initiateFailed.failedStage == SeerrQuickConnectStage::Initiate);
    assert(initiateFailed.error == "init failed");
    assert(initiated == 1 && authorized == 0 && authenticated == 0);

    initiated = authorized = authenticated = 0;
    auto authorizeRejected = runSeerrQuickConnect(
        [&] {
            ++initiated;
            return success(request());
        },
        [&](const std::string&) {
            ++authorized;
            return success(false);
        },
        [&](const SeerrQuickConnectRequest&) {
            ++authenticated;
            return success(std::string("unused"));
        });
    assert(!authorizeRejected.ok);
    assert(authorizeRejected.failedStage == SeerrQuickConnectStage::AuthorizeJellyfin);
    assert(authorizeRejected.error.empty());
    assert(initiated == 1 && authorized == 1 && authenticated == 0);

    auto authenticateFailed =
        runSeerrQuickConnect([&] { return success(request()); }, [&](const std::string&) { return success(true); },
                             [&](const SeerrQuickConnectRequest&) { return failure<std::string>("auth failed"); });
    assert(!authenticateFailed.ok);
    assert(authenticateFailed.failedStage == SeerrQuickConnectStage::AuthenticateSeerr);
    assert(authenticateFailed.error == "auth failed");
    assert(authenticateFailed.sessionCookie.empty());
    return 0;
}
