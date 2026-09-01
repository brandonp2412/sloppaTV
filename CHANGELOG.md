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

## 0.1.0 - 2026-09-01

- Initial public release.
