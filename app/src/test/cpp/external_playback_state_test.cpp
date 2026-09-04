#include "external_playback_state.hpp"

#include <cassert>
#include <utility>

int main() {
    ExternalPlaybackState state;
    assert(!state.hasPending());
    assert(!state.hasActive());

    ExternalPlaybackLaunch launch;
    launch.item.id = "episode-1";
    launch.player.packageName = "org.videolan.vlc";
    launch.player.label = "VLC";
    launch.url = "https://example.test/video";
    launch.subtitleUrl = "https://example.test/subtitle.srt";
    state.stage(launch);
    assert(state.hasPending());

    auto pending = state.takePending();
    assert(pending.has_value());
    assert(!state.hasPending());
    assert(pending->item.id == "episode-1");
    assert(pending->player.label == "VLC");
    assert(pending->url == launch.url);
    assert(pending->subtitleUrl == launch.subtitleUrl);

    state.beginActive(std::move(*pending));
    assert(state.hasActive());
    auto active = state.takeActive();
    assert(active.has_value());
    assert(!state.hasActive());
    assert(active->item.id == "episode-1");

    state.stage(launch);
    state.beginActive(launch);
    state.reset();
    assert(!state.hasPending());
    assert(!state.hasActive());
    return 0;
}
