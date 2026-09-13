#pragma once

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

inline int seerrProgressPercent(double size, double sizeLeft) {
    if (!(size > 0.0) || !std::isfinite(size) || !std::isfinite(sizeLeft)) return -1;
    const double completed = std::clamp((size - sizeLeft) / size, 0.0, 1.0);
    return static_cast<int>(std::lround(completed * 100.0));
}

inline std::string seerrCompactTimeLeft(std::string value) {
    if (value.empty()) return {};
    int days = 0;
    const auto firstColon = value.find(':');
    const auto dayDot = value.find('.');
    if (dayDot != std::string::npos && firstColon != std::string::npos && dayDot < firstColon) {
        try { days = std::max(0, std::stoi(value.substr(0, dayDot))); } catch (...) { return {}; }
        value.erase(0, dayDot + 1);
    }

    std::vector<int> parts;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ':')) {
        const auto fraction = token.find('.');
        if (fraction != std::string::npos) token.resize(fraction);
        try { parts.push_back(std::max(0, std::stoi(token))); } catch (...) { return {}; }
    }
    if (parts.size() != 3) return {};
    const int totalSeconds = days * 86400 + parts[0] * 3600 + parts[1] * 60 + parts[2];
    if (totalSeconds <= 0) return {};
    if (totalSeconds >= 86400) {
        const int d = totalSeconds / 86400;
        const int h = (totalSeconds % 86400) / 3600;
        return h > 0 ? std::to_string(d) + "d " + std::to_string(h) + "h left" : std::to_string(d) + "d left";
    }
    if (totalSeconds >= 3600) {
        const int h = totalSeconds / 3600;
        const int m = (totalSeconds % 3600) / 60;
        return m > 0 ? std::to_string(h) + "h " + std::to_string(m) + "m left" : std::to_string(h) + "h left";
    }
    if (totalSeconds >= 60) return std::to_string(totalSeconds / 60) + "m left";
    return "<1m left";
}

inline std::string seerrProgressStatus(
    const std::string& mediaType,
    int seasonNumber,
    int episodeNumber,
    int percent,
    const std::string& timeLeft
) {
    if (percent < 0) return {};
    std::string prefix;
    if (mediaType == "movie") prefix = percent >= 100 ? "Downloaded" : "Downloading";
    else if (seasonNumber >= 0 && episodeNumber >= 0) {
        prefix = "S" + std::to_string(seasonNumber) + "E" + std::to_string(episodeNumber);
    } else {
        prefix = "Season pack";
    }

    std::string result = prefix + " · " + std::to_string(percent) + "%";
    if (percent >= 100) return result + " · Waiting for import";
    const std::string compact = seerrCompactTimeLeft(timeLeft);
    if (!compact.empty()) result += " · " + compact;
    return result;
}
