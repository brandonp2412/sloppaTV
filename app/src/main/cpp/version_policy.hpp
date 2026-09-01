#pragma once

#include <array>
#include <charconv>
#include <string_view>

enum class ServerCompatibility {
    Supported,
    TooOld,
    Unknown,
};

inline std::array<int, 3> parseVersionTriplet(std::string_view value, bool& valid) {
    std::array<int, 3> parts{0, 0, 0};
    valid = true;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (value.empty()) {
            valid = false;
            return parts;
        }
        const std::size_t separator = value.find('.');
        const std::string_view part = value.substr(0, separator);
        if (part.empty()) {
            valid = false;
            return parts;
        }
        const char* begin = part.data();
        const char* end = part.data() + part.size();
        const auto [ptr, error] = std::from_chars(begin, end, parts[index]);
        if (error != std::errc{} || ptr != end || parts[index] < 0) {
            valid = false;
            return parts;
        }
        if (separator == std::string_view::npos) {
            if (index != parts.size() - 1) valid = false;
            return parts;
        }
        value.remove_prefix(separator + 1);
    }
    return parts;
}

inline ServerCompatibility jellyfinServerCompatibility(std::string_view version) {
    bool valid = false;
    const auto actual = parseVersionTriplet(version, valid);
    if (!valid) return ServerCompatibility::Unknown;
    constexpr std::array<int, 3> minimum{10, 10, 0};
    return actual < minimum ? ServerCompatibility::TooOld : ServerCompatibility::Supported;
}
