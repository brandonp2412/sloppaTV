#include "external_player_policy.hpp"

#include <cassert>

int main() {
    assert(externalPlayerKindForPackage("org.videolan.vlc") == ExternalPlayerKind::Vlc);
    assert(externalPlayerKindForPackage("com.mxtech.videoplayer.ad") == ExternalPlayerKind::MxPlayer);
    assert(externalPlayerKindForPackage("is.xyz.mpv") == ExternalPlayerKind::Mpv);
    assert(externalPlayerKindForPackage("app.mpvnova.player") == ExternalPlayerKind::Mpv);
    assert(externalPlayerKindForPackage("net.gtvbox.videoplayer") == ExternalPlayerKind::Vimu);
    assert(externalPlayerKindForPackage("com.example.player") == ExternalPlayerKind::Generic);

    const auto vlcOk = externalPlayerOutcomeForResult(ExternalPlayerKind::Vlc, -1, true);
    assert(vlcOk.success);
    assert(!vlcOk.completionKnown);

    const auto mxCancelled = externalPlayerOutcomeForResult(ExternalPlayerKind::MxPlayer, 0, false);
    assert(!mxCancelled.success);

    const auto mpvResume = externalPlayerOutcomeForResult(ExternalPlayerKind::Mpv, -1, true);
    assert(mpvResume.success);
    assert(mpvResume.completionKnown);
    assert(!mpvResume.completed);

    const auto mpvCompleted = externalPlayerOutcomeForResult(ExternalPlayerKind::Mpv, -1, false);
    assert(mpvCompleted.success);
    assert(mpvCompleted.completionKnown);
    assert(mpvCompleted.completed);

    const auto vimuInterrupted = externalPlayerOutcomeForResult(ExternalPlayerKind::Vimu, 0, true);
    assert(vimuInterrupted.success);
    assert(vimuInterrupted.completionKnown);
    assert(!vimuInterrupted.completed);

    const auto vimuCompleted = externalPlayerOutcomeForResult(ExternalPlayerKind::Vimu, 1, false);
    assert(vimuCompleted.success);
    assert(vimuCompleted.completionKnown);
    assert(vimuCompleted.completed);
    return 0;
}
