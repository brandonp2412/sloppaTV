#pragma once

#include "jellyfin_types.hpp"

#include <string_view>
#include <vector>

ApiValueResult<std::vector<JellyfinMediaSegment>> parseJellyfinMediaSegments(std::string_view responseBody);
