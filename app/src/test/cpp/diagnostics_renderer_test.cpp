#include "diagnostics_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float, int) {
        panels.push_back({x, y, width, height});
    }

    void roundedOutline(float x, float y, float width, float height, float, float, int) {
        outlines.push_back({x, y, width, height});
    }

    void rect(float x, float y, float width, float height, int) { dividers.push_back({x, y, width, height}); }

    std::vector<RectCall> panels;
    std::vector<RectCall> outlines;
    std::vector<RectCall> dividers;
};

bool has(const std::vector<std::string>& values, std::string_view expected) {
    for (const auto& value : values)
        if (value == expected) return true;
    return false;
}
} // namespace

int main() {
    DiagnosticsScreenData data;
    data.appVersion = "1.2.3";
    data.architecture = "ARM64";
    data.sessionServer = "https://jellyfin.example";
    data.serverName = "Living Room";
    data.serverVersion = "10.10.7";
    data.videoCodecs = {"h264", "hevc"};
    data.audioCodecs = {"aac", "ac3"};
    data.maxAudioOutputChannels = 8;
    data.maxHevcWidth = 3840;
    data.maxHevcHeight = 2160;
    data.hdrFormats = {"HDR10", "HLG"};
    data.lastPlaybackSummary = "DirectPlay • HEVC";

    const DiagnosticsRenderStyle<int> style{
        .cornerLarge = 28.0f,
        .panelAlt = 1,
        .outline = 2,
        .divider = 3,
        .tertiary = 4,
        .text = 5,
        .muted = 6,
    };

    FakeRenderer renderer;
    std::vector<std::string> headers;
    std::vector<std::string> chips;
    std::vector<std::string> leftAligned;
    std::vector<std::string> centered;

    renderDiagnosticsScreen(
        renderer, data, style, [&](std::string_view title) { headers.emplace_back(title); },
        [&](float, float, std::string_view label, bool, float, float, float) {
            chips.emplace_back(label);
            return 100.0f;
        },
        [&](float, float, float, float, float, std::string_view value, int) { leftAligned.emplace_back(value); },
        [&](float, float, float, float, float, std::string_view value, int, float, float) {
            centered.emplace_back(value);
        });

    assert(headers.size() == 1);
    assert(headers.front() == "Diagnostics");
    assert(renderer.panels.size() == 4);
    assert(renderer.outlines.size() == 4);
    assert(renderer.dividers.size() == 6);
    assert(renderer.panels[0].x == 85.0f && renderer.panels[0].y == 175.0f);
    assert(renderer.panels[1].x == 995.0f && renderer.panels[1].y == 175.0f);
    assert(renderer.panels[2].x == 85.0f && renderer.panels[2].y == 515.0f);
    assert(renderer.panels[3].x == 995.0f && renderer.panels[3].y == 515.0f);
    assert(chips.size() == 4);
    assert(chips[0] == "App & server");
    assert(chips[1] == "Video & display");
    assert(chips[2] == "Audio");
    assert(chips[3] == "Last playback");
    assert(leftAligned.size() == 20);
    assert(has(leftAligned, "App version"));
    assert(has(leftAligned, "1.2.3"));
    assert(has(leftAligned, "HDR display"));
    assert(has(leftAligned, "HDR10 / HLG"));
    assert(has(leftAligned, "Last playback"));
    assert(has(leftAligned, "DirectPlay • HEVC"));
    assert(centered.size() == 1);
    assert(centered.front() == "Back or OK returns to settings");

    return 0;
}
