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
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (const unsigned char byte : value) {
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
