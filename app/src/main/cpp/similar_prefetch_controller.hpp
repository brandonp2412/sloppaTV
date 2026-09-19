#pragma once

#include "jellyfin_types.hpp"
#include "seerr_jellyfin_adapter.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct SimilarPrefetchCompletion {
    JellyfinSession session;
    std::string itemId;
    std::string key;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

struct SimilarPrefetchWork {
    JellyfinSession session;
    std::string itemId;
    std::string key;
};

class SimilarPrefetchController {
public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] Clock::time_point dueDeadline() const {
        std::scoped_lock lock(mutex_);
        return due_;
    }

    [[nodiscard]] std::optional<std::vector<JellyfinItem>> cached(const JellyfinSession& session,
                                                                  const std::string& itemId) {
        std::scoped_lock lock(mutex_);
        const std::string cacheKey = key(session, itemId);
        const auto found = cache_.find(cacheKey);
        if (found == cache_.end()) return std::nullopt;
        if (!fresh(found->second, Clock::now())) {
            cache_.erase(found);
            return std::nullopt;
        }
        return found->second.items;
    }

    bool schedule(const JellyfinSession& session, const JellyfinItem& item, Clock::time_point now = Clock::now()) {
        std::scoped_lock lock(mutex_);
        if (!supports(session, item)) {
            clearPendingLocked();
            return false;
        }

        const std::string cacheKey = key(session, item.id);
        const auto cached = cache_.find(cacheKey);
        if (cached != cache_.end() && fresh(cached->second, now)) {
            clearPendingLocked();
            return false;
        }
        if (inFlight_.contains(cacheKey)) return false;

        candidateId_ = item.id;
        candidateKey_ = cacheKey;
        due_ = now + kDebounce;
        return true;
    }

    void clearPending() {
        std::scoped_lock lock(mutex_);
        clearPendingLocked();
    }

    [[nodiscard]] std::optional<SimilarPrefetchWork> takeDue(const JellyfinSession& session, bool eligible,
                                                             Clock::time_point now = Clock::now()) {
        std::scoped_lock lock(mutex_);
        if (due_ == Clock::time_point{} || now < due_) return std::nullopt;
        if (!eligible) {
            clearPendingLocked();
            return std::nullopt;
        }

        const std::string itemId = candidateId_;
        const std::string cacheKey = candidateKey_;
        clearPendingLocked();
        if (!session.valid() || itemId.empty() || cacheKey != key(session, itemId) || inFlight_.contains(cacheKey)) {
            return std::nullopt;
        }

        const auto cached = cache_.find(cacheKey);
        if (cached != cache_.end() && fresh(cached->second, now)) return std::nullopt;

        inFlight_.insert(cacheKey);
        return SimilarPrefetchWork{.session = session, .itemId = itemId, .key = cacheKey};
    }

    void submissionFailed(const std::string& cacheKey) {
        std::scoped_lock lock(mutex_);
        inFlight_.erase(cacheKey);
    }

    [[nodiscard]] std::optional<std::vector<JellyfinItem>> complete(SimilarPrefetchCompletion& completion) {
        std::scoped_lock lock(mutex_);
        inFlight_.erase(completion.key);
        if (!completion.result.ok) return std::nullopt;

        const std::string cacheKey = key(completion.session, completion.itemId);
        if (!cache_.contains(cacheKey) && cache_.size() >= kCapacity) cache_.erase(cache_.begin());

        CacheEntry entry{
            .items = std::move(completion.result.value),
            .loadedAt = Clock::now(),
        };
        auto [stored, inserted] = cache_.insert_or_assign(cacheKey, std::move(entry));
        (void)inserted;
        return stored->second.items;
    }

private:
    struct CacheEntry {
        std::vector<JellyfinItem> items;
        Clock::time_point loadedAt{};
    };

    static constexpr auto kDebounce = std::chrono::milliseconds(350);
    static constexpr auto kLifetime = std::chrono::minutes(5);
    static constexpr size_t kCapacity = 8;

    static std::string key(const JellyfinSession& session, const std::string& itemId) {
        return session.server + "\n" + session.userId + "\n" + itemId;
    }

    static bool supports(const JellyfinSession& session, const JellyfinItem& item) {
        if (!session.valid() || item.id.empty() || isSeerrItem(item)) return false;
        return item.type != "Folder" && item.type != "BoxSet" && item.type != "CollectionFolder" &&
               item.type != "Genre" && item.type != "Letter" && item.type != "Person";
    }

    static bool fresh(const CacheEntry& entry, Clock::time_point now) { return now - entry.loadedAt <= kLifetime; }

    void clearPendingLocked() {
        candidateId_.clear();
        candidateKey_.clear();
        due_ = {};
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::unordered_set<std::string> inFlight_;
    std::string candidateId_;
    std::string candidateKey_;
    Clock::time_point due_{};
};
