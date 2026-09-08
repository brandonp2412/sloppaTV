#pragma once

#include <string>
#include <string_view>

inline bool urlUnreserved(unsigned char value) {
    return (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z')
        || (value >= '0' && value <= '9')
        || value == '-'
        || value == '_'
        || value == '.'
        || value == '~';
}

inline std::string urlEncode(std::string_view value) {
    size_t firstEscaped = 0;
    while (firstEscaped < value.size() && urlUnreserved(static_cast<unsigned char>(value[firstEscaped]))) {
        ++firstEscaped;
    }
    if (firstEscaped == value.size()) return std::string(value);

    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    encoded.append(value.substr(0, firstEscaped));
    for (size_t index = firstEscaped; index < value.size(); ++index) {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if (urlUnreserved(byte)) {
            encoded.push_back(static_cast<char>(byte));
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hex[byte >> 4]);
        encoded.push_back(hex[byte & 0x0F]);
    }
    return encoded;
}
