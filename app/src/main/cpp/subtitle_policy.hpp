#pragma once

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

constexpr int kSubtitleServerDefaultIndex = -2;
constexpr int kSubtitleOffIndex = -1;

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
    auto replaceAll = [](std::string& value, std::string_view from, std::string_view to) {
        size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos) {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    };
    replaceAll(text, "&nbsp;", " ");
    replaceAll(text, "&amp;", "&");
    replaceAll(text, "&lt;", "<");
    replaceAll(text, "&gt;", ">");
    replaceAll(text, "&quot;", "\"");
    replaceAll(text, "&#39;", "'");

    auto isMarkupTag = [](std::string_view body) {
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.front()))) body.remove_prefix(1);
        if (!body.empty() && body.front() == '/') body.remove_prefix(1);
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.front()))) body.remove_prefix(1);
        size_t length = 0;
        while (length < body.size()) {
            const unsigned char c = static_cast<unsigned char>(body[length]);
            if (!std::isalnum(c)) break;
            ++length;
        }
        if (length == 0) return false;
        std::string name(body.substr(0, length));
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return name == "i" || name == "b" || name == "u" || name == "s"
            || name == "font" || name == "c" || name == "v" || name == "lang"
            || name == "ruby" || name == "rt" || name == "br";
    };

    std::string clean;
    clean.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        if (text[index] == '{' && index + 1 < text.size() && text[index + 1] == '\\') {
            const size_t end = text.find('}', index + 2);
            if (end != std::string::npos) {
                index = end + 1;
                continue;
            }
        }
        if (text[index] == '<') {
            const size_t end = text.find('>', index + 1);
            if (end != std::string::npos) {
                const std::string_view body(text.data() + index + 1, end - index - 1);
                if (isMarkupTag(body)) {
                    std::string_view normalized = body;
                    while (!normalized.empty() && std::isspace(static_cast<unsigned char>(normalized.front()))) normalized.remove_prefix(1);
                    if (!normalized.empty() && normalized.front() == '/') normalized.remove_prefix(1);
                    while (!normalized.empty() && std::isspace(static_cast<unsigned char>(normalized.front()))) normalized.remove_prefix(1);
                    if (normalized.size() >= 2
                        && std::tolower(static_cast<unsigned char>(normalized[0])) == 'b'
                        && std::tolower(static_cast<unsigned char>(normalized[1])) == 'r') {
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
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
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
        if (!std::isdigit(static_cast<unsigned char>(fractionDigits[index]))) return -1;
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

inline std::vector<SubtitleCue> parseSubRipCues(const std::string& input) {
    std::vector<SubtitleCue> cues;
    std::istringstream stream(input);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.find("-->") == std::string::npos) {
            if (!std::getline(stream, line)) break;
            if (!line.empty() && line.back() == '\r') line.pop_back();
        }
        const size_t arrow = line.find("-->");
        if (arrow == std::string::npos) continue;
        std::string left = line.substr(0, arrow);
        std::string right = line.substr(arrow + 3);
        auto trim = [](std::string& value) {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
        };
        trim(left);
        trim(right);
        const int start = parseSubtitleTimestamp(left);
        const int end = parseSubtitleTimestamp(right);
        if (start < 0 || end <= start) continue;
        std::string text;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            if (!text.empty()) text += ' ';
            text += line;
        }
        if (!text.empty()) cues.push_back({start, end, std::move(text)});
    }
    return cues;
}

struct SubtitlePreferenceCandidate {
    int index = -1;
    std::string language;
};

inline std::string normalizeSubtitleLanguage(std::string language) {
    std::transform(language.begin(), language.end(), language.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return language;
}

inline bool isLikelySignsOnlySubtitle(std::string label) {
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return label.find("sign") != std::string::npos && label.find("song") != std::string::npos;
}

// nullopt: no explicit viewer choice yet, begin with subtitles off.
// empty string: user explicitly disabled subtitles for this playback chain.
// non-empty: user selected a language; carry it forward when the next item exposes it.
inline int subtitleIndexForQueuePreference(
    const std::vector<SubtitlePreferenceCandidate>& subtitles,
    const std::optional<std::string>& languagePreference
) {
    if (!languagePreference.has_value()) return kSubtitleOffIndex;
    if (languagePreference->empty()) return kSubtitleOffIndex;

    const std::string preferred = normalizeSubtitleLanguage(*languagePreference);
    const auto match = std::find_if(subtitles.begin(), subtitles.end(), [&](const SubtitlePreferenceCandidate& subtitle) {
        return subtitle.index >= 0 && normalizeSubtitleLanguage(subtitle.language) == preferred;
    });
    return match == subtitles.end() ? kSubtitleOffIndex : match->index;
}
