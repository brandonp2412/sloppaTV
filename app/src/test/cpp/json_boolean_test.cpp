#include "json_boolean.hpp"

#include <cassert>

int main() {
    assert(parseJsonBoolean("true") == true);
    assert(parseJsonBoolean("false") == false);
    assert(parseJsonBoolean(" \t\r\ntrue\n") == true);
    assert(parseJsonBoolean("\n false \t") == false);

    assert(!parseJsonBoolean("").has_value());
    assert(!parseJsonBoolean("null").has_value());
    assert(!parseJsonBoolean("\"true\"").has_value());
    assert(!parseJsonBoolean("true false").has_value());
    assert(!parseJsonBoolean("{\"enabled\":true}").has_value());
    assert(!parseJsonBoolean("not true").has_value());
    return 0;
}
