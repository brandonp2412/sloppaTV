#pragma once

#include <optional>
#include <string>
#include <string_view>

inline std::optional<bool> parseJsonBoolean(std::string_view value) {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' || value.front() == '\n' || value.front() == '\r')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' || value.back() == '\n' || value.back() == '\r')) {
        value.remove_suffix(1);
    }
    if (value == "true") return true;
    if (value == "false") return false;
    return std::nullopt;
}

template <typename Json> bool jsonBooleanValue(const Json& object, const char* key, bool fallback = false) {
    const auto value = object.find(key);
    if (value == object.end() || value->is_null()) return fallback;
    if (value->is_boolean()) return value->template get<bool>();
    if (value->is_string()) {
        const auto parsed = parseJsonBoolean(value->template get<std::string>());
        if (parsed.has_value()) return *parsed;
    }
    return fallback;
}
