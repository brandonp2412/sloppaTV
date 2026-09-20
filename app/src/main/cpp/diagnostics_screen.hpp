#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

enum class DiagnosticsScreenInput {
    None,
    Back,
    Activate,
};

enum class DiagnosticsScreenCommandType {
    None,
    Exit,
};

struct DiagnosticsScreenCommand {
    DiagnosticsScreenCommandType type = DiagnosticsScreenCommandType::None;
};

struct DiagnosticsScreenData {
    std::string appVersion;
    std::string architecture;
    std::string sessionServer;
    std::string serverName;
    std::string serverVersion;
    bool serverLoading = false;
    std::vector<std::string> videoCodecs;
    std::vector<std::string> audioCodecs;
    int maxAudioOutputChannels = 0;
    int maxHevcWidth = 0;
    int maxHevcHeight = 0;
    std::vector<std::string> hdrFormats;
    std::string lastPlaybackSummary;
};

inline DiagnosticsScreenCommand handleDiagnosticsScreenInput(DiagnosticsScreenInput input) {
    if (input == DiagnosticsScreenInput::Back || input == DiagnosticsScreenInput::Activate)
        return {.type = DiagnosticsScreenCommandType::Exit};
    return {};
}

inline std::string joinDiagnosticsValues(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) result += " / ";
        result += value;
    }
    return result;
}

inline std::vector<std::pair<std::string, std::string>> diagnosticsRows(const DiagnosticsScreenData& data) {
    return {
        {"App version", data.appVersion},
        {"ABI", data.architecture},
        {"Server", data.serverName.empty() ? data.sessionServer : data.serverName},
        {"Jellyfin version",
         data.serverVersion.empty() ? (data.serverLoading ? "Loading…" : "UNKNOWN") : data.serverVersion},
        {"Video decoders", data.videoCodecs.empty() ? "NONE DETECTED" : joinDiagnosticsValues(data.videoCodecs)},
        {"Direct audio", data.audioCodecs.empty() ? "AAC TRANSCODE FALLBACK" : joinDiagnosticsValues(data.audioCodecs)},
        {"Audio output", std::to_string(data.maxAudioOutputChannels) + " CHANNELS"},
        {"HEVC maximum", data.maxHevcWidth > 0
                             ? std::to_string(data.maxHevcWidth) + "X" + std::to_string(data.maxHevcHeight)
                             : "UNKNOWN"},
        {"HDR display", data.hdrFormats.empty() ? "SDR / NONE DETECTED" : joinDiagnosticsValues(data.hdrFormats)},
        {"Last playback", data.lastPlaybackSummary.empty() ? "NOT YET PLAYED THIS SESSION" : data.lastPlaybackSummary},
    };
}
