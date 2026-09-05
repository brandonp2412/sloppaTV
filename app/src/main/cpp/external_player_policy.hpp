#pragma once

#include <string_view>

enum class ExternalPlayerKind {
    Generic,
    Vlc,
    MxPlayer,
    Mpv,
    Vimu,
};

struct ExternalPlayerOutcome {
    bool success = false;
    bool completionKnown = false;
    bool completed = false;
};

constexpr int externalMpvDecodeModeForPackage(std::string_view packageName) {
    return packageName == "app.gyrolet.mpvrx" ? 1 : 2;
}

constexpr ExternalPlayerKind externalPlayerKindForPackage(std::string_view packageName) {
    if (packageName == "org.videolan.vlc") return ExternalPlayerKind::Vlc;
    if (packageName == "com.mxtech.videoplayer.ad") return ExternalPlayerKind::MxPlayer;
    if (packageName == "is.xyz.mpv" || packageName == "app.mpvnova.player" || packageName == "app.gyrolet.mpvrx") return ExternalPlayerKind::Mpv;
    if (packageName == "net.gtvbox.videoplayer") return ExternalPlayerKind::Vimu;
    return ExternalPlayerKind::Generic;
}

constexpr ExternalPlayerOutcome externalPlayerOutcomeForResult(
    ExternalPlayerKind kind,
    int resultCode,
    bool hasPosition
) {
    switch (kind) {
        case ExternalPlayerKind::Vlc:
        case ExternalPlayerKind::MxPlayer:
            return {.success = resultCode == -1};
        case ExternalPlayerKind::Mpv:
            return {
                .success = resultCode == -1,
                .completionKnown = resultCode == -1,
                .completed = resultCode == -1 && !hasPosition,
            };
        case ExternalPlayerKind::Vimu:
            return {
                .success = resultCode == 0 || resultCode == 1,
                .completionKnown = resultCode == 0 || resultCode == 1,
                .completed = resultCode == 1,
            };
        case ExternalPlayerKind::Generic:
            return {.success = resultCode == -1};
    }
    return {};
}
