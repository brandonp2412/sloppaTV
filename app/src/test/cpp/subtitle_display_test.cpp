#include "subtitle_display.hpp"

#include <cassert>

int main() {
    assert(normalizeSubtitleDisplayText("plain subtitle") == "plain subtitle");
    assert(normalizeSubtitleDisplayText("  multiple   spaces\tand\nlines  ") == "multiple spaces and lines");
    assert(normalizeSubtitleDisplayText("Wait ... what ? Really !") == "Wait... what? Really!");
    assert(normalizeSubtitleDisplayText("Hello , world ! ( test )") == "Hello, world! ( test)");
    assert(normalizeSubtitleDisplayText("caf\xC3\xA9 \xE2\x80\x94 deja vu\xE2\x80\xA6") == "cafe - deja vu...");
    assert(normalizeSubtitleDisplayText("A\xE3\x81\x82 B") == "A B");
    return 0;
}
