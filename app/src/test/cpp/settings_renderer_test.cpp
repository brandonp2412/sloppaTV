#include "settings_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeRenderer {
    void text(float, float, float, std::string_view value, int, float) { texts.emplace_back(value); }

    void textVerticallyCentered(float, float, float, float, std::string_view value, int, float) {
        verticalTexts.emplace_back(value);
    }

    float textWidth(float scale, std::string_view value) const {
        return scale * static_cast<float>(value.size()) * 5.0f;
    }

    void roundedRect(float, float, float, float, float, int) { ++roundedRects; }

    void roundedOutline(float, float, float, float, float, float, int) { ++roundedOutlines; }

    void rect(float, float, float, float, int) { ++rects; }

    std::vector<std::string> texts;
    std::vector<std::string> verticalTexts;
    int roundedRects = 0;
    int roundedOutlines = 0;
    int rects = 0;
};

bool has(const std::vector<std::string>& values, std::string_view expected) {
    for (const auto& value : values)
        if (value == expected) return true;
    return false;
}

struct Harness {
    FakeRenderer renderer;
    int inputSurfaces = 0;
    int listSurfaces = 0;
    int emptyStates = 0;
    int switches = 0;
    int modals = 0;
    std::vector<std::string> centered;
    std::vector<std::string> chips;

    void render(const SettingsScreenState& screen, const AppSettings& settings, bool systemInput = false,
                bool seerrApiKeyTyping = false) {
        const SettingsRenderStyle<int> style{
            .headlineScale = 3.0f,
            .cornerMedium = 20.0f,
            .cornerSmall = 12.0f,
            .wideInputFocusScale = 1.04f,
            .wideListItemFocusScale = 1.04f,
            .listItemFocusScale = 1.02f,
            .text = 1,
            .secondaryText = 2,
            .muted = 3,
            .focus = 4,
            .focusSoft = 5,
            .panelAlt = 6,
            .outline = 7,
            .scrim = 8,
        };
        renderSettingsScreen(
            renderer,
            SettingsRenderState{
                .screen = screen,
                .settings = settings,
                .maxAudioOutputChannels = 6,
                .externalPlayer = "MPV",
                .username = "viewer",
                .systemSettingsInputActive = systemInput,
                .seerrApiKeyTyping = seerrApiKeyTyping,
            },
            style,
            [&](float x, float y, float width, float height, bool, float) {
                ++inputSurfaces;
                return std::array<float, 4>{x, y, width, height};
            },
            [&](float, float, float, float, float, std::string_view value, int, float, float) {
                centered.emplace_back(value);
            },
            [](std::string_view value, float, float, int) { return std::string(value); },
            [&](std::string_view, std::string_view) { ++emptyStates; },
            [&](float x, float y, float width, float height, bool, float, float) {
                ++listSurfaces;
                return std::array<float, 4>{x, y, width, height};
            },
            [](std::string_view value) { return std::string(value); }, [&](float, float, bool, bool) { ++switches; },
            [&](float, float, std::string_view label, bool, float, float, float) {
                chips.emplace_back(label);
                return 100.0f;
            },
            [&](float, float, float, float) { ++modals; });
    }
};
} // namespace

int main() {
    AppSettings settings;
    SettingsScreenState screen;
    screen.reset();

    Harness common;
    common.render(screen, settings);
    assert(common.inputSurfaces == 1);
    assert(common.listSurfaces == 6);
    assert(common.emptyStates == 0);
    assert(common.switches == 1);
    assert(has(common.renderer.texts, "Settings"));
    assert(has(common.renderer.texts, "Common settings"));
    assert(has(common.renderer.verticalTexts, "UI TEXT SIZE"));
    assert(has(common.renderer.verticalTexts, "ALL LANGUAGES"));
    assert(has(common.centered, "Left / Right changes   |   OK opens options   |   Up searches"));

    screen.setSearchText("definitely unmatched");
    Harness filtering;
    filtering.render(screen, settings, true);
    assert(filtering.inputSurfaces == 1);
    assert(filtering.listSurfaces == 0);
    assert(filtering.emptyStates == 0);
    assert(has(filtering.centered, "Type to filter settings"));

    screen.reset();
    screen.openSubtitleLanguagePicker();
    settings.subtitleLanguages = {"eng"};
    Harness picker;
    picker.render(screen, settings);
    assert(picker.renderer.rects == 1);
    assert(picker.modals == 1);
    assert(picker.listSurfaces == 14);
    assert(has(picker.renderer.texts, "Subtitle languages"));
    assert(has(picker.renderer.verticalTexts, "All languages"));
    assert(has(picker.renderer.verticalTexts, "ENGLISH"));
    assert(picker.chips.size() >= 8);
    assert(picker.chips[0] == "OFF");
    assert(picker.chips[1] == "ON");
    assert(has(picker.centered, "OK toggles selection   |   Back returns to settings"));

    return 0;
}
