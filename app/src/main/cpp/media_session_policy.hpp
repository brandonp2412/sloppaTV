#pragma once

#include <optional>

enum class MediaSessionState {
    Stopped,
    Buffering,
    Playing,
    Paused,
};

constexpr bool mediaSessionNeedsScreenOn(MediaSessionState state) {
    return state == MediaSessionState::Playing || state == MediaSessionState::Buffering;
}

constexpr bool shouldUpdateKeepScreenOn(const std::optional<bool>& applied, bool requested) {
    return !applied.has_value() || *applied != requested;
}

constexpr std::optional<bool> keepScreenOnAfterAttempt(
    const std::optional<bool>& applied,
    bool requested,
    bool succeeded
) {
    return succeeded ? std::optional<bool>{requested} : applied;
}
