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
    assert(displayText("A\xE2\x80\x8B"
                       "B") == "AB");
    assert(displayText("A\xE2\x80\x8E"
                       "B") == "AB");
    assert(displayText("A\xEF\xBB\xBF"
                       "B") == "AB");
    assert(displayText("A\xC2\xA0"
                       "B") == "A B");
    assert(displayText("x\xE2\x89\xA4y") == "x<=y");
    assert(displayText("\xE2\x99\xAA") == "~");
    assert(displayText("\xE3\x81\x82") == "?");
    assert(displayText("\xE3\x81\x82", '\0').empty());
    assert(displayText("\xC0\xAF") == "??");
    assert(displayText("\xED\xA0\x80") == "???");
    assert(displayText("\xF4\x90\x80\x80") == "????");

    size_t emptyIndex = 0;
    assert(nextUtf8CodePoint("", emptyIndex) == 0xFFFDu);
    assert(emptyIndex == 0);

    size_t endIndex = 1;
    assert(nextUtf8CodePoint("A", endIndex) == 0xFFFDu);
    assert(endIndex == 1);

    const std::string utf8 = "caf\xC3\xA9 \xE6\x98\xA0\xE7\x94\xBB \xF0\x9F\x93\xBA";
    const std::u16string utf16 = u"café 映画 📺";
    assert(utf8ToUtf16(utf8) == utf16);
    assert(utf16ToUtf8(utf16) == utf8);

    const std::string withNull{"A\0B", 3};
    const std::u16string withNull16{u'A', u'\0', u'B'};
    assert(utf8ToUtf16(withNull) == withNull16);
    assert(utf16ToUtf8(withNull16) == withNull);

    assert(utf8ToUtf16("\xC0\xAF") == u"\uFFFD\uFFFD");
    assert(utf8ToUtf16("\xED\xA0\x80") == u"\uFFFD\uFFFD\uFFFD");
    assert(utf8ToUtf16("\xF4\x90\x80\x80") == u"\uFFFD\uFFFD\uFFFD\uFFFD");
    assert(utf16ToUtf8(std::u16string{static_cast<char16_t>(0xD800)}) == "\xEF\xBF\xBD");
    assert(utf16ToUtf8(std::u16string{static_cast<char16_t>(0xDC00)}) == "\xEF\xBF\xBD");
    return 0;
}
