<p align="center">
  <img src="docs/brand/sloppatv-theme.png" alt="sloppaTV caracal artwork" width="100%">
</p>

# sloppaTV

Fast native Android TV client for Jellyfin, designed for remote-first couch use.

Scope: Movies, Series and Episodes. Music, photos, Live TV/DVR, launcher recommendations and server administration are intentionally out of scope.

Current status: **27/30 acceptance areas verified (90%)**. All in-scope areas are implemented; the remaining work is LAN discovery acceptance, real-microphone voice-search acceptance, and stable production signing in CI. The 30 acceptance areas are listed in [ROADMAP.md](ROADMAP.md).

## What it does

Home/browse/search/details, multiple users and servers, Quick Connect, watched/favorites, DirectPlay/DirectStream/transcoding, audio/subtitle switching, native SRT/VTT/ASS subtitle rendering, queues/autoplay, HDR and refresh-rate negotiation, external-player handoff, Android TV intents, screensaver, settings, and persistent sessions.

The app/UI policy and rendering are C++20 using Android `NativeActivity` and GLES3. Media3 and Android-only services use small Java/JNI bridges.

## Performance

Same-device Google TV Streamer measurements from 2026-09-04 against the installed Jellyfin Android TV development build:

| Metric | sloppaTV | Jellyfin Android TV dev build |
| --- | ---: | ---: |
| Cold launch median | **242.5 ms** | 404.0 ms |
| Settled PSS | **40,192 KB** | 152,328 KB |
| Rapid-DPAD p95 | **16.73 ms** | 33.34 ms |

Methodology and raw evidence: [PERFORMANCE.md](PERFORMANCE.md) and [`docs/benchmarks/google-tv-streamer-2026-09-04.json`](docs/benchmarks/google-tv-streamer-2026-09-04.json).

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
