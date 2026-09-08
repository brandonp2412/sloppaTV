#include "text_fit.hpp"

#include <cassert>
#include <string_view>

static float measure(std::string_view value) {
    float width = 0.0f;
    for (unsigned char c : value) width += c == ' ' ? 4.0f : 8.0f;
    return width;
}

int main() {
    assert(fitTextLinesMeasured("one two three", 200.0f, 2, measure) == "one two three");
    assert(fitTextLinesMeasured("one two three", 60.0f, 2, measure) == "one two\nthree");
    assert(fitTextLinesMeasured("one two three four", 60.0f, 1, measure) == "one t...");
    assert(fitTextLinesMeasured("", 60.0f, 2, measure).empty());
    return 0;
}
