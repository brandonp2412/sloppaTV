#include "external_player_policy.hpp"

#include <cassert>

int main() {
    assert(externalPlayerKindForPackage("org.videolan.vlc") == ExternalPlayerKind::Vlc);
    assert(externalPlayerKindForPackage("com.mxtech.videoplayer.ad") == ExternalPlayerKind::MxPlayer);
    assert(externalPlayerKindForPackage("is.xyz.mpv") == ExternalPlayerKind::Mpv);
    assert(externalPlayerKindForPackage("net.gtvbox.videoplayer") == ExternalPlayerKind::Vimu);
    assert(externalPlayerKindForPackage("com.example.player") == ExternalPlayerKind::Generic);
    return 0;
}
