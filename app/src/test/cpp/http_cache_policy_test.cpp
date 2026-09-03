#include "http_cache_policy.hpp"

#include <cassert>

int main() {
    assert(kMaxApiGetCacheEntries >= 8);
    assert(kMaxApiGetCacheEntries <= 64);
    assert(shouldJoinInFlightApiGet(0, 0));
    assert(shouldJoinInFlightApiGet(7, 7));
    assert(!shouldJoinInFlightApiGet(7, 8));

    assert(shouldCacheApiGet("https://jellyfin.example/Users/me/Items?ParentId=123"));
    assert(shouldCacheApiGet("https://jellyfin.example/Items/123"));
    assert(!shouldCacheApiGet("https://jellyfin.example/Items/123/Images/Primary"));
    assert(!shouldCacheApiGet("https://jellyfin.example/Videos/123/stream"));
    assert(!shouldCacheApiGet("https://jellyfin.example/Videos/123/Subtitles/2/0/Stream.srt"));
    assert(!shouldCacheApiGet("https://jellyfin.example/Users/me/Items/Resume"));
    assert(!shouldCacheApiGet("https://jellyfin.example/Shows/NextUp?UserId=me"));
    assert(!shouldCacheApiGet("https://jellyfin.example/Items?SortBy=Random"));
    return 0;
}
