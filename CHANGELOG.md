# Changelog

All notable user-visible and release-engineering changes are recorded here.

## Unreleased

### Added

- Structured navigation stack replacing screen-specific return variables.
- Native diagnostics screen for app version, ABI, Jellyfin server version, decoder/HDR capability and last playback path.
- Jellyfin server-version compatibility warning against the current tested 10.10+ baseline.
- In-flight GET request coalescing and a short-lived API response cache, with mutation invalidation.
- User settings for watched indicators, clock visibility and backdrop behavior.
- Host-side native policy test runner and navigation/server-version policy tests.
- Real-server acceptance harness for scoped browse/search/artwork/subtitle negotiation, with an opt-in Quick Connect authorize/authenticate round-trip that logs out its transient test sessions.
- GitHub Actions Android build/test workflow.
- Environment-based production release signing configuration and a separately debug-signed non-debuggable `benchmark` build type.
- Centralized version code/name properties compiled into native diagnostics.
- Distinct Jellyfin `DirectPlay`, `DirectStream` and `Transcode` playback-method reporting.
- Persisted subtitle size, background and vertical-position controls for the native SRT renderer.
- Native Details item-options menu with default metadata refresh and permission-aware media deletion.
- Two-step destructive delete confirmation with Cancel selected by default.
- Persisted 2-channel/8-channel maximum-audio capability negotiation in PlaybackInfo and the Jellyfin device profile.
- Saved authenticated user/server profiles with native switching, forgetting, add-account flow, single-session migration and expired-token cleanup.
- TV readability redesign with three large Home cards per row, four-column media grids, larger navigation/settings/player typography, larger focus targets and roomier spacing.
- Aspect-aware Details artwork so episodes use a large landscape frame while movies/series retain poster presentation.
- Side-by-side `.test` application IDs for Debug/Benchmark acceptance builds so real-device testing never requires replacing or clearing an installed production client.
- Persisted global UI text-size presets and per-edge overscan safe-area controls, with the native renderer applying them to UI/artwork/overlays while leaving playback video edge-to-edge.
- Persisted AVC, HEVC and HDR playback overrides that feed real Jellyfin PlaybackInfo capability negotiation rather than acting as display-only preferences.
- Native playback queue management with Series `PLAY ALL`, `PLAY NOW`, `PLAY NEXT`, move up/down, remove and hardware/media-next integration, shared with episode autoplay.
- Queue-native Shuffle plus Repeat Off/One/All semantics; shuffle preserves the current item and randomizes only the unplayed tail, while Repeat One affects natural completion without hijacking manual Next.
- Series Play All de-duplicates alternate Jellyfin episode entries by season/episode slot and probes duplicate static sources so an available copy is preferred over a stale library entry.
- Direct-play failure recovery can renegotiate a forced Jellyfin transcode when the initial PlaybackInfo response did not provide a fallback URL.
- Native cast/person browser with Jellyfin actor images and roles, plus server-filtered Movie/Series/Episode results for a selected person.
- Physical-streamer acceptance for Search, Genres, A-Z, Collections, Diagnostics, and reversible Favorite/Watched mutations with server-side state assertions.
- Android TV `ACTION_VIEW` item intents routed directly into native Details using validated Jellyfin item IDs.
- Android `ACTION_SEARCH` integration plus searchable/voice-recognizer metadata, routing external search text into the existing native Jellyfin Search screen.
- Lazy native Android `MediaSession` publication for playback title/episode metadata, duration, position and buffering/playing/paused state without claiming unimplemented system transport callbacks.
- Branded Android TV vector banner plus adaptive launcher/round icons replacing the previous flat placeholder banner.
- Persisted native in-app screensaver with Off/5/10/20/30-minute idle choices, a low-redraw moving clock/brand surface, playback/loading inhibition, and first-key dismissal back to the exact underlying screen.
- Jellyfin server-default subtitle selection on playback startup, plus queue/autoplay carry-forward of an explicitly selected subtitle language or explicit Off state.
- Native Jellyfin trickplay seek previews with tile-sheet metadata parsing, asynchronous JPEG fetch/decode, one-sheet bounded caching, and cropped GLES rendering positioned along the playback timeline.
- Native external-video-player discovery and persisted selection, with Jellyfin static-stream handoff plus VLC, MX Player, mpv and Vimu title/resume conventions and supported external-subtitle URL extras.
- Minimal `NativeActivity` result bridge for external-player return state, with player-specific completion/resume parsing and Jellyfin playback-stop reporting while keeping application/UI/playback logic in C++.
- Android system `DreamService` integration whose Java layer owns only the service/Surface lifecycle while the existing native GLES renderer draws the moving sloppaTV clock/brand screensaver.
- Android MediaSession transport callbacks for Play, Pause, Stop, Seek, Next and Previous, forwarded through the minimal platform bridge into the native player/queue.
- Media3/ExoPlayer 1.11 playback backend with persisted Auto/Large/Extra Large buffering presets matching the Android TV reference client's `DefaultLoadControl` ranges.
- Route-aware Android audio-output capability probing for AC3, E-AC3, DTS, DTS-HD and TrueHD, with PlaybackInfo direct-play/downmix negotiation capped to the currently attached output route.
- Direct-play ASS/SSA subtitles through the Media3 libass extension with exact embedded-track ordinal selection, using a Canvas overlay for broader Android TV GLES compatibility.
- DirectPlay embedded SRT/VTT rendering through Media3's text pipeline, including exact duplicate-language track selection without relying on Jellyfin's embedded-subtitle extraction endpoint.
- In-place DirectPlay embedded-audio switching through exact Media3 audio-track ordinals, avoiding a PlaybackInfo/player restart for ordinary track changes.
- `singleTask` runtime VIEW/SEARCH intent delivery into the existing native app loop so launcher/deep-link/search re-entry does not create a second playback Activity.
- Read-only real-server integration harness covering authentication, scoped libraries, Home endpoints, search, browse and artwork/HDR/trickplay inventory.
- Direct `MENU`/`INFO` item context actions from Home, browse, search, person results and episode grids.
- Off/Blurred/Clear backdrop modes and native compatibility/action notice banners.
- Saved-user chooser avatars loaded lazily from each authenticated Jellyfin profile, with a deterministic text-initial fallback and retry after re-authentication.
- Searchable Settings filter using the Android TV system IME, with the same system-keyboard bridge used for Jellyfin Search and login text entry.
- Anti-aliased Android system-font atlas for the native GLES UI, retaining the original pixel glyphs only as a fallback.
- Expanded Waydroid acceptance tooling for HOME/lifecycle restoration, runtime VIEW/SEARCH intents, MediaSession inspection, app-scoped fatal-log auditing, and timestamped PSS/RSS/CPU soak evidence.
- A `--final-suite` benchmark mode that enforces the performance-gate 20 startup / 5 memory / 5 navigation sample counts and rejects incomplete startup samples.

### Fixed

- Launch-intent JNI now attaches the `android_main` thread to the VM instead of reusing `ANativeActivity::env`; a physical-streamer CheckJNI abort exposed the invalid cross-thread `JNIEnv*` use.
- MediaSession lifetime is restricted to active playback so idle Home does not register or retain a sloppaTV media session.
- Subtitle-selected transcodes that fail to prepare now retry the same item without subtitles instead of dropping immediately back to Details; the original queue language preference is retained for later items.
- SRT/VTT client subtitle rendering remains on the native GLES text path after the Media3 migration rather than relying on ExoPlayer to draw text into a raw video `Surface`.
- Parent-inherited Jellyfin logo and backdrop tags/owner IDs are resolved, fixing artwork on the large majority of sampled episodes that inherit series artwork.
- Android HOME/window loss now preserves the live Media3 decoder, GLES context and video `SurfaceTexture`; only the EGL window surface is detached/recreated, avoiding MediaCodec teardown and restoring the same player instance on return.
- HTTP retry backoff is cancellable on task/session generation changes so teardown, Quick Connect cancellation and user switching do not leave stale retry sleeps running.
- Media3 JNI telemetry polling is bounded instead of querying Java on every render tick.
- DirectPlay resume/seeking no longer retains the old NuPlayer workaround that forced resumed MKV toward server streaming and restarted the Jellyfin stream for every direct seek; Media3 now receives the logical resume/seek position directly.
- Audio stream-copy and transcode-output negotiation now respects the selected stream codec/channel count and attached-route stereo/surround constraints instead of leaving `AllowAudioStreamCopy` unconditional.
- Media3 playback startup no longer resolves application classes from attached native worker threads, and Android `MediaSession` creation is marshalled onto the Activity main thread; both crashes were reproduced during real resume playback and fixed.
- NativeActivity explicitly loads the app library through the application classloader so Java-to-native system-keyboard callbacks resolve reliably; real IME typing now updates native Search/Settings/login state without `UnsatisfiedLinkError`.
- The fallback native virtual keyboard wraps horizontally at both row edges and remains available only when the configured Android TV IME cannot be opened.
- Home episode artwork now preserves the owner ID for inherited Jellyfin backdrops, preventing `ParentBackdropImageTag` from being requested against the episode ID; card rendering uses higher-resolution requests and aspect-preserving center crop rather than stretching.
- Home, browse, search, similar/person and season/episode list requests no longer fetch Details/playback-only Overview/full MediaSources payloads. Current real-server samples shrank representative Continue Watching, Latest Movies, 60-movie browse, Friends search and 97-record Friends Play All JSON by 69–87% while retaining the list/card metadata sloppaTV consumes; full details/media sources are still fetched on demand before Details/playback.
- Embedded text-subtitle delivery no longer depends on Jellyfin's unusable external SubRip/SRT extraction URL during DirectPlay; Media3 demuxes the embedded track directly while external subtitle failures remain nonfatal.
- Detached/replaced ExoPlayer instances can no longer overwrite the active bridge state through late release/error callbacks.
- DirectPlay audio switching now updates the active Media3 track in place and playback reporting carries the selected Jellyfin audio/subtitle stream indices.
- Manually selected audio language now follows the current queue/autoplay chain when the next episode exposes the same language, falls back to Jellyfin's server preference when unavailable, and resets on unrelated manual playback.
- Still Watching dismissal no longer leaves stale continuation state behind; the prompt now exposes a clear `KEEP WATCHING` action and Back terminates the autoplay chain.
- Playback completion and Still Watching now restart the in-app screensaver idle window, preventing long viewing sessions from immediately covering the post-playback/prompt UI with the saver.
- Failed Jellyfin playback start/progress/stop reports are logged with stage and item context instead of being silently discarded.
- Jellyfin 10.11 remux/server-stream URLs are classified as DirectStream when their `TranscodeReasons` contain only Jellyfin's direct-stream-safe container/audio reasons, avoiding full-transcode misreporting when the server intentionally returns `SupportsDirectStream=false`.
- The real-server acceptance harness now proves max-bitrate negotiation on one source at both unconstrained and constrained caps, requiring DirectPlay at the high cap and a bitrate-limited server stream at the low cap.
- Waydroid soak evidence now includes host-tested baseline/final median and peak PSS/RSS plus absolute/percentage growth summaries for leak analysis.
- Media3 buffer preset durations are isolated into a pure Java policy with host execution, preventing accidental drift in the Large/Extra Large load-control ranges.
- Physical-streamer acceptance now proves the configured Still Watching threshold/Keep Watching continuation and Android system MediaSession Play/Pause dispatch end-to-end.
- Long Details action labels use a slightly smaller local scale so `KEEP WATCHING` and `MARK WATCHED` remain single-line under the Extra Large global TV text preset.
- Saved user/server switching and forgetting now perform a full account-scoped media teardown: old-session playback is stopped/reported before credentials are cleared, and queues, autoplay/track carry, pending playback, navigation/detail data, artwork and diagnostics do not leak into the next profile.
- Seeks and media-segment skips now hold the requested logical position briefly until Media3 telemetry catches up, preventing a stale pre-seek position sample from snapping the progress UI/reporting backward immediately after the jump.
- JNI HTTP failures now map common Java network exceptions to concise TV-facing DNS, timeout, connection and TLS/certificate errors while retaining the full Throwable detail in logcat.
- The 5-minute native in-app screensaver path is now physically verified on the Google TV Streamer, including first-key dismissal back to the unchanged underlying screen.
- Background worker tasks now contain unexpected exceptions at the worker boundary, report the failure to logcat, and continue processing later queued work instead of allowing an exception to escape `std::thread` and terminate the app.
- The short-lived API GET cache now prunes expired responses during normal traffic and is capped at 32 entries, preventing unique browse/search URLs from leaving expired JSON resident for the lifetime of the process.
- Automatic transient transport retry is now limited to safe GET/HEAD requests; authentication, Quick Connect, metadata refresh, playback-report POSTs and DELETE actions are never invisibly replayed after an ambiguous connection failure.

## 0.1.0 - 2026-09-01

- Initial public release.
