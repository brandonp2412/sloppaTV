# sloppaTV parity roadmap

Target: a fast Netflix-style on-demand Jellyfin TV client, using the official `jellyfin-androidtv` checkout at `ccf78eb90` (2026-08-30) as the behavioral reference for overlapping features. Supported media is Movies, Series and Episodes. Music, photos, Live TV/DVR, Android TV launcher channels/recommendations, and server administration are explicitly out of scope.

The architectural constraint remains: keep the application/UI in C++/Android NDK. JNI access to Android platform services is allowed where Android exposes no practical NDK equivalent. Avoid Kotlin/Java application source unless a future Android platform limitation makes a tiny bridge objectively necessary.

Performance is part of parity: where the official client and sloppaTV implement the same interaction, sloppaTV should be measurably no worse and should normally be faster/lighter on the target TV hardware. Claims must be backed by repeatable real-device measurements, not by assuming native code is faster.

## Status legend

- [x] Implemented and verified on target Android TV hardware
- [~] Implemented, but not yet fully end-to-end verified or has a known limitation
- [ ] Not implemented

## Current status — 2026-09-02

### Native foundation

- [x] Standalone Android TV project using `NativeActivity`, C++20 and GLES3
- [x] No Kotlin/Java application source
- [x] Native DPAD navigation and native on-screen TV keyboard
- [x] 1920x1080 rendering verified on target Android TV hardware
- [x] Builds for both `armeabi-v7a` and `arm64-v8a`; 32-bit Android userspace is supported on compatible targets
- [x] Native session file persistence and stable generated device id
- [x] Bounded four-worker async task runner; no detached app-level request/report/image threads
- [x] Async task completion wakes the native looper instead of requiring continuous redraw
- [x] Event-driven static screens with short 60 Hz render bursts after DPAD input
- [~] Structured navigation stack now replaces screen-specific return variables and is covered by host policy tests; full Waydroid remote-navigation regression pass is still pending
- [~] Native host policy/unit test runner now covers player seek/startup policy, UI/grid/control policy, navigation-stack behavior and server-version policy, alongside Python renderer tests; broader integration coverage is still required

### Authentication and servers

- [x] Manual server + username + password login via `/Users/AuthenticateByName`, verified against the real server/account
- [x] Automatic Jellyfin base-path discovery, including the real server mounted at `/jellyfin`
- [x] Persisted authenticated session and relaunch into Home
- [~] Quick Connect initiate/code/poll/complete implementation; real server code generation verified, authorization round-trip not yet exercised
- [~] Native UDP Jellyfin LAN discovery on port 7359, including interface broadcast addresses; the current test network returns no discovery response even to an independent probe
- [~] Multiple authenticated servers are retained as saved session profiles and can be switched without re-entering credentials; multi-server device E2E is still pending
- [~] Multiple saved users are retained per server, exposed through a native Users & Servers chooser, and can be switched without password re-entry while their token remains valid; device E2E is pending
- [~] Saved-profile management supports `USE`, `FORGET` and `ADD ANOTHER ACCOUNT`; expired 401 profiles are automatically removed, while avatar/PIN/reordering parity remains out of scope or incomplete
- [~] Server public-info/version diagnostics and a tested Jellyfin 10.10+ baseline warning are implemented; older/newer real-server compatibility cases still need device E2E

### Home and browsing

- [x] Authenticated Home screen on the real server
- [x] Continue Watching row with artwork and resume state
- [x] Next Up row
- [x] Recommended row
- [x] Favorites row
- [x] Recently Added rows per scoped video library
- [x] Video-only Jellyfin library views with direct Movies/Shows top navigation
- [x] Text search for Movie, Series and Episode, physically verified on the Google TV Streamer with native-keyboard and direct key-event entry returning real server results
- [x] Rich item-details screen with real server data
- [x] Series Play action resolves the next episode
- [~] Library browsing for Movies, Shows, Mixed and folders
- [~] Seasons and episode lists
- [~] Nested folder/collection browsing
- [x] Dedicated Collections / box-set browse mode verified on the physical streamer against real Collections data, including opening a collection into its member list
- [x] Server-native Genres browser/filter verified on the physical streamer, including opening Action into real filtered movie results
- [x] A-Z / by-letter browser verified on the physical streamer with server-side `NameStartsWith`, including opening A into populated real movie results
- [~] Dedicated Favorites filter implemented in Movies/Shows browse; current account has no movie favorites for a populated E2E case
- [x] Cast names remain visible on Details and a dedicated native Cast browser now uses Jellyfin person IDs, actor image tags/roles, and server-side PersonIds filtering to open titles featuring a selected actor; verified end-to-end on the physical streamer with Braveheart → Mel Gibson → Featuring Mel Gibson → Braveheart and full Back-stack restoration
- [~] Details now exposes a native `MORE` item-options menu for maintenance actions; broader card-level/context-key access and physical-TV UI verification remain pending
- [x] Mark watched/unwatched mutation verified end-to-end on the physical streamer and asserted against server `UserData`, with the test item restored to its original unwatched state
- [x] Favorite/unfavorite mutation verified end-to-end on the physical streamer and asserted against server `UserData`, with the test item restored to its original non-favorite state
- [~] Delete media is gated by Jellyfin `CanDelete` and requires a second destructive confirmation with Cancel selected by default; real permission/filesystem behavior still needs device/server E2E
- [~] Metadata refresh can be requested from the item-options menu using Jellyfin's default metadata/image refresh modes; real permission/server completion behavior still needs E2E

### Artwork and presentation

- [x] Primary poster/thumb artwork download and GLES rendering on the real Home/Details UI
- [~] Backdrop download/decode/render pipeline
- [x] In-memory decoded-image/texture cache
- [x] 48 MiB / 256-file bounded Home artwork disk cache keyed by Jellyfin image tags
- [x] Image fetch and decode off the render thread; GLES upload occurs on the render context
- [ ] Logos
- [~] Watched / favorite / progress indicators
- [x] Ratings, year, runtime, content rating, genres and richer metadata
- [x] Cast metadata plus actor/person images are rendered from real Jellyfin person data on the physical streamer
- [ ] Theme/backdrop behavior parity
- [x] Home row/item and Movies/Shows top-nav focus restoration across refresh/navigation
- [x] Accessibility-friendly global UI text sizing (`NORMAL` / `LARGE` / `EXTRA LARGE`) and overscan safe-area controls (`OFF` / `2%` / `4%` / `6%` per edge), persisted and verified at the worst-case Extra Large + 6% combination on the physical Google TV Streamer; UI/artwork/overlays stay inside the safe area while video remains edge-to-edge

### Video playback

- [x] PlaybackInfo negotiation against the real Jellyfin server
- [x] Direct-play preference with Jellyfin HLS transcode fallback
- [x] Hardware-backed Android `MediaPlayer` driven from C++ through JNI
- [x] Real Jellyfin video rendered through `SurfaceTexture` / `GL_TEXTURE_EXTERNAL_OES` into the GLES scene
- [x] Resume position verified with real content
- [x] Play/pause
- [x] Configurable skip-back/skip-forward seeking
- [~] Playback start/progress/stop reporting; current cached-position/ticks reporting and immediate Details resume-state update are exercised in Waydroid, but server-state assertions still need automated tests
- [x] End-to-end playback verified against the real Jellyfin server and streamer
- [x] Native Android MediaCodecList capability probing for video/audio decoder families
- [~] Device-aware direct-play codec/container/profile/resolution/HDR matching; real HEVC Main10/MKV direct play is verified, and user AVC/HEVC/HDR overrides now feed the same PlaybackInfo capability decision; real HDR title playback E2E remains pending
- [~] Playback targets now distinguish Jellyfin `DirectPlay`, `DirectStream` and `Transcode` in session reporting and diagnostics while retaining server-stream seek policy; a real DirectStream/remux session assertion is still pending
- [x] Native playback overlay with title, time and progress bar
- [x] Native seek/progress UI
- [~] Jellyfin chapter parsing remains available internally, but the chapter button/interactive chapter control was intentionally removed from the simplified player overlay
- [ ] Trickplay thumbnails
- [~] Jellyfin audio streams and server stream indices are parsed and the player restarts/resolves playback to change server audio streams; prior real-streamer dual-audio direct-play switching was verified, while the current Waydroid multi-audio restart matrix is still pending
- [x] Subtitle track selection and off/on UI; in Waydroid Blue Planet II repeatedly survived OFF → ENG → OFF cycles, rendered actual English subtitle text, and remained moving through subtitle-active seek/pause/resume testing
- [~] Persisted native subtitle size, background on/off and low/middle/high vertical-position controls feed the GLES SRT renderer; physical-TV visual verification and richer ASS/font styling remain pending
- [~] Native SRT renderer strips ASS overrides after Jellyfin conversion; full libass-equivalent ASS/SSA styling/positioning is still missing
- [x] Playback speed option intentionally removed from the scoped player UI
- [~] In-player zoom/fill option intentionally removed from the simplified player controls; the persisted default-video-zoom setting/render path still exists
- [~] Quality/max-bitrate selection feeds PlaybackInfo negotiation
- [~] Fixed-source refresh-rate requests/clear implemented through `ANativeWindow`; real 23.976 request succeeds, but the streamer has system `match_content_frame_rate=0`, so physical mode switching is intentionally blocked by Android policy
- [~] Native HDR10/HDR10+/Dolby Vision/HLG display probing and per-item gating implemented; persisted HDR override modes (`AUTO`, `SDR ONLY`, `ALLOW ALL HDR`) now alter capability gating, but the current enumerated library returned no HDR-range item for a physical-streamer override assertion
- [ ] Audio output/downmix/passthrough behavior parity
- [x] Next Up prefetch/overlay/autoplay; real Friends S2E10 → S2E11 end-of-episode transition verified on the streamer
- [~] Configurable Still Watching guard and prompt; autoplay threshold logic implemented, prompt still needs dedicated threshold E2E
- [x] Server-native media-segment skip actions (Intro/Outro/Recap/Preview/Commercial); real Fallout S1E2 `SKIP RECAP` verified to jump to the server-provided 0:58 segment end
- [~] Native queue management / play-next / Series play-all is implemented and physically exercised on the Google TV Streamer: Friends Play All de-duplicated 97 raw Jellyfin episode records into 73 season/episode slots; move-down, play-next reordering and remove were verified; `PLAY NOW` started real S2E13 playback; and `KEYCODE_MEDIA_NEXT` advanced S2E13 → S2E14 with clean async teardown/restart. The pass also found a stale duplicate S1E1 whose static stream is 404; Play All now HEAD-probes duplicate slots and prefers an available copy, with one final post-fix streamer navigation rerun still pending.
- [ ] Repeat and shuffle
- [ ] External player support (VLC/mpv/Vimu/MX-style flows)
- [~] Suspend/window-loss pauses playback; full Android audio-focus/HDMI lifecycle parity remains
- [~] Simplified player controls are now limited to PLAY/PAUSE, AUDIO and SUBS; control count/sizing is covered by native policy tests and visually verified in Waydroid
- [~] Direct-play seeks use a fresh server-stream restart path while already-transcoded HLS seeks in-place, avoiding the frozen-video/audio-only failure seen with direct `MediaPlayer` seeking and avoiding concurrent PlaybackInfo/transcode resolution
- [~] Server stream changes are serialized: the old Jellyfin playback session is reported/stopped before resolving the replacement stream, with paused state and logical playback position preserved across restarts

### Waydroid viewing-acceptance checkpoint — 2026-09-01

Current automated/device acceptance is intentionally being run on a 1920x1080 Waydroid TV-like target rather than physical Android TV hardware.

Verified on the current Waydroid worktree/build:

- Wonder Egg Priority uses the Jellyfin transcode path instead of waiting roughly 40 seconds on synchronous HEVC/Opus MKV direct-play preparation; observed startup to visible decoded video is roughly 1–2 seconds in the successful runs.
- Wonder Egg and Planet Earth III Home/episode artwork prefers the series Primary image, with versioned cache keys preventing stale incorrect episode art.
- Planet Earth III `SKIP INTRO` was exercised while genuinely active; video frames continued changing several seconds and again 10+ seconds after the skip, and subsequent forward/back/repeated seeks plus pause/resume retained moving video.
- Blue Planet II repeatedly survived subtitle OFF → ENG → OFF stream changes. Actual English subtitle glyphs were captured on-screen, and subtitle-active forward seek, backward seek, rapid alternating seeks, pause and resume all retained video output.
- The previously reproduced Blue Planet `ENG → OFF` black/dead decoder state no longer reproduced after serialized stream handoff and clean player teardown; measured frame pairs continued changing after the switch.
- Player teardown no longer calls blocking `MediaPlayer.stop()` on the NativeActivity/input thread. A reproduced >5 second ANR in `NuPlayerDriver::stop/reset` was removed by detaching the player and asynchronously performing terminal `release()`; healthy Waydroid teardown now logs start→finish in milliseconds.
- A poisoned Waydroid media service from earlier wedged NuPlayer instances was distinguished from application regressions by restarting the Waydroid session; Jellyfin authentication/app data survived and the same build resumed moving video afterward.
- Browse/search/episode navigation uses the same column count as rendering; native boundary policy tests cover the old mismatch, and the current readability redesign intentionally uses four larger columns.
- Home/media cards and player text/control sizing were increased for TV readability.

Still required before the viewing-acceptance effort is considered complete: a current-build multi-audio title cycle, final full Wonder Egg lifecycle/30–60 second pass, final Planet Earth exit/re-enter/resume pass, a dedicated direct-play matrix, 10+ cross-title decoder/surface transitions, beginning/middle/near-end seeks, back-during-loading/immediately-after-restart cases, autoplay/next-up on the current build, explicit Jellyfin server progress-state assertions, and a final crash/ANR/HTTP/decoder audit.

### Real-streamer readability/playback checkpoint — 2026-09-02

Verified on the physical Google TV Streamer at the app's 1920x1080 render target using the isolated `nz.presley.sloppatv.test` Debug build, leaving the existing production package/data untouched:

- Home was redesigned from four cards/three visible rows to three substantially larger cards/two visible rows; Movies/Browse/Search/episode grids now use four larger columns, and Settings uses seven larger rows. The new Home, Movies, Details, Settings and player-overlay layouts were visually inspected from real-device screenshots.
- Global header/navigation type, item titles/metadata, focus targets, subtitles, player status/times/progress/controls and settings values were enlarged for couch-distance readability.
- The non-debuggable Benchmark build retained **16.67 ms median / 16.71 ms p95** rapid-DPAD SurfaceFlinger cadence with **0.8% >20 ms** over an 80-event real-streamer sample after the redesign.
- Episode Details now renders a landscape artwork frame rather than stretching 16:9 episode imagery into a portrait poster slot; movie/series Details retain the poster layout.
- Real Friends S2E13 playback started successfully through Jellyfin's transcode path. Two screen captures two seconds apart differed during normal playback, after a forward seek, after a backward seek, after resume, and after exiting/re-entering playback, providing simple frame-motion evidence that decoded video remained live.
- After pausing and allowing the overlay to expire, two screen captures two seconds apart were byte-identical, then diverged again after resume.
- Two playback exits/re-entries completed with asynchronous `MediaPlayer` teardown start/finish logs and no observed app ANR, fatal exception or fatal native signal. Home repopulated after teardown in roughly 0.2 s for primary rows and roughly 0.5 s including enrichment in the observed runs.

This is a focused readability/playback regression pass, not the final codec/HDR/audio/long-soak matrix. A follow-up accessibility pass also verified the global `EXTRA LARGE` UI text preset together with a 6% per-edge safe area on Home, Settings and the player overlay without clipping; video remained full-frame beneath the inset overlay. The same target-TV pass also exercised Search, Genres→Action, A-Z→A, Collections→collection members, Diagnostics, reversible Favorite/Watched mutations with server-side state assertions, and the new cast/person-image browser including opening a person's filtered title list and navigating back through the stack.

### Explicitly excluded from scope

- Music/audio libraries and playback
- Photo libraries/viewer/slideshow
- Live TV, DVR, guide, recordings and channel changing
- Android TV launcher home-screen channels/recommendations

### Android TV platform integration
- [ ] Global/deep-link content intents
- [ ] Voice search
- [ ] Media session integration
- [ ] Screensaver / DreamService equivalent
- [ ] In-app screensaver
- [ ] Notification/message presentation where relevant
- [ ] Proper TV banner/icon assets

### Settings parity

- [x] Native settings navigation and persistence
- [x] Max streaming bitrate
- [ ] Playback buffer presets are present in the pinned Android TV reference (`AUTO`, `LARGE`, `EXTRA LARGE` via ExoPlayer `DefaultLoadControl`), but Android `MediaPlayer` exposes no equivalent buffer-duration control; parity requires either a different player backend or a deliberate buffering layer rather than a fake setting
- [~] Persisted max-audio-channel setting advertises either stereo (2-channel) or 8-channel capability through PlaybackInfo and the Jellyfin device/transcode profile, matching the reference client's downmix-vs-surround capability policy; device/server transcode/direct-play matrix pending
- [~] Subtitle size, background and vertical-position preferences persist; preferred language/default/forced playback-mode behavior remains incomplete
- [x] Skip-ahead / skip-back lengths
- [~] Next Up autoplay behavior
- [~] Still Watching threshold
- [~] Match-video-refresh-rate setting persists and controls native fixed-source requests; physical switching requires the TV's system match-content setting
- [~] Watched-indicator visibility setting is persisted and gates Home/grid/Details watched badges; physical-TV UI pass pending
- [~] Clock visibility setting is persisted and renders local 24-hour time in native headers; physical-TV UI pass pending
- [~] Backdrop visibility setting is persisted and gates Details backdrop fetching/rendering; physical-TV UI pass pending
- [x] Default video zoom mode
- [~] Persisted AVC 4.0–6.2, HEVC 4.0–6.2 and HDR Auto/SDR-only/Allow-all controls feed real PlaybackInfo negotiation. On the physical streamer, the same HEVC Main10 Level 5.0 Hell's Paradise S1E1 item selected `DirectPlay` with HEVC Auto, then rejected direct HEVC and selected/started `Transcode` when capped to HEVC 4.0. AVC and a real HDR-range title still need equivalent device assertions
- [ ] Screensaver preferences
- [~] User-select behavior now has a saved Users & Servers chooser and Settings `SWITCH USER` flow; physical-TV navigation/expiry/multi-server acceptance remains pending
- [x] Diagnostics screen reports app version/ABI, Jellyfin server version, detected decoder/HDR capability and last playback path; physically verified on the Google TV Streamer against the real server (Jellyfin 10.11.11) and the streamer's detected H264/HEVC/VP8/VP9/AV1/MPEG2 plus HDR10/HDR10+/Dolby Vision/HLG capabilities

### Reliability, performance and release engineering

- [x] UI avoids Compose/RecyclerView/View hierarchy overhead by design
- [x] Repeatable SurfaceFlinger DPAD navigation benchmark on the real streamer
- [x] Repeatable process-cold startup benchmark against installed official Jellyfin
- [x] Settled process memory measurement against installed official Jellyfin
- [x] Idle static screens consume effectively 0% process CPU in the sampled `top` snapshot
- [~] Bounded HTTP retry/backoff added for transient JNI/connection failures after TV wake; cancellation during the short retry sleep is still not wired through
- [x] Graceful partial Home failures: one failed row does not blank Home
- [x] Pagination/incremental 60-item loading for large libraries
- [~] Identical in-flight GETs are coalesced and successful non-media API GETs receive a bounded 5-second transport cache with mutation invalidation; Waydroid workload/regression profiling pending
- [x] Directional Home image prefetch around the focused card
- [x] Bounded thread pool instead of one thread per request/report
- [~] Waydroid E2E harness (`tools/waydroid_e2e.py`) requires an explicit ADB serial, validates Waydroid identity/1920x1080, captures screenshots/frame-difference motion evidence and filtered player logs; the final full matrix is still in progress
- [~] A real player-teardown ANR was reproduced and fixed in Waydroid; two current-build physical-streamer playback exit/re-entry cycles completed cleanly with asynchronous teardown start/finish logs, while a broader ANR/crash soak remains pending
- [~] Memory profiling baseline complete; leak/long-session profiling still required
- [~] Release-candidate performance comparison underway; cold-launch and memory release measurements captured, final multi-run release suite still required
- [~] Production release signing is environment-configurable and never silently falls back to a debug key; Debug/Benchmark acceptance builds use an isolated `.test` application ID so they can be installed beside production without clearing user data, and the separate Benchmark variant is non-debuggable/debug-signed while a real production-key signing pass remains pending
- [~] GitHub Actions workflow builds/tests Debug, Benchmark and Release with the pinned SDK/NDK/CMake toolchain and uploads APKs; both the push and pull-request hosted runs passed after fixing runner `sdkmanager` discovery, while artifact install/device validation remains pending
- [x] Version code/name are centralized in Gradle properties, compiled into native diagnostics, and tracked in `CHANGELOG.md`
- [~] Two clean Astra unsigned Release builds produced the identical SHA-256 `fbcb843ad6c88aafdd2f145f482525fcd246edbe47d9663429cdb1c9646a4367`; the check is scripted, while final production-signed reproducibility remains pending

## Roadmap hardening checkpoint — 2026-09-01

The Astra hardening branch adds a real navigation stack, native diagnostics/server-version checks, in-flight GET coalescing plus a short-lived API cache, watched/clock/backdrop settings, centralized versioning/changelog, production-signing configuration, a separate installable benchmark variant, host test orchestration, CI, and a repeatable release-reproducibility check. All host tests and Debug/Benchmark/Release Android builds pass locally across the configured ABIs. The first hosted push and pull-request CI runs are also green. Items that change visible Android TV behavior remain partial until the required Waydroid/target-TV acceptance pass is run.

## Roadmap continuation checkpoint — 2026-09-02

Direct-stream session reporting now preserves Jellyfin's distinct DirectPlay/DirectStream/Transcode methods; the native subtitle renderer has persisted size/background/position controls; Details has a permission-aware item-options menu for default metadata refresh and `CanDelete`-gated deletion with a destructive confirmation; max audio channels now feeds PlaybackInfo/device-profile negotiation as 2-channel stereo or 8-channel surround; and authenticated users/servers are retained as switchable saved profiles with local forget/add-account management and 401 expiry cleanup. The TV UI was then reworked around real-streamer couch readability: three large Home cards, four-column browse grids, larger typography/focus targets/settings/player controls, and aspect-aware episode Details artwork. Persisted global UI-size and overscan safe-area controls are now physically verified at their worst-case Extra Large + 6% combination, with video intentionally excluded from the safe-area transform. AVC/HEVC/HDR overrides now affect real Jellyfin capability negotiation; a same-title streamer test proved HEVC Main10 Level 5.0 changes from DirectPlay on Auto to a working Transcode when capped to HEVC 4.0. Native queue management now shares one ordered queue with episode autoplay, supports Series Play All, Play Now/Next, reordering/removal and media-next, and was exercised on the physical streamer through a real S2E13 → S2E14 playback transition. That pass exposed a stale duplicate Friends S1E1 library item: its Jellyfin metadata was present but its static stream was HTTP 404 and every HLS segment request returned server HTTP 500. Duplicate episode slots now probe static-source availability and prefer a working copy; direct-play failures without a supplied fallback URL can also renegotiate forced transcoding asynchronously. The final post-fix S1E1 navigation rerun remains pending because physical-device input injection was blocked by the execution environment. The focused real-streamer playback pass above verifies normal motion, forward/back seeking, pause/resume and exit/re-entry on a real transcoded episode, while broader feature-specific acceptance remains partial. The reference client's configurable buffer presets were also re-audited: they rely on ExoPlayer `DefaultLoadControl`, so the existing `MediaPlayer` backend cannot honestly expose equivalent buffer-duration controls without a backend/buffering-layer change.

## Current performance evidence

Measured on the same Android TV target against the currently installed official `org.jellyfin.androidtv` client. Historical navigation numbers below came from the native debug build; newer release-candidate startup/memory evidence is tracked in `PERFORMANCE.md` and the final frozen-release suite remains a milestone.

| Metric | sloppaTV | Official Jellyfin TV | Current result |
| --- | ---: | ---: | --- |
| Process-cold Activity launch, 10-run median | 656.5 ms | 2105 ms | sloppaTV ~3.2x faster |
| Process-cold Activity launch, 10-run mean | 662.3 ms | 2090.4 ms | sloppaTV ~68% lower |
| Settled Home total PSS | ~63 MB | ~136 MB | sloppaTV ~54% lower |
| Settled Home total RSS | ~138 MB | ~228 MB | sloppaTV ~39% lower |
| Settled Java heap | ~8.7 MB | ~39.1 MB | sloppaTV ~78% lower |
| Rapid DPAD SurfaceFlinger median present interval | 16.67 ms | 16.67 ms | tie at 60 Hz median |
| Rapid DPAD SurfaceFlinger p95 present interval | 16.74 ms | 50.02 ms | sloppaTV substantially lower tail latency |
| Rapid DPAD intervals >20 ms | 0.8% | 17.5% | sloppaTV substantially less jank in this test |
| Sampled idle Home CPU | 0.0% | 0.0% | tie |

The SurfaceFlinger test sends repeated left/right DPAD events through ADB and compares present intervals. It is useful for regression/comparison but is not a substitute for a full Perfetto input-to-photon trace. The benchmark implementation lives at `tools/benchmark_tv.py`.

## Milestones

### M0 — Native proof of concept

Status: **complete**. Login → authenticated Home → real Details → resume/play → hardware video/overlay/chapter seek has been proven end-to-end against a real Jellyfin server on target Android TV hardware.

### M1 — Usable daily-driver core

Status: **well underway**. The Netflix-style Home, direct Movies/Shows entry, real direct playback, dual-audio switching, native subtitles, media-segment skipping and episode autoplay are working. Collections/Genres/A-Z/Favorites browse filters and richer Details discovery are build-clean but await the next awake-TV visual pass. Remaining core blockers include Still Watching threshold E2E, real HDR title validation, trickplay data/server support and final UI/reliability polish.

### M2 — Netflix-style on-demand parity

Movie/show browse parity, collections/genres/A-Z/favorites/people, polished Home rows, queues, full relevant playback settings, and a complete fast remote-control experience. Music, photos, Live TV/DVR and launcher TV channels remain out of scope.

### M3 — Parity hardening

Close every remaining observable behavior/settings gap within the scoped Netflix-style feature set. Run release-mode comparative startup/navigation/playback benchmarks, long-running soak/leak tests, codec/HDR matrices and reproducible signed builds on target hardware.

## Performance contract

A feature is not considered performance-complete because it is written in C++. For equivalent interactions on the target hardware:

1. sloppaTV must not regress median or p95 navigation/frame latency versus the pinned official client.
2. Static screens must not continuously redraw or consume material CPU just to remain visible.
3. Startup and memory regressions are tracked with the repeatable benchmark and should remain materially below the official client unless a parity feature has an unavoidable cost.
4. Playback performance must be compared at equal media/codec/transcode conditions; server transcoding time is not client rendering performance.
5. Any optimization that changes visible behavior or removes a parity feature fails the parity target even if its benchmark improves.

## Definition of parity

A roadmap item is not complete merely because an endpoint or screen exists. It must work against a real Jellyfin server on the target Android TV hardware, preserve expected remote-control navigation behavior, survive lifecycle/relaunch cases, and have no known material regression versus the equivalent official Android TV feature.
