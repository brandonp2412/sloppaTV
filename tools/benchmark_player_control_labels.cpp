#include "player_track_labels.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

[[gnu::noinline]] static size_t consume(std::string_view audio, std::string_view subtitle) {
    asm volatile("" : : "r"(audio.data()), "r"(subtitle.data()) : "memory");
    return audio.size() + subtitle.size();
}

static size_t rebuild(const JellyfinItem& item, const PlayerTrackState& state, int iterations) {
    size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        const std::string audio = "AUDIO  " + audioTrackLabel(item, state.selectedAudioServerIndex());
        const std::string subtitle = "SUBTITLES  " + subtitleTrackLabel(item, state);
        checksum += consume(audio, subtitle);
    }
    return checksum;
}

static size_t cached(const JellyfinItem& item, const PlayerTrackState& state, int iterations) {
    PlayerControlLabelCache cache;
    size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        cache.update(item, state);
        checksum += consume(cache.audio, cache.subtitle);
    }
    return checksum;
}

template <typename Function>
double measure(Function&& function, const JellyfinItem& item, const PlayerTrackState& state, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    checksum = function(item, state, iterations);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 3000000;
    JellyfinItem item;
    item.id = "episode-id";
    for (int i = 0; i < 6; ++i) {
        JellyfinAudioStream audio;
        audio.index = i;
        audio.language = i == 4 ? "jpn" : "eng";
        item.audios.push_back(std::move(audio));
    }
    for (int i = 0; i < 8; ++i) {
        JellyfinSubtitleStream subtitle;
        subtitle.index = 10 + i;
        subtitle.language = i == 5 ? "fra" : "eng";
        item.subtitles.push_back(std::move(subtitle));
    }
    PlayerTrackState state;
    state.setSelectedAudioServerIndex(4);
    state.setSelectedSubtitleServerIndex(15);

    size_t rebuildChecksum = 0;
    size_t cachedChecksum = 0;
    const double rebuildMs = measure(rebuild, item, state, iterations, rebuildChecksum);
    const double cachedMs = measure(cached, item, state, iterations, cachedChecksum);
    if (rebuildChecksum != cachedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " rebuild_ms=" << rebuildMs
              << " cached_ms=" << cachedMs
              << " speedup_pct=" << ((rebuildMs - cachedMs) * 100.0 / rebuildMs)
              << " checksum=" << cachedChecksum << '\n';
    return 0;
}
