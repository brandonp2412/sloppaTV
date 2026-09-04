#pragma once

#include "decoded_image.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

enum class ArtworkState {
    Loading,
    Ready,
    Failed,
};

struct ArtworkEntry {
    ArtworkState state = ArtworkState::Loading;
    DecodedImage decoded;
    int sourceWidth = 0;
    int sourceHeight = 0;
    uint32_t texture = 0;
    uint64_t textureGeneration = 0;
    uint64_t lastUse = 0;
    std::chrono::steady_clock::time_point retryAfter{};
};

class ArtworkCache {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit ArtworkCache(size_t maxEntries = 0) : maxEntries_(maxEntries) {}

    ArtworkEntry* find(const std::string& key) {
        const auto it = entries_.find(key);
        if (it == entries_.end()) return nullptr;
        it->second.lastUse = ++useCounter_;
        return &it->second;
    }

    const ArtworkEntry* peek(const std::string& key) const {
        const auto it = entries_.find(key);
        return it == entries_.end() ? nullptr : &it->second;
    }

    template <typename Release>
    bool beginLoad(const std::string& key, Release&& release, TimePoint now = Clock::now()) {
        const auto existing = entries_.find(key);
        if (existing != entries_.end()) {
            existing->second.lastUse = ++useCounter_;
            if (existing->second.state != ArtworkState::Failed || now < existing->second.retryAfter) return false;
            release(existing->second);
            const uint64_t lastUse = existing->second.lastUse;
            existing->second = ArtworkEntry{};
            existing->second.lastUse = lastUse;
            return true;
        }
        if (!makeRoom(std::forward<Release>(release))) return false;
        ArtworkEntry entry;
        entry.lastUse = ++useCounter_;
        entries_.emplace(key, std::move(entry));
        return true;
    }

    void markFailed(const std::string& key, TimePoint now = Clock::now()) {
        const auto it = entries_.find(key);
        if (it == entries_.end()) return;
        it->second.state = ArtworkState::Failed;
        it->second.retryAfter = now + std::chrono::seconds(30);
    }

    bool markReady(const std::string& key, DecodedImage decoded) {
        const auto it = entries_.find(key);
        if (it == entries_.end()) return false;
        it->second.sourceWidth = decoded.width;
        it->second.sourceHeight = decoded.height;
        it->second.decoded = std::move(decoded);
        it->second.state = ArtworkState::Ready;
        return true;
    }

    template <typename Release>
    void erase(const std::string& key, Release&& release) {
        const auto it = entries_.find(key);
        if (it == entries_.end()) return;
        release(it->second);
        entries_.erase(it);
    }

    template <typename Release>
    void clear(Release&& release) {
        for (auto& [key, entry] : entries_) {
            (void)key;
            release(entry);
        }
        entries_.clear();
        useCounter_ = 0;
    }

    [[nodiscard]] size_t size() const { return entries_.size(); }

private:
    template <typename Release>
    bool makeRoom(Release&& release) {
        if (maxEntries_ == 0) return true;
        while (entries_.size() >= maxEntries_) {
            auto victim = entries_.end();
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                if (it->second.state == ArtworkState::Loading) continue;
                if (victim == entries_.end() || it->second.lastUse < victim->second.lastUse) victim = it;
            }
            if (victim == entries_.end()) return false;
            release(victim->second);
            entries_.erase(victim);
        }
        return true;
    }

    size_t maxEntries_ = 0;
    uint64_t useCounter_ = 0;
    std::unordered_map<std::string, ArtworkEntry> entries_;
};
