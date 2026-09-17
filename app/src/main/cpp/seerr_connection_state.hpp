#pragma once

#include "seerr_media.hpp"

#include <optional>
#include <utility>

class SeerrConnectionState {
public:
    struct DeferredWork {
        std::optional<SeerrMediaItem> request;
        bool retrySearch = false;
    };

    [[nodiscard]] bool connecting() const { return connecting_; }

    bool beginConnect() {
        if (connecting_) return false;
        connecting_ = true;
        return true;
    }

    void endConnect() { connecting_ = false; }

    void failConnect() {
        connecting_ = false;
        clearDeferred();
    }

    void deferRequest(const SeerrMediaItem& item) { deferredRequest_ = item; }

    void deferSearchRetry() { retrySearch_ = true; }

    [[nodiscard]] DeferredWork takeDeferredWork() {
        DeferredWork work{
            .request = std::move(deferredRequest_),
            .retrySearch = retrySearch_,
        };
        clearDeferred();
        return work;
    }

private:
    void clearDeferred() {
        deferredRequest_.reset();
        retrySearch_ = false;
    }

    bool connecting_ = false;
    bool retrySearch_ = false;
    std::optional<SeerrMediaItem> deferredRequest_;
};
