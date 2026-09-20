#pragma once

#include <optional>
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
