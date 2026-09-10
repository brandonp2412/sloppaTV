#include "jellyfin_media_segment_parser.hpp"

#include <cassert>

int main() {
    auto result = parseJellyfinMediaSegments(R"({
        "Items": [
            {"Type":"Outro","StartTicks":900000000,"EndTicks":960000000},
            null,
            "noise",
            {"Type":"Intro","StartTicks":100000000,"EndTicks":180000000},
            {"Type":"BrokenNull","StartTicks":null,"EndTicks":200000000},
            {"Type":"BrokenType","StartTicks":"soon","EndTicks":300000000},
            {"Type":17,"StartTicks":400000000,"EndTicks":500000000},
            {"Type":"Negative","StartTicks":-1,"EndTicks":500000000},
            {"Type":"Backwards","StartTicks":600000000,"EndTicks":500000000}
        ]
    })");
    assert(result.ok);
    assert(result.value.size() == 2);
    assert(result.value[0].type == "Intro");
    assert(result.value[0].startTicks == 100000000);
    assert(result.value[1].type == "Outro");

    result = parseJellyfinMediaSegments(R"({"Items":null})");
    assert(!result.ok);
    assert(!result.error.empty());

    result = parseJellyfinMediaSegments("not-json");
    assert(!result.ok);
    assert(!result.error.empty());
    return 0;
}
