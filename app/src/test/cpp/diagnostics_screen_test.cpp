#include "diagnostics_screen.hpp"

#include <cassert>

int main() {
    assert(handleDiagnosticsScreenInput(DiagnosticsScreenInput::None).type ==
           DiagnosticsScreenCommandType::None);
    assert(handleDiagnosticsScreenInput(DiagnosticsScreenInput::Back).type ==
           DiagnosticsScreenCommandType::Exit);
    assert(handleDiagnosticsScreenInput(DiagnosticsScreenInput::Activate).type ==
           DiagnosticsScreenCommandType::Exit);

    DiagnosticsScreenData data;
    data.appVersion = "1.2.3";
    data.architecture = "ARM64";
    data.sessionServer = "https://jellyfin.example";
    data.serverLoading = true;
    data.videoCodecs = {"h264", "hevc"};
    data.audioCodecs = {"aac", "ac3"};
    data.maxAudioOutputChannels = 8;
    data.maxHevcWidth = 3840;
    data.maxHevcHeight = 2160;
    data.hdrFormats = {"HDR10", "HLG"};

    auto rows = diagnosticsRows(data);
    assert(rows.size() == 10);
    assert(rows[0].second == "1.2.3");
    assert(rows[1].second == "ARM64");
    assert(rows[2].second == "https://jellyfin.example");
    assert(rows[3].second == "Loading…");
    assert(rows[4].second == "h264 / hevc");
    assert(rows[5].second == "aac / ac3");
    assert(rows[6].second == "8 CHANNELS");
    assert(rows[7].second == "3840X2160");
    assert(rows[8].second == "HDR10 / HLG");
    assert(rows[9].second == "NOT YET PLAYED THIS SESSION");

    data.serverName = "Living Room";
    data.serverVersion = "10.10.7";
    data.serverLoading = false;
    data.videoCodecs.clear();
    data.audioCodecs.clear();
    data.maxHevcWidth = 0;
    data.hdrFormats.clear();
    data.lastPlaybackSummary = "DirectPlay • HEVC";

    rows = diagnosticsRows(data);
    assert(rows[2].second == "Living Room");
    assert(rows[3].second == "10.10.7");
    assert(rows[4].second == "NONE DETECTED");
    assert(rows[5].second == "AAC TRANSCODE FALLBACK");
    assert(rows[7].second == "UNKNOWN");
    assert(rows[8].second == "SDR / NONE DETECTED");
    assert(rows[9].second == "DirectPlay • HEVC");
    return 0;
}
