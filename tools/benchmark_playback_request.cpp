#include "playback_info.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 20000;
    JellyfinSession session;
    session.userId = "user";
    JellyfinItem item;
    item.id = "episode";
    item.mediaSourceId = "source";
    item.positionTicks = 123456789;
    PlaybackProfilePlan plan;
    plan.videoCodecs = {"hevc", "h264", "av1", "vp9", "vp8", "mpeg4"};
    plan.audioCodecs = {"aac", "mp3", "mp2", "pcm_s16le", "ac3", "eac3", "dts", "truehd", "flac", "opus", "vorbis"};
    plan.transcodeAudioCodecs = {"aac", "mp3", "ac3", "eac3"};
    plan.maxAudioChannels = 8;
    plan.allowAudioStreamCopy = true;

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        checksum += buildPlaybackInfoRequestBody(session, item, plan, {}, 120000000, 8, 1, 2).size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
