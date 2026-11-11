// Manual host diagnostic: build with subtitle_audio_probe.cpp and libfvad, then run
// with a locally held media file and its subtitle file. No media is bundled.
#include "subtitle_audio_probe.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: subtitle_sync_probe <media-url> <subtitle.srt> <work-directory> [start-ms]\n";
        return 1;
    }
    std::ifstream input(argv[2]);
    const std::string subtitle((std::istreambuf_iterator<char>(input)), {});
    SubtitleAudioSyncRequest request;
    request.mediaUrl = argv[1];
    request.privateFilesDir = argv[3];
    request.cues = parseTextSubtitleCues(subtitle, "srt");
    request.sampleStartMs = argc >= 5 ? std::stoi(argv[4]) : 0;
    const auto result = synchronizeSubtitleAudio(request);
    std::cout << "cues=" << request.cues.size() << " audioMs=" << result.audioMs
              << " confident=" << result.decision.confident << " offsetMs=" << result.decision.offsetMs
              << " reason=" << result.failure << '\n';
    return result.decision.confident ? 0 : 2;
}
