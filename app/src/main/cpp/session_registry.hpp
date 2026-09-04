#pragma once

#include "jellyfin_types.hpp"
#include "session_store.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class SessionRegistry {
public:
    static bool sameIdentity(const JellyfinSession& left, const JellyfinSession& right) {
        return left.server == right.server && left.userId == right.userId;
    }

    static StoredSession toStored(const JellyfinSession& session) {
        return {
            .server = session.server,
            .username = session.username,
            .userId = session.userId,
            .token = session.token,
        };
    }

    static JellyfinSession fromStored(const StoredSession& session, const std::string& deviceId) {
        return {
            .server = session.server,
            .username = session.username,
            .userId = session.userId,
            .token = session.token,
            .deviceId = deviceId,
        };
    }

    void importStored(const std::vector<StoredSession>& stored, const std::string& deviceId) {
        sessions_.clear();
        sessions_.reserve(std::min(stored.size(), kMaxSavedSessions));
        for (const auto& persisted : stored) {
            JellyfinSession session = fromStored(persisted, deviceId);
            if (!session.valid()) continue;
            const auto duplicate = std::find_if(sessions_.begin(), sessions_.end(), [&](const JellyfinSession& candidate) {
                return sameIdentity(candidate, session);
            });
            if (duplicate != sessions_.end()) continue;
            sessions_.push_back(std::move(session));
            if (sessions_.size() >= kMaxSavedSessions) break;
        }
    }

    [[nodiscard]] std::vector<StoredSession> exportStored() const {
        std::vector<StoredSession> stored;
        stored.reserve(sessions_.size());
        for (const auto& session : sessions_) stored.push_back(toStored(session));
        return stored;
    }

    void remember(const JellyfinSession& session, const std::string& deviceId) {
        if (!session.valid()) return;
        auto saved = session;
        saved.deviceId = deviceId;
        const auto existing = std::find_if(sessions_.begin(), sessions_.end(), [&](const JellyfinSession& candidate) {
            return sameIdentity(candidate, saved);
        });
        if (existing == sessions_.end()) {
            sessions_.insert(sessions_.begin(), std::move(saved));
        } else {
            *existing = std::move(saved);
            std::rotate(sessions_.begin(), existing, std::next(existing));
        }
        if (sessions_.size() > kMaxSavedSessions) sessions_.resize(kMaxSavedSessions);
    }

    bool removeIdentity(const JellyfinSession& session) {
        const auto previousSize = sessions_.size();
        std::erase_if(sessions_, [&](const JellyfinSession& saved) {
            return sameIdentity(saved, session);
        });
        return sessions_.size() != previousSize;
    }

    bool eraseAt(std::size_t index) {
        if (index >= sessions_.size()) return false;
        sessions_.erase(sessions_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    [[nodiscard]] bool empty() const { return sessions_.empty(); }
    [[nodiscard]] std::size_t size() const { return sessions_.size(); }
    [[nodiscard]] const JellyfinSession* at(std::size_t index) const {
        return index < sessions_.size() ? &sessions_[index] : nullptr;
    }
    [[nodiscard]] const std::vector<JellyfinSession>& sessions() const { return sessions_; }

private:
    static constexpr std::size_t kMaxSavedSessions = 16;
    std::vector<JellyfinSession> sessions_;
};
