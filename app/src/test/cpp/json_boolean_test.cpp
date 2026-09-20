#include "json_boolean.hpp"

#include <nlohmann/json.hpp>

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

    const nlohmann::json values = {
        {"enabled", true},      {"disabled", false}, {"stringTrue", "true"}, {"stringFalse", "false"},
        {"nullValue", nullptr}, {"numberValue", 1},  {"badString", "yes"},
    };
    assert(jsonBooleanValue(values, "enabled", false));
    assert(!jsonBooleanValue(values, "disabled", true));
    assert(jsonBooleanValue(values, "stringTrue", false));
    assert(!jsonBooleanValue(values, "stringFalse", true));
    assert(jsonBooleanValue(values, "nullValue", true));
    assert(!jsonBooleanValue(values, "numberValue", false));
    assert(jsonBooleanValue(values, "badString", true));
    assert(jsonBooleanValue(values, "missing", true));
    return 0;
}
