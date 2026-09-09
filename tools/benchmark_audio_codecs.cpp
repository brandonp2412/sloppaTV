#include "audio_policy.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000000;
    AudioCodecCapabilities caps;
    caps.aac = caps.mp3 = caps.mp2 = caps.pcm = true;
    caps.ac3 = caps.eac3 = caps.dts = caps.truehd = true;
    caps.flac = caps.opus = caps.vorbis = true;

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto advertised = advertisedAudioCodecs(caps, 8);
        const auto transcode = transcodingAudioCodecs(caps, 8);
        checksum += advertised.size() + transcode.size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
