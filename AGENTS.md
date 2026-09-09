# sloppaTV agent guidance

## Device deployment safety

- sloppaTV has one Android application ID: `app.sloppatv`. Do not create or use a parallel `.test` package for development, benchmarking, or acceptance testing.
- On physical devices, update `app.sloppatv` in place after the package rename. Preserve its app data, login/session state, and user settings.
- The historical package ID was `nz.presley.sloppatv`. On physical devices, do not uninstall or clear that legacy package until any required user data/session migration has been explicitly verified.
- Never uninstall `app.sloppatv`, run `pm clear`, delete its app data, or otherwise destructively reset SloppaTV on a physical device merely to make an APK install succeed. This includes the Google TV Streamer and phones/tablets.
- Waydroid is disposable test infrastructure. It is explicitly allowed to uninstall, clear, or replace `app.sloppatv` in Waydroid when needed for development and visual testing.
- If `adb install -r` reports a signature/certificate mismatch on a physical device such as `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, stop deployment and diagnose the signing identity instead of uninstalling the app. On Waydroid, wiping/reinstalling the app is allowed.
- Debug and Benchmark builds are allowed on the physical Google TV Streamer when needed for acceptance. Keep media volume muted during automated playback testing unless the user explicitly asks otherwise.

## Performance invariants

- Performance regressions are not acceptable. UI, rendering, playback, networking, startup, caching, and feature work must preserve or improve the relevant measured performance baseline unless the user explicitly approves a tradeoff.
