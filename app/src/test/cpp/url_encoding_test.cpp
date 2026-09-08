#include "url_encoding.hpp"

#include <cassert>

int main() {
    assert(urlEncode("") == "");
    assert(urlEncode("abcXYZ012-_.~") == "abcXYZ012-_.~");
    assert(urlEncode("hello world") == "hello%20world");
    assert(urlEncode("a/b?c=d&e") == "a%2Fb%3Fc%3Dd%26e");
    assert(urlEncode("caf\xC3\xA9") == "caf%C3%A9");
    assert(urlEncode("100%") == "100%25");
    return 0;
}
