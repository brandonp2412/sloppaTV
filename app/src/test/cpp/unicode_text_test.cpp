#include "unicode_text.hpp"

#include <cassert>
#include <string>

int main() {
    assert(displayText("plain ASCII") == "plain ASCII");
    assert(displayText("caf\xC3\xA9") == "cafe");
    assert(displayText("cafe\xCC\x81") == "cafe");
    assert(displayText("\xE2\x80\x9CHello\xE2\x80\x9D") == "\"Hello\"");
    assert(displayText("wait\xE2\x80\xA6") == "wait...");
    assert(displayText("left\xE2\x86\x92right") == "left>right");
    assert(displayText("A\xE2\x80\x8B" "B") == "AB");
    assert(displayText("A\xE2\x80\x8E" "B") == "AB");
    assert(displayText("A\xEF\xBB\xBF" "B") == "AB");
    assert(displayText("A\xC2\xA0" "B") == "A B");
    assert(displayText("x\xE2\x89\xA4y") == "x<=y");
    assert(displayText("\xE2\x99\xAA") == "~");
    assert(displayText("\xE3\x81\x82") == "?");
    assert(displayText("\xE3\x81\x82", '\0').empty());
    return 0;
}
