#include "seerr_drive_picker_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RoundedRectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    int color = 0;
};

struct TextCall {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float maxWidth = 0.0f;
    int color = 0;
};

struct ListItemCall {
    float x = 0.0f;
    float y = 0.0f;
    bool focused = false;
    float radius = 0.0f;
    float focusScale = 0.0f;
};

struct CenteredCall {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float, float, int color) {
        roundedRects.push_back({x, y, width, color});
    }

    void text(float x, float y, float, std::string_view value, int color, float maxWidth = 0.0f) {
        textCalls.push_back({std::string(value), x, y, maxWidth, color});
    }

    void textVerticallyCentered(float x, float y, float, float, std::string_view value, int color,
                                float maxWidth = 0.0f) {
        textCalls.push_back({std::string(value), x, y, maxWidth, color});
    }

    std::vector<RoundedRectCall> roundedRects;
    std::vector<TextCall> textCalls;
};
} // namespace

int main() {
    const SeerrDrivePickerRenderStyle<int> style{
        .headlineScale = 3.8f,
        .cornerMedium = 24.0f,
        .focusScale = 1.04f,
        .text = 1,
        .muted = 2,
        .secondaryText = 3,
        .focus = 4,
        .focusSoft = 5,
        .panelElevated = 6,
        .error = 7,
    };

    SeerrDrivePickerViewModel model;
    model.subtitle = "Choose storage for Example";
    model.rows = {
        {
            .row =
                {
                    .name = "media · movies",
                    .secondaryText = "420 / 1000 GB",
                    .statusText = "42% full",
                    .usedPercent = 42,
                    .hasCapacity = true,
                    .nearFull = false,
                },
            .focused = true,
        },
        {
            .row =
                {
                    .name = "media · shows",
                    .secondaryText = "950 / 1000 GB",
                    .statusText = "95% full",
                    .usedPercent = 95,
                    .hasCapacity = true,
                    .nearFull = true,
                },
            .focused = false,
        },
        {
            .row =
                {
                    .name = "archive",
                    .secondaryText = "/mnt/archive",
                    .statusText = "Free space unknown",
                    .usedPercent = 0,
                    .hasCapacity = false,
                    .nearFull = false,
                },
            .focused = false,
        },
    };

    FakeRenderer renderer;
    std::vector<ListItemCall> listItems;
    std::vector<CenteredCall> centered;
    int emptyCalls = 0;

    renderSeerrDrivePickerScreen(
        renderer, model, style,
        [&](float x, float y, float, float, bool focused, float radius, float focusScale) {
            listItems.push_back({x, y, focused, radius, focusScale});
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [&](float x, float y, float, float, float, std::string_view value, int color, float, float) {
            centered.push_back({std::string(value), x, y, color});
        },
        [&](const std::string&, const std::string&) { ++emptyCalls; });

    assert(emptyCalls == 0);
    assert(listItems.size() == 3);
    assert(listItems[0].x == 120.0f);
    assert(listItems[0].y == 220.0f);
    assert(listItems[0].focused);
    assert(listItems[0].radius == 24.0f);
    assert(listItems[0].focusScale == 1.04f);
    assert(listItems[1].y == 365.0f);
    assert(!listItems[1].focused);
    assert(listItems[2].y == 510.0f);

    assert(renderer.textCalls.size() == 8);
    assert(renderer.textCalls[0].text == "Choose storage");
    assert(renderer.textCalls[0].x == 80.0f);
    assert(renderer.textCalls[0].y == 56.0f);
    assert(renderer.textCalls[0].color == style.text);
    assert(renderer.textCalls[1].text == model.subtitle);
    assert(renderer.textCalls[1].maxWidth == 1450.0f);
    assert(renderer.textCalls[2].text == "media · movies");
    assert(renderer.textCalls[2].color == style.text);
    assert(renderer.textCalls[3].text == "420 / 1000 GB");
    assert(renderer.textCalls[4].text == "media · shows");
    assert(renderer.textCalls[4].color == style.secondaryText);
    assert(renderer.textCalls[6].text == "archive");
    assert(renderer.textCalls[7].text == "/mnt/archive");
    assert(renderer.textCalls[7].maxWidth == 920.0f);

    assert(renderer.roundedRects.size() == 13);
    assert(renderer.roundedRects[0].color == style.focusSoft);
    assert(renderer.roundedRects[1].color == style.focus);
    assert(renderer.roundedRects[2].color == style.focus);
    assert(renderer.roundedRects[3].x == 1085.0f);
    assert(renderer.roundedRects[3].width == 500.0f);
    assert(renderer.roundedRects[4].width == 210.0f);
    assert(renderer.roundedRects[4].color == style.focus);
    assert(renderer.roundedRects[8].width == 500.0f);
    assert(renderer.roundedRects[8].color == style.panelElevated);
    assert(renderer.roundedRects[9].width == 475.0f);
    assert(renderer.roundedRects[9].color == style.error);

    assert(centered.size() == 4);
    assert(centered[0].text == "42% full");
    assert(centered[0].color == style.text);
    assert(centered[1].text == "95% full");
    assert(centered[1].color == style.error);
    assert(centered[2].text == "Free space unknown");
    assert(centered[2].color == style.secondaryText);
    assert(centered[3].text == "Up / Down selects   ·   OK continues   ·   Back cancels");
    assert(centered[3].x == 590.0f);
    assert(centered[3].y == 970.0f);
    assert(centered[3].color == style.muted);

    SeerrDrivePickerViewModel emptyModel;
    emptyModel.subtitle = "Choose where Seerr should place this request";
    FakeRenderer emptyRenderer;
    int renderedEmpty = 0;
    renderSeerrDrivePickerScreen(
        emptyRenderer, emptyModel, style, [](float, float, float, float, bool, float, float) {},
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [&](const std::string& title, const std::string& message) {
            ++renderedEmpty;
            assert(title == "No storage targets");
            assert(message == "Back returns to search.");
        });

    assert(renderedEmpty == 1);
    assert(emptyRenderer.textCalls.size() == 2);
    assert(emptyRenderer.roundedRects.empty());

    return 0;
}
