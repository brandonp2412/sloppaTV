#pragma once

#include <cstdint>
#include <string>
#include <string_view>

inline uint32_t nextUtf8CodePoint(std::string_view text, size_t& index) {
    const auto first = static_cast<unsigned char>(text[index++]);
    if (first < 0x80) return first;
    auto continuation = [&](int count, uint32_t value) -> uint32_t {
        for (int offset = 0; offset < count; ++offset) {
            if (index >= text.size()) return '?';
            const auto byte = static_cast<unsigned char>(text[index]);
            if ((byte & 0xC0u) != 0x80u) return '?';
            ++index;
            value = (value << 6u) | (byte & 0x3Fu);
        }
        return value;
    };
    if ((first & 0xE0u) == 0xC0u) return continuation(1, first & 0x1Fu);
    if ((first & 0xF0u) == 0xE0u) return continuation(2, first & 0x0Fu);
    if ((first & 0xF8u) == 0xF0u) return continuation(3, first & 0x07u);
    return '?';
}

inline void appendDisplayCodePoint(std::string& output, uint32_t codePoint) {
    if (codePoint < 0x80u) {
        output.push_back(static_cast<char>(codePoint));
        return;
    }
    if ((codePoint >= 0x0300 && codePoint <= 0x036F)
        || (codePoint >= 0x1AB0 && codePoint <= 0x1AFF)
        || (codePoint >= 0x1DC0 && codePoint <= 0x1DFF)
        || (codePoint >= 0xFE00 && codePoint <= 0xFE0F)
        || (codePoint >= 0xFE20 && codePoint <= 0xFE2F)) {
        return;
    }
    if (codePoint == 0x00AD
        || (codePoint >= 0x200B && codePoint <= 0x200F)
        || (codePoint >= 0x202A && codePoint <= 0x202E)
        || codePoint == 0x2060
        || (codePoint >= 0x2066 && codePoint <= 0x2069)
        || codePoint == 0xFEFF) {
        return;
    }
    if (codePoint == 0x00A0
        || (codePoint >= 0x2000 && codePoint <= 0x200A)
        || codePoint == 0x202F
        || codePoint == 0x205F
        || codePoint == 0x3000) {
        output.push_back(' ');
        return;
    }
    switch (codePoint) {
        case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5: output.push_back('A'); return;
        case 0x00C6: output += "AE"; return;
        case 0x00C7: output.push_back('C'); return;
        case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: output.push_back('E'); return;
        case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: output.push_back('I'); return;
        case 0x00D0: output.push_back('D'); return;
        case 0x00D1: output.push_back('N'); return;
        case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8: output.push_back('O'); return;
        case 0x00D7: output.push_back('x'); return;
        case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: output.push_back('U'); return;
        case 0x00DD: output.push_back('Y'); return;
        case 0x00DF: output += "ss"; return;
        case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5: output.push_back('a'); return;
        case 0x00E6: output += "ae"; return;
        case 0x00E7: output.push_back('c'); return;
        case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: output.push_back('e'); return;
        case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: output.push_back('i'); return;
        case 0x00F0: output.push_back('d'); return;
        case 0x00F1: output.push_back('n'); return;
        case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8: output.push_back('o'); return;
        case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: output.push_back('u'); return;
        case 0x00FD: case 0x00FF: output.push_back('y'); return;
        case 0x0152: output += "OE"; return;
        case 0x0153: output += "oe"; return;
        case 0x2018: case 0x2019: case 0x201A: case 0x2032: output.push_back('\''); return;
        case 0x201C: case 0x201D: case 0x201E: case 0x2033: output.push_back('"'); return;
        case 0x2013: case 0x2014: case 0x2212: output.push_back('-'); return;
        case 0x2026: output += "..."; return;
        case 0x2022: case 0x00B7: output.push_back('*'); return;
        case 0x2190: output.push_back('<'); return;
        case 0x2192: output.push_back('>'); return;
        case 0x2260: output += "!="; return;
        case 0x2264: output += "<="; return;
        case 0x2265: output += ">="; return;
        case 0x266A: case 0x266B: output.push_back('~'); return;
        default: output.push_back('?'); return;
    }
}

inline std::string displayText(std::string_view text) {
    std::string output;
    output.reserve(text.size());
    size_t index = 0;
    while (index < text.size()) appendDisplayCodePoint(output, nextUtf8CodePoint(text, index));
    return output;
}
