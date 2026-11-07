#pragma once

#include <cctype>
#include <string>
#include <string_view>

// Keep exact title matches ahead of similarly named, less relevant media.
inline std::string normalizedSearchTitle(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    std::string result;
    result.reserve(text.size());
    for (unsigned char ch : text) result.push_back(static_cast<char>(std::tolower(ch)));
    return result;
}

inline int searchTitleMatchScore(std::string_view title, std::string_view query) {
    const std::string candidate = normalizedSearchTitle(title);
    const std::string term = normalizedSearchTitle(query);
    if (term.empty() || candidate.empty()) return 0;
    if (candidate == term) return 3;
    if (candidate.starts_with(term) && candidate.size() > term.size() &&
        !std::isalnum(static_cast<unsigned char>(candidate[term.size()])))
        return 2;
    if (candidate.find(term) != std::string::npos) return 1;
    return 0;
}
