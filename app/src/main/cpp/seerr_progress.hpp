#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
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
        try {
            days = std::max(0, std::stoi(value.substr(0, dayDot)));
        } catch (...) {
            return {};
        }
        value.erase(0, dayDot + 1);
    }

    std::vector<int> parts;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ':')) {
        const auto fraction = token.find('.');
        if (fraction != std::string::npos) token.resize(fraction);
        try {
            parts.push_back(std::max(0, std::stoi(token)));
        } catch (...) {
            return {};
        }
    }
    if (parts.size() != 3) return {};
    const int64_t totalSeconds = static_cast<int64_t>(days) * 86400 + static_cast<int64_t>(parts[0]) * 3600 +
                                 static_cast<int64_t>(parts[1]) * 60 + parts[2];
    if (totalSeconds <= 0) return {};
    if (totalSeconds >= 86400) {
        const int64_t d = totalSeconds / 86400;
        const int64_t h = (totalSeconds % 86400) / 3600;
        return h > 0 ? std::to_string(d) + "d " + std::to_string(h) + "h left" : std::to_string(d) + "d left";
    }
    if (totalSeconds >= 3600) {
        const int64_t h = totalSeconds / 3600;
        const int64_t m = (totalSeconds % 3600) / 60;
        return m > 0 ? std::to_string(h) + "h " + std::to_string(m) + "m left" : std::to_string(h) + "h left";
    }
    if (totalSeconds >= 60) return std::to_string(totalSeconds / 60) + "m left";
    return "<1m left";
}

inline std::string seerrProgressLabel(const std::string& mediaType, int seasonNumber, int episodeNumber, int percent) {
    if (percent < 0) return {};
    const bool complete = percent >= 100;
    if (mediaType == "movie") return complete ? "Downloaded" : "Downloading";
    if (seasonNumber >= 0 && episodeNumber >= 0) {
        return "S" + std::to_string(seasonNumber) + "E" + std::to_string(episodeNumber) +
               (complete ? " downloaded" : " downloading");
    }
    return complete ? "Season pack downloaded" : "Season pack downloading";
}

inline std::string seerrProgressEta(int percent, const std::string& timeLeft) {
    if (percent < 0) return {};
    if (percent >= 100) return "Waiting for import";
    return seerrCompactTimeLeft(timeLeft);
}

inline std::string seerrProgressStatus(const std::string& mediaType, int seasonNumber, int episodeNumber, int percent,
                                       const std::string& timeLeft) {
    if (percent < 0) return {};
    std::string result =
        seerrProgressLabel(mediaType, seasonNumber, episodeNumber, percent) + " " + std::to_string(percent) + "%";
    const std::string eta = seerrProgressEta(percent, timeLeft);
    if (!eta.empty()) result += "  " + eta;
    return result;
}
