# sloppaTV roadmap

Fast native Jellyfin Android TV client for Movies, Series and Episodes. Music, photos, Live TV/DVR, launcher recommendations and server administration are out of scope.

Status at 2026-09-05: **27/30 acceptance areas verified (90%)**. All in-scope areas are implemented; three areas still need final acceptance or release setup.

`[x]` = verified end to end. `[~]` = implemented but acceptance/setup remains. `[ ]` = not implemented.

Release requires every in-scope item to be `[x]`, unless an item is deliberately removed from scope.

## Accounts, navigation and browsing

- [x] Native C++20/GLES Android TV app with DPAD navigation, TV keyboard and persistent state.
- [x] arm64 and armv7 builds with bounded async work and event-driven rendering.
- [x] Manual login, base-path discovery, persisted sessions and authenticated Home launch.
- [x] Home rows, media grids, details, search, collections, genres, A-Z and cast browsing.
- [x] Watched and favorite mutations, including safe real-server verification and restoration.
- [x] Poster, backdrop, inherited artwork and logo handling with bounded artwork caches.
- [x] Focus restoration, large-text layouts, overscan-safe presentation and diagnostics.
- [x] Navigation regression coverage, server-version edge cases and Movies/Shows/Mixed/folder browsing.
- [x] Seasons, nested folders, populated favorites and direct item-options keys.
- [x] Delete and metadata-refresh permissions plus server-side mutation completion.
- [~] **LAN discovery:** app support is implemented, but Astra currently drops LAN UDP 7359. Distinct-user switching, Quick Connect, saved-profile lifecycle/expiry cleanup and saved-server endpoint switching are physically verified.

## Playback and platform

- [x] Media3 playback in GLES with resume, native video surface and MediaSession integration.
- [x] DirectPlay, DirectStream and Transcode reporting/classification.
- [x] Embedded audio/text switching plus native SRT/VTT/ASS/SSA subtitle rendering.
- [x] Quality negotiation, queueing, autoplay, Still Watching and Play All.
- [x] Android `ACTION_VIEW`, in-app screensaver and core settings persistence.
- [x] Representative codec/audio-route and long-play soak matrix, AVC/HEVC/HDR overrides and physical 23.976 Hz refresh-rate switching.
- [x] Playback reporting with automated Jellyfin session-state assertions.
- [x] Subtitle size/background/position plus dedicated ASS/SRT physical visual acceptance.
- [x] HDR-preserving server-stream path and generated Trickplay tile rendering.
- [x] External-player discovery, selection, playback handoff and return.
- [~] **Voice search:** implemented, but still needs one real microphone/Android recognizer invocation on TV hardware.
- [x] DreamService normal-idle launch, native rendering and DPAD dismissal.
- [x] Launcher banner/icon and single `app.sloppatv` package verified with the production signing identity.

## Settings, resilience and release

- [x] User switching, buffer presets, audio mode, watched indicators, clock, backdrops, zoom and refresh-rate preferences physically verified and restored.
- [x] Wake/retry, lifecycle, repeated player teardown, crash/ANR auditing and memory-soak coverage.
- [x] Automated 1920x1080 screenshot capture, validation, manifest and CI artifact.
- [x] Same-device benchmark suite verifies no accepted performance regression and materially lower sampled launch time/PSS/DPAD p95 than the installed Jellyfin Android TV development build.
- [x] Local production signing and reproducible APK payloads verified.
- [~] **Stable CI signing:** GitHub Actions supports production signing secrets but still falls back to an ephemeral identity because those repository secrets are not populated.

## Evidence

Performance methodology and current raw benchmark evidence are in [PERFORMANCE.md](PERFORMANCE.md) and [`docs/benchmarks/`](docs/benchmarks/). Detailed historical acceptance evidence remains available in git history without keeping a multi-hundred-line test diary in this roadmap.
