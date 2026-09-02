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
- [~] Application/navigation/playback policy remains C++/NDK; small Java platform bridges cover Android-only APIs: external-player/activity results, Media3/ExoPlayer construction and audio-route probing, exact embedded audio/text track selection, libass subtitle overlay attachment, MediaSession callbacks, runtime `Intent` forwarding, and the system `DreamService` Surface lifecycle. Navigation, Jellyfin policy/state, queueing, reporting and GLES application rendering remain native; Media3 construction, playback, seeking, track switching and HOME/window restoration are now exercised in Waydroid, while a fresh full target-TV matrix remains pending.
- [x] Native DPAD navigation and native on-screen TV keyboard
- [x] 1920x1080 rendering verified on target Android TV hardware
- [x] Builds for both `armeabi-v7a` and `arm64-v8a`; 32-bit Android userspace is supported on compatible targets
- [x] Native session file persistence and stable generated device id
- [x] Bounded four-worker async task runner; no detached app-level request/report/image threads
- [x] Async task completion wakes the native looper instead of requiring continuous redraw
- [x] Event-driven static screens with short 60 Hz render bursts after DPAD input
- [~] Structured navigation stack now replaces screen-specific return variables and is covered by host policy tests; full Waydroid remote-navigation regression pass is still pending
- [~] Native host policy/unit tests cover player seek/startup/audio/subtitle/direct-stream classification policy, UI/grid/control policy, queueing, navigation-stack behavior and server-version policy alongside Python renderer/tooling tests; a credential-local non-destructive real-server harness now covers authentication, Home endpoints, browse/search hierarchy, artwork inventory, ASS PlaybackInfo negotiation and Jellyfin 10.11 remux/DirectStream semantics. Full Android device integration remains required

### Authentication and servers

- [x] Manual server + username + password login via `/Users/AuthenticateByName`, verified against the real server/account
- [x] Automatic Jellyfin base-path discovery, including the real server mounted at `/jellyfin`
- [x] Persisted authenticated session and relaunch into Home
- [~] Quick Connect initiate/code/poll/complete implementation; the full Jellyfin 10.11.11 API round-trip is now exercised by the opt-in server harness (initiate unauthenticated → poll false → authenticated user authorizes code → poll true → `AuthenticateWithQuickConnect` issues the expected user token, followed by test-session logout). Native TV code-entry/progress UI acceptance remains pending
- [~] Native UDP Jellyfin LAN discovery on port 7359, including interface broadcast addresses; the current test network returns no discovery response even to an independent probe
- [~] Multiple authenticated servers are retained as saved session profiles and can be switched without re-entering credentials; multi-server device E2E is still pending
- [~] Multiple saved users are retained per server, exposed through a native Users & Servers chooser, and can be switched without password re-entry while their token remains valid; device E2E is pending
- [~] Saved-profile management supports `USE`, `FORGET` and `ADD ANOTHER ACCOUNT`; expired 401 profiles are automatically removed, recently authenticated profiles move to the front, and the chooser lazily renders Jellyfin user avatars with an initial fallback. The current `jellyfin` acceptance user has no profile image for populated avatar E2E; PIN protection is not a reference-client parity requirement because it remains an open upstream enhancement
- [~] Server public-info/version diagnostics and a tested Jellyfin 10.10+ baseline warning are implemented; older/newer real-server compatibility cases still need device E2E

### Home and browsing

- [x] Authenticated Home screen on the real server
- [x] Continue Watching row with artwork and resume state
- [x] Next Up row
- [x] Recommended row
- [x] Favorites row
- [x] Recently Added rows per scoped video library
- [x] Home/list/grid requests avoid Details/playback-only `Overview`/full `MediaSources` expansion. Against the current real server this reduced representative Continue Watching JSON by 81.7% (50.7 KB → 9.3 KB), a 24-item Latest Movies row by 87.4% (210.0 KB → 26.5 KB), a 60-movie browse page by 83.3% (394.0 KB → 65.9 KB), Friends search by 69.5% (229.8 KB → 70.1 KB), and the 97-record Friends Play All episode list by 83.6% (884.4 KB → 144.9 KB), while retaining the card/user/artwork/index metadata those screens consume. Full item details and MediaSources remain fetched on demand before Details/playback
- [x] Video-only Jellyfin library views with direct Movies/Shows top navigation
- [x] Text search for Movie, Series and Episode, physically verified on the Google TV Streamer with native-keyboard and direct key-event entry returning real server results
- [x] Rich item-details screen with real server data
- [x] Series Play action resolves the next episode
- [~] Library browsing for Movies, Shows, Mixed and folders is implemented; the real-server harness currently verifies populated Movies/Shows browse results, while Mixed/folder-specific target-TV acceptance depends on available library structure
- [~] Seasons and episode lists are implemented and now repeatably verified at the server/API layer (current sample: `91 Days` → `Season 1` → 13 episodes); current-build target-TV navigation acceptance remains pending
- [~] Nested folder/collection browsing is implemented; Collections remains physically verified from the earlier streamer pass and the real-server harness currently returns 10 box sets, while arbitrary nested-folder device acceptance remains pending
- [x] Dedicated Collections / box-set browse mode verified on the physical streamer against real Collections data, including opening a collection into its member list
- [x] Server-native Genres browser/filter verified on the physical streamer, including opening Action into real filtered movie results
- [x] A-Z / by-letter browser verified on the physical streamer with server-side `NameStartsWith`, including opening A into populated real movie results
- [~] Dedicated Favorites filter is implemented in Movies/Shows browse and the real-server harness verifies the request successfully; the current account still returns zero movie favorites, so a populated UI E2E case is unavailable
- [x] Cast names remain visible on Details and a dedicated native Cast browser now uses Jellyfin person IDs, actor image tags/roles, and server-side PersonIds filtering to open titles featuring a selected actor; verified end-to-end on the physical streamer with Braveheart → Mel Gibson → Featuring Mel Gibson → Braveheart and full Back-stack restoration
- [~] Native item options are available from Details and directly from Home/browse/search/person/episode cards through Android TV `MENU`/`INFO`, with async permission enrichment before destructive actions; physical-TV UI verification of the direct context-key path remains pending
- [x] Mark watched/unwatched mutation verified end-to-end on the physical streamer and asserted against server `UserData`, with the test item restored to its original unwatched state
- [x] Favorite/unfavorite mutation verified end-to-end on the physical streamer and asserted against server `UserData`, with the test item restored to its original non-favorite state
- [~] Delete media is gated by Jellyfin `CanDelete` and requires a second destructive confirmation with Cancel selected by default; real permission/filesystem behavior still needs device/server E2E
- [~] Metadata refresh can be requested from the item-options menu using Jellyfin's default metadata/image refresh modes; real permission/server completion behavior still needs E2E

### Artwork and presentation

- [x] Primary poster/thumb artwork download and GLES rendering on Home/Details; the 2026-09-02 Waydroid pass verifies higher-resolution 960×540 Home episode imagery with aspect-preserving crop and populated Continue Watching/Next Up cards.
- [x] Backdrop download/decode/render ownership handles both item-owned and parent-inherited Jellyfin tags. A reproduced Home bug was fixed by carrying `ParentBackdropItemId` with `ParentBackdropImageTag`, and current Waydroid Home/Details rendering verifies inherited ownership instead of issuing episode-ID HTTP 404s.
- [x] In-memory decoded-image/texture cache
- [x] 48 MiB / 256-file bounded Home artwork disk cache keyed by Jellyfin image tags
- [x] Image fetch and decode off the render thread; GLES upload occurs on the render context
- [x] Jellyfin logo artwork download/decode/GLES rendering supports both item-owned and parent-inherited logo tags/owner IDs; current Waydroid `Planet Earth III - S1E2` Details renders the inherited series logo, matching the real-server inventory where only 2/1000 sampled items had own logos versus 983 inherited logos.
- [~] Watched / favorite / progress indicators
- [x] Ratings, year, runtime, content rating, genres and richer metadata
- [x] Cast metadata plus actor/person images are rendered from real Jellyfin person data on the physical streamer
- [~] Persisted backdrop presentation now offers `OFF`, `BLURRED` and `CLEAR` modes on Home/Details with inherited-artwork resolution; final target-TV visual parity pass remains pending
- [x] Home row/item and Movies/Shows top-nav focus restoration across refresh/navigation
- [x] Accessibility-friendly global UI text sizing (`NORMAL` / `LARGE` / `EXTRA LARGE`) and overscan safe-area controls (`OFF` / `2%` / `4%` / `6%` per edge), persisted and verified at the worst-case Extra Large + 6% combination on the physical Google TV Streamer; UI/artwork/overlays stay inside the safe area while video remains edge-to-edge. The current Waydroid build additionally replaces the legacy pixel-only UI font with a bold anti-aliased Android system-font atlas and keeps the pixel glyphs only as a fallback.

### Video playback

- [x] PlaybackInfo negotiation against the real Jellyfin server
- [x] Direct-play preference with Jellyfin HLS transcode fallback
- [~] Hardware-backed Android Media3/ExoPlayer 1.11 backend driven by C++ policy through a minimal JNI bridge. Fresh Waydroid acceptance verifies Media3 construction, first-frame decode, main-thread Android `MediaSession` creation, real PLAYING advancement, exact embedded track overrides, and crash-free resume. A wedged Waydroid OMX/media service was distinguished from app behavior by restarting only the Android guest; after restart the same build reached READY, rendered the first frame and advanced normally. A broader target-TV codec/HDR/audio soak remains pending.
- [x] Real Jellyfin video rendered through `SurfaceTexture` / `GL_TEXTURE_EXTERNAL_OES` into the GLES scene
- [x] Resume position verified with real content; current Media3 Waydroid acceptance opens `Planet Earth III - S1E2` and resumes at ~9:31/58:28 without process death.
- [x] Play/pause
- [x] Configurable skip-back/skip-forward seeking
- [~] Playback start/progress/stop reporting includes the active Jellyfin audio/subtitle stream indices and is exercised through real Waydroid playback/seek/track-switch/lifecycle flows; dedicated automated server-session state assertions are still pending
- [x] End-to-end playback verified against the real Jellyfin server and streamer
- [x] Native Android MediaCodecList capability probing for video/audio decoder families
- [~] Device-aware direct-play codec/container/profile/resolution/HDR matching; real HEVC Main10/MKV direct play is verified, and user AVC/HEVC/HDR overrides now feed the same PlaybackInfo capability decision; real HDR title playback E2E remains pending
- [x] Playback targets distinguish Jellyfin `DirectPlay`, semantic `DirectStream`/remux and full `Transcode` in session reporting and diagnostics while retaining server-stream seek policy. Jellyfin 10.11.11 was verified to return a real HEVC/AAC MKV remux case (`Road to Hero`, `TranscodeReasons=ContainerNotSupported`) with `SupportsDirectStream=false`; sloppaTV now conservatively recognizes only Jellyfin's own direct-stream reason set from the server-stream URL, while subtitle/video-conversion reasons remain Transcode. The real-server harness repeats this non-destructive assertion and host tests cover mixed/encoded reason parsing
- [x] Native playback overlay with title, time and progress bar
- [x] Native seek/progress UI
- [~] Jellyfin chapter parsing remains available internally, but the chapter button/interactive chapter control was intentionally removed from the simplified player overlay
- [~] Jellyfin trickplay seek thumbnails are implemented: item `Trickplay` metadata is parsed, seek timestamps map to the correct tile sheet/cell, JPEG tile sheets are downloaded/decoded asynchronously, only one decoded sheet is retained, and the native player renders a large timestamped cell preview above the progress bar on left/right seek. Host policy tests and Android builds pass. The real server currently exposes no Trickplay metadata in the sampled Movie/Episode library; an earlier controlled `regenerateTrickplay=true` refresh for one known-good episode was rejected HTTP 403 by the then-used non-admin acceptance account, so physical thumbnail rendering remains pending rather than manufacturing admin credentials for the test.
- [x] Jellyfin audio streams/server indices are parsed and DirectPlay embedded audio switches in-place through exact Media3 `TrackSelectionOverride` ordinals instead of restarting the player. Current Waydroid acceptance verified Japanese → English → Latin American Spanish as embedded ordinals 0 → 1 → 2 with no additional PlaybackInfo request, no player release and the selected ASS subtitle preserved; server-stream cases still re-resolve only when an actual server stream change is required. A manually selected audio language is now carried only within the active queue/autoplay chain, with host-tested language matching and server-default fallback when the next episode lacks that language; fresh/manual playback resets the override so it cannot leak across unrelated series
- [x] Subtitle track selection and off/on UI. For DirectPlay embedded SRT/VTT, sloppaTV now bypasses Jellyfin 10.11.11's broken external-extraction endpoint and lets Media3 demux/render the exact embedded text stream; Waydroid verified duplicate-language English tracks as distinct ordinals 0 and 1 and observed nonempty cue callbacks for both. External text subtitles continue to use the native GLES path, with unusable optional server DeliveryUrls remaining nonfatal.
- [~] Persisted subtitle size/background/vertical-position controls feed the native GLES external SRT/VTT path; DirectPlay embedded text is rendered by Media3's `SubtitleView`, while styled ASS/SSA is handled separately by libass. Functional embedded-text cue delivery is verified in Waydroid; fresh physical-TV visual sizing/position acceptance remains pending
- [~] ASS/SSA direct play is implemented with the Media3 libass extension, preserving styles/positioning without video burn-in; the Canvas overlay is intentionally used because upstream has documented OpenGL-overlay failures on some Android TVs. For embedded ASS, exact Jellyfin stream selection is mapped to the corresponding embedded Media3 text ordinal rather than depending on an extracted subtitle URL. Waydroid verified English ordinal 0 → Latin American Spanish ordinal 1 with libass track changes, first-frame rendering and READY; a physical-TV styled-subtitle visual pass remains pending
- [x] Playback speed option intentionally removed from the scoped player UI
- [~] In-player zoom/fill option intentionally removed from the simplified player controls; the persisted default-video-zoom setting/render path still exists
- [x] Quality/max-bitrate selection feeds PlaybackInfo negotiation. The non-destructive real-server harness now negotiates the same 7.7 Mbit/s `Trash` source at 120 Mbit/s and ~1.9 Mbit/s caps: the high cap remains DirectPlay, while the low cap rejects DirectPlay and returns a constrained server stream with `ContainerBitrateExceedsLimit`
- [~] Fixed-source refresh-rate requests/clear implemented through `ANativeWindow`; real 23.976 request succeeds, but the streamer has system `match_content_frame_rate=0`, so physical mode switching is intentionally blocked by Android policy
- [~] Native HDR10/HDR10+/Dolby Vision/HLG display probing and per-item gating implemented; persisted HDR override modes (`AUTO`, `SDR ONLY`, `ALLOW ALL HDR`) now alter capability gating, but the current enumerated library returned no HDR-range item for a physical-streamer override assertion
- [~] Route-aware audio output/downmix/direct-play negotiation is implemented: Android reports attached-route channel capacity and direct AC3/E-AC3/DTS/DTS-HD/TrueHD support; stereo mode excludes surround/lossless codecs and disallows incompatible audio stream-copy so Jellyfin must downmix; surround mode is capped to the actual route; and server transcode output is limited to route-safe AAC/MP3 plus AC3/E-AC3 only when surround is usable. Media3 owns audio focus/noisy-route handling; receiver/TV codec matrix E2E remains pending
- [x] Next Up prefetch/overlay/autoplay; real Friends S2E10 → S2E11 end-of-episode transition verified on the streamer
- [~] Configurable Still Watching guard and prompt; autoplay threshold policy now has direct host coverage, the prompt exposes an explicit `KEEP WATCHING` action, and Back clears the continuation chain instead of leaking prompt state into a later manual play. Dedicated end-of-episode threshold E2E remains pending
- [x] Server-native media-segment skip actions (Intro/Outro/Recap/Preview/Commercial); real Fallout S1E2 `SKIP RECAP` verified to jump to the server-provided 0:58 segment end
- [~] Native queue management / play-next / Series play-all is implemented and physically exercised on the Google TV Streamer: Friends Play All de-duplicated 97 raw Jellyfin episode records into 73 season/episode slots; move-down, play-next reordering and remove were verified; `PLAY NOW` started real S2E13 playback; and `KEYCODE_MEDIA_NEXT` advanced S2E13 → S2E14 with clean async teardown/restart. The pass also found a stale duplicate S1E1 whose static stream is 404; Play All now HEAD-probes duplicate slots and prefers an available copy, with one final post-fix streamer navigation rerun still pending.
- [~] Queue-native Shuffle and Repeat Off/One/All are implemented with host policy coverage: shuffle preserves the current item and randomizes only the unplayed tail; Repeat One restarts only on natural completion; Repeat All wraps at queue end; and manual media-next remains an explicit advance. Physical-streamer remote/UI acceptance is still pending.
- [~] External player support mirrors the Android TV client architecture: native PackageManager discovery finds `ACTION_VIEW` `video/*` handlers, Settings persists an Internal/external-player choice, item options expose `PLAY EXTERNAL` when configured, and playback launches Jellyfin's authenticated static stream with resume/title extras plus known VLC/MX Player/mpv/Vimu conventions and a default external subtitle URL where supported. A deliberately tiny Java `NativeActivity` subclass now forwards `onActivityResult` into C++; native policy code parses player-specific returned position/completion semantics and reports the resulting stop position back to Jellyfin. Host tests, Android builds and CheckJNI startup pass on the physical streamer; that device currently has zero compatible external video players installed, so real handoff/result E2E remains pending.
- [x] Android HOME/window loss now preserves the live Media3 decoder, GLES context and `SurfaceTexture`; only the EGL window surface is detached and recreated. The Activity is `singleTask`, so launcher return brings the existing task forward, and runtime VIEW/SEARCH intents are forwarded through `onNewIntent` into the native loop. Waydroid verified a genuinely PLAYING session at 103000 ms returning PLAYING at 111895 ms with zero new player creation/release and zero runtime errors; Media3 handles audio focus/noisy-route events
- [~] Simplified player controls are now limited to PLAY/PAUSE, AUDIO and SUBS; control count/sizing is covered by native policy tests and visually verified in Waydroid
- [x] Media3 seeks stay in-place for both DirectPlay and server-streamed playback, matching the current reference ExoPlayer path and removing the old NuPlayer-era PlaybackInfo/restart round-trip on every direct seek. Waydroid verified an exact +10000 ms seek with zero new PlaybackInfo requests and zero runtime/player errors
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

Still required before the viewing-acceptance effort is considered complete: a broader 10+ cross-title codec/decoder soak, beginning/middle/near-end seek matrix across varied containers, back-during-loading/immediately-after-server-stream-restart cases, current-build autoplay/Still Watching threshold acceptance, explicit Jellyfin server progress-state assertions, a populated HDR/trickplay fixture when the server library exposes one, receiver/TV surround-passthrough validation, and a final long-run crash/ANR/HTTP/decoder audit. Current-build multi-audio, embedded SRT/ASS, in-place seek, runtime intent and HOME/window lifecycle cases are now covered below.

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
- [x] Native `ACTION_VIEW` content intents accept validated bare Jellyfin item IDs and route authenticated launches to Details after Home initialization; physically verified on the Google TV Streamer by externally launching real Blue Planet II and Wonder Egg item IDs into their Details screens
- [~] Android searchable metadata and native `ACTION_SEARCH` handling route external/voice-recognizer queries into the existing Jellyfin Search screen. `singleTask` + `onNewIntent` now handles SEARCH while the app is already running; Waydroid verified an active `ACTION_SEARCH` for `1917` stops the prior player, queries Jellyfin and renders the real result without creating another native Activity. An actual microphone/voice-recognizer invocation remains pending
- [~] Native Android `MediaSession` integration publishes playback title/episode metadata, duration, position and buffering/playing/paused state. The physical streamer verifies idle Home has no registered sloppaTV session, while active Wonder Egg playback reports sloppaTV as the addressed media-button session with `PLAYING`, a live position and `My Priority / Wonder Egg Priority - S0E1` metadata. A minimal Java `MediaSession.Callback` bridge now forwards Play/Pause/Stop/Seek/Next/Previous into the native player/queue and the session advertises only those supported actions; host/Android builds pass, while a fresh physical system-dispatch assertion is pending because the current execution layer blocks the required ADB input/media-session dispatch sequence.
- [~] System Screensaver / `DreamService` is implemented as an isolated Java service/Surface lifecycle bridge backed by the existing native GLES renderer; the service is registered with `BIND_DREAM_SERVICE` and physically visible to Android's Dream service resolver on the Google TV Streamer. A forced Dream launch cannot be run from the unprivileged ADB shell (`cmd dreams start-dreaming` requires root on this device), so normal-idle physical rendering/dismissal remains pending.
- [~] Native in-app screensaver is implemented with persisted Off/5/10/20/30-minute idle thresholds, a low-redraw clock/brand surface that changes safe position every 30 seconds, suppression during playback/loading/Quick Connect, and first-key dismissal back to the unchanged underlying screen. Playback completion and the Still Watching transition now restart the idle window so a long viewing session cannot immediately cover the resulting UI with the saver; host policy and Android builds pass, while timed physical activation/dismissal remains pending
- [~] Native in-app notice/banner presentation now surfaces persistent server-compatibility warnings and transient maintenance/action results without requesting irrelevant Android notification permissions; target-TV visual acceptance remains pending
- [~] Branded Android TV vector banner and adaptive launcher/round icons replace the previous flat placeholder banner; Android resource compilation is verified, while a physical launcher visual pass is still pending

### Settings parity

- [x] Native settings navigation and persistence
- [x] Searchable Settings filter is integrated at the top of Settings and verified in Waydroid; filtering is case-insensitive and covered by native policy tests.
- [x] Search, Settings search and login use the configured Android TV system IME when available. Waydroid verifies the real Android LatinIME overlay and live EditText→JNI→native filter updates; the native fallback keyboard remains available and its left/right row-edge wraparound has policy-test coverage.
- [x] Max streaming bitrate
- [~] Persisted playback buffer presets use Media3/ExoPlayer `DefaultLoadControl`: `AUTO`, `LARGE` (50–120 s, 2.5/5 s playback/rebuffer start), and `EXTRA LARGE` (80–240 s, 5/10 s playback/rebuffer start). The exact duration mapping is now isolated in a pure Java policy and executed by the host test suite, while dedicated on-device per-preset buffer-depth acceptance remains pending
- [~] Persisted stereo/direct-surround audio mode now combines the user preference with the attached Android output route: PlaybackInfo caps channels to the detected route, advertises decoder/direct-route compatible codecs, rejects incompatible selected-stream copying, and constrains transcode output to stereo-safe or route-safe formats; device/server/receiver transcode/direct-play matrix pending
- [~] Subtitle size/background/vertical-position preferences persist for the native external SRT/VTT renderer. Initial playback still allows Jellyfin to choose the account default; manual language or explicit Off is carried through queue/autoplay. DirectPlay embedded SRT/VTT is selected by exact Media3 text ordinal, and embedded ASS/SSA uses the same exact ordinal through libass instead of relying on Jellyfin extraction/burn-in; dedicated queue language-carry/forced-subtitle and physical-TV visual E2E remains pending.
- [x] Skip-ahead / skip-back lengths
- [~] Next Up autoplay behavior
- [~] Still Watching threshold
- [~] Match-video-refresh-rate setting persists and controls native fixed-source requests; physical switching requires the TV's system match-content setting
- [~] Watched-indicator visibility setting is persisted and gates Home/grid/Details watched badges; physical-TV UI pass pending
- [~] Clock visibility setting is persisted and renders local 24-hour time in native headers; physical-TV UI pass pending
- [~] Backdrop presentation persists as Off/Blurred/Clear and gates Home/Details inherited-backdrop fetching/rendering; physical-TV UI pass pending
- [x] Default video zoom mode
- [~] Persisted AVC 4.0–6.2, HEVC 4.0–6.2 and HDR Auto/SDR-only/Allow-all controls feed real PlaybackInfo negotiation. On the physical streamer, the same HEVC Main10 Level 5.0 Hell's Paradise S1E1 item selected `DirectPlay` with HEVC Auto, then rejected direct HEVC and selected/started `Transcode` when capped to HEVC 4.0. AVC and a real HDR-range title still need equivalent device assertions
- [~] Persisted in-app screensaver timeout preference offers Off/5/10/20/30 minutes and drives the native idle saver; physical timed-idle/dismissal acceptance remains pending
- [~] External-player selection persists in Settings and dynamically enumerates compatible Android `video/*` handlers; returned resume/completion state is bridged back into native code and Jellyfin stop reporting, while the physical streamer currently has no compatible external player installed for handoff/result E2E
- [~] User-select behavior now has a saved Users & Servers chooser and Settings `SWITCH USER` flow; physical-TV navigation/expiry/multi-server acceptance remains pending
- [x] Diagnostics screen reports app version/ABI, Jellyfin server version, detected decoder/HDR capability and last playback path; physically verified on the Google TV Streamer against the real server (Jellyfin 10.11.11) and the streamer's detected H264/HEVC/VP8/VP9/AV1/MPEG2 plus HDR10/HDR10+/Dolby Vision/HLG capabilities

### Reliability, performance and release engineering

- [x] UI avoids Compose/RecyclerView/View hierarchy overhead by design
- [x] Repeatable SurfaceFlinger DPAD navigation benchmark on the real streamer
- [x] Repeatable process-cold startup benchmark against installed official Jellyfin
- [x] Settled process memory measurement against installed official Jellyfin
- [x] Idle static screens consume effectively 0% process CPU in the sampled `top` snapshot
- [~] Bounded HTTP retry/backoff handles transient JNI/connection failures after TV wake, and cancellation now wakes backoff waits immediately on teardown, Quick Connect cancellation or session/task-generation changes; wake/device regression profiling remains pending
- [x] Graceful partial Home failures: one failed row does not blank Home
- [x] Pagination/incremental 60-item loading for large libraries
- [~] Identical in-flight GETs are coalesced and successful non-media API GETs receive a bounded 5-second transport cache with mutation invalidation; Waydroid workload/regression profiling pending
- [x] Directional Home image prefetch around the focused card
- [x] Bounded thread pool instead of one thread per request/report
- [~] Waydroid E2E harness (`tools/waydroid_e2e.py`) requires an explicit ADB serial, validates Waydroid identity/1920x1080, captures screenshots/frame-difference motion evidence and filtered player logs, and now has reusable lifecycle/HOME restoration, runtime VIEW/SEARCH, MediaSession dump, app-scoped fatal-log audit, and PSS/RSS/CPU soak commands. The 2026-09-02 pass additionally validates system-IME state, Home artwork, Media3 first-frame/PLAYING behavior, exact embedded SRT/ASS/audio track selection, in-place seek, runtime VIEW/SEARCH, and preserved HOME/window lifecycle; the expanded harness itself is host-tested, while the final broad codec/HDR/long-soak matrix remains in progress.
- [~] A real player-teardown ANR was reproduced and fixed in Waydroid; two current-build physical-streamer playback exit/re-entry cycles completed cleanly with asynchronous teardown start/finish logs, while a broader ANR/crash soak remains pending
- [~] Memory profiling baseline complete; the Waydroid harness records timestamped PSS/RSS/Java/native heap/CPU samples during configurable soaks and now automatically summarizes first/last median PSS/RSS, absolute/percentage growth and peak usage with host-tested calculations. The required long-session runs themselves remain pending
- [~] Release-candidate performance comparison underway; cold-launch and memory release measurements captured, final multi-run release suite still required
- [~] Production release signing is environment-configurable and never silently falls back to a debug key; Debug/Benchmark acceptance builds use an isolated `.test` application ID so they can be installed beside production without clearing user data, and the separate Benchmark variant is non-debuggable/debug-signed while a real production-key signing pass remains pending
- [~] GitHub Actions workflow builds/tests Debug, Benchmark and Release with the pinned SDK/NDK/CMake toolchain and uploads APKs; both the push and pull-request hosted runs passed after fixing runner `sdkmanager` discovery, while artifact install/device validation remains pending
- [x] Version code/name are centralized in Gradle properties, compiled into native diagnostics, and tracked in `CHANGELOG.md`
- [~] After the 2026-09-02 Media3/UI/input/artwork/resume fixes, two clean Glass unsigned Release builds produced an identical byte-for-byte APK, SHA-256 `60c1d37ef31b11c4ed8f1e3236aa304146c3b8d4494c5a3202b18d0d04647833`; final production-signed reproducibility remains pending

## Roadmap hardening checkpoint — 2026-09-01

The Astra hardening branch adds a real navigation stack, native diagnostics/server-version checks, in-flight GET coalescing plus a short-lived API cache, watched/clock/backdrop settings, centralized versioning/changelog, production-signing configuration, a separate installable benchmark variant, host test orchestration, CI, and a repeatable release-reproducibility check. All host tests and Debug/Benchmark/Release Android builds pass locally across the configured ABIs. The first hosted push and pull-request CI runs are also green. Items that change visible Android TV behavior remain partial until the required Waydroid/target-TV acceptance pass is run.

## Roadmap continuation checkpoint — 2026-09-02

Direct-stream session reporting now preserves Jellyfin's distinct DirectPlay/DirectStream/Transcode methods; the native subtitle renderer has persisted size/background/position controls; Details has a permission-aware item-options menu for default metadata refresh and `CanDelete`-gated deletion with a destructive confirmation; max audio channels now feeds PlaybackInfo/device-profile negotiation; and authenticated users/servers are retained as switchable saved profiles with local forget/add-account management and 401 expiry cleanup. The TV UI was then reworked around real-streamer couch readability: three large Home cards, four-column browse grids, larger typography/focus targets/settings/player controls, and aspect-aware episode Details artwork. Persisted global UI-size and overscan safe-area controls are now physically verified at their worst-case Extra Large + 6% combination, with video intentionally excluded from the safe-area transform. AVC/HEVC/HDR overrides now affect real Jellyfin capability negotiation; a same-title streamer test proved HEVC Main10 Level 5.0 changes from DirectPlay on Auto to a working Transcode when capped to HEVC 4.0. Native queue management now shares one ordered queue with episode autoplay, supports Series Play All, Play Now/Next, reordering/removal and media-next, and was exercised on the physical streamer through a real S2E13 → S2E14 playback transition. That pass exposed a stale duplicate Friends S1E1 library item: its Jellyfin metadata was present but its static stream was HTTP 404 and every HLS segment request returned server HTTP 500. Duplicate episode slots now probe static-source availability and prefer a working copy; direct-play failures without a supplied fallback URL can also renegotiate forced transcoding asynchronously. Android TV item/search intents are now handled natively; a physical CheckJNI abort exposed and fixed invalid cross-thread reuse of `ANativeActivity::env`. MediaSession publication is now lazy/playback-only so idle Home does not steal media ownership, and branded TV launcher assets replace the placeholder banner. The final post-fix S1E1 navigation rerun and external VIEW/SEARCH launch remain pending because those physical-device invocation forms are blocked by the execution environment. The focused real-streamer playback pass above verifies normal motion, forward/back seeking, pause/resume and exit/re-entry on the pre-Media3 build, while broader feature-specific acceptance remains partial.

### Media3 parity checkpoint — 2026-09-02

The old Android `MediaPlayer` backend has been replaced with Media3/ExoPlayer 1.11 while keeping playback policy/state in C++. This closes the previously impossible buffer-setting gap with real Auto/Large/Extra Large `DefaultLoadControl` ranges, adds decoder fallback and Media3 audio-focus/noisy-route handling, and preserves the existing GLES `SurfaceTexture` video path. The migration removes NuPlayer-specific seek/resume workarounds: ordinary DirectPlay resume is no longer forced toward a server stream, and normal seeks use ExoPlayer in-place instead of re-resolving PlaybackInfo/restarting the direct stream. DirectPlay embedded audio switches also stay in-place using exact Media3 audio ordinals. Audio negotiation is route-aware rather than decoder-only: attached-output channel capacity and direct AC3/E-AC3/DTS/DTS-HD/TrueHD support cap Jellyfin direct-play advertisement, selected audio stream-copy must fit the route, and transcode output is constrained to route-safe formats. HOME/window loss no longer tears down Media3 or its `SurfaceTexture`: only the EGL window surface is detached, the GLES context/decoder remain live, and the same player resumes when the window returns.

ASS/SSA no longer requires burn-in: the device profile advertises direct client support, Jellyfin 10.11.11 keeps a sampled embedded ASS title DirectPlay, and Media3/libass selects the exact embedded text ordinal through the Canvas overlay for broader TV compatibility. DirectPlay embedded SRT/VTT similarly bypasses Jellyfin's currently broken external SubRip extraction endpoint and is demuxed/rendered by Media3; external text subtitles retain the native GLES renderer. Waydroid verified duplicate-language embedded text selection/cue delivery and embedded ASS language switching. The real-server integration harness confirms Movies/Shows browse, Resume, Next Up and search endpoints and also exposed inherited-artwork behavior: in a 1000-item sample only 2 items had own logos/backdrops while 983 inherited both from parents, so logo/backdrop ownership parsing was fixed accordingly. The same sample still exposes zero Trickplay and zero HDR-range items, leaving those acceptance rows externally blocked by available server data rather than unimplemented code.

Host policy tests and real-server non-destructive integration checks pass after the migration. Waydroid is running again and the current Media3 build has fresh acceptance for the anti-aliased font, searchable Settings/system IME, high-resolution aspect-correct Home artwork, inherited logo/backdrop ownership, first-frame/PLAYING playback, exact embedded audio/SRT/ASS track selection, in-place seeking, active-task VIEW/SEARCH delivery, and HOME/window restoration. A wedged Waydroid media service encountered during repeated forced decoder/surface experiments was reset independently; the unchanged product build then returned to normal READY/PLAYING behavior, so no emulator-specific playback workaround was retained. The release APK hash below is refreshed whenever the frozen candidate changes.

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
