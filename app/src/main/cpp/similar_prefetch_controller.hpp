#pragma once

#include "jellyfin_types.hpp"
#include "seerr_jellyfin_adapter.hpp"

#include <algorithm>
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
    ApiValueResult<JellyfinItem> detail;
    ApiValueResult<std::vector<JellyfinItem>> result;
    std::string seriesId;
    ApiValueResult<JellyfinItem> seriesDetail;
    ApiValueResult<std::vector<JellyfinItem>> seasons;
    std::string seasonId;
    ApiValueResult<std::vector<JellyfinItem>> episodes;
    ApiValueResult<JellyfinItem> nextEpisodeDetail;
};

struct SimilarPrefetchWork {
    JellyfinSession session;
    JellyfinItem item;
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

    [[nodiscard]] std::optional<JellyfinItem> cachedDetail(const JellyfinSession& session, const std::string& itemId) {
        std::scoped_lock lock(mutex_);
        return cachedValue(details_, key(session, itemId), Clock::now());
    }

    [[nodiscard]] std::optional<std::vector<JellyfinItem>> cached(const JellyfinSession& session,
                                                                  const std::string& itemId) {
        std::scoped_lock lock(mutex_);
        return cachedValue(similar_, key(session, itemId), Clock::now());
    }

    [[nodiscard]] std::optional<std::vector<JellyfinItem>> cachedSeasons(const JellyfinSession& session,
                                                                         const std::string& seriesId) {
        std::scoped_lock lock(mutex_);
        return cachedValue(seasons_, key(session, seriesId), Clock::now());
    }

    [[nodiscard]] std::optional<JellyfinItem> cachedSeriesDetail(const JellyfinSession& session,
                                                                 const std::string& seriesId) {
        return cachedDetail(session, seriesId);
    }

    [[nodiscard]] std::optional<std::vector<JellyfinItem>>
    cachedEpisodes(const JellyfinSession& session, const std::string& seriesId, const std::string& seasonId) {
        std::scoped_lock lock(mutex_);
        return cachedValue(episodes_, episodeKey(session, seriesId, seasonId), Clock::now());
    }

    void rememberDetail(const JellyfinSession& session, JellyfinItem item) {
        if (!session.valid() || item.id.empty()) return;
        std::scoped_lock lock(mutex_);
        const std::string detailKey = key(session, item.id);
        store(details_, detailKey, std::move(item), Clock::now());
    }

    void rememberSimilar(const JellyfinSession& session, const std::string& itemId, std::vector<JellyfinItem> items) {
        if (!session.valid() || itemId.empty()) return;
        std::scoped_lock lock(mutex_);
        store(similar_, key(session, itemId), std::move(items), Clock::now());
    }

    void rememberSeasons(const JellyfinSession& session, const std::string& seriesId, std::vector<JellyfinItem> items) {
        if (!session.valid() || seriesId.empty()) return;
        std::scoped_lock lock(mutex_);
        store(seasons_, key(session, seriesId), std::move(items), Clock::now());
    }

    void rememberEpisodes(const JellyfinSession& session, const std::string& seriesId, const std::string& seasonId,
                          std::vector<JellyfinItem> items) {
        if (!session.valid() || seriesId.empty() || seasonId.empty()) return;
        std::scoped_lock lock(mutex_);
        store(episodes_, episodeKey(session, seriesId, seasonId), std::move(items), Clock::now());
    }

    bool schedule(const JellyfinSession& session, const JellyfinItem& item, Clock::time_point now = Clock::now()) {
        std::scoped_lock lock(mutex_);
        if (!supports(session, item)) {
            clearPendingLocked();
            return false;
        }

        const std::string cacheKey = key(session, item.id);
        if (!needsPrefetchLocked(session, item, now)) {
            clearPendingLocked();
            return false;
        }
        if (inFlight_.contains(cacheKey)) return false;
        if (candidateKey_ == cacheKey && due_ != Clock::time_point{}) return false;

        candidate_ = item;
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

        JellyfinItem item = candidate_;
        const std::string cacheKey = candidateKey_;
        clearPendingLocked();
        if (!session.valid() || item.id.empty() || cacheKey != key(session, item.id) || inFlight_.contains(cacheKey)) {
            return std::nullopt;
        }
        if (!needsPrefetchLocked(session, item, now)) return std::nullopt;

        inFlight_.insert(cacheKey);
        const std::string itemId = item.id;
        return SimilarPrefetchWork{
            .session = session,
            .item = std::move(item),
            .itemId = itemId,
            .key = cacheKey,
        };
    }

    void submissionFailed(const std::string& cacheKey) {
        std::scoped_lock lock(mutex_);
        inFlight_.erase(cacheKey);
    }

    [[nodiscard]] std::optional<std::vector<JellyfinItem>> complete(SimilarPrefetchCompletion& completion) {
        std::scoped_lock lock(mutex_);
        inFlight_.erase(completion.key);
        const auto now = Clock::now();

        if (completion.detail.ok && !completion.detail.value.id.empty()) {
            const std::string detailKey = key(completion.session, completion.detail.value.id);
            store(details_, detailKey, std::move(completion.detail.value), now);
        }
        if (completion.result.ok) {
            store(similar_, key(completion.session, completion.itemId), std::move(completion.result.value), now);
        }
        if (completion.seriesDetail.ok && !completion.seriesDetail.value.id.empty()) {
            const std::string seriesKey = key(completion.session, completion.seriesDetail.value.id);
            store(details_, seriesKey, std::move(completion.seriesDetail.value), now);
        }
        if (completion.seasons.ok && !completion.seriesId.empty()) {
            store(seasons_, key(completion.session, completion.seriesId), std::move(completion.seasons.value), now);
        }
        if (completion.episodes.ok && !completion.seriesId.empty() && !completion.seasonId.empty()) {
            store(episodes_, episodeKey(completion.session, completion.seriesId, completion.seasonId),
                  std::move(completion.episodes.value), now);
        }
        if (completion.nextEpisodeDetail.ok && !completion.nextEpisodeDetail.value.id.empty()) {
            const std::string nextEpisodeKey = key(completion.session, completion.nextEpisodeDetail.value.id);
            store(details_, nextEpisodeKey, std::move(completion.nextEpisodeDetail.value), now);
        }

        return cachedValue(similar_, key(completion.session, completion.itemId), now);
    }

private:
    template <typename T> struct CacheEntry {
        T value;
        Clock::time_point loadedAt{};
    };

    static constexpr auto kDebounce = std::chrono::milliseconds(180);
    static constexpr auto kLifetime = std::chrono::minutes(5);
    static constexpr size_t kCapacity = 16;

    static std::string key(const JellyfinSession& session, const std::string& itemId) {
        return session.server + "\n" + session.userId + "\n" + itemId;
    }

    static std::string episodeKey(const JellyfinSession& session, const std::string& seriesId,
                                  const std::string& seasonId) {
        return key(session, seriesId) + "\n" + seasonId;
    }

    static bool supports(const JellyfinSession& session, const JellyfinItem& item) {
        if (!session.valid() || item.id.empty() || isSeerrItem(item)) return false;
        return item.type != "Folder" && item.type != "BoxSet" && item.type != "CollectionFolder" &&
               item.type != "Genre" && item.type != "Letter" && item.type != "Person";
    }

    template <typename T> static bool fresh(const CacheEntry<T>& entry, Clock::time_point now) {
        return now - entry.loadedAt <= kLifetime;
    }

    template <typename T>
    static std::optional<T> cachedValue(std::unordered_map<std::string, CacheEntry<T>>& cache,
                                        const std::string& cacheKey, Clock::time_point now) {
        const auto found = cache.find(cacheKey);
        if (found == cache.end()) return std::nullopt;
        if (!fresh(found->second, now)) {
            cache.erase(found);
            return std::nullopt;
        }
        return found->second.value;
    }

    template <typename T>
    static bool hasFresh(std::unordered_map<std::string, CacheEntry<T>>& cache, const std::string& cacheKey,
                         Clock::time_point now) {
        const auto found = cache.find(cacheKey);
        if (found == cache.end()) return false;
        if (!fresh(found->second, now)) {
            cache.erase(found);
            return false;
        }
        return true;
    }

    template <typename T>
    static void store(std::unordered_map<std::string, CacheEntry<T>>& cache, std::string cacheKey, T value,
                      Clock::time_point now) {
        if (!cache.contains(cacheKey) && cache.size() >= kCapacity) cache.erase(cache.begin());
        cache.insert_or_assign(std::move(cacheKey), CacheEntry<T>{.value = std::move(value), .loadedAt = now});
    }

    bool needsPrefetchLocked(const JellyfinSession& session, const JellyfinItem& item, Clock::time_point now) {
        const std::string itemKey = key(session, item.id);
        if (item.type == "Season" && !item.seriesId.empty()) {
            return !hasFresh(episodes_, episodeKey(session, item.seriesId, item.id), now);
        }

        if (!hasFresh(details_, itemKey, now) || !hasFresh(similar_, itemKey, now)) return true;
        if (item.type == "Series") return !hasFresh(seasons_, itemKey, now);
        if (item.type != "Episode" || item.seriesId.empty()) return false;

        const std::string seriesKey = key(session, item.seriesId);
        if (!hasFresh(details_, seriesKey, now) || !hasFresh(seasons_, seriesKey, now)) return true;

        const auto seasons = cachedValue(seasons_, seriesKey, now);
        if (!seasons || item.parentIndexNumber < 0) return false;
        const auto season = std::find_if(seasons->begin(), seasons->end(), [&](const JellyfinItem& candidate) {
            return candidate.indexNumber == item.parentIndexNumber;
        });
        return season != seasons->end() && !hasFresh(episodes_, episodeKey(session, item.seriesId, season->id), now);
    }

    void clearPendingLocked() {
        candidate_ = {};
        candidateKey_.clear();
        due_ = {};
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry<JellyfinItem>> details_;
    std::unordered_map<std::string, CacheEntry<std::vector<JellyfinItem>>> similar_;
    std::unordered_map<std::string, CacheEntry<std::vector<JellyfinItem>>> seasons_;
    std::unordered_map<std::string, CacheEntry<std::vector<JellyfinItem>>> episodes_;
    std::unordered_set<std::string> inFlight_;
    JellyfinItem candidate_;
    std::string candidateKey_;
    Clock::time_point due_{};
};
