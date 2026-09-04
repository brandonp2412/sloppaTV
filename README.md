<p align="center">
  <img src="docs/brand/sloppatv-theme.png" alt="sloppaTV caracal artwork" width="100%">
</p>

# sloppaTV

A jellyfin Android client in C++

Current status: **27/30 acceptance areas verified (90%)**. All in-scope areas are implemented; the remaining work is LAN discovery acceptance, real-microphone voice-search acceptance, and stable production signing in CI. The 30 acceptance areas are listed in [ROADMAP.md](ROADMAP.md).

## What it does

Home/browse/search/details, multiple users and servers, Quick Connect, watched/favorites, DirectPlay/DirectStream/transcoding, audio/subtitle switching, native SRT/VTT/ASS subtitle rendering, queues/autoplay, HDR and refresh-rate negotiation, external-player handoff, Android TV intents, screensaver, settings, and persistent sessions.

The app/UI policy and rendering are C++20 using Android `NativeActivity` and GLES3. Media3 and Android-only services use small Java/JNI bridges.

## Build

Requires JDK 21, Android SDK 36, NDK `29.0.14206865`, CMake 3.22.1, and a C++20 host compiler.

```sh
./tools/run_host_tests.py
cp key.properties.example key.properties  # fill in signing values
./gradlew test assembleRelease
```

All Android variants use the single package `app.sloppatv` and the signing identity configured in `key.properties`.

## License

MIT. See [LICENSE.md](LICENSE.md).
