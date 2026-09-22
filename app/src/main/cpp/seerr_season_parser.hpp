#pragma once

#include "seerr_seasons.hpp"
#include <nlohmann/json.hpp>

inline std::vector<SeerrSeason> parseSeerrSeasons(const nlohmann::json& data, bool is4k) {
    std::vector<SeerrSeason> result;
    const auto seasons = data.find("seasons");
    if (seasons == data.end() || !seasons->is_array()) return result;
    for (const auto& value : *seasons) {
        if (!value.is_object() || !value.contains("seasonNumber") || !value["seasonNumber"].is_number_integer())
            continue;
        SeerrSeason season;
        season.number = value["seasonNumber"].get<int>();
        if (season.number < 0) continue;
        season.episodes = value.value("episodeCount", 0);
        season.name = value.value("name", std::string{});
        if (season.name.empty())
            season.name = season.number == 0 ? "Specials" : "Season " + std::to_string(season.number);
        if (value.contains("airDate") && value["airDate"].is_string())
            season.airDate = value["airDate"].get<std::string>();
        const auto media = data.find("mediaInfo");
        if (media != data.end() && media->is_object()) {
            const bool blocklisted = media->value(is4k ? "status4k" : "status", 0) == 6;
            const auto known = media->find("seasons");
            if (known != media->end() && known->is_array()) {
                for (const auto& entry : *known) {
                    if (entry.value("seasonNumber", -1) == season.number) {
                        season.status = entry.value(is4k ? "status4k" : "status", 0);
                        season.requested = season.status == 2 || season.status == 3;
                    }
                }
            }
            const auto requests = media->find("requests");
            if (requests != media->end() && requests->is_array()) {
                for (const auto& request : *requests) {
                    if (request.value("is4k", false) != is4k) continue;
                    const int status = request.value("status", 0);
                    if (status != 1 && status != 2) continue;
                    const auto requested = request.find("seasons");
                    if (requested == request.end() || !requested->is_array()) continue;
                    for (const auto& entry : *requested) {
                        const int seasonStatus = entry.value("status", status);
                        if (entry.value("seasonNumber", -1) == season.number &&
                            (seasonStatus == 1 || seasonStatus == 2))
                            season.requested = true;
                    }
                }
            }
            if (blocklisted) season.status = 6;
        }
        if (std::none_of(result.begin(), result.end(),
                         [&](const auto& existing) { return existing.number == season.number; }))
            result.push_back(std::move(season));
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.number < b.number; });
    return result;
}
