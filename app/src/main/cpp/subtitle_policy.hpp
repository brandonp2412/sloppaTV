#pragma once

#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr int kSubtitleServerDefaultIndex = -2;
constexpr int kSubtitleOffIndex = -1;

constexpr char subtitleAsciiLower(unsigned char value) {
    return static_cast<char>(value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value);
}

constexpr bool subtitleAsciiSpace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

constexpr int resolvedSubtitleIndex(int requestedIndex, int serverDefaultIndex) {
    return requestedIndex == kSubtitleServerDefaultIndex ? serverDefaultIndex : requestedIndex;
}

constexpr bool shouldRetryFailedSubtitleTranscode(bool isTranscode, int selectedSubtitleIndex) {
    return isTranscode && selectedSubtitleIndex >= 0;
}

constexpr bool shouldApplyLoadedSubtitle(
    std::string_view activeItemId,
    std::string_view loadedItemId,
    int selectedSubtitleIndex,
    int loadedSubtitleIndex,
    bool loaded
) {
    return loaded
        && activeItemId == loadedItemId
        && selectedSubtitleIndex == loadedSubtitleIndex;
}

inline std::string sanitizeSubtitleText(std::string text) {
    if (text.find_first_of("&<{") == std::string::npos) return text;

    auto replaceAll = [](std::string& value, std::string_view from, std::string_view to) {
        size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos) {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    };
    if (text.find('&') != std::string::npos) {
        replaceAll(text, "&nbsp;", " ");
        replaceAll(text, "&amp;", "&");
        replaceAll(text, "&lt;", "<");
        replaceAll(text, "&gt;", ">");
        replaceAll(text, "&quot;", "\"");
        replaceAll(text, "&#39;", "'");
    }

    auto equalsIgnoreCase = [](std::string_view left, std::string_view right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) {
            return subtitleAsciiLower(a) == subtitleAsciiLower(b);
        });
    };
    auto isMarkupTag = [&](std::string_view body) {
        while (!body.empty() && subtitleAsciiSpace(static_cast<unsigned char>(body.front()))) body.remove_prefix(1);
        if (!body.empty() && body.front() == '/') body.remove_prefix(1);
        while (!body.empty() && subtitleAsciiSpace(static_cast<unsigned char>(body.front()))) body.remove_prefix(1);
        size_t length = 0;
        while (length < body.size()) {
            const unsigned char c = static_cast<unsigned char>(body[length]);
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) break;
            ++length;
        }
        if (length == 0) return false;
        const std::string_view name = body.substr(0, length);
        return equalsIgnoreCase(name, "i") || equalsIgnoreCase(name, "b")
            || equalsIgnoreCase(name, "u") || equalsIgnoreCase(name, "s")
            || equalsIgnoreCase(name, "font") || equalsIgnoreCase(name, "c")
            || equalsIgnoreCase(name, "v") || equalsIgnoreCase(name, "lang")
            || equalsIgnoreCase(name, "ruby") || equalsIgnoreCase(name, "rt")
            || equalsIgnoreCase(name, "br");
    };

    std::string clean;
    clean.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        if (text[index] == '{') {
            const size_t end = text.find('}', index + 1);
            if (end != std::string::npos) {
                const std::string_view body(text.data() + index + 1, end - index - 1);
                const bool assOverride = !body.empty() && body.front() == '\\';
                const bool startsWithAn = body.size() >= 2
                    && subtitleAsciiLower(static_cast<unsigned char>(body[0])) == 'a'
                    && subtitleAsciiLower(static_cast<unsigned char>(body[1])) == 'n';
                const bool malformedAlignment = startsWithAn
                    && (body.size() == 2 || (body.size() == 3 && body[2] >= '1' && body[2] <= '9'));
                if (assOverride || malformedAlignment) {
                    index = end + 1;
                    continue;
                }
            }
        }
        if (text[index] == '<') {
            const size_t end = text.find('>', index + 1);
            if (end != std::string::npos) {
                const std::string_view body(text.data() + index + 1, end - index - 1);
                if (isMarkupTag(body)) {
                    std::string_view normalized = body;
                    while (!normalized.empty() && subtitleAsciiSpace(static_cast<unsigned char>(normalized.front()))) normalized.remove_prefix(1);
                    if (!normalized.empty() && normalized.front() == '/') normalized.remove_prefix(1);
                    while (!normalized.empty() && subtitleAsciiSpace(static_cast<unsigned char>(normalized.front()))) normalized.remove_prefix(1);
                    if (normalized.size() >= 2
                        && subtitleAsciiLower(static_cast<unsigned char>(normalized[0])) == 'b'
                        && subtitleAsciiLower(static_cast<unsigned char>(normalized[1])) == 'r') {
                        if (!clean.empty() && clean.back() != ' ') clean.push_back(' ');
                    }
                    index = end + 1;
                    continue;
                }
            }
        }
        clean.push_back(text[index++]);
    }
    return clean;
}

inline int parseSubtitleTimestamp(std::string_view input) {
    while (!input.empty() && subtitleAsciiSpace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
    const size_t whitespace = input.find_first_of(" \t\r\n");
    if (whitespace != std::string_view::npos) input = input.substr(0, whitespace);
    if (input.empty()) return -1;

    const size_t firstColon = input.find(':');
    if (firstColon == std::string_view::npos) return -1;
    const size_t secondColon = input.find(':', firstColon + 1);
    const size_t fraction = input.find_first_of(".,", secondColon == std::string_view::npos ? firstColon + 1 : secondColon + 1);
    if (fraction == std::string_view::npos) return -1;

    auto parseUnsigned = [](std::string_view value, int& output) {
        if (value.empty()) return false;
        const char* begin = value.data();
        const char* end = begin + value.size();
        const auto [parsed, error] = std::from_chars(begin, end, output);
        return error == std::errc{} && parsed == end && output >= 0;
    };

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    if (secondColon == std::string_view::npos) {
        if (!parseUnsigned(input.substr(0, firstColon), minutes)
            || !parseUnsigned(input.substr(firstColon + 1, fraction - firstColon - 1), seconds)) {
            return -1;
        }
    } else {
        if (!parseUnsigned(input.substr(0, firstColon), hours)
            || !parseUnsigned(input.substr(firstColon + 1, secondColon - firstColon - 1), minutes)
            || !parseUnsigned(input.substr(secondColon + 1, fraction - secondColon - 1), seconds)) {
            return -1;
        }
    }
    if (minutes > 59 || seconds > 59) return -1;

    const std::string_view fractionDigits = input.substr(fraction + 1);
    if (fractionDigits.empty()) return -1;
    int rawFraction = 0;
    const size_t usedDigits = std::min<size_t>(3, fractionDigits.size());
    if (!parseUnsigned(fractionDigits.substr(0, usedDigits), rawFraction)) return -1;
    int milliseconds = rawFraction;
    if (usedDigits == 1) milliseconds *= 100;
    else if (usedDigits == 2) milliseconds *= 10;
    for (size_t index = usedDigits; index < fractionDigits.size(); ++index) {
        const unsigned char digit = static_cast<unsigned char>(fractionDigits[index]);
        if (digit < '0' || digit > '9') return -1;
    }

    const int64_t total = (((static_cast<int64_t>(hours) * 60) + minutes) * 60 + seconds) * 1000 + milliseconds;
    if (total > std::numeric_limits<int>::max()) return -1;
    return static_cast<int>(total);
}

struct SubtitleCue {
    int startMs = 0;
    int endMs = 0;
    std::string text;
};

inline void sortSubtitleCues(std::vector<SubtitleCue>& cues) {
    const auto ordered = [](const SubtitleCue& left, const SubtitleCue& right) {
        return left.startMs < right.startMs || (left.startMs == right.startMs && left.endMs < right.endMs);
    };
    if (!std::is_sorted(cues.begin(), cues.end(), ordered)) std::stable_sort(cues.begin(), cues.end(), ordered);
}

inline std::string subtitleTextFormat(std::string codec) {
    std::transform(codec.begin(), codec.end(), codec.begin(), subtitleAsciiLower);
    if (codec == "subrip") return "srt";
    if (codec == "webvtt") return "vtt";
    if (codec == "ass" || codec == "ssa" || codec == "srt" || codec == "vtt" || codec == "mov_text") {
        return codec;
    }
    return {};
}

inline bool nextSubtitleLine(std::string_view& input, std::string_view& line) {
    if (input.empty()) return false;
    const size_t newline = input.find('\n');
    if (newline == std::string_view::npos) {
        line = input;
        input = {};
    } else {
        line = input.substr(0, newline);
        input.remove_prefix(newline + 1);
    }
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return true;
}

inline std::vector<SubtitleCue> parseSubRipCues(const std::string& input) {
    std::vector<SubtitleCue> cues;
    cues.reserve(input.size() / 80);
    std::string_view remaining(input);
    std::string_view line;
    while (nextSubtitleLine(remaining, line)) {
        if (line.empty()) continue;
        if (line.find("-->") == std::string_view::npos) {
            if (!nextSubtitleLine(remaining, line)) break;
        }
        const size_t arrow = line.find("-->");
        if (arrow == std::string_view::npos) continue;
        const int start = parseSubtitleTimestamp(line.substr(0, arrow));
        const int end = parseSubtitleTimestamp(line.substr(arrow + 3));
        if (start < 0 || end <= start) continue;
        std::string text;
        while (nextSubtitleLine(remaining, line)) {
            if (line.empty()) break;
            if (!text.empty()) text += ' ';
            text.append(line);
        }
        text = sanitizeSubtitleText(std::move(text));
        if (!text.empty()) cues.push_back({start, end, std::move(text)});
    }
    sortSubtitleCues(cues);
    return cues;
}

inline std::vector<SubtitleCue> parseAssCues(const std::string& input) {
    std::vector<SubtitleCue> cues;
    cues.reserve(input.size() / 100);
    std::string_view remaining(input);
    std::string_view line;
    bool inEvents = false;
    int startColumn = -1;
    int endColumn = -1;
    int textColumn = -1;
    int fieldCount = 0;

    auto trim = [](std::string_view value) {
        while (!value.empty() && subtitleAsciiSpace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
        while (!value.empty() && subtitleAsciiSpace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
        return value;
    };
    std::vector<std::string_view> fields;
    auto split = [&](std::string_view value, int maxFields = -1) {
        fields.clear();
        if (maxFields > 0 && fields.capacity() < static_cast<size_t>(maxFields)) {
            fields.reserve(static_cast<size_t>(maxFields));
        }
        size_t start = 0;
        while (start <= value.size()) {
            if (maxFields > 0 && static_cast<int>(fields.size()) == maxFields - 1) {
                fields.push_back(trim(value.substr(start)));
                break;
            }
            const size_t comma = value.find(',', start);
            if (comma == std::string_view::npos) {
                fields.push_back(trim(value.substr(start)));
                break;
            }
            fields.push_back(trim(value.substr(start, comma - start)));
            start = comma + 1;
        }
    };
    auto equalsIgnoreCase = [](std::string_view left, std::string_view right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) {
            return subtitleAsciiLower(a) == subtitleAsciiLower(b);
        });
    };

    while (nextSubtitleLine(remaining, line)) {
        const std::string_view trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == ';') continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            inEvents = equalsIgnoreCase(trimmed, "[events]");
            continue;
        }
        if (!inEvents) continue;
        const size_t colon = trimmed.find(':');
        if (colon == std::string_view::npos) continue;
        if (equalsIgnoreCase(trimmed.substr(0, colon), "Format")) {
            split(trimmed.substr(colon + 1));
            fieldCount = static_cast<int>(fields.size());
            startColumn = endColumn = textColumn = -1;
            for (int i = 0; i < fieldCount; ++i) {
                const std::string_view name = fields[static_cast<size_t>(i)];
                if (equalsIgnoreCase(name, "start")) startColumn = i;
                else if (equalsIgnoreCase(name, "end")) endColumn = i;
                else if (equalsIgnoreCase(name, "text")) textColumn = i;
            }
            continue;
        }
        if (!equalsIgnoreCase(trimmed.substr(0, colon), "dialogue")) continue;
        if (fieldCount <= 0 || startColumn < 0 || endColumn < 0 || textColumn < 0) {
            fieldCount = 10;
            startColumn = 1;
            endColumn = 2;
            textColumn = 9;
        }
        split(trimmed.substr(colon + 1), fieldCount);
        if (static_cast<int>(fields.size()) <= std::max({startColumn, endColumn, textColumn})) continue;
        const int startMs = parseSubtitleTimestamp(fields[static_cast<size_t>(startColumn)]);
        const int endMs = parseSubtitleTimestamp(fields[static_cast<size_t>(endColumn)]);
        if (startMs < 0 || endMs <= startMs) continue;
        std::string text(fields[static_cast<size_t>(textColumn)]);
        size_t pos = 0;
        while ((pos = text.find("\\N", pos)) != std::string::npos) text.replace(pos, 2, " ");
        pos = 0;
        while ((pos = text.find("\\n", pos)) != std::string::npos) text.replace(pos, 2, " ");
        text = sanitizeSubtitleText(std::move(text));
        if (!text.empty()) cues.push_back({startMs, endMs, std::move(text)});
    }

    sortSubtitleCues(cues);
    return cues;
}

inline std::vector<SubtitleCue> parseTextSubtitleCues(const std::string& input, std::string codec) {
    std::transform(codec.begin(), codec.end(), codec.begin(), subtitleAsciiLower);
    if (codec == "ass" || codec == "ssa") return parseAssCues(input);
    return parseSubRipCues(input);
}

struct SubtitlePreferenceCandidate {
    int index = -1;
    std::string language;
};

inline std::string normalizeSubtitleLanguage(std::string language) {
    std::transform(language.begin(), language.end(), language.begin(), subtitleAsciiLower);
    if (language == "en" || language == "english") return "eng";
    if (language == "mi" || language == "mao" || language == "maori" || language == "māori") return "mri";
    if (language == "ja" || language == "japanese") return "jpn";
    if (language == "es" || language == "spanish") return "spa";
    if (language == "fr" || language == "fre" || language == "french") return "fra";
    if (language == "de" || language == "ger" || language == "german") return "deu";
    if (language == "it" || language == "italian") return "ita";
    if (language == "pt" || language == "portuguese") return "por";
    if (language == "ko" || language == "korean") return "kor";
    if (language == "zh" || language == "chi" || language == "chinese") return "zho";
    if (language == "ar" || language == "arabic") return "ara";
    if (language == "nl" || language == "dut" || language == "dutch") return "nld";
    if (language == "ru" || language == "russian") return "rus";
    if (language == "hi" || language == "hindi") return "hin";
    if (language == "sv" || language == "swedish") return "swe";
    if (language == "no" || language == "norwegian") return "nor";
    return language;
}

inline bool subtitleLanguageAllowed(const std::string& language, const std::vector<std::string>& allowedLanguages) {
    if (allowedLanguages.empty()) return true;
    const std::string normalized = normalizeSubtitleLanguage(language);
    return std::find(allowedLanguages.begin(), allowedLanguages.end(), normalized) != allowedLanguages.end();
}

inline bool isLikelySignsOnlySubtitle(std::string label) {
    std::transform(label.begin(), label.end(), label.begin(), subtitleAsciiLower);
    return label.find("sign") != std::string::npos && label.find("song") != std::string::npos;
}

inline int subtitleIndexForQueuePreference(
    const std::vector<SubtitlePreferenceCandidate>& subtitles,
    const std::optional<std::string>& languagePreference
) {
    if (!languagePreference.has_value() || languagePreference->empty()) return kSubtitleOffIndex;

    const std::string preferred = normalizeSubtitleLanguage(*languagePreference);
    const auto match = std::find_if(subtitles.begin(), subtitles.end(), [&](const SubtitlePreferenceCandidate& subtitle) {
        if (subtitle.index < 0) return false;
        if (subtitle.language.size() == 3 && preferred.size() == 3) {
            return std::equal(subtitle.language.begin(), subtitle.language.end(), preferred.begin(), [](unsigned char a, unsigned char b) {
                return subtitleAsciiLower(a) == subtitleAsciiLower(b);
            });
        }
        return normalizeSubtitleLanguage(subtitle.language) == preferred;
    });
    return match == subtitles.end() ? kSubtitleOffIndex : match->index;
}
