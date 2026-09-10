#include "unicode_text.hpp"

#include <cassert>
#include <string>

int main() {
    assert(utf8PrefixLength("plain", 3) == 3);
    assert(utf8PrefixLength("plain", 10) == 5);
    assert(utf8PrefixLength("A\xC4\x81", 2) == 1);
    assert(utf8PrefixLength("A\xC4\x81", 3) == 3);
    assert(truncateUtf8Bytes("A\xC4\x81", 2) == "A");
    assert(truncateUtf8Bytes("A\xC4\x81", 3) == "A\xC4\x81");

    std::string erase = "M\xC4\x81";
    assert(eraseLastUtf8CodePoint(erase));
    assert(erase == "M");
    assert(eraseLastUtf8CodePoint(erase));
    assert(erase.empty());
    assert(!eraseLastUtf8CodePoint(erase));

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
