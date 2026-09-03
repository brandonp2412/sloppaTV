# sloppaTV agent guidance

## Device deployment safety

- sloppaTV has one Android application ID: `app.sloppatv`. Do not create or use a parallel `.test` package for development, benchmarking, or acceptance testing.
- Update `app.sloppatv` in place after the package rename. Preserve its app data, login/session state, and user settings.
- The historical package ID was `nz.presley.sloppatv`. During migration, do not uninstall or clear that legacy package until any required user data/session migration has been explicitly verified.
- Never uninstall `app.sloppatv`, run `pm clear`, delete its app data, or otherwise destructively reset the installed app merely to make an APK install succeed.
- If `adb install -r` reports a signature/certificate mismatch such as `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, stop deployment and diagnose the signing identity. Compare the installed APK certificate with the candidate APK certificate and fix the signing/build configuration instead of uninstalling the app.
- Debug and Benchmark builds are allowed on the physical Google TV Streamer when needed for acceptance. Keep media volume muted during automated playback testing unless the user explicitly asks otherwise.

## Acceptance expectations

- Device-visible roadmap work is not complete solely because it builds or passes host tests. Exercise the affected behavior end to end on the selected Android TV target and inspect screenshots/logs where relevant.
- Prefer non-destructive test flows. Restore reversible Jellyfin mutations and user-visible settings after acceptance tests.
- Commit coherent tested changes and keep `ROADMAP.md` current with verified behavior and remaining external blockers.
