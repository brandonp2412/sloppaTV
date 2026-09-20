#include "jellyfin_media_segment_parser.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <limits>
#include <optional>
#include <string>

using nlohmann::json;

namespace {
std::string stringValue(const json& value, const char* key) {
    const auto match = value.find(key);
    return match != value.end() && match->is_string() ? match->get<std::string>() : std::string{};
}

std::optional<int64_t> integerValue(const json& value, const char* key) {
    const auto match = value.find(key);
    if (match == value.end() || !match->is_number_integer()) return std::nullopt;
    try {
        if (match->is_number_unsigned()) {
            const uint64_t number = match->get<uint64_t>();
            if (number > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) return std::nullopt;
            return static_cast<int64_t>(number);
        }
        return match->get<int64_t>();
    } catch (const json::exception&) {
        return std::nullopt;
    }
}
} // namespace

ApiValueResult<std::vector<JellyfinMediaSegment>> parseJellyfinMediaSegments(std::string_view responseBody) {
    ApiValueResult<std::vector<JellyfinMediaSegment>> result;
    try {
        const auto data = json::parse(responseBody);
        if (!data.is_object() || !data.contains("Items") || !data["Items"].is_array()) {
            result.error = "Jellyfin media-segment response did not contain Items";
            return result;
        }

        for (const auto& value : data["Items"]) {
            if (!value.is_object()) continue;
            const std::string type = stringValue(value, "Type");
            const auto startTicks = integerValue(value, "StartTicks");
            const auto endTicks = integerValue(value, "EndTicks");
            if (type.empty() || !startTicks || !endTicks || *startTicks < 0 || *endTicks <= *startTicks) continue;
            result.value.push_back(JellyfinMediaSegment{type, *startTicks, *endTicks});
        }

        std::sort(result.value.begin(), result.value.end(),
                  [](const auto& left, const auto& right) { return left.startTicks < right.startTicks; });
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse media segments: ") + e.what();
    }
    return result;
}
