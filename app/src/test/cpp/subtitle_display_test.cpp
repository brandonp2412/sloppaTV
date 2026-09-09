#include "subtitle_display.hpp"

#include <cassert>
#include <vector>

int main() {
    assert(normalizeSubtitleDisplayText("plain subtitle") == "plain subtitle");
    assert(normalizeSubtitleDisplayText("  multiple   spaces\tand\nlines  ") == "multiple spaces and lines");
    assert(normalizeSubtitleDisplayText("Wait ... what ? Really !") == "Wait... what? Really!");
    assert(normalizeSubtitleDisplayText("Hello , world ! ( test )") == "Hello, world! ( test)");
    assert(normalizeSubtitleDisplayText("caf\xC3\xA9 \xE2\x80\x94 deja vu\xE2\x80\xA6") == "cafe - deja vu...");
    assert(normalizeSubtitleDisplayText("A\xE3\x81\x82 B") == "A B");

    std::vector<std::string> lines;
    splitSubtitleDisplayLines("first\nsecond\n\nthird", lines);
    assert((lines == std::vector<std::string>{"first", "second", "third"}));
    splitSubtitleDisplayLines("", lines);
    assert((lines == std::vector<std::string>{""}));
    return 0;
}
