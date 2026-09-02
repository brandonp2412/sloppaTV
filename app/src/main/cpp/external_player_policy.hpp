#pragma once

#include <string_view>

enum class ExternalPlayerKind {
    Generic,
    Vlc,
    MxPlayer,
    Mpv,
    Vimu,
};

constexpr ExternalPlayerKind externalPlayerKindForPackage(std::string_view packageName) {
    if (packageName == "org.videolan.vlc") return ExternalPlayerKind::Vlc;
    if (packageName == "com.mxtech.videoplayer.ad") return ExternalPlayerKind::MxPlayer;
    if (packageName == "is.xyz.mpv") return ExternalPlayerKind::Mpv;
    if (packageName == "net.gtvbox.videoplayer") return ExternalPlayerKind::Vimu;
    return ExternalPlayerKind::Generic;
}
