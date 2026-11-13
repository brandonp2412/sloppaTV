#pragma once
#include "subtitle_auto_sync.hpp"
#include <cstdint>
#include <optional>
#include <string>

inline uint64_t subtitleCueFingerprint(const std::vector<SubtitleCue>& cues) {
    if (cues.empty()) return 0;
    uint64_t hash = 14695981039346656037ULL;
    const auto byte = [&](unsigned char value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    const auto integer = [&](uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            byte(static_cast<unsigned char>(value & 255));
            value >>= 8;
        }
    };
    integer(cues.size());
    for (const auto& cue : cues) {
        integer(static_cast<uint64_t>(cue.startMs));
        integer(static_cast<uint64_t>(cue.endMs));
        integer(cue.text.size());
        for (unsigned char c : cue.text) byte(c);
    }
    return hash;
}

struct SubtitleSyncContext {
    uint64_t generation = 0;
    std::string itemId;
    std::string sourceId;
    std::string sourceFingerprint;
    std::string playSessionId;
    std::string mediaUrl;
    int timelineBaseMs = 0;
    int audioIndex = -1;
    int subtitleIndex = -1;
    uint64_t cueFingerprint = 0;
    std::optional<int> timelineOriginMs;
    bool operator==(const SubtitleSyncContext&) const = default;

    bool trustworthy() const {
        return !itemId.empty() && !sourceId.empty() && !sourceFingerprint.empty() && !playSessionId.empty() &&
               !mediaUrl.empty() && timelineBaseMs >= 0 && audioIndex >= 0 && subtitleIndex >= 0 && cueFingerprint != 0;
    }
};

// Only reuse a verified correction inside the exact same playback/audio/cue context.
// Off/on changes the request revision, but does not change these identities.
class SubtitleSyncCorrectionCache {
public:
    void store(SubtitleSyncContext context, SubtitleSyncDecision decision) {
        if (!context.trustworthy() || !decision.confident || !std::isfinite(decision.timeScale) ||
            decision.timeScale < 0.9 || decision.timeScale > 1.1 || decision.offsetMs < -120000 ||
            decision.offsetMs > 120000)
            return;
        context_ = std::move(context);
        decision_ = decision;
    }

    std::optional<SubtitleSyncDecision> find(const SubtitleSyncContext& context) const {
        if (!context.trustworthy() || !context_ || *context_ != context) return std::nullopt;
        return decision_;
    }

private:
    std::optional<SubtitleSyncContext> context_;
    std::optional<SubtitleSyncDecision> decision_;
};
