#pragma once

#include "jellyfin_types.hpp"

#include <nlohmann/json_fwd.hpp>

#include <vector>

JellyfinItem parseJellyfinItem(const nlohmann::json& value);
std::vector<JellyfinItem> parseJellyfinItems(const nlohmann::json& values);
