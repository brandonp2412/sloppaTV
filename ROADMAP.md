# sloppaTV parity roadmap

## Goal

Fast, native Jellyfin Android TV client for Movies, Series, and Episodes.
The reference client is `jellyfin-androidtv` at `ccf78eb90` (2026-08-30).

Use C++/NDK and GLES for app/UI policy and rendering. Small Java bridges are
allowed only for Android-only services. Parity includes measured performance.

Out of scope: music, photos, Live TV/DVR, launcher recommendations, and server
administration.

## Completion rule

- [x] Verified end to end at the appropriate level.
- [~] Implemented, but device acceptance or a fixture is still outstanding.
- [ ] Not implemented.

Release requires every in-scope item to be `[x]`; blockers remain work until
resolved or deliberately removed from scope.

Status at 2026-09-03: 81 / 138 verified (58.7%); 57 partial; none unbuilt.

Waydroid may validate full hardware-independent flows. Google TV Streamer is
required for TV hardware, Android TV integration, and final performance work.

## Verified capabilities

- [x] NativeActivity C++20/GLES app; DPAD navigation; TV keyboard; persistence.
- [x] arm64 and armv7 builds; bounded async work; event-driven rendering.
- [x] Manual login, base-path discovery, persisted sessions, and Home launch.
- [x] Home rows, media grids, details, search, collections, genres, A-Z, cast.
- [x] Watched/favorite mutations are verified and restored after testing.
- [x] Poster, backdrop, inherited artwork, logos, and bounded artwork cache.
- [x] Focus restoration, large-text and overscan-safe layouts, diagnostics.
- [x] Media3 playback in GLES, resume, native video surface, and MediaSession.
- [x] DirectPlay, DirectStream, and Transcode reporting/classification.
- [x] Embedded audio/text switching; ASS/SSA via libass; SRT/VTT support.
- [x] Quality negotiation, queueing, autoplay, Still Watching, and Play All.
- [x] Android `ACTION_VIEW`, in-app screensaver, and core settings persistence.
- [x] Benchmarks beat the official client in sampled startup, memory, and DPAD.
- [x] Single `app.sloppatv` package and an in-place signed streamer deployment.

## Remaining acceptance and fixtures

### Accounts, navigation, and browsing

- [~] Quick Connect TV UI; LAN discovery; profile, user, and server switching.
- [~] Navigation regression; server version edge cases; mixed/folder browsing.
- [~] Seasons, nested folders, populated favorites, direct item-options keys.
- [~] Delete and refresh permissions plus server-side mutation completion.

### Playback and platform

- [~] Target-TV codec, HDR, audio-route, refresh-rate, and long-soak matrix.
- [~] Playback reports need automated Jellyfin session-state assertions.
- [~] Subtitle appearance/position; ASS visual pass; Trickplay thumbnail render.
- [~] HDR needs an HDR title; Trickplay needs server-generated tile data.
- [~] External-player handoff needs a compatible installed player.
- [~] Voice search needs a real microphone/recognizer invocation.
- [~] DreamService needs normal-idle render/dismissal acceptance.
- [~] Launcher banner/icon needs reinstall verification with the matching key.

### Settings, resilience, and release

- [~] Buffer presets, audio mode, display preferences, and user switching E2E.
- [~] Wake/retry, lifecycle, crash/ANR, and memory long-soak coverage.
- [~] Production signing, reproducibility, and stable CI artifact signing.

## Acceptance policy

- Test real-server mutations safely and restore changed user settings.

## Evidence highlights

- 2026-09-03: signed Release installed and cold-launched on the streamer.
- 2026-09-03: Waydroid playback passed motion, pause/resume, and seek checks.
- 2026-09-03: streamer benchmark median: 222.5 ms startup and 37.6 MB PSS;
  official Jellyfin: 414 ms and 150.4 MB PSS.
