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

Status at 2026-09-04: 111 / 139 verified (79.9%); 28 partial; none unbuilt.

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

- [~] LAN discovery remains; distinct-user switching, Quick Connect, saved-profile lifecycle/expiry cleanup, and saved-server endpoint switching are physically verified.
- [x] Navigation regression, server-version edge cases, and Movies/Shows/Mixed/folder browsing are physically verified.
- [x] Seasons, nested folders, populated favorites, direct item-options keys.
- [x] Delete and refresh permissions plus server-side mutation completion.

### Playback and platform

- [x] Target-TV representative codec/audio-route and dynamic long-play soak matrix, AVC/HEVC/HDR override negotiation, and physical 23.976 Hz refresh-rate switching are verified.
- [x] Playback reports have automated Jellyfin session-state assertions.
- [x] Subtitle size/background/position and dedicated ASS/SRT visual passes are physically verified on the Google TV Streamer.
- [x] HDR fixture/HDR-preserving server-stream path and generated Trickplay tile rendering are verified on the Google TV Streamer.
- [x] External-player discovery, selection, silent playback handoff, and return are verified on the Google TV Streamer.
- [~] Voice search needs a real microphone/recognizer invocation.
- [x] DreamService normal-idle launch, native render, and DPAD dismissal are physically verified.
- [x] Launcher banner/icon reinstalled and visually verified with the matching key.

### Settings, resilience, and release

- [x] User switching E2E, buffer presets, audio mode, watched-indicator, clock, backdrop, zoom, and refresh-rate display preferences are physically verified and restored.
- [x] Wake/retry, lifecycle, repeated player teardown, crash/ANR auditing, and memory soak coverage are physically verified.
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
  variants. A later physical SPY x FAMILY S1E1 pass lowered the LOW subtitle baseline closer to the bottom edge while
  keeping active player controls clear: a live English cue rendered at the new no-overlay baseline and another cue
  remained above the progress/control area while the timed overlay was visible. Media3 stayed READY, the app-scoped
  fatal/native-signal/ANR audit was clean, and the setting itself was left unchanged. The original isolated-fixture
  settings were restored and the temporary library/files were removed.
- 2026-09-04: production-signed caracal branding is visually accepted on the Google TV launcher: the
  installed `sloppaTV` tile renders the caracal artwork, app name, and Jellyfin-for-TV subtitle after
  an in-place update with the matching production key.
- 2026-09-04: DreamService normal-idle acceptance passed on the physical Google TV Streamer. With
  `app.sloppatv/.SloppaDreamService` temporarily selected and the idle timeout reduced to five seconds,
  Android launched the real non-preview dream, the native GLES clock rendered at 1920x1080, and DPAD
  input dismissed it. Logs recorded clean renderer start/stop with no sloppaTV fatal exception, native
  signal, or ANR. Google Backdrop and the original 10-minute idle timeout were restored afterward.
- 2026-09-04: the broader physical playback matrix is complete on the Google TV Streamer. Six real-library
  cases reached live Jellyfin DirectPlay while muted: AV1 Main10 + Opus stereo, H.264 High10 10-bit + AAC,
  H.264 + E-AC3 5.1, H.264 + AC3 5.1, H.264 + DTS 5.1, and HEVC Main10 + FLAC 5.1. A separate 10-minute
  dynamic HDR10 HEVC playback soak remained active as Jellyfin DirectStream and advanced normally. The
  initial decoder/buffer warm-up raised memory to its operating plateau; across the 24 samples from 126 s
  onward, median PSS changed from 233,804 KB to 233,693 KB (-0.05%) and RSS from 305,117 KB to 305,733 KB
  (+0.2%), with no sloppaTV fatal exception, native signal, or ANR. Media volume was restored to 15/15.
- 2026-09-04: saved-server/profile lifecycle acceptance is physically complete without touching the missing
  `lounge` user. The existing `jellyfin` account was Quick-Connected through the still-valid alternate
  `https://arr.presley.nz/jellyfin` endpoint, producing two saved endpoint profiles. Switching back to the
  canonical endpoint exercised the real expired-token path: Home received 401, the stale profile was removed,
  and Login displayed `SESSION EXPIRED - LOG IN AGAIN`. Canonical Quick Connect then issued a fresh session,
  Home loaded normally, the temporary alternate endpoint was forgotten through the UI, and the chooser was
  restored to exactly one `jellyfin` profile at `https://jellyfin.presley.nz`. Final app audit was clean.
- 2026-09-04: persisted AVC/HEVC/HDR playback overrides are physically accepted on the production
  Google TV build. `1917` (H.264 High Level 4.1) DirectPlayed with AVC AUTO, then AVC 4.0 explicitly
  removed direct H.264 from the device profile and the same item reached READY through Transcode.
  SPY x FAMILY `FOLLOW MAMA AND PAPA` (HEVC Main 10 Level 5.1 HDR10) used HDR-preserving HEVC
  DirectStream with HDR AUTO, switched to H.264/AAC Transcode under SDR ONLY after direct HEVC was
  rejected for HDR, and returned to HEVC DirectStream under ALLOW ALL. AVC and HDR were restored to
  AUTO, media volume was restored to 15/15, and the final app-scoped fatal/native-signal/ANR audit was clean.
- 2026-09-04: Quick Connect TV UI is physically accepted on the production Google TV build. From
  USERS & SERVERS, ADD ANOTHER ACCOUNT retained the current server, QUICK CONNECT rendered code
  `265624` with the authorization instructions/waiting state, and the normal Jellyfin Quick Connect
  authorize endpoint approved it as the existing `jellyfin` user. The app observed authorization on
  its five-second poll, completed `AuthenticateWithQuickConnect`, returned to a populated Home in
  159 ms, and retained exactly one deduplicated saved `jellyfin` profile. The app-scoped log audit was clean.
- 2026-09-04: physical navigation-stack regression is complete on the production Google TV build.
  External `ACTION_VIEW` opened Firefly `Trash` Details and Back restored Home; external `ACTION_SEARCH`
  for `1917` rendered Search and Back restored Home; Home Movies opened the real Movies grid, `1917`
  Details, Back restored the same Movies grid/focus, and a second Back restored Home; Settings likewise
  returned to Home through Back. The final app-scoped log audit found no fatal exception, native signal,
  or ANR across the sequence.
- 2026-09-04: server-version compatibility notices are physically accepted with the isolated loopback
  Jellyfin fixture on the Google TV Streamer. A synthetic 10.9.11 server loaded Home while showing the
  persistent below-10.10 upgrade warning, and a synthetic `dev` version loaded Home while showing the
  transient unrecognized-version compatibility warning. Both runs completed without a sloppaTV fatal
  exception or ANR. The original session was restored byte-for-byte, ADB reverse was removed, and the
  production Release returned to the real server afterward.
- 2026-09-04: playback-buffer and audio-output settings are physically accepted on the Google TV
  Streamer. The same Hell's Paradise episode produced about 50.6 s buffered with AUTO, 106.2 s with
  LARGE, and 213.2 s with EXTRA LARGE after their respective settling intervals; Media3 also logged
  the configured default/50-120 s/80-240 s load-control policies. Firefly `Trash` (AAC 5.1) selected
  DirectPlay with the 8-channel direct route, while DOWNMIX TO STEREO capped negotiation to two channels,
  disabled audio stream copy, selected DirectStream, and Jellyfin `/Sessions` reported AAC stereo with
  `AudioChannelsNotSupported` as the only transcode reason. Buffer and audio settings were restored to
  AUTO and DIRECT / 8CH ROUTE, playback was stopped, and media volume was restored to 15/15 afterward.
- 2026-09-04: the resilience gate is physically complete. A production-key Debug build temporarily
  used an opaque backed-up session plus an ADB-reverse loopback Jellyfin fixture on Glass. The fixture
  intentionally severed the first safe `/Views` GET; the real JNI/HttpURLConnection path logged an
  unexpected-end-of-stream failure, waited 250 ms, retried, and rendered the fixture Home successfully
  in 488 ms. A separate physical sleep/wake cycle changed Android from Awake to Asleep to Awake while
  preserving the same sloppaTV PID and restored `SloppaNativeActivity` with a clean 1920x1080 EGL
  reattach and no fatal exception, native signal, or ANR. The original session was restored byte-for-byte,
  ADB reverse was removed, and the production Release was reinstalled in place afterward.
- 2026-09-04: repeated clean production-key builds have identical ZIP entries, metadata, timestamps,
  and payload hashes; whole-APK SHA-256 differs only in the Android APK Signing Block. CI now accepts
  a stable production keystore and credentials from Actions secrets, with an ephemeral identity fallback
  for runs where secrets are unavailable. The repository secrets are not yet populated, so stable CI
  artifact signing remains unresolved and the combined release-engineering item stays partial.
- 2026-09-04: the production-signed 20/5/5 physical-TV performance suite measured sloppaTV versus the
  installed `org.jellyfin.androidtv` development build (`0.0.0-dev.1`) on the same Google TV Streamer.
  sloppaTV cold-launch median was 242.5 ms versus 404.0 ms, settled PSS 40,192 KB versus 152,328 KB,
  and rapid-DPAD p95 16.73 ms versus 33.34 ms. Median frame cadence and sampled idle CPU tied at
  16.67 ms and 0.0%. Raw samples are tracked under `docs/benchmarks/` and summarized in the README.
- 2026-09-04: card/button/text-field labels now use measured horizontal and vertical centering, including
  login controls, keyboard keys, profile actions, browse filters, Search/Settings fields, queue actions,
  player/detail primary actions, confirmation buttons, status pills, and artwork-placeholder cards.
  Rounded rectangles/outlines now feather their edges instead of exposing six-segment corner faceting.
  Physical-streamer Home, Settings, focused Settings, and placeholder-card screenshots were inspected
  with a clean fatal/ANR audit. An exact clean-tree versus modified 20/5/5 Benchmark comparison found
  startup 252.5 -> 246.0 ms median, identical 16.67/16.73 ms navigation median/p95 and 0.0% idle CPU;
  the sub-1.2% upward memory medians had strongly overlapping samples and no distinguishable 5-vs-5
  permutation result. Raw before/after evidence is tracked under `docs/benchmarks/`.
- 2026-09-04: Home/navigation/Search/Settings acceptance is complete for the couch-UI pass. Long-holding
  a Home media card opens a compact popover over the existing screen, repeat key events remain consumed
  until button-up, and the first Favorite action does not fire repeatedly. Home vertical focus now holds
  the current two-row viewport until focus would leave it; physical 0→1, 1→2, and 2→1 transitions verified
  the expected no-scroll/one-scroll/no-scroll behavior. Search executes after a 180 ms typing debounce and
  physically returned `spy` results without a submit action; episode hits render inherited series cover art.
  Backdrops now render on Home when enabled, Home labels are larger, and Settings opens on common controls
  without the unusable section rail, with codec/bitrate/HDR/device controls behind Advanced Settings.
  A real Brooklyn Nine-Nine `48 Hours` Continue Watching card was hidden, disappeared from Home, then the
  Brooklyn Nine-Nine series was searched and `PLAY NEXT` reached Media3 READY/first-frame playback; after
  playback exit and Home reload the hidden `48 Hours` card was present again. Media volume was restored to
  15/15 and the app-scoped fatal/native-signal/ANR audit was clean. The exact pre-change APK versus new
  production-signed Release 20/5/5 regression suites showed unchanged 16.67 ms navigation median, 16.72→16.73
  ms p95, 0.8% >20 ms and 0.0% idle CPU; startup mean moved 249.2→253.0 ms but was not distinguishable in a
  200,000-draw permutation check (p≈0.640). A follow-up 5-old/5-new build-interleaved memory control reversed
  the initial sequential RSS shift, with median PSS 46,153→44,952 KB and RSS 112,707→111,474 KB. Raw evidence
  is tracked in `docs/benchmarks/home-navigation-settings-regression-2026-09-04.json`.
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
  user switch. A later physical check found both saved `lounge` and `jellyfin` profiles present on
  the canonical `https://jellyfin.presley.nz` endpoint. Switching `lounge` -> `jellyfin` rendered
  the distinct `jellyfin` Home and avatar in 178 ms primary-row readiness; switching back to
  `lounge` restored its distinct Home in 173 ms. Both transitions completed without a sloppaTV
  fatal/native-signal/ANR finding, and the original `lounge` session was restored afterward.
- 2026-09-04: architecture cleanup reduced the `SloppaApp` monolith by moving settings/search presentation policy,
  session persistence, and subtitle parsing into cohesive host-testable units; consolidated duplicated JVM attach/detach
  lifetime handling into one JNI utility; removed unused Jellyfin/player APIs and the unreachable Media3/libass subtitle
  overlay path; and dropped the now-unused `ass-media`/`media3-ui` dependencies. The full host suite and optimized
  `assembleRelease` pass, the signed release updated the physical Google TV Streamer in place with its existing session
  intact, and the app rendered normally after wake. The first playback smoke was temporarily blocked by local DNS;
  connectivity later recovered and subsequent physical playback acceptance passed.
- 2026-09-04: the Settings vertical slice now owns its search/filter/navigation state, setting mutation rules,
  side-effect flags, and displayed values instead of scattering those across `SloppaApp`. Host tests cover focus,
  scrolling, filtering, common/advanced switching, mutation effects, and value formatting. The optimized Release
  passed the authenticated physical-TV screenshot suite; manual streamer acceptance also verified NORMAL -> LARGE ->
  NORMAL restoration, search focus, full-list scrolling to Advanced Settings, and the advanced/common transition with
  no fatal/native-signal/ANR finding.
- 2026-09-04: the Home vertical slice now owns row/toolbar focus, per-row selection, visible-row scrolling, long-press
  state, and Home artwork-selection policy. Home-only helpers were removed from the generic UI policy module and their
  tests moved beside Home behavior. The full host suite and optimized Release build pass; the physical streamer passed
  the authenticated screenshot suite, row navigation/scrolling and long-press item-menu acceptance with no fatal,
  native-signal or ANR finding. A 10-startup/3-memory/3-navigation optimized-device checkpoint measured 240.0 ms median
  cold launch, 39,725 KB PSS, 0.0% idle CPU, 16.67 ms navigation median, 16.71 ms p95 and 0.8% intervals over 20 ms,
  matching or improving the prior 16.67/16.73/0.8% navigation checkpoint rather than introducing a hot-path regression.
- 2026-09-04: asynchronous ownership is now split into independent auth, Home, Search, content, playback and session
  epochs instead of one global generation counter. Starting Details no longer cancels Home enrichment, Search no longer
  invalidates unrelated content, and server mutations are tied only to the authenticated-session lifetime. Home refresh
  and server-mutation busy state are also separate from foreground loading so a background completion cannot clear an
  unrelated screen's loading state or leak a Home-only warning onto another screen. Host tests cover epoch invalidation
  and cross-domain independence; the optimized Release build and authenticated streamer screenshot suite pass. Physical
  DirectPlay of Brooklyn Nine-Nine `48 Hours` then passed Jellyfin session-state acceptance through progress, pause,
  stable pause, resume and stop with media volume restored to 15/15 and no fatal/native-signal/ANR finding. The matching
  10/3/3 performance checkpoint measured 242.5 ms median cold launch, 39,968 KB PSS, 0.0% idle CPU and 16.67/16.72 ms
  navigation median/p95 with 0.8% intervals over 20 ms, effectively unchanged from the immediately preceding
  240.0 ms, 39,725 KB, 0.0%, 16.67/16.71 ms and 0.8% checkpoint.
  `main.cpp` is now 6,456 lines, down from 7,194 before the architecture pass despite adding explicit request ownership.
- 2026-09-04: Jellyfin playback negotiation now delegates device/video/audio/subtitle capability planning, direct-request
  flags, and server-route classification to the host-testable `playback_profile.hpp` instead of embedding those decisions
  inside the HTTP method. Tests cover codec level/profile/resolution/HDR rejection, audio-route limits, client-vs-server
  subtitle handling, force-transcode behavior, and Jellyfin's semantic DirectStream case. The full host suite and optimized
  Release build pass, as does the real-server negotiation probe against Jellyfin 10.11.11. On the physical Google TV
  Streamer, Brooklyn Nine-Nine `48 Hours` remained DirectPlay with its AAC stream, then switching on its external SUBRIP
  track remained DirectPlay, loaded 593 native cues, and rendered subtitles on screen. The corresponding resolved-target
  to first-frame checkpoints were 0.994 s without subtitles and 1.001 s after the subtitle stream restart. No fatal,
  native-signal or ANR finding was observed; media volume was restored to 15/15 and the streamer returned to sleep.
- 2026-09-04: Jellyfin data-only domain structs now live in `jellyfin_types.hpp`, leaving `jellyfin.hpp` focused on the
  client/API boundary instead of forcing feature state to depend on JNI/HTTP declarations. The Browse vertical slice now
  owns its content mode, filter focus/selection, pagination, genre/letter state, nested-container history, headings, and
  cached-item removal in `BrowseScreenState`; host tests cover filter transitions and nested restoration. The full host
  suite and optimized Release build pass. Physical streamer acceptance verified Movies browsing, Genres -> Action -> Back,
  A-Z -> A -> Back, and Collections -> Braveheart Collection -> member -> Back restoration with no fatal/native-signal/ANR
  finding. A matching 10/3/3 optimized-device checkpoint measured 237.5 ms median cold launch, 39,543 KB PSS, 0.0% idle
  CPU, and 16.67/16.73 ms navigation median/p95 with 0.8% intervals over 20 ms, effectively flat or improved versus the
  immediately preceding 242.5 ms, 39,968 KB, 0.0%, 16.67/16.72 ms and 0.8% checkpoint.
- 2026-09-04: Search query/results/selection, fallback-keyboard state, loading, and live-search debounce now belong to
  `SearchScreenState` instead of seven unrelated `SloppaApp` fields. Host tests cover debounce timing/cancellation,
  stale-result rejection, query editing, result replacement and grid navigation. The full host suite and optimized Release
  build pass. Physical streamer acceptance verified ACTION_SEARCH for `brooklyn`, then live editing through the Android TV
  text input to `fallout`, with the result grid updating to the new query and no fatal/native-signal/ANR finding. A matching
  10/3/3 checkpoint measured 237.0 ms median cold launch, 39,399 KB PSS, 0.0% idle CPU and 16.67/16.73 ms navigation
  median/p95 with 0.8% intervals over 20 ms, unchanged or slightly improved from the preceding Browse checkpoint. Media
  volume was restored to 15/15 and the streamer returned to sleep.
- 2026-09-04: Details-screen action focus, More Like This focus/selection, item-menu navigation/delete confirmation, and
  cast-grid focus now live together in `DetailsScreenState` instead of eight independent `SloppaApp` fields. Action and
  item-menu label derivation moved with that state so user-data changes and focus rules stay beside the UI behavior they
  affect. Host tests cover action/menu bounds, similar-item selection, delete-confirmation defaults, and cast-grid movement;
  the full host suite and optimized Release build pass. Physical streamer acceptance on Brooklyn Nine-Nine `48 Hours`
  verified Details action navigation, opening and moving through Cast, returning with action focus preserved, and opening
  and navigating the More item menu without mutating Jellyfin state. No fatal/native-signal/ANR finding was observed. The
  matching 10/3/3 checkpoint measured 242.0 ms median cold launch, 39,833 KB PSS, 0.0% idle CPU and 16.67/16.73 ms
  navigation median/p95 with 0.8% intervals over 20 ms, within the established run-to-run range. Media volume was restored
  to 15/15 and the streamer returned to sleep.
- 2026-09-05: `DetailsScreenState` now also owns the subordinate Person -> titles and Series -> Seasons -> Episodes state,
  including selections, cached user-data updates/removal, and nested restoration. This removes nine more feature-specific
  fields from `SloppaApp`; `main.cpp` is 6,440 lines versus 7,194 before the architecture pass. Host tests cover person,
  season and episode selection plus cache mutation/removal, and the full host suite and optimized Release build pass.
  Physical streamer acceptance verified Cast -> Andy Samberg -> titles, Brooklyn Nine-Nine -> Seasons -> Season 2 ->
  Episodes, Back restoring Season 2 focus, and Back restoring the Series Details screen with its Episodes action focus.
  No fatal/native-signal/ANR finding was observed. The matching 10/3/3 checkpoint measured 241.0 ms median cold launch,
  40,471 KB PSS, 0.0% idle CPU and 16.67/16.72 ms navigation median/p95 with 0.8% intervals over 20 ms, remaining within
  the established run-to-run range. Media volume was restored to 15/15 and the streamer returned to sleep.
- 2026-09-05: Playback queue ownership is now localized in `PlaybackQueueState` rather than six independent `SloppaApp`
  fields. Queue item/current-index state, overlay visibility, item/action focus, repeat mode, reordering, removal, shuffle,
  and autoplay advancement now move together behind one feature boundary, while playback resolution remains in the app
  coordinator. Host tests cover queue replacement, selection clamping, movement/removal, repeat cycling, lookup/matching,
  shuffle and autoplay behavior; the full host suite and optimized Release build pass. Physical streamer acceptance used
  Brooklyn Nine-Nine `PLAY ALL` to build a 152-item queue, opened and navigated the overlay, cycled repeat through ONE/ALL
  and back to OFF, closed it, and stopped playback with no fatal/native-signal/ANR finding. Two confirming 10/3/3 checkpoints
  measured 248/243 ms median cold launch, 39,810/39,875 KB PSS, 0.0% idle CPU and 16.67/16.73-16.74 ms navigation
  median/p95 with 0.8% intervals over 20 ms, remaining within the established run-to-run range. Media volume was restored
  to 15/15 and the streamer returned to sleep.
- 2026-09-05: Login, Quick Connect, discovery and saved-profile focus now live together in `AccountScreenState` instead of
  eight independent `SloppaApp` fields. It owns the three login values, field/action focus, fallback-keyboard mode, Quick
  Connect code/lifetime, discovery status, saved-profile selection and USE/FORGET action focus; authentication/network work
  remains in the app coordinator. Host tests cover field editing, login/action wrapping, Quick Connect lifecycle, discovery
  status, account initialization and profile navigation. The full host suite and optimized Release build pass. Physical
  streamer acceptance verified saved-profile row/action navigation, the Add Another Account login screen, wrapping from the
  login action row to SAVED USERS, and restoring the original `lounge` session without deleting accounts or clearing app
  data. No fatal/native-signal/ANR finding was observed. The matching 10/3/3 checkpoint measured 240.0 ms median cold launch,
  39,647 KB PSS, 0.0% idle CPU and 16.67/16.74 ms navigation median/p95 with 0.8% intervals over 20 ms, remaining within the
  established run-to-run range. `main.cpp` is now 6,337 lines versus 7,194 before the architecture pass. Media volume was
  restored to 15/15 and the streamer returned to sleep.
- 2026-09-05: Audio/subtitle selection state now lives in `PlayerTrackState` instead of nine independent `SloppaApp` fields.
  The feature boundary owns selected server stream indices, carried language preferences, subtitle-load ownership, active
  native cues/language and cue lookup, while playback negotiation and transport remain in the app coordinator. Host tests
  cover subtitle-work exclusion, stream/preference state, cue activation boundaries, failure cleanup and full-session reset;
  the full host suite and optimized Release build pass. Physical streamer acceptance resumed Brooklyn Nine-Nine `48 Hours`,
  verified `AUDIO ENG` / `SUBTITLES OFF`, selected the English SUBRIP track, logged 593 parsed native cues and visibly rendered
  dialogue, then cycled subtitles back to OFF. The acceptance run's temporary playback progress was returned exactly to the
  pre-test 3:51 position before stopping, and no fatal/native-signal/ANR finding was observed. Two 10/3/3 checkpoints measured
  252/239 ms median cold launch, 40,332/39,936 KB PSS, 0.0% idle CPU and 16.67/16.72-16.73 ms navigation median/p95 with 0.8%
  intervals over 20 ms, matching the established run-to-run range. `main.cpp` is now 6,292 lines versus 7,194 before the
  architecture pass.
- 2026-09-05: Player control/overlay and seek telemetry state now lives in `PlayerScreenState` instead of seven independent
  `SloppaApp` fields. It owns control focus, overlay lifetime, cached position/duration, pending seek targets and the post-seek
  telemetry holdoff; the corresponding player-only policy was removed from the generic `ui_policy.hpp`. Host tests cover
  control clamping, overlay/back behavior, playback initialization, seek holdoff/acceptance and session reset; the full host
  suite and optimized Release build pass. Physical streamer acceptance resumed Brooklyn Nine-Nine `48 Hours` at the preserved
  3:51 position, verified player control focus, Back dismissing controls without exiting, a paused 3:51 -> 4:01 -> 3:51 seek
  round trip, and a subsequent Back exiting playback while preserving the original progress. No fatal/native-signal/ANR
  finding was observed. The matching 10/3/3 checkpoint measured 244.5 ms median cold launch, 39,864 KB PSS, 0.0% idle CPU and
  16.67/16.72 ms navigation median/p95 with 0.8% intervals over 20 ms, within the established run-to-run range. `main.cpp` is
  now 6,253 lines versus 7,194 before the architecture pass.
- 2026-09-05: Trickplay preview ownership now lives in `TrickplayPreviewState`, with the pure decoded-image value type split out
  of the JNI decoder header. Preview item/tile identity, loading/failure state, decoded pixels, texture generation, seek
  position and visibility lifetime no longer sit as independent `SloppaApp` fields. Host tests cover tile identity, preview
  expiry, decoded-image readiness, texture lifecycle metadata, failure and reset; the full host suite and optimized Release
  build pass. Physical streamer acceptance resumed Brooklyn Nine-Nine `48 Hours` at the preserved 3:51 position, exercised a
  paused seek to 4:01 and back to 3:51 through the trickplay request path, then stopped without changing the saved progress.
  This item does not expose a visible trickplay tile on the streamer, so acceptance covered the no-preview path plus clean
  fatal/native-signal/ANR logs. The matching 10/3/3 checkpoint measured 234.5 ms median cold launch, 40,529 KB PSS, 0.0% idle
  CPU and 16.67/16.72 ms navigation median/p95 with 0.8% intervals over 20 ms, within the established run-to-run range.
  `main.cpp` is now 6,229 lines versus 7,194 before the architecture pass.
- 2026-09-05: Playback handoff/restart state now lives in `PlaybackTransitionState` rather than scattered pending-target,
  restart-pause, stream-restart, audio-index, transition-loading and transcode-fallback fields in `SloppaApp`. The state owns
  one coherent staged transition plus loading/fallback flags and pause-after-restart behavior; host tests cover staging/taking,
  restart metadata, loading/fallback state and reset. The full host suite and optimized Release build pass. Physical streamer
  acceptance resumed Brooklyn Nine-Nine `48 Hours` at the preserved 3:51 position, kept playback paused while enabling the
  English SUBRIP track through a real stream restart, parsed all 593 cues, rendered the cue at 3:51, then disabled subtitles
  again and exited without changing saved progress. No fatal/native-signal/ANR finding was observed. The matching 10/3/3
  checkpoint measured 246.0 ms median cold launch, 39,975 KB PSS, 0.0% idle CPU and 16.67/16.74 ms navigation median/p95
  with 0.8% intervals over 20 ms, within the established run-to-run range. `main.cpp` is now 6,205 lines versus 7,194 before
  the architecture pass.
- 2026-09-05: Android launch-intent decoding is now isolated in `launch_intent.{hpp,cpp}` instead of a ~100-line JNI parser in
  `main.cpp`. Cold-start and runtime intents share one normalization path for Jellyfin item VIEWs and SEARCH queries, with a
  host test covering action routing and normalization. The full host suite and optimized Release build pass. Physical streamer
  acceptance verified a cold-start VIEW opening Brooklyn Nine-Nine `48 Hours` and a runtime SEARCH for `Brooklyn` returning
  Brooklyn Nine-Nine, with clean fatal/native-signal/ANR logs. The matching 10/3/3 checkpoint measured 244.5 ms median cold
  launch, 40,006 KB PSS, 0.0% idle CPU and 16.67/16.72 ms navigation median/p95 with 0.8% intervals over 20 ms, effectively
  unchanged from the immediately preceding checkpoint. `main.cpp` is now 6,126 lines versus 7,194 before the architecture pass.
- 2026-09-05: External-player handoff ownership now lives in `ExternalPlaybackState`, with the pure external-player app/result
  value types split out of the JNI adapter header. Pending launch and active-result correlation no longer live as two independent
  `SloppaApp` optionals; host tests cover staging, pending consumption, active ownership, result handoff and reset. The full host
  suite and optimized Release build pass. The physical streamer has no app resolving the external video-player intent, so the
  real external-player launch/return branch remains unavailable for device acceptance; the installed Release cold-launched to
  the authenticated Home screen with clean fatal/native-signal/ANR logs. The matching 10/3/3 checkpoint measured 248.0 ms median
  cold launch, 39,931 KB PSS, 0.0% idle CPU and 16.67/16.73 ms navigation median/p95 with 0.8% intervals over 20 ms, within the
  established run-to-run range.
- 2026-09-05: Saved Jellyfin account/session ownership now lives in `SessionRegistry` instead of a raw `SloppaApp` vector plus
  conversion, identity, remember and removal helpers. The registry owns stored/runtime conversion, identity matching, MRU
  ordering, the 16-session cap, import/export and safe indexed access. Host tests cover update-in-place, MRU ordering,
  persistence round-trips, device-id rebinding and removal. The full host suite and optimized Release build pass. Physical
  streamer acceptance opened the two saved profiles, switched from `lounge` to `jellyfin`, then restored `lounge` through the
  saved-user UI without clearing app data; clean fatal/native-signal/ANR logs were observed. The matching 10/3/3 checkpoint
  measured 229.0 ms median cold launch, 39,985 KB PSS, 0.0% idle CPU and 16.67/16.73 ms navigation median/p95 with 0.8%
  intervals over 20 ms. `main.cpp` is now 6,076 lines versus 7,194 before the architecture pass.
- 2026-09-05: Player window/focus restoration state now lives in `PlayerScreenState` instead of two independent `SloppaApp`
  booleans. The player-screen boundary owns pending window restoration and resume-on-focus intent alongside the existing control,
  overlay, position and seek state. Host tests cover paused/restoring, resume gating, consumption and session reset; the full host
  suite and optimized Release build pass. Physical streamer acceptance resumed Brooklyn Nine-Nine `48 Hours`, paused it at the
  preserved 3:51 position, sent the activity Home to force window/focus loss, then brought the existing task back. Media3/GLES
  restoration was preserved, playback remained paused at 3:51, and logs reported the preserved-context restore path with no
  fatal/native-signal/ANR finding. The matching 10/3/3 checkpoint measured 244.0 ms median cold launch, 39,919 KB PSS, 0.0% idle
  CPU and 16.67/16.73 ms navigation median/p95 with 0.8% intervals over 20 ms, within the established run-to-run range.
- 2026-09-05: JNI string conversion now has one shared implementation in `jni_env.hpp` instead of duplicated UTF acquire/release
  helpers in the app callback bridge, launch-intent bridge, Media3 adapter, external-player adapter, device-capability probe and
  HTTP exception path. This leaves subsystem-specific JNI exception reporting local while centralizing only the mechanical string
  lifetime behavior. The full host suite and optimized Release build pass. Physical streamer acceptance cold-started through a
  Jellyfin VIEW intent to Brooklyn Nine-Nine `48 Hours` with the authenticated session intact and clean fatal/native-signal/ANR
  logs. The matching 10/3/3 checkpoint measured 236.5 ms median cold launch, 39,532 KB PSS, 0.0% idle CPU and 16.67/16.72 ms
  navigation median/p95 with 0.8% intervals over 20 ms. `main.cpp` is now 6,056 lines versus 7,194 before the architecture pass.
- 2026-09-05: Next-episode/autoplay ownership now lives in `PlaybackContinuationState` instead of four independent `SloppaApp`
  fields. It owns the prefetched next item, next-episode request latch, autoplay-chain count and Still Watching prompt state, with
  dedicated host coverage for request gating, next-item lifecycle, chain counting and reset. The full host suite and optimized
  Release build pass. Physical streamer acceptance resumed Brooklyn Nine-Nine `48 Hours` at the preserved 3:51 position and loaded
  its real media-segment response while paused, with no fatal/native-signal/ANR finding. The matching 10/3/3 checkpoint measured
  243.0 ms median cold launch, 41,036 KB PSS, 0.0% idle CPU and 16.67/16.73 ms navigation median/p95 with 0.8% intervals over
  20 ms, within the established run-to-run range. `main.cpp` is now 6,050 lines versus 7,194 before the architecture pass.
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
