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

Status at 2026-09-04: 99 / 139 verified (71.2%); 40 partial; none unbuilt.

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
- [x] Embedded audio/text switching; SRT/VTT and ASS/SSA render through the native subtitle path.
- [x] Quality negotiation, queueing, autoplay, Still Watching, and Play All.
- [x] Android `ACTION_VIEW`, in-app screensaver, and core settings persistence.
- [x] Benchmarks beat the official client in sampled startup, memory, and DPAD.
- [x] Single `app.sloppatv` package and an in-place signed streamer deployment.
- [x] Automated 1920x1080 screenshot capture, validation, manifest, and CI artifact.

## Remaining acceptance and fixtures

### Accounts, navigation, and browsing

- [~] Quick Connect TV UI; LAN discovery; profile, user, and server switching.
- [~] Navigation regression and server-version edge cases remain; Movies/Shows/Mixed/folder browsing is physically verified.
- [x] Seasons, nested folders, populated favorites, direct item-options keys.
- [x] Delete and refresh permissions plus server-side mutation completion.

### Playback and platform

- [~] Target-TV codec, HDR, audio-route, and long-soak matrix; physical 23.976 Hz refresh-rate switching is verified.
- [x] Playback reports have automated Jellyfin session-state assertions.
- [x] Subtitle size/background/position and dedicated ASS/SRT visual passes are physically verified on the Google TV Streamer.
- [x] HDR fixture/HDR-preserving server-stream path and generated Trickplay tile rendering are verified on the Google TV Streamer.
- [x] External-player discovery, selection, silent playback handoff, and return are verified on the Google TV Streamer.
- [~] Voice search needs a real microphone/recognizer invocation.
- [~] DreamService needs normal-idle render/dismissal acceptance.
- [x] Launcher banner/icon reinstalled and visually verified with the matching key.

### Settings, resilience, and release

- [~] Buffer presets, audio mode, display preferences, and user switching E2E; watched-indicator, clock, backdrop, zoom, and refresh-rate display preferences are physically verified and restored.
- [~] Wake/retry remains outstanding; lifecycle, repeated player teardown, crash/ANR auditing, and memory soak coverage are physically verified.
- [~] Production signing, reproducibility, and stable CI artifact signing.

## Acceptance policy

- Test real-server mutations safely and restore changed user settings.

## Evidence highlights

- 2026-09-04: real-server acceptance against Jellyfin 10.11.11 passed transient Quick Connect
  authorization/authentication, seasons/episodes hierarchy, collections, ASS external delivery,
  constrained-bitrate transcoding, and semantic DirectStream negotiation. An expanded 5,000-item
  scan found 12 HDR10 HEVC Main 10 titles but no Trickplay data, so Trickplay remains fixture-dependent.
- 2026-09-04: browsing acceptance closed seasons/nested-folder/favorites/options coverage. The
  production streamer opened the real `Back to the Future` folder from Movies and rendered its three
  child films; both `MENU` and `INFO` opened Item Options. `Big Buck Bunny` was temporarily favorited,
  appeared as the sole Movies Favorites item, then was unfavorited and the server was confirmed back
  at zero favorite movies.
- 2026-09-04: refresh/delete acceptance is complete with disposable media fixtures. Explicit refresh
  previously used `metadataRefreshMode=Default`, which accepted the request but did not reread an
  edited local NFO. Matching explicit-refresh semantics used by other Jellyfin clients (`FullRefresh`
  plus `replaceAllMetadata=true`) made a TV-triggered refresh change the same server item from
  `SloppaTV Refresh After` to `SloppaTV App Refresh Verified`. A separate 15-second test movie under
  `/media` verified `CanDelete`, the destructive confirmation UI, server-side DELETE completion (404),
  and physical fixture removal. All fixture files were removed and library directory modes restored.
- 2026-09-04: LAN discovery is blocked by Astra infrastructure rather than sloppaTV parsing. Jellyfin
  10.11.11 has AutoDiscovery enabled and listens on UDP 7359; direct local probes return valid server
  JSON, while inbound UDP from Glass and the streamer receives no reply. `/etc/nftables.conf` uses an
  input policy of `drop` and permits LAN UDP only on 137/138, excluding 7359. The required narrow fix
  is to allow UDP 7359 only from the existing private LAN source ranges; this tunnel cannot modify the
  root-owned firewall configuration. An experimental /24 unicast scan was removed after confirming the
  firewall also blocks inbound unicast UDP.
- 2026-09-04: Google TV Streamer SEARCH intent and HOME/lifecycle restoration passed with the
  same app process and no sloppaTV fatal exception, native signal, or ANR. A five-minute physical-TV
  soak stayed flat (PSS -0.4%, RSS -0.2%); a later 15-minute static soak collected 31 samples and also
  stayed flat at -0.1% PSS growth and effectively 0.0% RSS growth with a clean fatal-log audit.
- 2026-09-04: automated physical-TV playback-report acceptance verified Jellyfin `/Sessions`
  position advance while playing, immediate paused state with stable position, resumed state, and
  stopped-session clearing for the active sloppaTV item. The harness mutes media during automation,
  restores the original volume, and writes reproducible JSON evidence.
- 2026-09-04: HDR10 acceptance found an audio-transcode regression: the HEVC Main 10 source was
  being converted to AVC because the HLS server-stream profile advertised only H.264. The corrected
  item-aware profile advertises HEVC before H.264 when HEVC remains allowed for the title. A real
  Jellyfin stream probe changed from H.264 8-bit BT.709 to HEVC Main 10 10-bit BT.2020/PQ while
  transcoding TrueHD to AAC. Production-signed streamer acceptance now confirms DirectStream,
  `c2.mtk.hevc.decoder`, HDR color metadata, a 1920x1080 video surface, and first-frame render.
- 2026-09-04: Trickplay acceptance is complete with an isolated disposable Jellyfin library. Normal
  libraries remained disabled for Trickplay while the temporary library generated a 320x180 sheet
  containing six thumbnails at 10-second intervals for a 60-second video-only fixture. On the physical
  streamer, pausing and seeking +10 seconds rendered the bordered Trickplay preview with the correct
  `0:11` timestamp. The temporary library and media were removed afterward and all normal library
  Trickplay settings were confirmed restored to disabled.
- 2026-09-04: external-player acceptance is complete. Installing VLC first exposed Android
  package-visibility filtering; adding the wildcard URI scheme (`android:scheme="*"`) made real
  video handlers visible in Settings. VLC's first-run storage flow was unsuitable for unattended
  acceptance, so an official mpv-android build was selected instead and a disposable video-only
  Jellyfin fixture avoided all HDMI-audio side effects. `is.xyz.mpv/.MPVActivity` became the resumed
  activity and rendered the stream; Back returned to `app.sloppatv/.SloppaNativeActivity`, where
  sloppaTV received `success=1`, `completedKnown=1`, and `completed=1`. The player setting was then
  restored to INTERNAL, both temporary player apps were removed, and the fixture was deleted.
- 2026-09-04: physical refresh-rate matching is verified with a disposable video-only 23.976 fps
  fixture. With Google TV's Match content frame rate preference at its original `Never` value,
  sloppaTV correctly requested fixed-source 23.976 fps but Android kept the display at 60 Hz. With
  that user preference temporarily changed to `Always`, the same request switched the actual
  3840x2160 output from 60.000004 Hz to 23.976 Hz; leaving playback logged a successful preference
  clear and restored 60 Hz. The Google TV preference and sloppaTV's own matching setting were both
  restored to their original disabled state afterward, and the fixture was removed.
- 2026-09-04: a batched physical-TV acceptance pass closed ten previously-partial subitems. A disposable
  Mixed library containing two video-only movies opened from the real Home `MY MEDIA` row and rendered a
  populated native grid, completing Mixed/folder browse acceptance. Watched indicators were toggled OFF/ON
  against a played disposable item and visibly disappeared/reappeared; Clock was toggled OFF/ON on Home;
  Backdrops were exercised as OFF and CLEAR on Hell's Paradise Details and restored to BLURRED. The native
  player visibly exposes only the intended PAUSE/AUDIO/SUBTITLES control set. Existing physical metadata-refresh
  acceptance supplies the native notice/banner visual pass. A 20-cycle silent player enter/exit stress produced
  no app fatal exception, native signal, or ANR; together with the prior 31-sample 15-minute soak this closes the
  dedicated teardown/crash and memory-profile acceptance subitems. All changed settings were restored and all
  disposable libraries/files were removed afterward.
- 2026-09-04: a silent isolated subtitle library reproduced the physical NativeActivity overlay failure with both
  embedded SRT and ASS: Media3 delivered the selected SRT cue and libass built the ASS track/render, but Java overlay
  views produced no pixels above the native EGL surface. Selected SRT/VTT now stay on the native GLES renderer and
  selected ASS/SSA use Jellyfin's `Stream.srt` conversion into that same renderer. A fast local-server race was also
  fixed so subtitle cues can commit before the Player screen transition instead of being silently discarded. Physical
  captures verified SRT and ASS dialogue plus LARGE/ON/HIGH versus SMALL/OFF/LOW size, background, and position
  variants. The original LARGE/OFF/HIGH settings were restored and the temporary library/files were removed.
- 2026-09-04: production-signed caracal branding is visually accepted on the Google TV launcher: the
  installed `sloppaTV` tile renders the caracal artwork, app name, and Jellyfin-for-TV subtitle after
  an in-place update with the matching production key.
- 2026-09-04: repeated clean production-key builds have identical ZIP entries, metadata, timestamps,
  and payload hashes; whole-APK SHA-256 differs only in the Android APK Signing Block. CI still creates
  a fresh one-day signing identity on every run, so stable CI artifact signing remains unresolved and
  the combined release-engineering item stays partial.
- 2026-09-04: the production-signed 20/5/5 physical-TV performance suite measured sloppaTV versus the
  installed `org.jellyfin.androidtv` development build (`0.0.0-dev.1`) on the same Google TV Streamer.
  sloppaTV cold-launch median was 242.5 ms versus 404.0 ms, settled PSS 40,192 KB versus 152,328 KB,
  and rapid-DPAD p95 16.73 ms versus 33.34 ms. Median frame cadence and sampled idle CPU tied at
  16.67 ms and 0.0%. Raw samples are tracked under `docs/benchmarks/` and summarized in the README.
- 2026-09-04: physical-TV capture tooling now wakes a sleeping target before visual acceptance;
  this was added after a sleeping streamer produced valid-size but entirely black screenshots.
- 2026-09-04: the production-signed streamer install exposed overlapping Search result metadata.
  Bounded single-line card titles and portrait/landscape-aware row heights are now accepted on the
  physical streamer against both mixed `Friends` results and a single HDR episode, with no metadata
  collisions and a clean fatal/ANR log audit.
- 2026-09-04: Glass's production signing material was recovered onto Astra after verifying its
  Release certificate SHA-256 exactly matches the installed `app.sloppatv` certificate. Clean
  production-signed Releases now update the streamer successfully with `adb install -r` while
  preserving app data; later acceptance builds include the merged caracal branding commit.
- 2026-09-04: Settings acceptance exercised and restored AUTO/LARGE/EXTRA LARGE buffers, direct
  versus stereo audio mode, FIT/FILL zoom, refresh-rate matching, and a temporary Quick Connect
  user switch. Cleanup accidentally removed the saved `lounge` profile instead of the temporary
  account; the TV is currently usable on the saved `jellyfin` profile, but `lounge` must be
  reauthorized before user-switching acceptance can be considered complete.
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
