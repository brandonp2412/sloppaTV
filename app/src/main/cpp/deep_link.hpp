#pragma once

#include <cctype>
#include <string>

inline std::string trimExternalText(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline std::string normalizeJellyfinItemId(std::string value) {
    value = trimExternalText(std::move(value));
    if (value.empty()) return {};

    std::string compact;
    compact.reserve(32);
    for (const unsigned char c : value) {
        if (c == '-') continue;
        if (!std::isxdigit(c)) return {};
        compact.push_back(static_cast<char>(std::tolower(c)));
    }
    return compact.size() == 32 ? compact : std::string{};
}

inline std::string normalizeExternalSearchQuery(std::string value) {
    value = trimExternalText(std::move(value));
    if (value.empty()) return {};
    for (char& c : value) {
        if (static_cast<unsigned char>(c) < 0x20) c = ' ';
    }
    if (value.size() > 120) value.resize(120);
    return trimExternalText(std::move(value));
}
