#pragma once

#include <cstddef>
#include <string_view>

constexpr std::size_t transientHttpRetryCount(std::string_view method) {
    // Only automatically replay safe, read-only requests. Jellyfin uses POST for
    // authentication, Quick Connect, metadata refresh and playback reporting;
    // replaying any of those after an ambiguous connection failure can duplicate
    // server-side work. DELETE is semantically idempotent but a replay after a
    // successful first delete can turn a successful operation into a misleading 404.
    return method == "GET" || method == "HEAD" ? 2U : 0U;
}
