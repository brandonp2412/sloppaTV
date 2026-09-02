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

### Fixed

- Launch-intent JNI now attaches the `android_main` thread to the VM instead of reusing `ANativeActivity::env`; a physical-streamer CheckJNI abort exposed the invalid cross-thread `JNIEnv*` use.
- MediaSession lifetime is restricted to active playback so idle Home does not register or retain a sloppaTV media session.
- Subtitle-selected transcodes that fail to prepare now retry the same item without subtitles instead of dropping immediately back to Details; the original queue language preference is retained for later items.

## 0.1.0 - 2026-09-01

- Initial public release.
