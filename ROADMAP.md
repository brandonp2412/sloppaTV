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

Status at 2026-09-04: 83 / 139 verified (59.7%); 56 partial; none unbuilt.

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
- [x] Automated 1920x1080 screenshot capture, validation, manifest, and CI artifact.

## Remaining acceptance and fixtures

### Accounts, navigation, and browsing

- [~] Quick Connect TV UI; LAN discovery; profile, user, and server switching.
- [~] Navigation regression; server version edge cases; mixed/folder browsing.
- [~] Seasons, nested folders, populated favorites, direct item-options keys.
- [~] Delete and refresh permissions plus server-side mutation completion.

### Playback and platform

- [~] Target-TV codec, HDR, audio-route, refresh-rate, and long-soak matrix.
- [x] Playback reports have automated Jellyfin session-state assertions.
- [~] Subtitle appearance/position; ASS visual pass; Trickplay thumbnail render.
- [~] HDR fixture and HDR-preserving server-stream path verified; new-build streamer acceptance is still required. Trickplay still needs server-generated tile data.
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

- 2026-09-04: real-server acceptance against Jellyfin 10.11.11 passed transient Quick Connect
  authorization/authentication, seasons/episodes hierarchy, collections, ASS external delivery,
  constrained-bitrate transcoding, and semantic DirectStream negotiation. An expanded 5,000-item
  scan found 12 HDR10 HEVC Main 10 titles but no Trickplay data; the acceptance user currently has
  no favorite movies, so Trickplay and populated-favorites acceptance remain fixture-dependent.
- 2026-09-04: Google TV Streamer SEARCH intent and HOME/lifecycle restoration passed with the
  same app process and no sloppaTV fatal exception, native signal, or ANR. A five-minute physical-TV
  soak stayed flat (PSS -0.4%, RSS -0.2%) and also completed with a clean fatal-log audit.
- 2026-09-04: automated physical-TV playback-report acceptance verified Jellyfin `/Sessions`
  position advance while playing, immediate paused state with stable position, resumed state, and
  stopped-session clearing for the active sloppaTV item. The harness mutes media during automation,
  restores the original volume, and writes reproducible JSON evidence.
- 2026-09-04: HDR10 acceptance found an audio-transcode regression: the HEVC Main 10 source was
  being converted to AVC because the HLS server-stream profile advertised only H.264. The corrected
  item-aware profile advertises HEVC before H.264 when HEVC remains allowed for the title. A real
  Jellyfin stream probe changed from H.264 8-bit BT.709 to HEVC Main 10 10-bit BT.2020/PQ while
  transcoding TrueHD to AAC. Host/unit/Release builds pass; streamer acceptance of the new APK is
  still blocked until Astra has the installed production signing key rather than the temporary
  bughunt identity.
- 2026-09-04: physical-TV capture tooling now wakes a sleeping target before visual acceptance;
  this was added after a sleeping streamer produced valid-size but entirely black screenshots.
- 2026-09-04: the production-signed streamer install exposed overlapping Search result metadata.
  A local layout fix now uses bounded single-line card titles and row heights that account for
  portrait versus landscape media. Host tests and Release assembly pass; device acceptance of this
  fix is blocked non-destructively because Astra currently points at a temporary Bughunt signing
  key whose certificate does not match the production certificate already installed on the TV.
- 2026-09-03: Waydroid regression reproduced missing selected ASS subtitles on Hell's
  Paradise S1E1, then verified the transcoded ASS-to-native-SRT fallback end to end with
  an on-screen English dialogue cue, `SUBTITLES ENG`, preserved app data, and a clean fatal-log audit.
- 2026-09-03: Streamer regression pass verified subtitle-off startup, explicit full-dialogue
  ASS selection, player-overlay Back dismissal, empty-queue Down handling, and revised
  Home/Search/Genres layouts without clearing app data.
- 2026-09-03: reproducible screenshot suites added for CI and authenticated TV sessions.
- 2026-09-03: signed Release installed and cold-launched on the streamer.
- 2026-09-03: Waydroid playback passed motion, pause/resume, and seek checks.
- 2026-09-03: streamer benchmark median: 222.5 ms startup and 37.6 MB PSS;
  official Jellyfin: 414 ms and 150.4 MB PSS.
